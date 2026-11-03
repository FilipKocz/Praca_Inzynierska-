#include "Discretizer.h"
// Konkretne nagłówki zamiast Medusa_fwd.hpp, żeby nie ciągnąć modułu HDF5
#include <medusa/bits/types/Vec.hpp>
#include <medusa/bits/domains/DomainDiscretization.hpp>
#include <medusa/bits/domains/DomainShape.hpp>
#include <medusa/bits/domains/BoxShape.hpp>
#include <medusa/bits/domains/BallShape.hpp>
#include <medusa/bits/domains/ShapeUnion.hpp>
#include <medusa/bits/domains/GeneralFill.hpp>

#include <cmath>
#include <memory>

using namespace mm;

namespace {

// Zamienia opis z GUI na kształt Medusy.
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
    }
    return nullptr;
}

}  // namespace

PointCloud discretize(const std::vector<ShapeSpec>& shapes, double dx) {
    if (shapes.empty()) return {};

    // NOWE: pojedyncze bryły zachowujemy osobno - potrzebne są do sprawdzenia,
    // w ilu bryłach leży każdy punkt.
    std::vector<std::unique_ptr<DomainShape<Vec3d>>> parts;
    for (const ShapeSpec& s : shapes) parts.push_back(makeShape(s));

    // Suma brył: A ∪ B ∪ C ... budowana parami.
    // ShapeUnion robi kopię obu argumentów, więc stary obiekt można bezpiecznie
    // zastąpić nowym. To samo daje zapis `a + b` na kształtach Medusy.
    std::unique_ptr<DomainShape<Vec3d>> domain(parts[0]->clone());
    for (size_t i = 1; i < parts.size(); ++i)
        domain = std::make_unique<ShapeUnion<Vec3d>>(*domain, *parts[i]);

    // 1) węzły na brzegu sumy (ShapeUnion usuwa części brzegów leżące
    //    wewnątrz innych brył), 2) wypełnienie wnętrza.
    DomainDiscretization<Vec3d> d = domain->discretizeBoundaryWithStep(dx);
    GeneralFill<Vec3d> fill;
    fill.seed(42);
    fill(d, dx);

    PointCloud pc;
    const int n = d.size();
    pc.xyz.reserve(3 * n);
    pc.type.reserve(n);
    pc.overlap.reserve(n);   // NOWE
    for (int i = 0; i < n; ++i) {
        const Vec3d& p = d.pos(i);
        pc.xyz.push_back(static_cast<float>(p[0]));
        pc.xyz.push_back(static_cast<float>(p[1]));
        pc.xyz.push_back(static_cast<float>(p[2]));
        pc.type.push_back(d.type(i));

        // NOWE: w ilu bryłach leży punkt - contains() z Medusy,
        // ten sam test, którego używa ShapeUnion.
        int count = 0;
        for (const auto& part : parts)
            if (part->contains(p)) ++count;
        pc.overlap.push_back(count);
    }
    return pc;
}

double estimatePointCount(const std::vector<ShapeSpec>& shapes, double dx) {
    // Suma objętości przeszacowuje przy nachodzeniu brył; jako bezpiecznik wystarczy.
    double volume = 0.0;
    for (const ShapeSpec& s : shapes) {
        if (s.kind == ShapeKind::Ball) volume += 4.0 / 3.0 * M_PI * s.r * s.r * s.r;
        else                           volume += 8.0 * s.r * s.r * s.r;
    }
    return volume / (dx * dx * dx);
}
