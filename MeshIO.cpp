#include "MeshIO.h"
#include "Discretizer.h"

#include <CGAL/Polygon_mesh_processing/IO/polygon_mesh_io.h>
#include <CGAL/Polygon_mesh_processing/bbox.h>
#include <CGAL/Polygon_mesh_processing/measure.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/boost/graph/helpers.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <thread>

namespace PMP = CGAL::Polygon_mesh_processing;

namespace {

// CGAL rozpoznaje format tylko po rozszerzeniu pisanym małymi literami.
std::string lowerExtension(const std::string& path) {
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return {};
    std::string ext = path.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

}  // namespace

bool loadMesh(const std::string& path, double scale, double tx, double ty, double tz,
              Mesh& out, std::string& error) {
    out.clear();

    if (!std::ifstream(path)) {
        error = "Nie można otworzyć pliku.";
        return false;
    }
    const std::string ext = lowerExtension(path);
    if (ext != "stl" && ext != "off" && ext != "obj" && ext != "ply") {
        error = "Nieobsługiwany format (dozwolone: STL, OFF, OBJ, PLY).";
        return false;
    }

    // Wersja z PMP skleja wierzchołki - w STL każdy trójkąt ma własne kopie.
    std::string readPath = path;
    std::string tempPath;
    if (path.compare(path.size() - ext.size(), ext.size(), ext) != 0) {
        // ".STL": czytamy przez kopię. Nazwa zależy od wątku, bo plik czyta też GUI.
        const auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
        tempPath = (std::filesystem::temp_directory_path() /
                    ("dv_mesh_" + std::to_string(tid) + "." + ext)).string();
        std::filesystem::copy_file(path, tempPath,
                                   std::filesystem::copy_options::overwrite_existing);
        readPath = tempPath;
    }
    const bool ok = PMP::IO::read_polygon_mesh(readPath, out);
    if (!tempPath.empty()) std::filesystem::remove(tempPath);

    if (!ok || out.is_empty()) {
        error = "Nie udało się wczytać siatki (plik uszkodzony albo powierzchnia "
                "nie jest rozmaitością).";
        return false;
    }
    if (!CGAL::is_triangle_mesh(out)) {
        error = "Siatka musi składać się z trójkątów.";
        return false;
    }
    if (!CGAL::is_closed(out)) {
        error = "Siatka nie jest zamknięta (ma dziury), więc nie da się określić "
                "jej wnętrza.";
        return false;
    }

    // Normalne muszą wskazywać na zewnątrz (używa ich Medusa).
    if (!PMP::is_outward_oriented(out)) PMP::reverse_face_orientations(out);

    for (auto v : out.vertices()) {
        const auto& p = out.point(v);
        out.point(v) = MeshKernel::Point_3(p.x() * scale + tx, p.y() * scale + ty,
                                           p.z() * scale + tz);
    }
    return true;
}

bool inspectMeshFile(const std::string& path, MeshInfo& info, std::string& error) {
    Mesh mesh;
    if (!loadMesh(path, 1.0, 0.0, 0.0, 0.0, mesh, error)) return false;

    info.vertices = static_cast<int>(mesh.number_of_vertices());
    info.faces = static_cast<int>(mesh.number_of_faces());
    info.volume = PMP::volume(mesh);

    const auto b = PMP::bbox(mesh);
    info.min[0] = b.xmin(); info.min[1] = b.ymin(); info.min[2] = b.zmin();
    info.max[0] = b.xmax(); info.max[1] = b.ymax(); info.max[2] = b.zmax();
    return true;
}
