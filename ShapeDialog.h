#pragma once
#include <QDialog>
#include "Discretizer.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;

class ShapeDialog : public QDialog {
    Q_OBJECT
public:
    explicit ShapeDialog(QWidget* parent = nullptr);

    void setSpec(const ShapeSpec& s);
    ShapeSpec spec() const;

    void accept() override;   // blokuje OK przy błędnym pliku siatki

private:
    void updateForKind();
    void chooseFile();
    bool inspectFile(const QString& path);

    QComboBox* kind_ = nullptr;
    QComboBox* op_ = nullptr;
    QDoubleSpinBox* cx_ = nullptr;
    QDoubleSpinBox* cy_ = nullptr;
    QDoubleSpinBox* cz_ = nullptr;
    QDoubleSpinBox* r_ = nullptr;
    QLabel* centerLabel_ = nullptr;
    QLabel* sizeLabel_ = nullptr;

    QWidget* fileRow_ = nullptr;
    QLabel* fileLabel_ = nullptr;
    QLineEdit* file_ = nullptr;
    QLabel* fileInfo_ = nullptr;

    bool fileOk_ = false;
    double meshVolume_ = 0.0;
};
