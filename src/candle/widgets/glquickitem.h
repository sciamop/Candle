// This file is a part of "Candle" application.
// Copyright 2015-2025 Hayrullin Denis Ravilevich

#pragma once

#include <QtQuick/QQuickFramebufferObject>
#include <QMatrix4x4>
#include <QVector3D>
#include <QColor>
#include <QTime>
#include <QTimer>
#include <QList>
#include "../drawers/shaderdrawable.h"

class GLQuickItem : public QQuickFramebufferObject
{
    Q_OBJECT

    Q_PROPERTY(QString parserStatus     READ parserStatus     WRITE setParserStatus     NOTIFY parserStatusChanged)
    Q_PROPERTY(QString speedState       READ speedState       WRITE setSpeedState       NOTIFY speedStateChanged)
    Q_PROPERTY(QString pinState         READ pinState         WRITE setPinState         NOTIFY pinStateChanged)
    Q_PROPERTY(QString bufferState      READ bufferState      WRITE setBufferState      NOTIFY bufferStateChanged)
    Q_PROPERTY(QString spendTimeStr     READ spendTimeStr                               NOTIFY spendTimeChanged)
    Q_PROPERTY(QString estimatedTimeStr READ estimatedTimeStr                           NOTIFY estimatedTimeChanged)
    Q_PROPERTY(bool    updating         READ updating         WRITE setUpdating         NOTIFY updatingChanged)
    Q_PROPERTY(int     fps              READ fps                                        NOTIFY fpsChanged)
    Q_PROPERTY(bool    perspective      READ perspective      WRITE setPerspective      NOTIFY perspectiveChanged)
    Q_PROPERTY(QColor  colorBackground  READ colorBackground  WRITE setColorBackground  NOTIFY colorBackgroundChanged)
    Q_PROPERTY(double  boundsXMin       READ boundsXMin                                NOTIFY boundsChanged)
    Q_PROPERTY(double  boundsXMax       READ boundsXMax                                NOTIFY boundsChanged)
    Q_PROPERTY(double  boundsYMin       READ boundsYMin                                NOTIFY boundsChanged)
    Q_PROPERTY(double  boundsYMax       READ boundsYMax                                NOTIFY boundsChanged)
    Q_PROPERTY(double  boundsZMin       READ boundsZMin                                NOTIFY boundsChanged)
    Q_PROPERTY(double  boundsZMax       READ boundsZMax                                NOTIFY boundsChanged)
    Q_PROPERTY(double  rangeX           READ rangeX                                    NOTIFY boundsChanged)
    Q_PROPERTY(double  rangeY           READ rangeY                                    NOTIFY boundsChanged)
    Q_PROPERTY(double  rangeZ           READ rangeZ                                    NOTIFY boundsChanged)
    Q_PROPERTY(int     vertices         READ vertices                                  NOTIFY verticesChanged)

public:
    // Render-thread state snapshot, copied during synchronize()
    struct RenderState {
        QMatrix4x4 projectionMatrix;
        QMatrix4x4 viewMatrix;
        QColor colorBackground;
        bool antialiasing   = false;
        bool msaa           = false;
        bool zBuffer        = false;
        double lineWidth    = 1.0;
        double pointSize    = 1.0;
        double windowSizeWorld = 0.0;
        QList<ShaderDrawable *> drawables;
    };

    explicit GLQuickItem(QQuickItem *parent = nullptr);
    ~GLQuickItem() override;

    Renderer *createRenderer() const override;

    // Public for Renderer access during synchronize()
    RenderState m_renderState;

public slots:
    // Called from render thread via QMetaObject::invokeMethod (queued)
    void reportFps(int fps);
    void reportVertices(int vertices);

    // Drawable management (matches GLWidget API)
    void addDrawable(ShaderDrawable *drawable);
    void updateModelBounds(ShaderDrawable *drawable);
    void fitDrawable(ShaderDrawable *drawable = nullptr);

    // Render settings
    bool antialiasing() const;   void setAntialiasing(bool v);
    double lineWidth() const;    void setLineWidth(double v);
    double pointSize() const;    void setPointSize(double v);
    bool msaa() const;           void setMsaa(bool v);
    bool vsync() const;          void setVsync(bool v);
    bool zBuffer() const;        void setZBuffer(bool v);
    void setFps(int fps);
    bool updatesEnabled() const; void setUpdatesEnabled(bool v);

