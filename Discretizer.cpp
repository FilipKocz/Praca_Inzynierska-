#include "Discretizer.h"
// Zamiast Medusa_fwd.hpp, który wciąga moduł HDF5:
#include <medusa/bits/types/Vec.hpp>
#include <medusa/bits/domains/DomainDiscretization.hpp>
#include <medusa/bits/domains/DomainShape.hpp>
#include <medusa/bits/domains/BoxShape.hpp>
#include <medusa/bits/domains/BallShape.hpp>
#include <medusa/bits/domains/ShapeUnion.hpp>
#include <medusa/bits/domains/ShapeDifference.hpp>
#include <medusa/bits/domains/PolyhedronShape.hpp>
#include <medusa/bits/domains/GeneralFill.hpp>

#include "MeshIO.h"

#include <cmath>
#include <memory>
#include <stdexcept>

using namespace mm;

namespace {

std::unique_ptr<DomainShape<Vec3d>> makeShape(const ShapeSpec& s) {
    const Vec3d c(s.cx, s.cy, s.cz);
    switch (s.kind) {
        case ShapeKind::Box: {
            Vec3d lo = c, hi = c;
            for (int i = 0; i < 3; ++i) { lo[i] -= s.r; hi[i] += s.r; }
            return std::make_unique<BoxShape<Vec3d>>(lo, hi);
        }
        case ShapeKind::Ball:
            return std::make_unique<BallShape<Vec3d>>(c, s.r);
        case ShapeKind::Mesh: {
            // Plik czyta CGAL (Medusa sama czyta tylko OFF i binarny STL).
            Mesh mesh;
            std::string error;
            if (!loadMesh(s.file, s.r, s.cx, s.cy, s.cz, mesh, error))
                throw std::runtime_error(error);
            return std::make_unique<PolyhedronShape<Vec3d>>(mesh);
        }
    }
    return nullptr;
}

}  // namespace

PointCloud discretize(const std::vector<ShapeSpec>& shapes, double dx) {
    if (shapes.empty()) return {};

    // Osobno, żeby potem policzyć, w ilu bryłach leży punkt.
    std::vector<std::unique_ptr<DomainShape<Vec3d>>> parts;
    for (const ShapeSpec& s : shapes) parts.push_back(makeShape(s));

    // ShapeUnion / ShapeDifference kopiują argumenty, więc domain można nadpisać.
    std::unique_ptr<DomainShape<Vec3d>> domain;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (!shapes[i].subtract) {
            if (!domain) domain.reset(parts[i]->clone());
            else domain = std::make_unique<ShapeUnion<Vec3d>>(*domain, *parts[i]);
        } else if (domain) {
            domain = std::make_unique<ShapeDifference<Vec3d>>(*domain, *parts[i]);
        }
    }
    if (!domain) return {};

    DomainDiscretization<Vec3d> d = domain->discretizeBoundaryWithStep(dx);
    GeneralFill<Vec3d> fill;
    fill.seed(42);
    fill(d, dx);

    PointCloud pc;
    const int n = d.size();
    pc.xyz.reserve(3 * n);
    pc.type.reserve(n);
    pc.normals.reserve(3 * n);
    pc.overlap.reserve(n);
    for (int i = 0; i < n; ++i) {
        const Vec3d& p = d.pos(i);
        pc.xyz.push_back(p[0]);
        pc.xyz.push_back(p[1]);
        pc.xyz.push_back(p[2]);
        pc.type.push_back(d.type(i));

        // Medusa trzyma normalne tylko dla węzłów brzegowych (type < 0).
        const Vec3d nrm = d.type(i) < 0 ? d.normal(i) : Vec3d(0.0, 0.0, 0.0);
        pc.normals.push_back(nrm[0]);
        pc.normals.push_back(nrm[1]);
        pc.normals.push_back(nrm[2]);

        // Tylko bryły dodawane - wycinane nie tworzą części wspólnej.
        int count = 0;
        for (size_t k = 0; k < parts.size(); ++k)
            if (!shapes[k].subtract && parts[k]->contains(p)) ++count;
        pc.overlap.push_back(count);
    }
    return pc;
}

double estimatePointCount(const std::vector<ShapeSpec>& shapes, double dx) {
    double volume = 0.0;
    for (const ShapeSpec& s : shapes) {
        if (s.subtract) continue;
        switch (s.kind) {
            case ShapeKind::Ball: volume += 4.0 / 3.0 * M_PI * s.r * s.r * s.r; break;
            case ShapeKind::Box:  volume += 8.0 * s.r * s.r * s.r; break;
            case ShapeKind::Mesh: volume += s.meshVolume * s.r * s.r * s.r; break;
        }
    }
    return volume / (dx * dx * dx);
}
