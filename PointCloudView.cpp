#include "PointCloudView.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <limits>
#include <queue>

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

static const char* kHighlightVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
uniform float uPointSize;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    gl_PointSize = uPointSize;
}
)";

static const char* kHighlightFragmentShader = R"(
#version 330 core
uniform vec3 uColor;
uniform int uRound;
out vec4 fragColor;
void main() {
    if (uRound != 0) {   // gl_PointCoord ma sens tylko dla punktów, nie dla linii
        vec2 c = gl_PointCoord * 2.0 - 1.0;
        if (dot(c, c) > 1.0) discard;
    }
    fragColor = vec4(uColor, 1.0);
}
)";

PointCloudView::PointCloudView(QWidget* parent) : QOpenGLWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);   // ruchy myszy także bez wciśniętego przycisku
    hoverTimer_.setSingleShot(true);
    hoverTimer_.setInterval(40);
    connect(&hoverTimer_, &QTimer::timeout, this, [this] { pickAt(hoverPos_); });
}

PointCloudView::~PointCloudView() {
    makeCurrent();
    vbo_.destroy();
    vao_.destroy();
    hlVbo_.destroy();
    hlVao_.destroy();
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

    hlProgram_.addShaderFromSourceCode(QOpenGLShader::Vertex, kHighlightVertexShader);
    hlProgram_.addShaderFromSourceCode(QOpenGLShader::Fragment, kHighlightFragmentShader);
    if (!hlProgram_.link()) qWarning() << hlProgram_.log();
    hlVao_.create();
    hlVao_.bind();
    hlVbo_.create();
    hlVbo_.bind();
    hlVbo_.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    hlVao_.release();
    hlVbo_.release();
}

void PointCloudView::resizeGL(int, int) {}

void PointCloudView::setPointCloud(const PointCloud& pc, bool fitCamera) {
    const int n = pc.size();
    pending_.resize(5 * n);
    positions_.resize(3 * n);

    QVector3D mn( std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max());
    QVector3D mx = -mn;
    for (int i = 0; i < n; ++i) {
        QVector3D p(float(pc.xyz[3*i]), float(pc.xyz[3*i+1]), float(pc.xyz[3*i+2]));
        pending_[5*i]   = p.x();
        pending_[5*i+1] = p.y();
        pending_[5*i+2] = p.z();
        pending_[5*i+3] = static_cast<float>(pc.type[i]);
        pending_[5*i+4] = pc.overlap.empty() ? 1.0f : static_cast<float>(pc.overlap[i]);
        positions_[3*i] = p.x();
        positions_[3*i+1] = p.y();
        positions_[3*i+2] = p.z();
        mn = QVector3D(std::min(mn.x(), p.x()), std::min(mn.y(), p.y()), std::min(mn.z(), p.z()));
        mx = QVector3D(std::max(mx.x(), p.x()), std::max(mx.y(), p.y()), std::max(mx.z(), p.z()));
    }

    if (n > 0) {
        sceneCenter_ = (mn + mx) * 0.5f;
        sceneRadius_ = std::max(0.5f * (mx - mn).length(), 1e-3f);
        if (fitCamera) resetCamera();
    }

    hovered_ = -2;      // wymusza odświeżenie, nawet jeśli wynik to znów -1
    setHovered(-1);     // stare indeksy nie pasują do nowej chmury

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

    const QMatrix4x4 mvp = projectionMatrix() * viewMatrix();
    const float dpr = float(devicePixelRatioF());

    program_.bind();
    program_.setUniformValue("uMVP", mvp);
    program_.setUniformValue("uPointSize", pointSize_ * dpr);
    vao_.bind();
    glDrawArrays(GL_POINTS, 0, count_);
    vao_.release();
    program_.release();

    if (hlDirty_) {
        hlVbo_.bind();
        hlVbo_.allocate(hlData_.data(), static_cast<int>(hlData_.size() * sizeof(float)));
        hlVbo_.release();
        hlDirty_ = false;
    }
    if (hovered_ < 0) return;

    // Podświetlenie rysowane bez testu głębi - widoczne także przez inne punkty.
    const int k = static_cast<int>(neighbors_.size());
    glDisable(GL_DEPTH_TEST);
    hlProgram_.bind();
    hlProgram_.setUniformValue("uMVP", mvp);
    hlVao_.bind();
    hlProgram_.setUniformValue("uRound", GLint(0));
    hlProgram_.setUniformValue("uColor", QVector3D(1.0f, 0.85f, 0.2f));
    glDrawArrays(GL_LINES, 1 + k, 2 * k);
    hlProgram_.setUniformValue("uRound", GLint(1));
    hlProgram_.setUniformValue("uPointSize", pointSize_ * 1.7f * dpr);
    hlProgram_.setUniformValue("uColor", QVector3D(1.0f, 0.55f, 0.1f));
    glDrawArrays(GL_POINTS, 1, k);
    hlProgram_.setUniformValue("uPointSize", pointSize_ * 2.6f * dpr);
    hlProgram_.setUniformValue("uColor", QVector3D(1.0f, 1.0f, 1.0f));
    glDrawArrays(GL_POINTS, 0, 1);
    hlVao_.release();
    hlProgram_.release();
    glEnable(GL_DEPTH_TEST);
}

