#pragma once
#include <QWidget>
#include "Discretizer.h"

class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QLabel;
class QStackedWidget;
class DensityPlot;

// Ustawienia funkcji zagęszczenia h: stała, profil h(s) albo własny wzór.
class DensityPanel : public QWidget {
    Q_OBJECT
public:
    explicit DensityPanel(QWidget* parent = nullptr);

    DensitySpec spec() const;
    void setSpec(const DensitySpec& spec);
    bool formulaValid() const { return formulaOk_; }

    // Zakres osi s na wykresie - mniej więcej największa możliwa odległość od brzegu.
    void setPlotRange(double sMax);

signals:
    void changed();

private:
    void showPage(int index);
    void onChanged();
    void checkFormula();

    QComboBox* mode_ = nullptr;
    QStackedWidget* pages_ = nullptr;
    QDoubleSpinBox* h_ = nullptr;
    QDoubleSpinBox* hMin_ = nullptr;
    QDoubleSpinBox* hMax_ = nullptr;
    QDoubleSpinBox* s1_ = nullptr;
    QDoubleSpinBox* s2_ = nullptr;
    QLineEdit* formula_ = nullptr;
    QLabel* formulaInfo_ = nullptr;
    DensityPlot* plot_ = nullptr;
    double plotMax_ = 1.0;
    bool formulaOk_ = true;
    bool updating_ = false;   // setSpec nie emituje changed() dla każdego pola osobno
};
