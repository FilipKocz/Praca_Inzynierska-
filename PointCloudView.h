#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QVector3D>
#include <QPoint>
#include <QTimer>
#include <QVector>
#include <vector>
#include "Discretizer.h"

// Kamera orbitalna: LPM obrót, PPM przesuwanie, kółko zoom.
class PointCloudView : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT
public:
    explicit PointCloudView(QWidget* parent = nullptr);
    ~PointCloudView() override;

    void setPointCloud(const PointCloud& pc, bool fitCamera);
    void setPointSize(float px);
    void resetCamera();
    void setNeighborCount(int k);

signals:
    // index = -1: kursor nie wskazuje żadnego punktu
    void pointHovered(int index, const QVector<int>& neighbors, const QVector<double>& distances);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    QVector3D eyePosition() const;
    QMatrix4x4 viewMatrix() const;
    QMatrix4x4 projectionMatrix() const;
    void pickAt(const QPoint& pos);
    void setHovered(int index);

    QOpenGLShaderProgram program_;
    QOpenGLBuffer vbo_{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject vao_;

    std::vector<float> pending_;   // czeka na wysłanie do GPU w paintGL()
    bool dirty_ = false;
    int count_ = 0;
    float pointSize_ = 4.0f;

    QVector3D sceneCenter_{0, 0, 0};
    float sceneRadius_ = 1.0f;
    QVector3D target_{0, 0, 0};
    float distance_ = 3.0f;
    float yaw_ = 35.0f;
    float pitch_ = 25.0f;
    QPoint lastMouse_;

    // Wskazywanie myszą i podświetlanie sąsiadów
    std::vector<float> positions_;   // kopia xyz na CPU (dane w GPU są niedostępne)
    QOpenGLShaderProgram hlProgram_;
    QOpenGLBuffer hlVbo_{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject hlVao_;
    std::vector<float> hlData_;      // [punkt][sąsiedzi][linie punkt-sąsiad]
    bool hlDirty_ = false;
    int hovered_ = -1;
    std::vector<int> neighbors_;
    int neighborCount_ = 12;
    QTimer hoverTimer_;              // wskazywanie dopiero, gdy mysz na chwilę się zatrzyma
    QPoint hoverPos_;
};
