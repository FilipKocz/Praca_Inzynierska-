#include "DensityPanel.h"
#include "Density.h"
#include "Expression.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <cmath>

// Wykres h(s); dla wzoru rysowany przy x = y = z = 0.
class DensityPlot : public QWidget {
public:
    explicit DensityPlot(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedHeight(110);
    }
    void setData(const DensitySpec& spec, double sMax, bool valid) {
        spec_ = spec;
        sMax_ = sMax;
        valid_ = valid;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF area = rect().adjusted(34, 6, -6, -18);
        p.fillRect(rect(), palette().base());
        p.setPen(palette().mid().color());
        p.drawRect(area);

        if (!valid_) {
            p.setPen(palette().text().color());
            p.drawText(area, Qt::AlignCenter, "błędny wzór");
            return;
        }

        const int n = 120;
        std::vector<double> hs(n + 1);
        Expression e;
        std::string err;
        if (spec_.mode == DensityMode::Formula) e.parse(spec_.formula, err);
        double hLo = 1e300, hHi = -1e300;
        for (int i = 0; i <= n; ++i) {
            const double s = sMax_ * i / n;
            double h = spec_.h;
            if (spec_.mode == DensityMode::Profile) h = profileH(spec_, s);
            else if (spec_.mode == DensityMode::Formula) h = e.eval(0, 0, 0, s);
            hs[i] = h;
            if (std::isfinite(h)) { hLo = std::min(hLo, h); hHi = std::max(hHi, h); }
        }
        if (hLo > hHi) return;
        // Oś h od zera, żeby było widać, jak bardzo zagęszczamy.
        hLo = std::min(0.0, hLo);
        if (hHi - hLo < 1e-12) hHi = hLo + 1.0;
        hHi *= 1.1;

        auto toScreen = [&](double s, double h) {
            return QPointF(area.left() + area.width() * s / sMax_,
                           area.bottom() - area.height() * (h - hLo) / (hHi - hLo));
        };
        QPainterPath path;
        for (int i = 0; i <= n; ++i) {
            const QPointF pt = toScreen(sMax_ * i / n, hs[i]);
            if (i == 0) path.moveTo(pt); else path.lineTo(pt);
        }
        p.setPen(QPen(QColor(40, 110, 210), 2));
        p.drawPath(path);

        p.setPen(palette().text().color());
        QFont f = font();
        f.setPointSizeF(f.pointSizeF() * 0.85);
        p.setFont(f);
        p.drawText(QRectF(0, area.top() - 2, 32, 14), Qt::AlignRight, QString::number(hHi / 1.1, 'g', 3));
        p.drawText(QRectF(0, area.bottom() - 12, 32, 14), Qt::AlignRight, QString::number(hLo, 'g', 3));
        p.drawText(QRectF(area.left(), area.bottom() + 2, area.width(), 14), Qt::AlignLeft, "s = 0");
        p.drawText(QRectF(area.left(), area.bottom() + 2, area.width(), 14), Qt::AlignRight,
                   QString("s = %1").arg(sMax_, 0, 'g', 3));
        p.drawText(QRectF(area.left() + 4, area.top() + 2, area.width() - 8, 14), Qt::AlignLeft,
                   spec_.mode == DensityMode::Formula ? "h(s) przy x = y = z = 0" : "h(s)");
    }

private:
    DensitySpec spec_;
    double sMax_ = 1.0;
    bool valid_ = true;
};

namespace {

QDoubleSpinBox* makeSpin(double value) {
    auto* s = new QDoubleSpinBox;
    s->setDecimals(4);
    s->setRange(0.0, 1e6);
    s->setSingleStep(0.005);
    s->setValue(value);
    s->setKeyboardTracking(false);   // bez przeliczania przy każdym wpisanym znaku
    return s;
}

}  // namespace

