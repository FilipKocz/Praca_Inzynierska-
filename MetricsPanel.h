#pragma once
#include <QVector>
#include <QWidget>
#include <vector>
#include "Discretizer.h"

class QLabel;
class QSpinBox;
class Histogram;

// Histogram odległości wskazanego punktu do jego k podświetlonych sąsiadów
// oraz globalne statystyki d_min całej dyskretyzacji.
class MetricsPanel : public QWidget {
    Q_OBJECT
public:
    explicit MetricsPanel(QWidget* parent = nullptr);
    void setCloud(const PointCloud& pc);
    // index = -1 (kursor poza punktem) nic nie zmienia - zostaje ostatnie sąsiedztwo.
    void setNeighbors(int index, const QVector<double>& distances, const QString& info);
    int neighborCount() const;

signals:
    void neighborCountChanged(int k);

private:
    void refresh();

    QLabel* title_ = nullptr;
    Histogram* histogram_ = nullptr;
    QSpinBox* bins_ = nullptr;
    QSpinBox* neighbors_ = nullptr;
    QLabel* hoverInfo_ = nullptr;
    QLabel* globalStats_ = nullptr;
    std::vector<double> neighborDists_;
    int hoveredIndex_ = -1;
};
