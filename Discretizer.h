#pragma once
#include <string>
#include <vector>

// GUI nie widzi nagłówków Medusy: ciężki kod (Eigen, szablony) jest tylko w Discretizer.cpp.

struct PointCloud {
    std::vector<double> xyz;      // double, bo idzie do eksportu (float: ~7 cyfr)
    std::vector<int>    type;     // <0 brzeg, >0 wnętrze
    std::vector<double> normals;  // nx,ny,nz; zewnętrzna normalna brzegu, 0 dla wnętrza
    std::vector<int>    overlap;  // w ilu bryłach leży punkt
    std::vector<double> nnDist;   // d_min: odległość do najbliższego sąsiada
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

enum class DensityMode { Constant, Profile, Formula };

// Funkcja zagęszczenia h = odległość między sąsiednimi punktami.
// s = odległość punktu od brzegu obszaru.
//   Constant: h
//   Profile:  hMin dla s <= s1, liniowo do hMax przy s2, hMax dla s >= s2
//   Formula:  dowolny wzór h(x, y, z, s)
struct DensitySpec {
    DensityMode mode = DensityMode::Constant;
    double h = 0.05;
    double hMin = 0.02, hMax = 0.1;
    double s1 = 0.0, s2 = 0.3;
    std::string formula = "0.02 + 0.1*s";
};

// Wartość profilu (tryb Profile) - wspólna dla Medusy i wykresu w GUI.
double profileH(const DensitySpec& d, double s);

// Bryły łączone w kolejności listy, jak `a - b + c` w Medusie.
// Rzuca std::runtime_error, gdy wzór jest błędny albo punktów byłoby więcej niż maxPoints.
PointCloud discretize(const std::vector<ShapeSpec>& shapes, const DensitySpec& density,
                      double maxPoints);

// Szybki szacunek przed uruchomieniem Medusy; 0 = nie da się oszacować (wzór).
double estimatePointCount(const std::vector<ShapeSpec>& shapes, const DensitySpec& density);
