#pragma once
#include <functional>
#include "Discretizer.h"

using DensityFunction = std::function<double(double x, double y, double z, double s)>;

// Rzuca std::runtime_error przy błędnym wzorze. Zwrócona funkcja rzuca,
// gdy h wyjdzie <= 0 albo NaN - inaczej GeneralFill zapętliłby się.
DensityFunction makeDensity(const DensitySpec& spec);
