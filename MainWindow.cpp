#include "MainWindow.h"
#include "PointCloudView.h"
#include "ShapeDialog.h"
#include <QListWidget>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDockWidget>
#include <QtConcurrent>
#include <QStringList>
#include <QException>
#include <QFileInfo>
#include <exception>
#include <algorithm>
#include <cmath>

namespace {

constexpr double kMaxPoints = 3e6;

QString shapeName(const std::vector<ShapeSpec>& shapes, size_t index) {
    const ShapeKind kind = shapes[index].kind;
    int number = 0;
    for (size_t i = 0; i <= index; ++i)
        if (shapes[i].kind == kind) ++number;
    const char* base = kind == ShapeKind::Ball ? "Kula" : kind == ShapeKind::Box ? "Sześcian" : "Siatka";
    return QString("%1 %2").arg(base).arg(number);
}

QString describe(const std::vector<ShapeSpec>& shapes, size_t index) {
    const ShapeSpec& s = shapes[index];
    const QString center = QString("(%1, %2, %3)").arg(s.cx).arg(s.cy).arg(s.cz);
    const QString op = s.subtract ? QString::fromUtf8("−  ") : QString("+  ");
    if (s.kind == ShapeKind::Mesh)
        return op + QString("%1   %2   przes.=%3   skala=%4")
            .arg(shapeName(shapes, index), QFileInfo(QString::fromStdString(s.file)).fileName(),
                 center)
            .arg(s.r);
    return op + QString("%1   S=%2   %3=%4")
        .arg(shapeName(shapes, index), center, s.kind == ShapeKind::Ball ? "r" : "a/2")
        .arg(s.r);
}

// Bryła schowana w całości w innej nic nie zmienia - ostrzegamy o tym.
bool isInside(const ShapeSpec& in, const ShapeSpec& out) {
    if (in.kind == ShapeKind::Mesh || out.kind == ShapeKind::Mesh) return false;
    const double eps = 1e-9;
    const double c[3] = {in.cx, in.cy, in.cz};
    const double o[3] = {out.cx, out.cy, out.cz};

    if (out.kind == ShapeKind::Box) {
        for (int i = 0; i < 3; ++i)
            if (std::abs(c[i] - o[i]) + in.r > out.r + eps) return false;
        return true;
    }

    double far2 = 0.0;
    if (in.kind == ShapeKind::Ball) {
        const double d = std::sqrt((c[0]-o[0])*(c[0]-o[0]) + (c[1]-o[1])*(c[1]-o[1]) +
                                   (c[2]-o[2])*(c[2]-o[2]));
        return d + in.r <= out.r + eps;
    }
    for (int i = 0; i < 3; ++i) {          // najdalszy narożnik sześcianu
        const double f = std::abs(c[i] - o[i]) + in.r;
        far2 += f * f;
    }
    return std::sqrt(far2) <= out.r + eps;
}

QString containmentWarnings(const std::vector<ShapeSpec>& shapes) {
    QStringList lines;

    for (size_t i = 0; i < shapes.size() && shapes[i].subtract; ++i)
        lines << QString("Uwaga: %1 jest wycinana, zanim cokolwiek dodano - "
                         "przesuń ją niżej na liście.").arg(shapeName(shapes, i));

    for (size_t i = 0; i < shapes.size(); ++i)
        for (size_t j = 0; j < shapes.size(); ++j)
            if (i != j && !shapes[i].subtract && !shapes[j].subtract &&
                isInside(shapes[i], shapes[j]) &&
                !(j > i && isInside(shapes[j], shapes[i]))) {   // identyczne bryły: zgłoś raz
                lines << QString("Uwaga: %1 leży w całości wewnątrz: %2.")
                             .arg(shapeName(shapes, i), shapeName(shapes, j));
                break;
            }
    return lines.join("\n");
}

}  // namespace

