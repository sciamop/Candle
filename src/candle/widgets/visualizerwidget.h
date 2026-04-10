// This file is a part of "Candle" application.
// Copyright 2015-2025 Hayrullin Denis Ravilevich

#pragma once

#include <QQuickWidget>
#include <QTime>
#include <QColor>
#include "glquickitem.h"
#include "../drawers/shaderdrawable.h"

// Drop-in replacement for GLWidget.  Hosts the QML visualizer scene and
// delegates the full GLWidget public API to the embedded GLQuickItem.
class VisualizerWidget : public QQuickWidget
{
    Q_OBJECT
public:
    explicit VisualizerWidget(QWidget *parent = nullptr);
    ~VisualizerWidget() override;

    // Drawable management
    void addDrawable(ShaderDrawable *drawable);
    void updateModelBounds(ShaderDrawable *drawable);
    void fitDrawable(ShaderDrawable *drawable = nullptr);

    // Render settings
    bool antialiasing() const;   void setAntialiasing(bool v);
    double lineWidth() const;    void setLineWidth(double v);
    bool msaa() const;           void setMsaa(bool v);
    bool zBuffer() const;        void setZBuffer(bool v);
    bool vsync() const;          void setVsync(bool v);
    void setFps(int fps);
    bool updatesEnabled() const; void setUpdatesEnabled(bool v);

    // Status / HUD
    QString parserStatus() const;   void setParserStatus(const QString &v);
    QString speedState() const;     void setSpeedState(const QString &v);
    QString pinState() const;       void setPinState(const QString &v);
    QString bufferState() const;    void setBufferState(const QString &v);
    QTime spendTime() const;        void setSpendTime(const QTime &v);
    QTime estimatedTime() const;    void setEstimatedTime(const QTime &v);
    bool updating() const;          void setUpdating(bool v);
    bool perspective() const;       void setPerspective(bool v);
    QColor colorBackground() const; void setColorBackground(const QColor &v);
    QColor colorText() const;       void setColorText(const QColor &v);

    // View presets (mirrors GLWidget)
    void setTopView();
    void setFrontView();
    void setLeftView();
    void setIsometricView();

signals:
    void resized();
    void rotationChanged();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    GLQuickItem *m_glItem = nullptr;
};
