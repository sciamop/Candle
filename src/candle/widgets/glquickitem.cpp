// This file is a part of "Candle" application.
// Copyright 2015-2025 Hayrullin Denis Ravilevich

#include "glquickitem.h"

#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLPaintDevice>
#include <QPainter>
#include <QEasingCurve>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QtMath>

#define ZOOMSTEP 1.1

// ---------------------------------------------------------------------------
// Renderer — lives on the render thread
// ---------------------------------------------------------------------------

class GLQuickItemRenderer : public QQuickFramebufferObject::Renderer,
                             protected QOpenGLFunctions
{
public:
    GLQuickItemRenderer() = default;
    ~GLQuickItemRenderer() override
    {
        // GL context is not guaranteed current in destructor;
        // ShaderDrawable::dispose() is called from VisualizerWidget on teardown.
        delete m_shaderProgram;
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        QOpenGLFramebufferObjectFormat fmt;
        fmt.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        fmt.setSamples(4);
        return new QOpenGLFramebufferObject(size, fmt);
    }

    void synchronize(QQuickFramebufferObject *item) override
    {
        m_item = static_cast<GLQuickItem *>(item);
        m_state = m_item->m_renderState;
        m_viewSize = QSizeF(item->width(), item->height());
    }

    void render() override
    {
        if (!m_initialized) {
            initializeOpenGLFunctions();
            initShader();
            m_initialized = true;
        }

        drawScene();
        drawPainterOverlay();

        // FPS accounting
        m_frames++;
        qint64 now = m_fpsTimer.elapsed();
        if (now >= 1000) {
            if (m_item)
                QMetaObject::invokeMethod(m_item, "reportFps",
                    Qt::QueuedConnection, Q_ARG(int, m_frames));
            m_frames = 0;
            m_fpsTimer.restart();
        } else if (!m_fpsTimer.isValid()) {
            m_fpsTimer.start();
        }
    }

private:
    GLQuickItem *m_item = nullptr;
    GLQuickItem::RenderState m_state;
    QSizeF m_viewSize;
    QOpenGLShaderProgram *m_shaderProgram = nullptr;
    bool m_initialized = false;
    int m_frames = 0;
    QElapsedTimer m_fpsTimer;

    void initShader()
    {
        m_shaderProgram = new QOpenGLShaderProgram();
        m_shaderProgram->addShaderFromSourceFile(QOpenGLShader::Vertex,   ":/shaders/vshader.glsl");
        m_shaderProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/fshader.glsl");
        m_shaderProgram->link();
    }

    void drawScene()
    {
        const auto &bg = m_state.colorBackground;
        glClearColor(bg.redF(), bg.greenF(), bg.blueF(), 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_PROGRAM_POINT_SIZE);

        if (m_state.antialiasing) {
            if (m_state.msaa) {
                glEnable(GL_MULTISAMPLE);
            } else {
                glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
                glEnable(GL_LINE_SMOOTH);
                glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
                glEnable(GL_POINT_SMOOTH);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glEnable(GL_BLEND);
            }
        }

        if (m_state.zBuffer)
            glEnable(GL_DEPTH_TEST);

        if (m_shaderProgram) {
            m_shaderProgram->bind();
            m_shaderProgram->setUniformValue("p_matrix", m_state.projectionMatrix);
            m_shaderProgram->setUniformValue("v_matrix", m_state.viewMatrix);

            int totalVertices = 0;

            for (ShaderDrawable *d : m_state.drawables) {
                if (d->needsUpdateGeometry())
                    d->updateGeometry(m_shaderProgram);
            }

            for (ShaderDrawable *d : m_state.drawables) {
                if (d->visible()) {
                    if (d->windowScaling())
                        d->setWorldScale(m_state.windowSizeWorld * d->windowScale());
                    m_shaderProgram->setUniformValue("m_matrix", d->modelMatrix());
                    d->draw(m_shaderProgram);
                    totalVertices += d->getVertexCount();
                }
            }

            m_shaderProgram->release();

            if (m_item)
                QMetaObject::invokeMethod(m_item, "reportVertices",
                    Qt::QueuedConnection, Q_ARG(int, totalVertices));
        }

        glDisable(GL_MULTISAMPLE);
        glDisable(GL_LINE_SMOOTH);
        glDisable(GL_POINT_SMOOTH);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
    }

    void drawPainterOverlay()
    {
        // Let each drawable paint 2D annotations (axis labels, origin markers, etc.)
        bool hasDrawerOverlays = false;
        for (ShaderDrawable *d : m_state.drawables) {
            if (d->visible()) {
                hasDrawerOverlays = true;
                break;
            }
        }
        if (!hasDrawerOverlays)
            return;

        QOpenGLPaintDevice paintDevice(framebufferObject()->size());
        QPainter painter(&paintDevice);
        painter.setRenderHint(QPainter::Antialiasing);

        int w = framebufferObject()->width();
        int h = framebufferObject()->height();

        QMatrix4x4 worldToScreen;
        worldToScreen.scale(w / 2.0, -h / 2.0);
        worldToScreen.translate(1, -1);
        worldToScreen *= m_state.projectionMatrix * m_state.viewMatrix;

        double ratio = (m_state.windowSizeWorld > 0.0)
            ? h / m_state.windowSizeWorld
            : 1.0;

        for (ShaderDrawable *d : m_state.drawables) {
            if (d->visible()) {
                painter.save();
                d->drawPainter(painter, worldToScreen * d->modelMatrix(), ratio);
                painter.restore();
            }
        }
    }
};

