#include <QApplication>
#include <QSurfaceFormat>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    // Format musi być ustawiony PRZED utworzeniem QApplication
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setSamples(4);   // antyaliasing
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    MainWindow w;
    w.setWindowTitle("Discretization Viewer");
    w.show();
    return app.exec();
}
