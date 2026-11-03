cmake_minimum_required(VERSION 3.16)
project(DiscretizationViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets Concurrent)

# --- Medusa bez HDF5 --------------------------------------------------------
# Nie używamy add_subdirectory(external/medusa), bo główny CMakeLists Medusy
# wymaga HDF5 (na Windowsie/MinGW zwykle go brak). Zamiast tego budujemy
# własną bibliotekę statyczną z plików źródłowych Medusy, pomijając moduł HDF.
set(MEDUSA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/medusa)

file(GLOB_RECURSE MEDUSA_SOURCES CONFIGURE_DEPENDS ${MEDUSA_DIR}/src/*.cpp)
# HDF  -> wymaga biblioteki HDF5
# Polyhedron -> wymaga biblioteki CGAL (siatki 3D/STL; wrócimy do tego w celu 2)
list(FILTER MEDUSA_SOURCES EXCLUDE REGEX "[Hh][Dd][Ff]|[Pp]olyhedron")

add_library(medusa_nohdf STATIC ${MEDUSA_SOURCES})
target_include_directories(medusa_nohdf PUBLIC ${MEDUSA_DIR}/include)
target_compile_definitions(medusa_nohdf PUBLIC _USE_MATH_DEFINES)

# MinGW: duże pliki obiektowe Medusy (Eigen + szablony) przekraczają domyślny
# limit sekcji formatu COFF -> "too many sections / file too big".
# -mbig-obj podnosi ten limit. PUBLIC, bo Discretizer.cpp też jest ciężki.
if(MINGW)
    target_compile_options(medusa_nohdf PUBLIC -Wa,-mbig-obj -Wno-deprecated-declarations)
endif()
# ---------------------------------------------------------------------------

add_executable(viewer
    main.cpp
    MainWindow.h MainWindow.cpp
    PointCloudView.h PointCloudView.cpp
    Discretizer.h Discretizer.cpp
    ShapeDialog.h ShapeDialog.cpp
)

target_link_libraries(viewer PRIVATE
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Concurrent
    medusa_nohdf
)
