#include "PointExport.h"
#include <fstream>
#include <limits>
#include <locale>

namespace {

// Locale "C": kropka dziesiętna niezależnie od ustawień systemu (w pl_PL byłby przecinek).
// max_digits10: zapis double bez utraty precyzji.
bool openOut(std::ofstream& out, const std::string& path, std::string& error) {
    out.open(path);
    if (!out) {
        error = "Nie można zapisać pliku: " + path;
        return false;
    }
    out.imbue(std::locale::classic());
    out.precision(std::numeric_limits<double>::max_digits10);
    return true;
}

double normalAt(const PointCloud& pc, int i, int k) {
    return pc.normals.empty() ? 0.0 : pc.normals[3 * i + k];
}

int overlapAt(const PointCloud& pc, int i) {
    return pc.overlap.empty() ? 1 : pc.overlap[i];
}

}  // namespace

bool exportCsv(const PointCloud& pc, const std::string& path, std::string& error) {
    std::ofstream out;
    if (!openOut(out, path, error)) return false;

    out << "x,y,z,type,nx,ny,nz,overlap\n";
    for (int i = 0; i < pc.size(); ++i) {
        out << pc.xyz[3 * i] << ',' << pc.xyz[3 * i + 1] << ',' << pc.xyz[3 * i + 2] << ','
            << pc.type[i] << ','
            << normalAt(pc, i, 0) << ',' << normalAt(pc, i, 1) << ',' << normalAt(pc, i, 2) << ','
            << overlapAt(pc, i) << '\n';
    }
    if (!out) {
        error = "Błąd zapisu pliku: " + path;
        return false;
    }
    return true;
}

bool exportVtk(const PointCloud& pc, const std::string& path, std::string& error) {
    std::ofstream out;
    if (!openOut(out, path, error)) return false;
    const int n = pc.size();

    out << "# vtk DataFile Version 3.0\n"
        << "Dyskretyzacja (DiscretizationViewer)\n"
        << "ASCII\n"
        << "DATASET POLYDATA\n"
        << "POINTS " << n << " double\n";
    for (int i = 0; i < n; ++i)
        out << pc.xyz[3 * i] << ' ' << pc.xyz[3 * i + 1] << ' ' << pc.xyz[3 * i + 2] << '\n';

    // Bez komórek VERTICES ParaView wczyta punkty, ale ich nie narysuje.
    out << "VERTICES " << n << ' ' << 2 * n << '\n';
    for (int i = 0; i < n; ++i) out << "1 " << i << '\n';

    out << "POINT_DATA " << n << '\n'
        << "SCALARS type int 1\nLOOKUP_TABLE default\n";
    for (int i = 0; i < n; ++i) out << pc.type[i] << '\n';
    out << "SCALARS overlap int 1\nLOOKUP_TABLE default\n";
    for (int i = 0; i < n; ++i) out << overlapAt(pc, i) << '\n';
    out << "NORMALS normals double\n";
    for (int i = 0; i < n; ++i)
        out << normalAt(pc, i, 0) << ' ' << normalAt(pc, i, 1) << ' ' << normalAt(pc, i, 2) << '\n';

    if (!out) {
        error = "Błąd zapisu pliku: " + path;
        return false;
    }
    return true;
}
