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
    void moveShape(int delta);   // -1 w górę, +1 w dół
    void shapesChanged();
    void startJob();
    void onJobFinished();

    PointCloudView* view_ = nullptr;
    QListWidget* list_ = nullptr;
    QDoubleSpinBox* dx_ = nullptr;
    QSlider* pointSize_ = nullptr;
    QLabel* status_ = nullptr;

    std::vector<ShapeSpec> shapes_;

    QTimer debounce_;
    QFutureWatcher<PointCloud> watcher_;
    bool pending_ = false;   // zmiana w trakcie liczenia -> policz jeszcze raz
    bool fitRequested_ = false;
    bool jobFit_ = false;   // fitRequested_ z chwili startu zadania
    QElapsedTimer clock_;
};
