#pragma once
#include <vector>

// Warstwa pośrednia: GUI nie widzi żadnych nagłówków Medusy.
// Dzięki temu Medusa (Eigen, szablony) kompiluje się tylko w Discretizer.cpp.

struct PointCloud {
    std::vector<float> xyz;   // x0,y0,z0, x1,y1,z1, ...
    std::vector<int>   type;  // typ węzła z Medusy: <0 brzeg, >0 wnętrze
    int size() const { return static_cast<int>(type.size()); }
    std::vector<int> overlap; // w ilu bryłach leży punkt (>=2 -> część wspólna)
};

enum class ShapeKind { Box, Ball };

// Opis jednej bryły ustawianej z GUI.
// Dla kuli r to promień, dla sześcianu r to połowa długości krawędzi.
struct ShapeSpec {
    ShapeKind kind = ShapeKind::Ball;
    double cx = 0.0, cy = 0.0, cz = 0.0;   // środek
    double r = 0.5;
};


PointCloud discretize(const std::vector<ShapeSpec>& shapes, double dx);


double estimatePointCount(const std::vector<ShapeSpec>& shapes, double dx);
