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
#include <medusa/bits/spatial_search/KDTree.hpp>

#include "Density.h"
#include "MeshIO.h"

#include <cmath>
#include <memory>
#include <random>
#include <sstream>
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

// Szacunek N ~ objętość * średnia z 1/h^3 po losowych punktach obszaru.
// Robiony przed GeneralFill, bo tego wypełniania nie da się przerwać.
void checkPointCount(const DomainShape<Vec3d>& domain,
                     const std::function<double(const Vec3d&)>& h,
                     int boundaryCount, double maxPoints) {
    const auto [lo, hi] = domain.bbox();
    const double boxVolume = (hi[0] - lo[0]) * (hi[1] - lo[1]) * (hi[2] - lo[2]);
    std::mt19937 gen(7);
    std::uniform_real_distribution<double> u(0.0, 1.0);
    const int samples = 20000;
    double sum = 0.0;
    for (int i = 0; i < samples; ++i) {
        Vec3d p;
        for (int k = 0; k < 3; ++k) p[k] = lo[k] + u(gen) * (hi[k] - lo[k]);
        if (domain.contains(p)) sum += 1.0 / std::pow(h(p), 3);
    }
    const double estimate = boundaryCount + boxVolume * sum / samples;
    if (estimate > maxPoints) {
        std::ostringstream msg;
        msg.precision(2);
        msg << std::fixed << "Za dużo punktów (ok. " << estimate / 1e6
            << " mln). Zwiększ h albo zmniejsz bryły.";
        throw std::runtime_error(msg.str());
    }
}

// Medusa przy odejmowaniu (DomainDiscretization::subtract) dodaje węzły brzegu wycinanej
// bryły, które leżą w pierwszej bryle *razem z marginesem*. Gdy bryły mają wspólną ścianę
// (np. dwa sześciany o tym samym boku), węzły tej ściany przechodzą test, choć obszaru po
// żadnej stronie już nie ma - zostaje "brzeg o zerowej grubości".
// Prawdziwy węzeł brzegowy ma obszar tuż pod sobą (p - eps*n) i nie ma go tuż nad sobą (p + eps*n).
void removeDegenerateBoundary(DomainDiscretization<Vec3d>& d, const DomainShape<Vec3d>& domain) {
    if (!domain.hasContains()) return;
    const auto [lo, hi] = domain.bbox();
    const double eps = 1e-6 * (hi - lo).norm();   // >> margines Medusy (1e-10)
    Range<int> toRemove;
    for (int i = 0; i < d.size(); ++i) {
        if (d.type(i) >= 0) continue;
        const Vec3d& p = d.pos(i);
        const Vec3d& n = d.normal(i);
        if (!domain.contains(p - eps * n) || domain.contains(p + eps * n))
            toRemove.push_back(i);
    }
    if (!toRemove.empty()) d.removeNodes(toRemove);
}

}  // namespace

PointCloud discretize(const std::vector<ShapeSpec>& shapes, const DensitySpec& density,
                      double maxPoints) {
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

    const DensityFunction h = makeDensity(density);

    // Na brzegu s = 0.
    DomainDiscretization<Vec3d> d = domain->discretizeBoundaryWithDensity(
        [&](const Vec3d& p) { return h(p[0], p[1], p[2], 0.0); });
    if (d.size() == 0) return {};

    removeDegenerateBoundary(d, *domain);
    if (d.size() == 0) return {};

    // s(p) = odległość do najbliższego punktu brzegu (drzewo KD z Medusy).
    // Działa dla każdego kształtu, także STL i wycięć; błąd s jest rzędu h przy brzegu.
    // Drzewo kopiuje punkty, więc węzły dodawane przez GeneralFill go nie zmieniają.
    const KDTree<Vec3d> boundaryTree(d.positions());
    const std::function<double(const Vec3d&)> hFill = [&](const Vec3d& p) {
        const double s = std::sqrt(boundaryTree.query(p).second[0]);
        return h(p[0], p[1], p[2], s);
    };

    checkPointCount(*domain, hFill, d.size(), maxPoints);

    GeneralFill<Vec3d> fill;
    fill.seed(42);
    fill(d, hFill);

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

    // d_min(i): drzewo ze wszystkich punktów; pytamy o 2 najbliższe,
    // bo pierwszym jest sam punkt (odległość 0).
    if (n >= 2) {
        const KDTree<Vec3d> allTree(d.positions());
        pc.nnDist.resize(n);
        for (int i = 0; i < n; ++i)
            pc.nnDist[i] = std::sqrt(allTree.query(d.pos(i), 2).second[1]);
    }
    return pc;
}