MainWindow::MainWindow() {
    view_ = new PointCloudView(this);
    setCentralWidget(view_);

    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);

    layout->addWidget(new QLabel("Bryły"));
    list_ = new QListWidget;
    layout->addWidget(list_);

    auto* addBallBtn = new QPushButton("Dodaj kulę...");
    auto* addBoxBtn = new QPushButton("Dodaj sześcian...");
    auto* addRow = new QHBoxLayout;
    addRow->addWidget(addBallBtn);
    addRow->addWidget(addBoxBtn);
    layout->addLayout(addRow);
    auto* addMeshBtn = new QPushButton("Wczytaj STL...");
    layout->addWidget(addMeshBtn);

    auto* editBtn = new QPushButton("Edytuj...");
    auto* removeBtn = new QPushButton("Usuń");
    auto* editRow = new QHBoxLayout;
    editRow->addWidget(editBtn);
    editRow->addWidget(removeBtn);
    layout->addLayout(editRow);

    // Przy wycinaniu kolejność zmienia wynik.
    auto* upBtn = new QPushButton("W górę");
    auto* downBtn = new QPushButton("W dół");
    auto* orderRow = new QHBoxLayout;
    orderRow->addWidget(upBtn);
    orderRow->addWidget(downBtn);
    layout->addLayout(orderRow);

    auto* form = new QFormLayout;
    dx_ = new QDoubleSpinBox;
    dx_->setDecimals(4);
    dx_->setRange(0.001, 10.0);
    dx_->setSingleStep(0.005);
    dx_->setValue(0.05);
    form->addRow("Krok dx", dx_);

    pointSize_ = new QSlider(Qt::Horizontal);
    pointSize_->setRange(1, 20);
    pointSize_->setValue(4);
    form->addRow("Rozmiar punktu", pointSize_);
    layout->addLayout(form);

    auto* resetBtn = new QPushButton("Resetuj kamerę");
    layout->addWidget(resetBtn);

    status_ = new QLabel;
    status_->setWordWrap(true);
    layout->addWidget(status_);

    auto* dock = new QDockWidget("Parametry", this);
    dock->setWidget(panel);
    addDockWidget(Qt::LeftDockWidgetArea, dock);

    debounce_.setSingleShot(true);
    debounce_.setInterval(200);
    connect(&debounce_, &QTimer::timeout, this, &MainWindow::startJob);

    connect(addBallBtn, &QPushButton::clicked, this, [this] { addShape(ShapeKind::Ball); });
    connect(addBoxBtn, &QPushButton::clicked, this, [this] { addShape(ShapeKind::Box); });
    connect(addMeshBtn, &QPushButton::clicked, this, [this] { addShape(ShapeKind::Mesh); });
    connect(editBtn, &QPushButton::clicked, this, &MainWindow::editShape);
    connect(removeBtn, &QPushButton::clicked, this, &MainWindow::removeShape);
    connect(upBtn, &QPushButton::clicked, this, [this] { moveShape(-1); });
    connect(downBtn, &QPushButton::clicked, this, [this] { moveShape(+1); });
    connect(list_, &QListWidget::itemDoubleClicked, this, &MainWindow::editShape);
    connect(dx_, &QDoubleSpinBox::valueChanged, this, [this] { debounce_.start(); });
    connect(pointSize_, &QSlider::valueChanged, this, [this](int v) { view_->setPointSize(float(v)); });
    connect(resetBtn, &QPushButton::clicked, view_, &PointCloudView::resetCamera);
    connect(&watcher_, &QFutureWatcher<PointCloud>::finished, this, &MainWindow::onJobFinished);

    resize(1200, 800);
    status_->setText("Brak brył. Dodaj kulę, sześcian albo wczytaj plik STL.");
}

void MainWindow::addShape(ShapeKind kind) {
    // Przesunięcie, żeby nowa bryła nie pokryła się dokładnie z poprzednią.
    ShapeSpec proposal;
    proposal.kind = kind;
    if (kind == ShapeKind::Mesh) {
        proposal.r = 1.0;
    } else {
        // Pomijamy siatki - dla nich r to skala, nie rozmiar.
        for (auto it = shapes_.rbegin(); it != shapes_.rend(); ++it) {
            if (it->kind == ShapeKind::Mesh) continue;
            proposal.cx = it->cx + it->r;
            proposal.cy = it->cy;
            proposal.cz = it->cz;
            proposal.r = it->r;
            break;
        }
    }

    ShapeDialog dlg(this);
    dlg.setSpec(proposal);
    if (dlg.exec() != QDialog::Accepted) return;
    shapes_.push_back(dlg.spec());
    shapesChanged();
    list_->setCurrentRow(int(shapes_.size()) - 1);
}

