#include "PointCloudView.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <limits>

static const char* kVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in float aType;
layout(location = 2) in float aOverlap;
uniform mat4 uMVP;
uniform float uPointSize;
out vec3 vColor;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    gl_PointSize = uPointSize;
    if (aType < 0.0)
        vColor = vec3(0.85, 0.25, 0.20);   // brzeg
    else if (aOverlap >= 2.0)
        vColor = vec3(0.25, 0.80, 0.35);   // część wspólna
    else
        vColor = vec3(0.20, 0.45, 0.85);   // jedna bryła
}
)";

static const char* kFragmentShader = R"(
#version 330 core
in vec3 vColor;
out vec4 fragColor;
void main() {
    vec2 c = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(c, c);
    if (r2 > 1.0) discard;                 // okrągłe punkty
    float shade = 1.0 - 0.35 * r2;
    fragColor = vec4(vColor * shade, 1.0);
}
)";

PointCloudView::PointCloudView(QWidget* parent) : QOpenGLWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
}

PointCloudView::~PointCloudView() {
    makeCurrent();
    vbo_.destroy();
    vao_.destroy();
    doneCurrent();
}

void PointCloudView::initializeGL() {
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);

    program_.addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader);
    program_.addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader);
    if (!program_.link()) qWarning() << program_.log();

    vao_.create();
    vao_.bind();
    vbo_.create();
    vbo_.bind();
    vbo_.setUsagePattern(QOpenGLBuffer::StaticDraw);

    // [x y z typ overlap] na punkt
    const int stride = 5 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(4 * sizeof(float)));
    vao_.release();
    vbo_.release();
}

void PointCloudView::resizeGL(int, int) {}

void PointCloudView::setPointCloud(const PointCloud& pc, bool fitCamera) {
    const int n = pc.size();
    pending_.resize(5 * n);

    QVector3D mn( std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max());
    QVector3D mx = -mn;
    for (int i = 0; i < n; ++i) {
        QVector3D p(pc.xyz[3*i], pc.xyz[3*i+1], pc.xyz[3*i+2]);
        pending_[5*i]   = p.x();
        pending_[5*i+1] = p.y();
        pending_[5*i+2] = p.z();
        pending_[5*i+3] = static_cast<float>(pc.type[i]);
        pending_[5*i+4] = pc.overlap.empty() ? 1.0f : static_cast<float>(pc.overlap[i]);
        mn = QVector3D(std::min(mn.x(), p.x()), std::min(mn.y(), p.y()), std::min(mn.z(), p.z()));
        mx = QVector3D(std::max(mx.x(), p.x()), std::max(mx.y(), p.y()), std::max(mx.z(), p.z()));
    }

    if (n > 0) {
        sceneCenter_ = (mn + mx) * 0.5f;
        sceneRadius_ = std::max(0.5f * (mx - mn).length(), 1e-3f);
        if (fitCamera) resetCamera();
    }

    dirty_ = true;
    update();
}

void PointCloudView::setPointSize(float px) {
    pointSize_ = px;
    update();
}

void PointCloudView::resetCamera() {
    target_ = sceneCenter_;
    distance_ = sceneRadius_ * 3.0f;
    yaw_ = 35.0f;
    pitch_ = 25.0f;
    update();
}

QVector3D PointCloudView::eyePosition() const {
    const float y = qDegreesToRadians(yaw_);
    const float p = qDegreesToRadians(pitch_);
    return target_ + distance_ * QVector3D(std::cos(p) * std::cos(y),
                                           std::sin(p),
                                           std::cos(p) * std::sin(y));
}

QMatrix4x4 PointCloudView::viewMatrix() const {
    QMatrix4x4 v;
    v.lookAt(eyePosition(), target_, QVector3D(0, 1, 0));
    return v;
}

void PointCloudView::paintGL() {
    if (dirty_) {   // tylko tu kontekst GL jest na pewno aktywny
        vbo_.bind();
        vbo_.allocate(pending_.data(), static_cast<int>(pending_.size() * sizeof(float)));
        vbo_.release();
        count_ = static_cast<int>(pending_.size() / 5);
        pending_.clear();
        pending_.shrink_to_fit();
        dirty_ = false;
    }

    glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (count_ == 0) return;

    QMatrix4x4 proj;
    const float aspect = width() / float(std::max(height(), 1));
    proj.perspective(45.0f, aspect,
                     std::max(distance_ - 2.0f * sceneRadius_, distance_ * 0.01f),
                     distance_ + 2.0f * sceneRadius_);

    program_.bind();
    program_.setUniformValue("uMVP", proj * viewMatrix());
    program_.setUniformValue("uPointSize", pointSize_ * float(devicePixelRatioF()));
    vao_.bind();
    glDrawArrays(GL_POINTS, 0, count_);
    vao_.release();
    program_.release();
}

void PointCloudView::mousePressEvent(QMouseEvent* e) {
    lastMouse_ = e->position().toPoint();
}

void PointCloudView::mouseMoveEvent(QMouseEvent* e) {
    const QPoint pos = e->position().toPoint();
    const QPoint d = pos - lastMouse_;
    lastMouse_ = pos;

    if (e->buttons() & Qt::LeftButton) {
        yaw_   += d.x() * 0.4f;
        pitch_ = std::clamp(pitch_ + d.y() * 0.4f, -89.0f, 89.0f);
    } else if (e->buttons() & (Qt::RightButton | Qt::MiddleButton)) {
        const QMatrix4x4 v = viewMatrix();
        const QVector3D right(v(0, 0), v(0, 1), v(0, 2));
        const QVector3D up(v(1, 0), v(1, 1), v(1, 2));
        const float s = distance_ * 0.0015f;
        target_ += (-right * d.x() + up * d.y()) * s;
    }
    update();
}

void PointCloudView::wheelEvent(QWheelEvent* e) {
    const float steps = e->angleDelta().y() / 120.0f;
    distance_ = std::clamp(distance_ * std::pow(0.9f, steps),
                           sceneRadius_ * 0.05f, sceneRadius_ * 50.0f);
    update();
}
