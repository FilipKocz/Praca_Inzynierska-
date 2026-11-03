#pragma once
#include <QDialog>
#include "Discretizer.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;

// Osobne okienko do dodawania i edycji jednej bryły:
// typ, środek (x, y, z) i promień / połowa krawędzi.
class ShapeDialog : public QDialog {
    Q_OBJECT
public:
    explicit ShapeDialog(QWidget* parent = nullptr);

    void setSpec(const ShapeSpec& s);
    ShapeSpec spec() const;

private:
    void updateSizeLabel();

    QComboBox* kind_ = nullptr;
    QDoubleSpinBox* cx_ = nullptr;
    QDoubleSpinBox* cy_ = nullptr;
    QDoubleSpinBox* cz_ = nullptr;
    QDoubleSpinBox* r_ = nullptr;
    QLabel* sizeLabel_ = nullptr;
};