// ---------------------------------------------------------------------------
// GLQuickItem — main thread
// ---------------------------------------------------------------------------

GLQuickItem::GLQuickItem(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
    , m_zoom(1.0)
    , m_distance(100.0)
    , m_perspective(false)
    , m_gestureProcessing(false)
    , m_pinchStartZoom(1.0)
    , m_planeDepth(0.0)
    , m_windowSizeWorld(0.0)
    , m_lineWidth(1.0)
    , m_pointSize(1.0)
    , m_antialiasing(false)
    , m_msaa(false)
    , m_vsync(false)
    , m_zBuffer(false)
    , m_targetFps(60)
    , m_updatesEnabled(false)
    , m_updating(false)
    , m_animateView(false)
    , m_animationFrame(0)
    , m_fps(0)
    , m_vertices(0)
    , m_colorBackground(Qt::black)
    , m_colorText(Qt::white)
    , m_paintTimer(new QTimer(this))
{
    m_rot = QPointF(90.0, 0.0);
    m_rotAnimationTarget = m_rot;
    m_spendTime.setHMS(0, 0, 0);
    m_estimatedTime.setHMS(0, 0, 0);

    setMirrorVertically(true);  // FBO y-axis flip

    connect(m_paintTimer, &QTimer::timeout, this, &GLQuickItem::onPaintTimer);
    updateProjection();
    updateView();
}

GLQuickItem::~GLQuickItem() = default;

QQuickFramebufferObject::Renderer *GLQuickItem::createRenderer() const
{
    return new GLQuickItemRenderer();
}

// --- Drawable management ---------------------------------------------------

void GLQuickItem::addDrawable(ShaderDrawable *drawable)
{
    m_shaderDrawables.append(drawable);
}

void GLQuickItem::updateModelBounds(ShaderDrawable *drawable)
{
    m_modelLowerBounds = drawable->getModelLowerBounds();
    m_modelUpperBounds = drawable->getModelUpperBounds();
    m_modelRanges      = drawable->getModelRanges();
    emit boundsChanged();
}

void GLQuickItem::fitDrawable(ShaderDrawable *drawable)
{
    stopViewAnimation();

    if (drawable) {
        m_viewLowerBounds = drawable->getViewLowerBounds();
        m_viewUpperBounds = drawable->getViewUpperBounds();
        m_viewRanges      = drawable->getViewRanges();

        m_modelLowerBounds = drawable->getModelLowerBounds();
        m_modelUpperBounds = drawable->getModelUpperBounds();
        m_modelRanges      = drawable->getModelRanges();

        double a = m_viewRanges.y() / 2 / 0.25 * 1.3
                   + (m_viewUpperBounds - m_viewLowerBounds).z() / 2;
        double b = m_viewRanges.x() / 2 / 0.25 * 1.3
                   / ((width() > 0 && height() > 0) ? width() / height() : 1.0)
                   + (m_viewUpperBounds - m_viewLowerBounds).z() / 2;
        m_distance = qMax(a, b);
        if (m_distance == 0) m_distance = 200;

        m_lookAt = QVector3D(
            (m_viewUpperBounds.x() - m_viewLowerBounds.x()) / 2 + m_viewLowerBounds.x(),
            (m_viewUpperBounds.z() - m_viewLowerBounds.z()) / 2 + m_viewLowerBounds.z(),
            -((m_viewUpperBounds.y() - m_viewLowerBounds.y()) / 2 + m_viewLowerBounds.y())
        );
    } else {
        m_distance     = 200;
        m_lookAt       = QVector3D();
        m_viewLowerBounds = m_viewUpperBounds = m_viewRanges     = QVector3D();
        m_modelLowerBounds = m_modelUpperBounds = m_modelRanges  = QVector3D();
    }

    m_pan  = QPointF();
    m_zoom = 1.0;

    updateProjection();
    updateView();
    emit boundsChanged();
}

// --- View presets ----------------------------------------------------------

void GLQuickItem::setTopView()
{
    m_rotAnimationTarget = QPointF(90, m_rot.y() > 180 ? 360 : 0);
    beginViewAnimation();
}

void GLQuickItem::setFrontView()
{
    m_rotAnimationTarget = QPointF(0, m_rot.y() > 180 ? 360 : 0);
    beginViewAnimation();
}

void GLQuickItem::setLeftView()
{
    m_rotAnimationTarget = QPointF(0, m_rot.y() > 270 ? 450 : 90);
    beginViewAnimation();
}

void GLQuickItem::setIsometricView()
{
    m_rotAnimationTarget = QPointF(45, m_rot.y() > 180 ? 405 : 45);
    beginViewAnimation();
}

void GLQuickItem::fitView()
{
    fitDrawable(nullptr);
}

// --- Mouse / touch input --------------------------------------------------

void GLQuickItem::mousePressed(qreal x, qreal y)
{
    m_lastMousePos = QPoint(qRound(x), qRound(y));
    m_storedRot    = m_rot;
    m_storedPan    = m_pan;
}

void GLQuickItem::mouseMoved(qreal x, qreal y, int buttons, bool shiftHeld)
{
    if (m_gestureProcessing) {
        m_lastMousePos = QPoint(qRound(x), qRound(y));
        m_storedRot    = m_rot;
        m_storedPan    = m_pan;
        return;
    }

    QPoint cur(qRound(x), qRound(y));

    bool rotateBt  = (buttons & Qt::LeftButton)   && !shiftHeld;
    bool rotateMid = (buttons & Qt::MiddleButton)  && !shiftHeld;
    bool panBt     = (buttons & Qt::RightButton)
                   || ((buttons & Qt::LeftButton)  && shiftHeld)
                   || ((buttons & Qt::MiddleButton) && shiftHeld);

    if (rotateBt || rotateMid) {
        stopViewAnimation();
        m_rot.setY(normalizeAngle(m_storedRot.y() - (cur.x() - m_lastMousePos.x()) * 0.5));
        m_rot.setX(m_storedRot.x() + (cur.y() - m_lastMousePos.y()) * 0.5);
        m_rot.setX(qBound(-90.0, m_rot.x(), 90.0));
        updateView();
        emit rotationChanged();
    }

    if (panBt) {
        m_pan.setX(m_storedPan.x() - (cur.x() - m_lastMousePos.x()) / (double)width());
        m_pan.setY(m_storedPan.y() + (cur.y() - m_lastMousePos.y()) / (double)height());
        updateProjection();
    }
}

void GLQuickItem::mouseWheeled(qreal x, qreal y, qreal angleDeltaX, qreal angleDeltaY)
{
    if (qAbs(angleDeltaY) >= 120 || qAbs(angleDeltaX) >= 120) {
        double delta = (qAbs(angleDeltaY) >= 120) ? angleDeltaY : angleDeltaX;
        if (m_zoom > 0.1 && delta < 0) {
            m_pan.setX(m_pan.x() - (x / width()  - 0.5 + m_pan.x()) * (1.0 - 1.0 / ZOOMSTEP));
            m_pan.setY(m_pan.y() + (y / height() - 0.5 - m_pan.y()) * (1.0 - 1.0 / ZOOMSTEP));
            m_zoom /= ZOOMSTEP;
        } else if (m_zoom < 10 && delta > 0) {
            m_pan.setX(m_pan.x() - (x / width()  - 0.5 + m_pan.x()) * (1.0 - ZOOMSTEP));
            m_pan.setY(m_pan.y() + (y / height() - 0.5 - m_pan.y()) * (1.0 - ZOOMSTEP));
            m_zoom *= ZOOMSTEP;
        }
        updateProjection();
        updateView();
    } else {
        m_pan.setX(m_pan.x() - angleDeltaX / width());
        m_pan.setY(m_pan.y() + angleDeltaY / height());
        updateProjection();
    }
}

void GLQuickItem::pinchStarted(qreal cx, qreal cy)
{
    Q_UNUSED(cx); Q_UNUSED(cy);
    m_gestureProcessing = true;
    m_pinchStartZoom = m_zoom;
    m_pinchStartPan  = m_pan;
}

void GLQuickItem::pinchUpdated(qreal cx, qreal cy, qreal scale)
{
    double newZoom = m_pinchStartZoom * scale;
    if (newZoom > 0.1 && newZoom < 10.0) {
        m_pan.setX(m_pinchStartPan.x() - (cx / width()  - 0.5 + m_pinchStartPan.x()) * (1.0 - scale));
        m_pan.setY(m_pinchStartPan.y() + (cy / height() - 0.5 - m_pinchStartPan.y()) * (1.0 - scale));
        m_zoom = newZoom;
    }
    updateProjection();
    updateView();
}

void GLQuickItem::pinchFinished()
{
    m_gestureProcessing = false;
}

// --- Properties -----------------------------------------------------------

QString GLQuickItem::parserStatus() const { return m_parserStatus; }
void GLQuickItem::setParserStatus(const QString &v)
{
    if (m_parserStatus == v) return;
    m_parserStatus = v;
    emit parserStatusChanged();
}

QString GLQuickItem::speedState() const { return m_speedState; }
void GLQuickItem::setSpeedState(const QString &v)
{
    if (m_speedState == v) return;
    m_speedState = v;
    emit speedStateChanged();
}

QString GLQuickItem::pinState() const { return m_pinState; }
void GLQuickItem::setPinState(const QString &v)
{
    if (m_pinState == v) return;
    m_pinState = v;
    emit pinStateChanged();
}

QString GLQuickItem::bufferState() const { return m_bufferState; }
void GLQuickItem::setBufferState(const QString &v)
{
    if (m_bufferState == v) return;
    m_bufferState = v;
    emit bufferStateChanged();
}

QTime GLQuickItem::spendTime() const { return m_spendTime; }
void GLQuickItem::setSpendTime(const QTime &v)
{
    m_spendTime = v;
    emit spendTimeChanged();
}

QTime GLQuickItem::estimatedTime() const { return m_estimatedTime; }
void GLQuickItem::setEstimatedTime(const QTime &v)
{
    m_estimatedTime = v;
    emit estimatedTimeChanged();
}

QString GLQuickItem::spendTimeStr() const     { return m_spendTime.toString("hh:mm:ss"); }
QString GLQuickItem::estimatedTimeStr() const { return m_estimatedTime.toString("hh:mm:ss"); }

bool GLQuickItem::updating() const { return m_updating; }
void GLQuickItem::setUpdating(bool v)
{
    if (m_updating == v) return;
    m_updating = v;
    emit updatingChanged();
}

int GLQuickItem::fps() const { return m_fps; }

bool GLQuickItem::perspective() const { return m_perspective; }
void GLQuickItem::setPerspective(bool v)
{
    if (m_perspective == v) return;
    m_perspective = v;
    updateProjection();
    updateView();
    emit perspectiveChanged();
}

QColor GLQuickItem::colorBackground() const { return m_colorBackground; }
void GLQuickItem::setColorBackground(const QColor &v)
{
    if (m_colorBackground == v) return;
    m_colorBackground = v;
    emit colorBackgroundChanged();
}

QColor GLQuickItem::colorText() const { return m_colorText; }
void GLQuickItem::setColorText(const QColor &v) { m_colorText = v; }

bool GLQuickItem::antialiasing() const { return m_antialiasing; }
void GLQuickItem::setAntialiasing(bool v) { m_antialiasing = v; }

double GLQuickItem::lineWidth() const { return m_lineWidth; }
void GLQuickItem::setLineWidth(double v) { m_lineWidth = v; }

double GLQuickItem::pointSize() const { return m_pointSize; }
void GLQuickItem::setPointSize(double v) { m_pointSize = v; }

bool GLQuickItem::msaa() const { return m_msaa; }
void GLQuickItem::setMsaa(bool v) { m_msaa = v; }

bool GLQuickItem::vsync() const { return m_vsync; }
void GLQuickItem::setVsync(bool v) { m_vsync = v; }

bool GLQuickItem::zBuffer() const { return m_zBuffer; }
void GLQuickItem::setZBuffer(bool v) { m_zBuffer = v; }

void GLQuickItem::setFps(int fps)
{
    if (fps <= 0) return;
    m_targetFps = fps;
    setUpdatesEnabled(m_updatesEnabled);
}

bool GLQuickItem::updatesEnabled() const { return m_updatesEnabled; }
void GLQuickItem::setUpdatesEnabled(bool v)
{
    m_updatesEnabled = v;
    if (v) {
        m_paintTimer->start(1000 / m_targetFps);
    } else {
        m_paintTimer->stop();
    }
}

// --- Bounds ---------------------------------------------------------------

double GLQuickItem::boundsXMin() const { return m_modelLowerBounds.x(); }
double GLQuickItem::boundsXMax() const { return m_modelUpperBounds.x(); }
double GLQuickItem::boundsYMin() const { return m_modelLowerBounds.y(); }
double GLQuickItem::boundsYMax() const { return m_modelUpperBounds.y(); }
double GLQuickItem::boundsZMin() const { return m_modelLowerBounds.z(); }
double GLQuickItem::boundsZMax() const { return m_modelUpperBounds.z(); }
double GLQuickItem::rangeX() const { return m_modelRanges.x(); }
double GLQuickItem::rangeY() const { return m_modelRanges.y(); }
double GLQuickItem::rangeZ() const { return m_modelRanges.z(); }
int    GLQuickItem::vertices() const { return m_vertices; }

// --- Render-thread callbacks (queued) ------------------------------------

void GLQuickItem::reportFps(int fps)
{
    if (m_fps == fps) return;
    m_fps = fps;
    emit fpsChanged();
}

void GLQuickItem::reportVertices(int vertices)
{
    if (m_vertices == vertices) return;
    m_vertices = vertices;
    emit verticesChanged();
}

// --- Timer / animation ---------------------------------------------------

void GLQuickItem::onPaintTimer()
{
    if (m_animateView)
        viewAnimation();
    updateRenderState();
    update();
}

void GLQuickItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickFramebufferObject::geometryChanged(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        updateProjection();
        emit resized();
    }
}

