#pragma once
#include <QMainWindow>
#include <QFutureWatcher>
#include <QTimer>
#include <QElapsedTimer>
#include <vector>
#include "Discretizer.h"

class PointCloudView;
class QListWidget;
class QDoubleSpinBox;
class QSlider;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

private:
    void addShape(ShapeKind kind);
    void editShape();
    void removeShape();
    void shapesChanged();     // odświeża listę i zleca nową generację
    void startJob();
    void onJobFinished();

    PointCloudView* view_ = nullptr;
    QListWidget* list_ = nullptr;
    QDoubleSpinBox* dx_ = nullptr;
    QSlider* pointSize_ = nullptr;
    QLabel* status_ = nullptr;

    std::vector<ShapeSpec> shapes_;         // bryły w scenie (na starcie puste)

    QTimer debounce_;                       // nie generujemy przy każdym kliknięciu spinboxa
    QFutureWatcher<PointCloud> watcher_;    // generacja w osobnym wątku -> GUI nie zamarza
    bool pending_ = false;                  // w trakcie liczenia przyszła zmiana -> policz ponownie
    bool fitRequested_ = false;             // po zmianie brył dopasuj kamerę do nowej sceny
    bool jobFit_ = false;                   // czy bieżące zadanie ma dopasować kamerę
    QElapsedTimer clock_;
};
