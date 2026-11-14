#include "ShapeDialog.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

namespace {

QDoubleSpinBox* makeSpin(double min, double max, double value) {
    auto* s = new QDoubleSpinBox;
    s->setDecimals(3);
    s->setRange(min, max);
    s->setSingleStep(0.1);
    s->setValue(value);
    return s;
}

}  // namespace

ShapeDialog::ShapeDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Bryła");
    setMinimumWidth(460);

    kind_ = new QComboBox;
    kind_->addItem("Kula", int(ShapeKind::Ball));
    kind_->addItem("Sześcian", int(ShapeKind::Box));
    kind_->addItem("Siatka z pliku (STL)", int(ShapeKind::Mesh));

    op_ = new QComboBox;
    op_->addItem("Dodaj do obszaru (suma)", false);
    op_->addItem("Wytnij z obszaru (różnica)", true);
    op_->setToolTip("Operacje wykonywane są w kolejności listy brył,\n"
                    "np. sześcian, potem wycięta kula, potem dodany mały sześcian.");

    file_ = new QLineEdit;
    file_->setReadOnly(true);
    file_->setPlaceholderText("nie wybrano pliku");
    auto* browse = new QPushButton("Wybierz...");
    fileRow_ = new QWidget;
    auto* fileLayout = new QHBoxLayout(fileRow_);
    fileLayout->setContentsMargins(0, 0, 0, 0);
    fileLayout->addWidget(file_);
    fileLayout->addWidget(browse);
    fileLabel_ = new QLabel("Plik");
    fileInfo_ = new QLabel;
    fileInfo_->setWordWrap(true);

    cx_ = makeSpin(-1e6, 1e6, 0.0);
    cy_ = makeSpin(-1e6, 1e6, 0.0);
    cz_ = makeSpin(-1e6, 1e6, 0.0);
    auto* center = new QHBoxLayout;
    center->addWidget(new QLabel("x"));
    center->addWidget(cx_);
    center->addWidget(new QLabel("y"));
    center->addWidget(cy_);
    center->addWidget(new QLabel("z"));
    center->addWidget(cz_);
    centerLabel_ = new QLabel;

    r_ = makeSpin(0.0001, 1e6, 0.5);
    r_->setDecimals(4);
    sizeLabel_ = new QLabel;

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &ShapeDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* form = new QFormLayout(this);
    form->addRow("Typ", kind_);
    form->addRow("Operacja", op_);
    form->addRow(fileLabel_, fileRow_);
    form->addRow(fileInfo_);
    form->addRow(centerLabel_, center);
    form->addRow(sizeLabel_, r_);
    form->addRow(buttons);

    connect(kind_, &QComboBox::currentIndexChanged, this, &ShapeDialog::updateForKind);
    connect(browse, &QPushButton::clicked, this, &ShapeDialog::chooseFile);
    updateForKind();
}

void ShapeDialog::updateForKind() {
    const auto kind = static_cast<ShapeKind>(kind_->currentData().toInt());
    const bool mesh = (kind == ShapeKind::Mesh);

    fileLabel_->setVisible(mesh);
    fileRow_->setVisible(mesh);
    fileInfo_->setVisible(mesh);

    centerLabel_->setText(mesh ? "Przesunięcie" : "Środek");
    sizeLabel_->setText(mesh ? "Skala" : kind == ShapeKind::Ball ? "Promień" : "Połowa krawędzi");
    adjustSize();
}

void ShapeDialog::chooseFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Wybierz plik geometrii", file_->text(),
        "Siatki 3D (*.stl *.STL *.off *.obj *.ply);;Wszystkie pliki (*)");
    if (path.isEmpty()) return;
    file_->setText(path);
    if (inspectFile(path)) {
        r_->setValue(1.0);
        cx_->setValue(0.0);
        cy_->setValue(0.0);
        cz_->setValue(0.0);
    }
}

bool ShapeDialog::inspectFile(const QString& path) {
    MeshInfo info;
    std::string error;
    fileOk_ = inspectMeshFile(path.toStdString(), info, error);
    if (!fileOk_) {
        fileInfo_->setText("Błąd: " + QString::fromStdString(error));
        adjustSize();
        return false;
    }
    meshVolume_ = info.volume;
    fileInfo_->setText(
        QString("Wierzchołki: %1, trójkąty: %2\nWymiary (skala 1): %3 × %4 × %5\n"
                "Zakres x: %6 … %7")
            .arg(info.vertices).arg(info.faces)
            .arg(info.max[0] - info.min[0], 0, 'g', 4)
            .arg(info.max[1] - info.min[1], 0, 'g', 4)
            .arg(info.max[2] - info.min[2], 0, 'g', 4)
            .arg(info.min[0], 0, 'g', 4).arg(info.max[0], 0, 'g', 4));
    adjustSize();
    return true;
}

void ShapeDialog::accept() {
    const auto kind = static_cast<ShapeKind>(kind_->currentData().toInt());
    if (kind == ShapeKind::Mesh && !fileOk_) {
        QMessageBox::warning(this, "Siatka z pliku",
                             file_->text().isEmpty() ? "Wybierz plik siatki."
                                                     : "Wybrany plik nie nadaje się do użycia.\n" +
                                                           fileInfo_->text());
        return;
    }
    QDialog::accept();
}

void ShapeDialog::setSpec(const ShapeSpec& s) {
    kind_->setCurrentIndex(kind_->findData(int(s.kind)));
    op_->setCurrentIndex(s.subtract ? 1 : 0);
    cx_->setValue(s.cx);
    cy_->setValue(s.cy);
    cz_->setValue(s.cz);
    r_->setValue(s.r);
    file_->setText(QString::fromStdString(s.file));
    fileOk_ = false;
    if (s.kind == ShapeKind::Mesh && !s.file.empty()) inspectFile(file_->text());
}

ShapeSpec ShapeDialog::spec() const {
    ShapeSpec s;
    s.kind = static_cast<ShapeKind>(kind_->currentData().toInt());
    s.subtract = op_->currentData().toBool();
    s.cx = cx_->value();
    s.cy = cy_->value();
    s.cz = cz_->value();
    s.r = r_->value();
    if (s.kind == ShapeKind::Mesh) {
        s.file = file_->text().toStdString();
        s.meshVolume = meshVolume_;
    }
    return s;
}