// --- View math (ported from GLWidget) ------------------------------------

void GLQuickItem::updateProjection()
{
    if (width() <= 0 || height() <= 0)
        return;

    m_projectionMatrix.setToIdentity();

    if (m_perspective) {
        const double zNear = 2.0;
        const double zFar  = m_distance * 10;
        double asp = (height() > 0) ? width() / height() : 1.0;

        m_projectionMatrix.frustum(
            (-0.5 + m_pan.x()) * asp, (0.5 + m_pan.x()) * asp,
            -0.5 + m_pan.y(),         0.5 + m_pan.y(),
            zNear, zFar
        );
        double z = m_distance + (m_viewUpperBounds + m_viewLowerBounds).z() / 2 * m_zoom;
        m_planeDepth = (1.0 / z - 1.0 / zNear) / (1.0 / zFar - 1.0 / zNear) * 2.0 - 1.0;
    } else {
        const float paddingScale = 1.3f;
        if (qFuzzyIsNull(m_viewRanges.length()))
            m_viewRanges = QVector3D(100, 100, 0) / paddingScale;

        double screenToWorld = qMax(
            m_viewRanges.x() / width(),
            m_viewRanges.y() / height()
        ) * paddingScale;
        double clip = qMax(1000.0, (double)m_viewRanges.length());

        m_projectionMatrix.ortho(
            (-width()  / 2 + m_pan.x() * width())  * screenToWorld / m_zoom,
            ( width()  / 2 + m_pan.x() * width())  * screenToWorld / m_zoom,
            (-height() / 2 + m_pan.y() * height()) * screenToWorld / m_zoom,
            ( height() / 2 + m_pan.y() * height()) * screenToWorld / m_zoom,
            -clip, clip
        );
        m_planeDepth = 0;
    }
}

