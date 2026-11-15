#include "ProjectIO.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

const char* kFormat = "DiscretizationViewer";
const int kVersion = 1;

QString kindName(ShapeKind k) {
    switch (k) {
        case ShapeKind::Ball: return "ball";
        case ShapeKind::Box:  return "box";
        case ShapeKind::Mesh: return "mesh";
    }
    return {};
}

bool kindFromName(const QString& name, ShapeKind& k) {
    if (name == "ball") k = ShapeKind::Ball;
    else if (name == "box") k = ShapeKind::Box;
    else if (name == "mesh") k = ShapeKind::Mesh;
    else return false;
    return true;
}

QJsonArray vec3(double x, double y, double z) { return QJsonArray{x, y, z}; }

}  // namespace

bool saveProject(const QString& path, const ProjectState& state, QString& error) {
    // Ścieżki do plików STL zapisujemy względem pliku projektu,
    // żeby folder z projektem dało się przenieść na inny komputer.
    const QDir projectDir = QFileInfo(path).absoluteDir();

    QJsonArray shapes;
    for (const ShapeSpec& s : state.shapes) {
        QJsonObject o;
        o["kind"] = kindName(s.kind);
        o["operation"] = s.subtract ? "subtract" : "add";
        if (s.kind == ShapeKind::Mesh) {
            o["file"] = projectDir.relativeFilePath(QString::fromStdString(s.file));
            o["offset"] = vec3(s.cx, s.cy, s.cz);
            o["scale"] = s.r;
        } else {
            o["center"] = vec3(s.cx, s.cy, s.cz);
            o[s.kind == ShapeKind::Ball ? "radius" : "halfEdge"] = s.r;
        }
        shapes.append(o);
    }

    QJsonObject root;
    root["format"] = kFormat;
    root["version"] = kVersion;
    root["dx"] = state.dx;
    root["pointSize"] = state.pointSize;
    root["shapes"] = shapes;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        error = "Nie można zapisać pliku: " + f.errorString();
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool loadProject(const QString& path, ProjectState& state, QStringList& warnings,
                 QString& error) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        error = "Nie można otworzyć pliku: " + f.errorString();
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parseError);
    if (doc.isNull() || !doc.isObject()) {
        error = "Niepoprawny JSON: " + parseError.errorString();
        return false;
    }
    const QJsonObject root = doc.object();
    if (root["format"].toString() != kFormat) {
        error = "To nie jest plik projektu DiscretizationViewer.";
        return false;
    }
    if (root["version"].toInt() > kVersion)
        warnings << "Plik zapisany nowszą wersją programu - część danych może zostać pominięta.";

    const QDir projectDir = QFileInfo(path).absoluteDir();
    ProjectState loaded;
    loaded.dx = root["dx"].toDouble(0.05);
    loaded.pointSize = root["pointSize"].toInt(4);

    const QJsonArray shapes = root["shapes"].toArray();
    for (int i = 0; i < shapes.size(); ++i) {
        const QJsonObject o = shapes[i].toObject();
        ShapeSpec s;
        if (!kindFromName(o["kind"].toString(), s.kind)) {
            warnings << QString("Bryła %1: nieznany typ \"%2\" - pominięta.")
                            .arg(i + 1).arg(o["kind"].toString());
            continue;
        }
        s.subtract = (o["operation"].toString() == "subtract");

        const QJsonArray c = o[s.kind == ShapeKind::Mesh ? "offset" : "center"].toArray();
        s.cx = c.at(0).toDouble();
        s.cy = c.at(1).toDouble();
        s.cz = c.at(2).toDouble();

        if (s.kind == ShapeKind::Mesh) {
            s.r = o["scale"].toDouble(1.0);
            const QString file = QDir::cleanPath(projectDir.absoluteFilePath(o["file"].toString()));
            s.file = file.toStdString();
            // meshVolume nie jest zapisywane - liczymy z pliku, bo plik mógł się zmienić.
            MeshInfo info;
            std::string meshError;
            if (inspectMeshFile(s.file, info, meshError))
                s.meshVolume = info.volume;
            else
                warnings << QString("Siatka %1 (%2): %3")
                                .arg(i + 1).arg(file, QString::fromStdString(meshError));
        } else {
            s.r = o[s.kind == ShapeKind::Ball ? "radius" : "halfEdge"].toDouble(0.5);
        }
        loaded.shapes.push_back(s);
    }

    state = loaded;
    return true;
}
