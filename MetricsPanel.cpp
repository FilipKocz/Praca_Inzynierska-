#include "MetricsPanel.h"
#include <QFormLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QSpinBox>
#include <QToolTip>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

class Histogram : public QWidget {
public:
    explicit Histogram(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(160);
        setMouseTracking(true);
    }

    void setEmptyText(const QString& t) { emptyText_ = t; update(); }

    void setData(const std::vector<double>& values, int bins, double mean) {
        counts_.assign(bins, 0);
        mean_ = mean;
        if (values.empty()) { lo_ = hi_ = 0; update(); return; }
        const auto [mn, mx] = std::minmax_element(values.begin(), values.end());
        lo_ = *mn;
        hi_ = *mx;
        const double width = hi_ > lo_ ? hi_ - lo_ : 1.0;
        for (double v : values) {
            int b = int((v - lo_) / width * bins);
            counts_[std::clamp(b, 0, bins - 1)]++;   // v == max trafia do ostatniego przedziału
        }
        update();
    }

protected:
    QRectF plotArea() const { return rect().adjusted(46, 8, -8, -22); }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), palette().base());
        const QRectF area = plotArea();
        p.setPen(palette().mid().color());
        p.drawRect(area);
        if (counts_.empty() || (lo_ == 0 && hi_ == 0)) {
            p.setPen(palette().text().color());
            p.drawText(area.adjusted(6, 0, -6, 0), Qt::AlignCenter | Qt::TextWordWrap, emptyText_);
            return;
        }

        const int maxCount = *std::max_element(counts_.begin(), counts_.end());
        const double barW = area.width() / counts_.size();
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(40, 110, 210));
        for (size_t b = 0; b < counts_.size(); ++b) {
            const double hgt = area.height() * counts_[b] / std::max(maxCount, 1);
            p.drawRect(QRectF(area.left() + b * barW + 0.5, area.bottom() - hgt, barW - 1.0, hgt));
        }

        if (hi_ > lo_) {   // linia średniej
            const double x = area.left() + area.width() * (mean_ - lo_) / (hi_ - lo_);
            p.setPen(QPen(QColor(220, 80, 50), 1.5, Qt::DashLine));
            p.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
        }

        p.setPen(palette().text().color());
        QFont f = font();
        f.setPointSizeF(f.pointSizeF() * 0.85);
        p.setFont(f);
        p.drawText(QRectF(0, area.top() - 2, 42, 14), Qt::AlignRight, QString::number(maxCount));
        p.drawText(QRectF(0, area.bottom() - 12, 42, 14), Qt::AlignRight, "0");
        p.drawText(QRectF(area.left(), area.bottom() + 3, area.width(), 14), Qt::AlignLeft,
                   QString::number(lo_, 'g', 4));
        p.drawText(QRectF(area.left(), area.bottom() + 3, area.width(), 14), Qt::AlignRight,
                   QString::number(hi_, 'g', 4));
        p.drawText(QRectF(area.left(), area.bottom() + 3, area.width(), 14), Qt::AlignHCenter,
                   "odległość");
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        const QRectF area = plotArea();
        if (counts_.empty() || !area.contains(e->position())) { QToolTip::hideText(); return; }
        const int b = std::clamp(int((e->position().x() - area.left()) / area.width() * counts_.size()),
                                 0, int(counts_.size()) - 1);
        const double w = (hi_ - lo_) / counts_.size();
        QToolTip::showText(e->globalPosition().toPoint(),
                           QString("odległość: %1 – %2\nsąsiadów: %3")
                               .arg(lo_ + b * w, 0, 'g', 4).arg(lo_ + (b + 1) * w, 0, 'g', 4)
                               .arg(counts_[b]),
                           this);
    }

private:
    std::vector<int> counts_;
    double lo_ = 0, hi_ = 0, mean_ = 0;
    QString emptyText_;
};

