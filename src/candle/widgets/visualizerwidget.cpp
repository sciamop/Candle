// This file is a part of "Candle" application.
// Copyright 2015-2025 Hayrullin Denis Ravilevich

#include "visualizerwidget.h"

#include <QQmlEngine>
#include <QQmlContext>
#include <QResizeEvent>
#include <QSurfaceFormat>
#include <QtQml>

VisualizerWidget::VisualizerWidget(QWidget *parent)
    : QQuickWidget(parent)
{
    // Shared OpenGL context for the scene graph
    QSurfaceFormat fmt;
    fmt.setSamples(4);
    QSurfaceFormat::setDefaultFormat(fmt);

    setResizeMode(QQuickWidget::SizeRootObjectToView);

    // Register our QML type before loading
    qmlRegisterType<GLQuickItem>("Candle", 1, 0, "GlVisualizer");

    setSource(QUrl(QStringLiteral("qrc:/qml/VisualizerView.qml")));

    if (status() != QQuickWidget::Ready) {
        qWarning() << "VisualizerWidget: QML failed to load:" << errors();
        return;
    }

    // Locate the GLQuickItem from the loaded scene
    m_glItem = rootObject()->findChild<GLQuickItem *>();
    if (!m_glItem)
        qWarning() << "VisualizerWidget: GLQuickItem not found in QML scene";
}

VisualizerWidget::~VisualizerWidget()
{
    // Dispose GL resources while a context is available
    if (m_glItem) {
        // GLQuickItem's drawables have their GL resources cleaned up
        // when the scene graph tears down.
    }
}

// --- Drawable management --------------------------------------------------

void VisualizerWidget::addDrawable(ShaderDrawable *drawable)
{
    if (m_glItem) m_glItem->addDrawable(drawable);
}

void VisualizerWidget::updateModelBounds(ShaderDrawable *drawable)
{
    if (m_glItem) m_glItem->updateModelBounds(drawable);
}

void VisualizerWidget::fitDrawable(ShaderDrawable *drawable)
{
    if (m_glItem) m_glItem->fitDrawable(drawable);
}

// --- Render settings ------------------------------------------------------

bool   VisualizerWidget::antialiasing() const     { return m_glItem ? m_glItem->antialiasing() : false; }
void   VisualizerWidget::setAntialiasing(bool v)  { if (m_glItem) m_glItem->setAntialiasing(v); }
double VisualizerWidget::lineWidth() const         { return m_glItem ? m_glItem->lineWidth() : 1.0; }
void   VisualizerWidget::setLineWidth(double v)    { if (m_glItem) m_glItem->setLineWidth(v); }
bool   VisualizerWidget::msaa() const              { return m_glItem ? m_glItem->msaa() : false; }
void   VisualizerWidget::setMsaa(bool v)           { if (m_glItem) m_glItem->setMsaa(v); }
bool   VisualizerWidget::zBuffer() const           { return m_glItem ? m_glItem->zBuffer() : false; }
void   VisualizerWidget::setZBuffer(bool v)        { if (m_glItem) m_glItem->setZBuffer(v); }
bool   VisualizerWidget::vsync() const             { return m_glItem ? m_glItem->vsync() : false; }
void   VisualizerWidget::setVsync(bool v)          { if (m_glItem) m_glItem->setVsync(v); }
void   VisualizerWidget::setFps(int fps)            { if (m_glItem) m_glItem->setFps(fps); }
bool   VisualizerWidget::updatesEnabled() const    { return m_glItem ? m_glItem->updatesEnabled() : false; }
void   VisualizerWidget::setUpdatesEnabled(bool v) { if (m_glItem) m_glItem->setUpdatesEnabled(v); }

// --- Status / HUD ---------------------------------------------------------

QString VisualizerWidget::parserStatus() const          { return m_glItem ? m_glItem->parserStatus() : QString(); }
void    VisualizerWidget::setParserStatus(const QString &v) { if (m_glItem) m_glItem->setParserStatus(v); }
QString VisualizerWidget::speedState() const            { return m_glItem ? m_glItem->speedState() : QString(); }
void    VisualizerWidget::setSpeedState(const QString &v)   { if (m_glItem) m_glItem->setSpeedState(v); }
QString VisualizerWidget::pinState() const              { return m_glItem ? m_glItem->pinState() : QString(); }
void    VisualizerWidget::setPinState(const QString &v)     { if (m_glItem) m_glItem->setPinState(v); }
QString VisualizerWidget::bufferState() const           { return m_glItem ? m_glItem->bufferState() : QString(); }
void    VisualizerWidget::setBufferState(const QString &v)  { if (m_glItem) m_glItem->setBufferState(v); }
QTime   VisualizerWidget::spendTime() const             { return m_glItem ? m_glItem->spendTime() : QTime(); }
void    VisualizerWidget::setSpendTime(const QTime &v)      { if (m_glItem) m_glItem->setSpendTime(v); }
QTime   VisualizerWidget::estimatedTime() const         { return m_glItem ? m_glItem->estimatedTime() : QTime(); }
void    VisualizerWidget::setEstimatedTime(const QTime &v)  { if (m_glItem) m_glItem->setEstimatedTime(v); }
bool    VisualizerWidget::updating() const              { return m_glItem ? m_glItem->updating() : false; }
void    VisualizerWidget::setUpdating(bool v)               { if (m_glItem) m_glItem->setUpdating(v); }
bool    VisualizerWidget::perspective() const           { return m_glItem ? m_glItem->perspective() : false; }
void    VisualizerWidget::setPerspective(bool v)            { if (m_glItem) m_glItem->setPerspective(v); }
QColor  VisualizerWidget::colorBackground() const      { return m_glItem ? m_glItem->colorBackground() : Qt::black; }
void    VisualizerWidget::setColorBackground(const QColor &v) { if (m_glItem) m_glItem->setColorBackground(v); }
QColor  VisualizerWidget::colorText() const            { return m_glItem ? m_glItem->colorText() : Qt::white; }
void    VisualizerWidget::setColorText(const QColor &v)   { if (m_glItem) m_glItem->setColorText(v); }

// --- View presets ---------------------------------------------------------

void VisualizerWidget::setTopView()        { if (m_glItem) m_glItem->setTopView(); }
void VisualizerWidget::setFrontView()      { if (m_glItem) m_glItem->setFrontView(); }
void VisualizerWidget::setLeftView()       { if (m_glItem) m_glItem->setLeftView(); }
void VisualizerWidget::setIsometricView()  { if (m_glItem) m_glItem->setIsometricView(); }

// --- Events ---------------------------------------------------------------

void VisualizerWidget::resizeEvent(QResizeEvent *event)
{
    QQuickWidget::resizeEvent(event);
    emit resized();
}