QMatrix4x4 PointCloudView::projectionMatrix() const {
    QMatrix4x4 proj;
    const float aspect = width() / float(std::max(height(), 1));
    proj.perspective(45.0f, aspect,
                     std::max(distance_ - 2.0f * sceneRadius_, distance_ * 0.01f),
                     distance_ + 2.0f * sceneRadius_);
    return proj;
}

// Rzutuje wszystkie punkty na ekran i wybiera ten pod kursorem; przy kilku
// - najbliższy kamery (najmniejsza głębokość), bo to on jest widoczny.
void PointCloudView::pickAt(const QPoint& pos) {
    if (positions_.empty()) return;
    const QMatrix4x4 mvp = projectionMatrix() * viewMatrix();
    const float* m = mvp.constData();   // kolumnami
    const float w = float(width()), h = float(height());
    const float radius = std::max(pointSize_, 6.0f);
    const float r2 = radius * radius;
    const float mx = float(pos.x()), my = float(pos.y());

    int best = -1;
    float bestDepth = 2.0f;
    const int n = static_cast<int>(positions_.size() / 3);
    for (int i = 0; i < n; ++i) {
        const float x = positions_[3*i], y = positions_[3*i+1], z = positions_[3*i+2];
        const float cw = m[3]*x + m[7]*y + m[11]*z + m[15];
        if (cw <= 0.0f) continue;
        const float inv = 1.0f / cw;
        const float nz = (m[2]*x + m[6]*y + m[10]*z + m[14]) * inv;
        if (nz < -1.0f || nz > 1.0f || nz >= bestDepth) continue;
        const float sx = ((m[0]*x + m[4]*y + m[8]*z + m[12]) * inv * 0.5f + 0.5f) * w;
        const float sy = (0.5f - (m[1]*x + m[5]*y + m[9]*z + m[13]) * inv * 0.5f) * h;
        if ((sx - mx) * (sx - mx) + (sy - my) * (sy - my) <= r2) {
            bestDepth = nz;
            best = i;
        }
    }
    setHovered(best);
}

void PointCloudView::setHovered(int index) {
    if (index == hovered_) return;
    hovered_ = index;
    neighbors_.clear();
    hlData_.clear();
    QVector<int> nbrs;
    QVector<double> dists;

    if (index >= 0) {
        // k najbliższych: jedno przejście po wszystkich punktach z kopcem rozmiaru k.
        // Liczone tylko przy zmianie wskazanego punktu, więc O(n) wystarcza.
        const int n = static_cast<int>(positions_.size() / 3);
        const float px = positions_[3*index], py = positions_[3*index+1], pz = positions_[3*index+2];
        std::priority_queue<std::pair<float, int>> heap;   // największa odległość na szczycie
        for (int j = 0; j < n; ++j) {
            if (j == index) continue;
            const float dx = positions_[3*j] - px, dy = positions_[3*j+1] - py, dz = positions_[3*j+2] - pz;
            const float d2 = dx*dx + dy*dy + dz*dz;
            if (int(heap.size()) < neighborCount_) heap.push({d2, j});
            else if (d2 < heap.top().first) { heap.pop(); heap.push({d2, j}); }
        }
        neighbors_.resize(heap.size());
        dists.resize(int(heap.size()));
        for (int i = int(heap.size()) - 1; i >= 0; --i) {   // od najbliższego
            neighbors_[i] = heap.top().second;
            dists[i] = std::sqrt(double(heap.top().first));
            heap.pop();
        }
        nbrs = QVector<int>(neighbors_.begin(), neighbors_.end());

        auto push = [this](int i) {
            hlData_.insert(hlData_.end(), {positions_[3*i], positions_[3*i+1], positions_[3*i+2]});
        };
        push(index);
        for (int j : neighbors_) push(j);
        for (int j : neighbors_) { push(index); push(j); }
    }
    hlDirty_ = true;
    update();
    emit pointHovered(index, nbrs, dists);
}

void PointCloudView::setNeighborCount(int k) {
    neighborCount_ = std::max(1, k);
    const int current = hovered_;
    hovered_ = -2;
    setHovered(current);
}

void PointCloudView::leaveEvent(QEvent*) {
    hoverTimer_.stop();
    setHovered(-1);
}

void PointCloudView::mousePressEvent(QMouseEvent* e) {
    lastMouse_ = e->position().toPoint();
}

void PointCloudView::mouseMoveEvent(QMouseEvent* e) {
    const QPoint pos = e->position().toPoint();
    if (e->buttons() == Qt::NoButton) {
        hoverPos_ = pos;
        hoverTimer_.start();
        return;
    }
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