void MainWindow::editShape() {
    const int row = list_->currentRow();
    if (row < 0) return;
    ShapeDialog dlg(this);
    dlg.setSpec(shapes_[row]);
    if (dlg.exec() != QDialog::Accepted) return;
    shapes_[row] = dlg.spec();
    shapesChanged();
    list_->setCurrentRow(row);
}

void MainWindow::removeShape() {
    const int row = list_->currentRow();
    if (row < 0) return;
    shapes_.erase(shapes_.begin() + row);
    shapesChanged();
    list_->setCurrentRow(std::min(row, int(shapes_.size()) - 1));
}

void MainWindow::moveShape(int delta) {
    const int row = list_->currentRow();
    const int target = row + delta;
    if (row < 0 || target < 0 || target >= int(shapes_.size())) return;
    std::swap(shapes_[row], shapes_[target]);
    shapesChanged();
    list_->setCurrentRow(target);
}

void MainWindow::shapesChanged() {
    list_->clear();
    for (size_t i = 0; i < shapes_.size(); ++i) list_->addItem(describe(shapes_, i));
    fitRequested_ = true;
    debounce_.start();
}

void MainWindow::startJob() {
    if (watcher_.isRunning()) {
        pending_ = true;
        return;
    }

    if (shapes_.empty()) {
        view_->setPointCloud(PointCloud{}, false);
        status_->setText("Brak brył. Dodaj kulę, sześcian albo wczytaj plik STL.");
        return;
    }

    const double dx = dx_->value();
    const double estimate = estimatePointCount(shapes_, dx);
    if (estimate > kMaxPoints) {
        status_->setText(QString("Za dużo punktów (ok. %1 mln). Zwiększ dx albo zmniejsz bryły.")
                             .arg(estimate / 1e6, 0, 'f', 1));
        return;
    }

    jobFit_ = fitRequested_;
    fitRequested_ = false;
    status_->setText("Generowanie...");
    clock_.start();
    const std::vector<ShapeSpec> shapes = shapes_;   // kopia dla wątku roboczego
    watcher_.setFuture(QtConcurrent::run([shapes, dx] { return discretize(shapes, dx); }));
}

void MainWindow::onJobFinished() {
    // Wyjątek z wątku roboczego Qt rzuca dopiero przy result().
    PointCloud pc;
    QString error;
    try {
        pc = watcher_.result();
    } catch (const QUnhandledException& e) {
        error = "nieznany błąd";
        try {
            if (e.exception()) std::rethrow_exception(e.exception());
        } catch (const std::exception& inner) {
            error = QString::fromStdString(inner.what());
        } catch (...) {
        }
    } catch (const std::exception& e) {
        error = QString::fromStdString(e.what());
    }
    if (!error.isEmpty()) {
        status_->setText("Błąd generacji: " + error);
        if (pending_) {
            pending_ = false;
            startJob();
        }
        return;
    }

    view_->setPointCloud(pc, jobFit_);
    int common = 0;
    for (int i = 0; i < pc.size(); ++i)
        if (pc.type[i] > 0 && !pc.overlap.empty() && pc.overlap[i] >= 2) ++common;
    int balls = 0, boxes = 0, meshes = 0, cuts = 0;
    for (const ShapeSpec& s : shapes_) {
        balls += (s.kind == ShapeKind::Ball);
        boxes += (s.kind == ShapeKind::Box);
        meshes += (s.kind == ShapeKind::Mesh);
        cuts += s.subtract;
    }
    QString text = QString("Kule: %1, sześciany: %2, siatki: %3 (w tym wycinanych: %4)\n"
                           "Punkty: %5 (w części wspólnej: %6)\nCzas: %7 ms")
                       .arg(balls).arg(boxes).arg(meshes).arg(cuts)
                       .arg(pc.size()).arg(common).arg(clock_.elapsed());
    const QString warnings = containmentWarnings(shapes_);
    if (!warnings.isEmpty()) text += "\n\n" + warnings;
    status_->setText(text);
    if (pending_) {
        pending_ = false;
        startJob();
    }
}
