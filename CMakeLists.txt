cmake_minimum_required(VERSION 3.16)
project(DiscretizationViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets Concurrent)
find_package(CGAL REQUIRED)

# Medusa budowana z plików źródłowych, bez modułu HDF5 (wymaga biblioteki, której nie używamy).
set(MEDUSA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/medusa)

file(GLOB_RECURSE MEDUSA_SOURCES CONFIGURE_DEPENDS ${MEDUSA_DIR}/src/*.cpp)
list(FILTER MEDUSA_SOURCES EXCLUDE REGEX "[Hh][Dd][Ff]")

add_library(medusa_nohdf STATIC ${MEDUSA_SOURCES})
target_include_directories(medusa_nohdf PUBLIC ${MEDUSA_DIR}/include)
target_compile_definitions(medusa_nohdf PUBLIC _USE_MATH_DEFINES)
target_link_libraries(medusa_nohdf PUBLIC CGAL::CGAL)

# Medusa nie kompiluje się z CGAL 6 (property_map() zwraca std::optional).
# Poprawiona kopia PolyhedronShape.hpp w build/ ma pierwszeństwo przed oryginałem.
if(CGAL_VERSION VERSION_GREATER_EQUAL 6.0)
    set(POLY_SRC ${MEDUSA_DIR}/include/medusa/bits/domains/PolyhedronShape.hpp)
    set(COMPAT_DIR ${CMAKE_CURRENT_BINARY_DIR}/medusa_cgal6_compat)
    file(READ ${POLY_SRC} POLY_CODE)
    # std::tie(a, b) = expr;  ->  { auto o = expr; b = o.has_value(); if (b) a = *o; }
    string(REGEX REPLACE
        "std::tie\\(([A-Za-z0-9_]+),[ \t]*([A-Za-z0-9_]+)\\)[ \t\r\n]*=[ \t\r\n]*([^;]+);"
        "{ auto dv_opt_ = \\3; \\2 = dv_opt_.has_value(); if (\\2) \\1 = *dv_opt_; }"
        POLY_CODE "${POLY_CODE}")
    # Względne include'y muszą wskazywać oryginalny katalog Medusy.
    string(REGEX REPLACE "#include \"([^\"/]+)\"" "#include <medusa/bits/domains/\\1>"
        POLY_CODE "${POLY_CODE}")
    string(FIND "${POLY_CODE}" "std::tie(" POLY_LEFT)
    if(NOT POLY_LEFT EQUAL -1)
        message(WARNING "PolyhedronShape.hpp: nie wszystkie std::tie zostały poprawione.")
    endif()
    file(WRITE ${COMPAT_DIR}/medusa/bits/domains/PolyhedronShape.hpp "${POLY_CODE}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${POLY_SRC})
    target_include_directories(medusa_nohdf BEFORE PUBLIC ${COMPAT_DIR})
    message(STATUS "CGAL ${CGAL_VERSION}: używam poprawionego PolyhedronShape.hpp")
endif()

# MinGW (Windows): limit sekcji w plikach obiektowych ("too many sections").
if(MINGW)
    target_compile_options(medusa_nohdf PUBLIC -Wa,-mbig-obj -Wno-deprecated-declarations)
endif()

add_executable(viewer
    main.cpp
    MainWindow.h MainWindow.cpp
    PointCloudView.h PointCloudView.cpp
    Discretizer.h Discretizer.cpp
    ShapeDialog.h ShapeDialog.cpp
    MeshIO.h MeshIO.cpp
)

target_link_libraries(viewer PRIVATE
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Concurrent
    medusa_nohdf
)