    // Status properties
    QString parserStatus() const;   void setParserStatus(const QString &v);
    QString speedState() const;     void setSpeedState(const QString &v);
    QString pinState() const;       void setPinState(const QString &v);
    QString bufferState() const;    void setBufferState(const QString &v);
    QTime spendTime() const;        void setSpendTime(const QTime &v);
    QTime estimatedTime() const;    void setEstimatedTime(const QTime &v);
    QString spendTimeStr() const;
    QString estimatedTimeStr() const;
    bool updating() const;          void setUpdating(bool v);
    int fps() const;
    bool perspective() const;       void setPerspective(bool v);
    QColor colorBackground() const; void setColorBackground(const QColor &v);
    QColor colorText() const;       void setColorText(const QColor &v);

    // Bounds (exposed to QML HUD)
    double boundsXMin() const;
    double boundsXMax() const;
    double boundsYMin() const;
    double boundsYMax() const;
    double boundsZMin() const;
    double boundsZMax() const;
    double rangeX() const;
    double rangeY() const;
    double rangeZ() const;
    int vertices() const;

    // View presets (invokable from QML)
    Q_INVOKABLE void setTopView();
    Q_INVOKABLE void setFrontView();
    Q_INVOKABLE void setLeftView();
    Q_INVOKABLE void setIsometricView();
    Q_INVOKABLE void fitView();

    // Mouse/touch input forwarded from QML
    Q_INVOKABLE void mousePressed(qreal x, qreal y);
    Q_INVOKABLE void mouseMoved(qreal x, qreal y, int buttons, bool shiftHeld);
    Q_INVOKABLE void mouseWheeled(qreal x, qreal y, qreal angleDeltaX, qreal angleDeltaY);
    Q_INVOKABLE void pinchStarted(qreal cx, qreal cy);
    Q_INVOKABLE void pinchUpdated(qreal cx, qreal cy, qreal scale);
    Q_INVOKABLE void pinchFinished();

signals:
    void resized();
    void rotationChanged();
    void parserStatusChanged();
    void speedStateChanged();
    void pinStateChanged();
    void bufferStateChanged();
    void spendTimeChanged();
    void estimatedTimeChanged();
    void updatingChanged();
    void fpsChanged();
    void perspectiveChanged();
    void colorBackgroundChanged();
    void boundsChanged();
    void verticesChanged();

protected:
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private slots:
    void onPaintTimer();

private:
    // View state (main thread)
    QPointF   m_rot;
    QPointF   m_storedRot;
    QPointF   m_pan;
    QPointF   m_storedPan;
    QVector3D m_lookAt;
    QPoint    m_lastMousePos;
    double    m_zoom;
    double    m_distance;
    bool      m_perspective;
    bool      m_gestureProcessing;
    qreal     m_pinchStartZoom;
    QPointF   m_pinchStartPan;

    QVector3D m_viewLowerBounds;
    QVector3D m_viewUpperBounds;
    QVector3D m_viewRanges;
    QVector3D m_modelLowerBounds;
    QVector3D m_modelUpperBounds;
    QVector3D m_modelRanges;

    QMatrix4x4 m_projectionMatrix;
    QMatrix4x4 m_viewMatrix;
    double     m_planeDepth;
    double     m_windowSizeWorld;

    // Settings
    double m_lineWidth;
    double m_pointSize;
    bool   m_antialiasing;
    bool   m_msaa;
    bool   m_vsync;
    bool   m_zBuffer;
    int    m_targetFps;
    bool   m_updatesEnabled;

    // Status
    QString m_parserStatus;
    QString m_speedState;
    QString m_pinState;
    QString m_bufferState;
    QTime   m_spendTime;
    QTime   m_estimatedTime;
    bool    m_updating;

    // View animation
    bool    m_animateView;
    int     m_animationFrame;
    QPointF m_rotAnimationStart;
    QPointF m_rotAnimationTarget;

    // Perf counters (updated from render thread via queued invoke)
    int m_fps;
    int m_vertices;

    QColor m_colorBackground;
    QColor m_colorText;

    QList<ShaderDrawable *> m_shaderDrawables;
    QTimer *m_paintTimer;

    void updateProjection();
    void updateView();
    void updateRenderState();
    double normalizeAngle(double angle);
    void beginViewAnimation();
    void stopViewAnimation();
    void viewAnimation();
};