namespace {

struct Stats { double min = 0, max = 0, mean = 0, stdev = 0; };

Stats computeStats(const std::vector<double>& v) {
    Stats st;
    if (v.empty()) return st;
    const auto [mn, mx] = std::minmax_element(v.begin(), v.end());
    double sum = 0.0, sum2 = 0.0;
    for (double x : v) { sum += x; sum2 += x * x; }
    const double n = double(v.size());
    st.min = *mn;
    st.max = *mx;
    st.mean = sum / n;
    st.stdev = std::sqrt(std::max(0.0, sum2 / n - st.mean * st.mean));
    return st;
}

QString statsText(const Stats& st) {
    return QString("min: %1   max: %2\nśrednia: %3   odch. std.: %4\nmin / max: %5")
        .arg(st.min, 0, 'g', 5).arg(st.max, 0, 'g', 5)
        .arg(st.mean, 0, 'g', 5).arg(st.stdev, 0, 'g', 5)
        .arg(st.max > 0 ? st.min / st.max : 0.0, 0, 'f', 3);
}

}  // namespace

MetricsPanel::MetricsPanel(QWidget* parent) : QWidget(parent) {
    title_ = new QLabel;
    title_->setWordWrap(true);
    histogram_ = new Histogram;
    histogram_->setEmptyText("Najedź myszą na punkt - histogram pokaże odległości do jego sąsiadów.");
    bins_ = new QSpinBox;
    bins_->setRange(1, 200);
    bins_->setValue(10);
    neighbors_ = new QSpinBox;
    neighbors_->setRange(1, 100);
    neighbors_->setValue(12);
    hoverInfo_ = new QLabel;
    hoverInfo_->setWordWrap(true);
    hoverInfo_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    globalStats_ = new QLabel;
    globalStats_->setWordWrap(true);
    globalStats_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(title_);
    layout->addWidget(histogram_, 1);
    auto* form = new QFormLayout;
    form->addRow("Liczba sąsiadów k", neighbors_);
    form->addRow("Liczba przedziałów", bins_);
    layout->addLayout(form);
    layout->addWidget(hoverInfo_);
    layout->addSpacing(8);
    layout->addWidget(new QLabel("<b>Cała dyskretyzacja: d_min</b>"));
    layout->addWidget(globalStats_);

    connect(bins_, &QSpinBox::valueChanged, this, &MetricsPanel::refresh);
    connect(neighbors_, &QSpinBox::valueChanged, this, &MetricsPanel::neighborCountChanged);
    refresh();
}

int MetricsPanel::neighborCount() const { return neighbors_->value(); }

void MetricsPanel::setCloud(const PointCloud& pc) {
    // Indeksy sąsiadów odnosiły się do poprzedniej chmury.
    neighborDists_.clear();
    hoveredIndex_ = -1;
    hoverInfo_->clear();

    const Stats st = computeStats(pc.nnDist);
    globalStats_->setText(pc.nnDist.empty()
                              ? "Brak punktów."
                              : QString("Punkty: %1\n").arg(pc.nnDist.size()) + statsText(st));
    refresh();
}

void MetricsPanel::setNeighbors(int index, const QVector<double>& distances, const QString& info) {
    if (index < 0) return;   // kursor zjechał z punktu - zostaje ostatnie sąsiedztwo
    hoveredIndex_ = index;
    neighborDists_.assign(distances.begin(), distances.end());
    hoverInfo_->setText(info);
    refresh();
}

void MetricsPanel::refresh() {
    if (neighborDists_.empty()) {
        title_->setText("<b>Odległości do sąsiadów</b>");
        histogram_->setData({}, bins_->value(), 0.0);
        return;
    }
    const Stats st = computeStats(neighborDists_);
    title_->setText(QString("<b>Odległości do %1 sąsiadów punktu #%2</b>")
                        .arg(neighborDists_.size()).arg(hoveredIndex_));
    histogram_->setData(neighborDists_, bins_->value(), st.mean);
}
