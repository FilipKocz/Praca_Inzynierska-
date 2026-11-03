#include "ShapeDialog.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>

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

    kind_ = new QComboBox;
    kind_->addItem("Kula", int(ShapeKind::Ball));
    kind_->addItem("Sześcian", int(ShapeKind::Box));

    cx_ = makeSpin(-1000.0, 1000.0, 0.0);
    cy_ = makeSpin(-1000.0, 1000.0, 0.0);
    cz_ = makeSpin(-1000.0, 1000.0, 0.0);
    auto* center = new QHBoxLayout;
    center->addWidget(new QLabel("x"));
    center->addWidget(cx_);
    center->addWidget(new QLabel("y"));
    center->addWidget(cy_);
    center->addWidget(new QLabel("z"));
    center->addWidget(cz_);

    r_ = makeSpin(0.001, 1000.0, 0.5);
    sizeLabel_ = new QLabel;

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* form = new QFormLayout(this);
    form->addRow("Typ", kind_);
    form->addRow("Środek", center);
    form->addRow(sizeLabel_, r_);
    form->addRow(buttons);

    connect(kind_, &QComboBox::currentIndexChanged, this, &ShapeDialog::updateSizeLabel);
    updateSizeLabel();
}

void ShapeDialog::updateSizeLabel() {
    const auto kind = static_cast<ShapeKind>(kind_->currentData().toInt());
    sizeLabel_->setText(kind == ShapeKind::Ball ? "Promień" : "Połowa krawędzi");
}

void ShapeDialog::setSpec(const ShapeSpec& s) {
    kind_->setCurrentIndex(kind_->findData(int(s.kind)));
    cx_->setValue(s.cx);
    cy_->setValue(s.cy);
    cz_->setValue(s.cz);
    r_->setValue(s.r);
}

ShapeSpec ShapeDialog::spec() const {
    ShapeSpec s;
    s.kind = static_cast<ShapeKind>(kind_->currentData().toInt());
    s.cx = cx_->value();
    s.cy = cy_->value();
    s.cz = cz_->value();
    s.r = r_->value();
    return s;
}