void GLQuickItem::updateView()
{
    m_viewMatrix.setToIdentity();
    QPointF ang = m_rot * M_PI / 180.0;

    QVector3D eye = m_lookAt + QVector3D(
        cos(ang.x()) * sin(ang.y()),
        sin(ang.x()),
        cos(ang.x()) * cos(ang.y())
    ) * m_distance;

    QVector3D up(
        fabs(m_rot.x()) == 90 ? -sin(ang.y() + (m_rot.x() < 0 ? M_PI : 0)) : 0,
        cos(ang.x()),
        fabs(m_rot.x()) == 90 ? -cos(ang.y() + (m_rot.x() < 0 ? M_PI : 0)) : 0
    );

    m_viewMatrix.lookAt(eye, m_lookAt, up.normalized());

    if (m_perspective) {
        m_viewMatrix.translate(m_lookAt);
        m_viewMatrix.scale(m_zoom);
        m_viewMatrix.translate(-m_lookAt);
    }

    m_viewMatrix.rotate(-90, 1.0, 0.0, 0.0);

    QMatrix4x4 ivp = (m_projectionMatrix * m_viewMatrix).inverted();
    m_windowSizeWorld = (ivp * QVector3D(0, 1, m_planeDepth)
                       - ivp * QVector3D(0, 0, m_planeDepth)).length() * 2.0;
}

