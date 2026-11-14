#pragma once
// Nagłówki CGAL są ciężkie: ten plik dołączają tylko Discretizer.cpp i MeshIO.cpp.

#include <CGAL/Simple_cartesian.h>
#include <CGAL/Surface_mesh.h>
#include <string>

// Ten sam typ, którego oczekuje mm::PolyhedronShape.
using MeshKernel = CGAL::Simple_cartesian<double>;
using Mesh = CGAL::Surface_mesh<MeshKernel::Point_3>;

// Akceptuje tylko zamkniętą siatkę trójkątną - inaczej nie da się określić wnętrza.
bool loadMesh(const std::string& path, double scale, double tx, double ty, double tz,
              Mesh& out, std::string& error);
