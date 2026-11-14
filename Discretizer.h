#pragma once
#include <string>
#include <vector>

// GUI nie widzi nagłówków Medusy: ciężki kod (Eigen, szablony) jest tylko w Discretizer.cpp.

struct PointCloud {
    std::vector<float> xyz;
    std::vector<int>   type;  // <0 brzeg, >0 wnętrze
    std::vector<int>   overlap;  // w ilu bryłach leży punkt
    int size() const { return static_cast<int>(type.size()); }
};

enum class ShapeKind { Box, Ball, Mesh };

// Sześcian: r = połowa krawędzi. Siatka: (cx, cy, cz) = przesunięcie, r = skala.
struct ShapeSpec {
    ShapeKind kind = ShapeKind::Ball;
    bool subtract = false;
    double cx = 0.0, cy = 0.0, cz = 0.0;
    double r = 0.5;
    std::string file;
    double meshVolume = 0.0;   // w skali 1
};

struct MeshInfo {
    int vertices = 0, faces = 0;
    double volume = 0.0;
    double min[3] = {0, 0, 0}, max[3] = {0, 0, 0};
};

bool inspectMeshFile(const std::string& path, MeshInfo& info, std::string& error);

// Bryły łączone w kolejności listy, jak `a - b + c` w Medusie.
PointCloud discretize(const std::vector<ShapeSpec>& shapes, double dx);

double estimatePointCount(const std::vector<ShapeSpec>& shapes, double dx);