void GLQuickItem::updateRenderState()
{
    m_renderState.projectionMatrix = m_projectionMatrix;
    m_renderState.viewMatrix       = m_viewMatrix;
    m_renderState.colorBackground  = m_colorBackground;
    m_renderState.antialiasing     = m_antialiasing;
    m_renderState.msaa             = m_msaa;
    m_renderState.zBuffer          = m_zBuffer;
    m_renderState.lineWidth        = m_lineWidth;
    m_renderState.pointSize        = m_pointSize;
    m_renderState.windowSizeWorld  = m_windowSizeWorld;
    m_renderState.drawables        = m_shaderDrawables;
}

double GLQuickItem::normalizeAngle(double angle)
{
    while (angle < 0)   angle += 360;
    while (angle > 360) angle -= 360;
    return angle;
}

void GLQuickItem::beginViewAnimation()
{
    m_rotAnimationStart = m_rot;
    m_animationFrame    = 0;
    m_animateView       = true;
}

void GLQuickItem::stopViewAnimation()
{
    m_animateView = false;
}

void GLQuickItem::viewAnimation()
{
    double t = (double)m_animationFrame++ / (m_targetFps * 0.2);
    if (t >= 1.0) {
        stopViewAnimation();
        t = 1.0;
    }
    QEasingCurve ec(QEasingCurve::OutExpo);
    double val = ec.valueForProgress(t);
    m_rot = m_rotAnimationStart + (m_rotAnimationTarget - m_rotAnimationStart) * val;
    updateView();
}
