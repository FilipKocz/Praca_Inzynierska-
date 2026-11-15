#pragma once
#include <QString>
#include <QStringList>
#include <vector>
#include "Discretizer.h"

// Wszystko, co trzeba, żeby odtworzyć scenę. Punktów nie zapisujemy -
// po wczytaniu generuje je ponownie Medusa (GeneralFill ma stałe ziarno).
struct ProjectState {
    std::vector<ShapeSpec> shapes;
    double dx = 0.05;
    int pointSize = 4;
};

bool saveProject(const QString& path, const ProjectState& state, QString& error);

// warnings: problemy, które nie blokują wczytania (np. brak pliku STL).
bool loadProject(const QString& path, ProjectState& state, QStringList& warnings,
                 QString& error);
