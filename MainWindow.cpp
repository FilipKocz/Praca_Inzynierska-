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

namespace {

// Powyżej tej (szacowanej) liczby punktów nie uruchamiamy generacji.
constexpr double kMaxPoints = 3e6;

QString describe(const ShapeSpec& s) {
    const QString center = QString("(%1, %2, %3)").arg(s.cx).arg(s.cy).arg(s.cz);
    return s.kind == ShapeKind::Ball
        ? QString("Kula  S=%1  r=%2").arg(center).arg(s.r)
        : QString("Sześcian  S=%1  a/2=%2").arg(center).arg(s.r);
}

}  // namespace

MainWindow::MainWindow() {
    view_ = new PointCloudView(this);
    setCentralWidget(view_);

    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);

    // --- lista brył ---
    layout->addWidget(new QLabel("Bryły"));
    list_ = new QListWidget;
    layout->addWidget(list_);

    auto* addBallBtn = new QPushButton("Dodaj kulę...");
    auto* addBoxBtn  = new QPushButton("Dodaj sześcian...");
    auto* addRow = new QHBoxLayout;
    addRow->addWidget(addBallBtn);
    addRow->addWidget(addBoxBtn);
    layout->addLayout(addRow);

    auto* editBtn = new QPushButton("Edytuj...");
    auto* removeBtn = new QPushButton("Usuń");
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(editBtn);
    buttons->addWidget(removeBtn);
    layout->addLayout(buttons);

    // --- parametry dyskretyzacji i widoku ---
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
    connect(addBoxBtn,  &QPushButton::clicked, this, [this] { addShape(ShapeKind::Box); });
    connect(editBtn, &QPushButton::clicked, this, &MainWindow::editShape);
    connect(removeBtn, &QPushButton::clicked, this, &MainWindow::removeShape);
    connect(list_, &QListWidget::itemDoubleClicked, this, &MainWindow::editShape);
    connect(dx_, &QDoubleSpinBox::valueChanged, this, [this] { debounce_.start(); });
    connect(pointSize_, &QSlider::valueChanged, this, [this](int v) { view_->setPointSize(float(v)); });
    connect(resetBtn, &QPushButton::clicked, view_, &PointCloudView::resetCamera);
    connect(&watcher_, &QFutureWatcher<PointCloud>::finished, this, &MainWindow::onJobFinished);

    resize(1200, 800);
    status_->setText("Brak brył. Kliknij „Dodaj bryłę...”.");
    // Na starcie scena jest pusta, więc nic nie generujemy.
}

void MainWindow::addShape(ShapeKind kind) {
    ShapeSpec proposal;
    proposal.kind = kind;
    if (!shapes_.empty()) {
        const ShapeSpec& last = shapes_.back();
        proposal.cx = last.cx + last.r;   // przesunięcie o promień wzdłuż x
        proposal.cy = last.cy;
        proposal.cz = last.cz;
        proposal.r  = last.r;
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

void MainWindow::shapesChanged() {
    list_->clear();
    for (const ShapeSpec& s : shapes_) list_->addItem(describe(s));
    fitRequested_ = true;
    debounce_.start();
}

void MainWindow::startJob() {
    if (watcher_.isRunning()) {   // poprzednia generacja trwa -> uruchomimy ponownie po niej
        pending_ = true;
        return;
    }

    if (shapes_.empty()) {        // pusta scena: czyścimy widok bez uruchamiania Medusy
        view_->setPointCloud(PointCloud{}, false);
        status_->setText("Brak brył. Kliknij „Dodaj bryłę...”.");
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
    const PointCloud pc = watcher_.result();
    view_->setPointCloud(pc, jobFit_);

    // punkty wnętrza leżące w co najmniej dwóch bryłach
    int common = 0;
    for (int i = 0; i < pc.size(); ++i)
        if (pc.type[i] > 0 && !pc.overlap.empty() && pc.overlap[i] >= 2) ++common;

    status_->setText(QString("Bryły: %1\nPunkty: %2 (w części wspólnej: %3)\nCzas: %4 ms")
                         .arg(shapes_.size()).arg(pc.size()).arg(common).arg(clock_.elapsed()));
    if (pending_) {
        pending_ = false;
        startJob();
    }
}
