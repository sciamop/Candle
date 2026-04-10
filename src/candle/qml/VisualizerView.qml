// Candle — CNC Visualizer
// Design intent: precision instrument, not an app.
// Every element earns its place; nothing decorates.

import QtQuick 2.15
import QtQuick.Controls 2.15
import Candle 1.0

Item {
    id: root

    // ── Design tokens ─────────────────────────────────────────────────────
    // Cold steel: machined aluminum + coolant-tinted accent
    readonly property color clrRaise:      "#1a1e24"   // HUD panel bg
    readonly property color clrLift:       "#22272e"   // button hover
    readonly property color clrActiveFace: "#0f2b27"   // active button (teal tint)
    readonly property color clrBorder:     Qt.rgba(1, 1, 1, 0.07)
    readonly property color clrAccent:     "#2a8a7c"   // coolant teal
    readonly property color txtPrimary:    "#c8cdd3"
    readonly property color txtSecondary:  "#5a6370"
    readonly property color txtMuted:      "#333a42"
    readonly property string monoFont:     "monospace"

    // ── 3D renderer ───────────────────────────────────────────────────────
    GlVisualizer {
        id: gl3d
        objectName: "gl3d"
        anchors.fill: parent
    }

    // ── Input layer ───────────────────────────────────────────────────────
    // PinchArea wraps MouseArea so both work without conflict.
    PinchArea {
        anchors.fill: parent

        onPinchStarted:  gl3d.pinchStarted(pinch.center.x, pinch.center.y)
        onPinchUpdated:  gl3d.pinchUpdated(pinch.center.x, pinch.center.y, pinch.scale)
        onPinchFinished: gl3d.pinchFinished()

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton

            onPressed:         gl3d.mousePressed(mouseX, mouseY)
            onPositionChanged: gl3d.mouseMoved(mouseX, mouseY, pressedButtons,
                                   (mouse.modifiers & Qt.ShiftModifier) ? true : false)
            onWheel:           gl3d.mouseWheeled(wheel.x, wheel.y,
                                   wheel.angleDelta.x, wheel.angleDelta.y)
        }
    }

    // ── Top-left: Machine state readouts ──────────────────────────────────
    // Flush to corner, monospace. No background panel — just readable text
    // with enough contrast against the dark viewport.
    Column {
        anchors.top:     parent.top
        anchors.left:    parent.left
        anchors.margins: 10
        spacing: 3

        StatusText { text: gl3d.parserStatus;  visible: gl3d.parserStatus  !== "" }
        StatusText { text: gl3d.speedState;    visible: gl3d.speedState    !== "" }
        StatusText { text: gl3d.pinState;      visible: gl3d.pinState      !== "" }
    }

    // ── Bottom-left: Model bounds ──────────────────────────────────────────
    // Four lines: X bounds, Y bounds, Z bounds, ranges.
    // Reads like a DRO panel — axis label + value pair.
    Column {
        anchors.bottom:  parent.bottom
        anchors.left:    parent.left
        anchors.margins: 10
        spacing: 2

        BoundsRow {
            axisLabel: "X"
            minVal:    gl3d.boundsXMin
            maxVal:    gl3d.boundsXMax
        }
        BoundsRow {
            axisLabel: "Y"
            minVal:    gl3d.boundsYMin
            maxVal:    gl3d.boundsYMax
        }
        BoundsRow {
            axisLabel: "Z"
            minVal:    gl3d.boundsZMin
            maxVal:    gl3d.boundsZMax
        }
        DimText {
            text: gl3d.rangeX.toFixed(3) + "  /  "
                + gl3d.rangeY.toFixed(3) + "  /  "
                + gl3d.rangeZ.toFixed(3)
        }
    }

    // ── Bottom-right: Perf metrics ────────────────────────────────────────
    // Right-aligned, compact. FPS + vertex count are diagnostic.
    // Time display is the most important — job progress.
    Column {
        anchors.bottom:  parent.bottom
        anchors.right:   parent.right
        anchors.margins: 10
        spacing: 2

        MetricRow { label: "VTX"; value: gl3d.vertices.toLocaleString() }
        MetricRow { label: "FPS"; value: gl3d.fps.toString() }

        DimText {
            anchors.right: parent.right
            text: gl3d.spendTimeStr + "  /  " + gl3d.estimatedTimeStr
            color: root.txtSecondary
        }
        DimText {
            anchors.right: parent.right
            text: gl3d.bufferState
            visible: gl3d.bufferState !== ""
        }
    }

    // ── Center: Updating indicator ────────────────────────────────────────
    // Only visible when recalculating geometry. Breathes to signal activity.
    Text {
        anchors.centerIn: parent
        text: "UPDATING"
        font.family:      root.monoFont
        font.pixelSize:   11
        font.letterSpacing: 3
        color:            root.txtSecondary
        visible:          gl3d.updating

        SequentialAnimation on opacity {
            running: gl3d.updating
            loops:   Animation.Infinite
            NumberAnimation { to: 0.25; duration: 600; easing.type: Easing.InOutSine }
            NumberAnimation { to: 1.00; duration: 600; easing.type: Easing.InOutSine }
        }
    }

    // ══ Inline component definitions ══════════════════════════════════════

    // Plain status line — parser status, speed, pin state
    component StatusText: Text {
        font.family:  root.monoFont
        font.pixelSize: 11
        color:        root.txtSecondary
    }

    // Dim annotation text — bounds suffix, buffer state
    component DimText: Text {
        font.family:  root.monoFont
        font.pixelSize: 11
        color:        root.txtMuted
    }

    // Single axis bounds row:  X  -10.000 ···  150.000
    component BoundsRow: Row {
        property string axisLabel: "X"
        property double minVal: 0
        property double maxVal: 0
        spacing: 4

        Text {
            width: 14
            text: axisLabel
            font.family:  root.monoFont
            font.pixelSize: 11
            color:        root.txtMuted
            horizontalAlignment: Text.AlignRight
        }
        Text {
            // right-pad the numbers so the separator stays column-stable
            text: minVal.toFixed(3) + "  ···  " + maxVal.toFixed(3)
            font.family:  root.monoFont
            font.pixelSize: 11
            color:        root.txtSecondary
        }
    }

    // Metric row — right-aligned label + value pair
    component MetricRow: Row {
        property string label: ""
        property string value: ""
        spacing: 0

        Text {
            width: 36
            text:  parent.label
            font.family:  root.monoFont
            font.pixelSize: 10
            color:        root.txtMuted
            horizontalAlignment: Text.AlignRight
        }
        Text {
            width: 52
            text:  parent.value
            font.family:  root.monoFont
            font.pixelSize: 11
            color:        root.txtSecondary
            horizontalAlignment: Text.AlignRight
        }
    }
}
