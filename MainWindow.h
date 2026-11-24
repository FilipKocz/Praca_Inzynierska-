#pragma once
#include <QMainWindow>
#include <QFutureWatcher>
#include <QTimer>
#include <QElapsedTimer>
#include <vector>
#include "Discretizer.h"
#include "ProjectIO.h"

class PointCloudView;
class QListWidget;
class QDoubleSpinBox;
class QSlider;
class QLabel;
class QCloseEvent;
class DensityPanel;
class MetricsPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

protected:
    void closeEvent(QCloseEvent* e) override;

private:
    void buildMenu();
    void onNewProject();
    void onOpenProject();
    bool onSaveProject();
    bool onSaveProjectAs();
    bool writeProject(const QString& path);
    void exportPoints(bool vtk);
    bool maybeSave();          // false = użytkownik anulował
    void setModified(bool modified);
    void updateTitle();
    ProjectState currentState() const;
    void applyState(const ProjectState& state);

    void addShape(ShapeKind kind);
    void editShape();
    void removeShape();
    void moveShape(int delta);   // -1 w górę, +1 w dół
    void shapesChanged();
    void startJob();
    void onJobFinished();

    PointCloudView* view_ = nullptr;
    QListWidget* list_ = nullptr;
    DensityPanel* density_ = nullptr;
    MetricsPanel* metrics_ = nullptr;
    QSlider* pointSize_ = nullptr;
    QLabel* status_ = nullptr;

    std::vector<ShapeSpec> shapes_;

    QTimer debounce_;
    QFutureWatcher<PointCloud> watcher_;
    bool pending_ = false;   // zmiana w trakcie liczenia -> policz jeszcze raz
    bool fitRequested_ = false;
    bool jobFit_ = false;   // fitRequested_ z chwili startu zadania
    QElapsedTimer clock_;

    PointCloud lastCloud_;          // ostatni wynik, do eksportu
    bool cloudUpToDate_ = true;     // czy lastCloud_ odpowiada obecnym ustawieniom
    QString projectPath_;
    bool loading_ = false;          // przy wczytywaniu projektu nie oznaczamy zmian
};
