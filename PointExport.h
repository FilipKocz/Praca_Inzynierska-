#pragma once
#include <string>
#include "Discretizer.h"

// CSV: x,y,z,type,nx,ny,nz,overlap,nn_dist - jeden wiersz na punkt.
bool exportCsv(const PointCloud& pc, const std::string& path, std::string& error);

// VTK legacy (ASCII) - otwiera się bezpośrednio w ParaView.
bool exportVtk(const PointCloud& pc, const std::string& path, std::string& error);
