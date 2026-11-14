#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QVector3D>
#include <QPoint>
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

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;

private:
    QVector3D eyePosition() const;
    QMatrix4x4 viewMatrix() const;

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
};
