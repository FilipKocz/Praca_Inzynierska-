#include "Density.h"
#include "Expression.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

double profileH(const DensitySpec& d, double s) {
    if (s <= d.s1) return d.hMin;
    if (s >= d.s2) return d.hMax;
    const double t = (s - d.s1) / (d.s2 - d.s1);
    return d.hMin + t * (d.hMax - d.hMin);
}

DensityFunction makeDensity(const DensitySpec& spec) {
    DensityFunction raw;
    switch (spec.mode) {
        case DensityMode::Constant: {
            const double h = spec.h;
            raw = [h](double, double, double, double) { return h; };
            break;
        }
        case DensityMode::Profile: {
            raw = [spec](double, double, double, double s) { return profileH(spec, s); };
            break;
        }
        case DensityMode::Formula: {
            Expression e;
            std::string error;
            if (!e.parse(spec.formula, error)) throw std::runtime_error("Błąd we wzorze h: " + error);
            raw = [e](double x, double y, double z, double s) { return e.eval(x, y, z, s); };
            break;
        }
    }
    return [raw](double x, double y, double z, double s) {
        const double h = raw(x, y, z, s);
        if (!(h > 0.0) || !std::isfinite(h)) {
            std::ostringstream msg;
            msg << "Funkcja zagęszczenia daje h = " << h << " w punkcie (" << x << ", " << y
                << ", " << z << "), s = " << s << ". Wartość h musi być dodatnia.";
            throw std::runtime_error(msg.str());
        }
        return h;
    };
}

double estimatePointCount(const std::vector<ShapeSpec>& shapes, const DensitySpec& density) {
    double volume = 0.0, area = 0.0;
    for (const ShapeSpec& s : shapes) {
        if (s.subtract) continue;
        switch (s.kind) {
            case ShapeKind::Ball:
                volume += 4.0 / 3.0 * M_PI * std::pow(s.r, 3);
                area += 4.0 * M_PI * s.r * s.r;
                break;
            case ShapeKind::Box:
                volume += 8.0 * std::pow(s.r, 3);
                area += 24.0 * s.r * s.r;
                break;
            case ShapeKind::Mesh: {
                const double v = s.meshVolume * std::pow(s.r, 3);
                volume += v;
                area += 4.84 * std::pow(v, 2.0 / 3.0);   // pole kuli o tej objętości (dolne oszacowanie)
                break;
            }
        }
    }

    if (density.mode == DensityMode::Constant)
        return volume / std::pow(density.h, 3);
    if (density.mode == DensityMode::Formula)
        return 0.0;   // wzór może zależeć od x, y, z - sprawdza to dopiero Discretizer

    // Profil: warstwa przy brzegu o grubości ds ma objętość ~ area*ds
    // (przybliżenie cienkiej warstwy), sumujemy aż do pełnej objętości.
    double count = area / std::pow(density.hMin, 2);   // punkty brzegu
    const double ds = std::max(density.hMin, 1e-12) / 4.0;
    double filled = 0.0;
    for (double s = 0.0; filled < volume; s += ds) {
        const double layer = std::min(area * ds, volume - filled);
        count += layer / std::pow(profileH(density, s), 3);
        filled += layer;
        if (count > 1e12) break;
    }
    return count;
}