DensityPanel::DensityPanel(QWidget* parent) : QWidget(parent) {
    const DensitySpec def;

    mode_ = new QComboBox;
    mode_->addItem("Stała", int(DensityMode::Constant));
    mode_->addItem("Profil od brzegu h(s)", int(DensityMode::Profile));
    mode_->addItem("Własny wzór", int(DensityMode::Formula));

    // Strona 1: stała
    h_ = makeSpin(def.h);
    h_->setMinimum(0.001);
    auto* constPage = new QWidget;
    auto* constForm = new QFormLayout(constPage);
    constForm->setContentsMargins(0, 0, 0, 0);
    constForm->addRow("h", h_);

    // Strona 2: profil
    hMin_ = makeSpin(def.hMin);
    hMax_ = makeSpin(def.hMax);
    hMin_->setMinimum(0.001);
    hMax_->setMinimum(0.001);
    s1_ = makeSpin(def.s1);
    s2_ = makeSpin(def.s2);
    auto* profilePage = new QWidget;
    auto* profileForm = new QFormLayout(profilePage);
    profileForm->setContentsMargins(0, 0, 0, 0);
    profileForm->addRow("h przy brzegu", hMin_);
    profileForm->addRow("h w głębi", hMax_);
    profileForm->addRow("stałe do s₁", s1_);
    profileForm->addRow("liniowo do s₂", s2_);

    // Strona 3: wzór
    formula_ = new QLineEdit(QString::fromStdString(def.formula));
    formulaInfo_ = new QLabel("zmienne: x, y, z, s\nfunkcje: sin cos exp log sqrt abs\nmin max pow tanh, stałe: pi e");
    formulaInfo_->setWordWrap(true);
    auto* formulaPage = new QWidget;
    auto* formulaLayout = new QVBoxLayout(formulaPage);
    formulaLayout->setContentsMargins(0, 0, 0, 0);
    formulaLayout->addWidget(new QLabel("h(x, y, z, s) ="));
    formulaLayout->addWidget(formula_);
    formulaLayout->addWidget(formulaInfo_);
    formulaLayout->addStretch(1);

    pages_ = new QStackedWidget;
    pages_->addWidget(constPage);
    pages_->addWidget(profilePage);
    pages_->addWidget(formulaPage);

    plot_ = new DensityPlot;

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* modeForm = new QFormLayout;
    modeForm->addRow("Zagęszczenie", mode_);
    layout->addLayout(modeForm);
    layout->addWidget(pages_);
    layout->addWidget(plot_);

    connect(mode_, &QComboBox::currentIndexChanged, this, [this](int i) {
        showPage(i);
        onChanged();
    });
    for (QDoubleSpinBox* s : {h_, hMin_, hMax_, s1_, s2_})
        connect(s, &QDoubleSpinBox::valueChanged, this, &DensityPanel::onChanged);
    // Wzór przeliczamy po Enter / wyjściu z pola, nie po każdym znaku.
    connect(formula_, &QLineEdit::editingFinished, this, &DensityPanel::onChanged);
    connect(formula_, &QLineEdit::textEdited, this, &DensityPanel::checkFormula);

    showPage(0);
    onChanged();
}

void DensityPanel::showPage(int index) {
    pages_->setCurrentIndex(index);
    // QStackedWidget bierze wysokość największej strony - dopasowujemy do bieżącej.
    for (int k = 0; k < pages_->count(); ++k)
        pages_->widget(k)->setSizePolicy(QSizePolicy::Preferred,
                                         k == index ? QSizePolicy::Maximum : QSizePolicy::Ignored);
    pages_->adjustSize();
}

void DensityPanel::checkFormula() {
    Expression e;
    std::string err;
    formulaOk_ = e.parse(formula_->text().toStdString(), err);
    formula_->setStyleSheet(formulaOk_ ? "" : "QLineEdit { background: #ffd6d6; }");
    formula_->setToolTip(formulaOk_ ? "" : QString::fromStdString(err));
    if (!formulaOk_) formulaInfo_->setText("Błąd: " + QString::fromStdString(err));
    else formulaInfo_->setText("zmienne: x, y, z, s\nfunkcje: sin cos exp log sqrt abs\nmin max pow tanh, stałe: pi e");
}

void DensityPanel::onChanged() {
    // s₁ <= s₂ - inaczej profil nie ma sensu.
    if (s2_->value() < s1_->value()) s2_->setValue(s1_->value());
    checkFormula();
    plot_->setData(spec(), plotMax_, spec().mode != DensityMode::Formula || formulaOk_);
    if (!updating_) emit changed();
}

void DensityPanel::setPlotRange(double sMax) {
    plotMax_ = sMax > 0 ? sMax : 1.0;
    if (mode_->currentIndex() == 1) plotMax_ = std::max(plotMax_, s2_->value() * 1.2);
    plot_->setData(spec(), plotMax_, spec().mode != DensityMode::Formula || formulaOk_);
}

DensitySpec DensityPanel::spec() const {
    DensitySpec s;
    s.mode = static_cast<DensityMode>(mode_->currentData().toInt());
    s.h = h_->value();
    s.hMin = hMin_->value();
    s.hMax = hMax_->value();
    s.s1 = s1_->value();
    s.s2 = s2_->value();
    s.formula = formula_->text().toStdString();
    return s;
}

void DensityPanel::setSpec(const DensitySpec& s) {
    updating_ = true;
    h_->setValue(s.h);
    hMin_->setValue(s.hMin);
    hMax_->setValue(s.hMax);
    s1_->setValue(s.s1);
    s2_->setValue(s.s2);
    formula_->setText(QString::fromStdString(s.formula));
    mode_->setCurrentIndex(mode_->findData(int(s.mode)));
    updating_ = false;
    onChanged();
}
