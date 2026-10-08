// SPDX-License-Identifier: MIT

import QtQuick 6.5

import MapLibre 4.0

Window {
    id: window
    width: Qt.platform.os === "android" ? Screen.width : 512
    height: Qt.platform.os === "android" ? Screen.height : 512
    visible: true

    property bool fullWindow: false  // toggle full map with the 'F' key

    // ---- Application (UI) scale -------------------------------------------------
    // Scales the whole application, including the map and its labels, via
    // QT_SCALE_FACTOR. Applied on startup, so a restart is required to change it.
    property double appliedScale: appScale.appliedScale()
    property double selectedScale: appliedScale
    property bool confirmOpen: false
    readonly property var scalePresets: [0.75, 1.0, 1.25, 1.5, 2.0]
    readonly property bool scalePending: Math.abs(selectedScale - appliedScale) > 0.001

    function nearestPresetIndex(value) {
        let best = 0;
        let bestDist = Math.abs(scalePresets[0] - value);
        for (let i = 1; i < scalePresets.length; ++i) {
            const d = Math.abs(scalePresets[i] - value);
            if (d < bestDist) {
                bestDist = d;
                best = i;
            }
        }
        return best;
    }

    function stepScale(direction) {
        let index = nearestPresetIndex(selectedScale) + direction;
        index = Math.max(0, Math.min(scalePresets.length - 1, index));
        selectedScale = scalePresets[index];
        confirmOpen = true;
    }

    function restartNow() {
        confirmOpen = false;
        appScale.applyAndRestart(selectedScale);
    }

    Shortcut { sequence: "Ctrl+="; onActivated: window.stepScale(1) }
    Shortcut { sequence: "Ctrl+Shift+="; onActivated: window.stepScale(1) }
    Shortcut { sequence: "Ctrl+-"; onActivated: window.stepScale(-1) }
    Shortcut { sequence: "Ctrl+0"; onActivated: { window.selectedScale = 1.0; window.confirmOpen = true; } }
    Shortcut { sequence: "Ctrl+R"; onActivated: { if (window.scalePending) window.restartNow(); } }

    Rectangle {
        color: "blue"
        anchors.fill: parent
        focus: true

        Text {
            text: "Press 'F' or tap here to toggle full map"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 10
            color: "white"
            font.pixelSize: 16
        }

        MouseArea {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: window.fullWindow ? 0 : Math.round(parent.height / 3)
            enabled: !window.fullWindow
            onClicked: {
                window.fullWindow = !window.fullWindow
            }
        }

        Shortcut {
            sequence: 'F'
            onActivated: {
                window.fullWindow = !window.fullWindow
            }
        }
    }

    Rectangle {
        color: "red"
        anchors.fill: parent
        anchors.topMargin: fullWindow ? 0 : Math.round(parent.height / 3)

        MapLibre {
            id: map
            anchors.fill: parent
            anchors.topMargin: fullWindow ? 0 : Math.round(parent.height / 6)
            anchors.leftMargin: fullWindow ? 0 : Math.round(parent.width / 6)
            focus: true

            // Liberty style (OSM / OpenFreeMap) - https://openfreemap.org
            style: "https://tiles.openfreemap.org/styles/liberty"
            zoomLevel: 4
            coordinate: [59.91, 10.75]

            PinchHandler {
                id: pinch
                target: null
                onScaleChanged: (delta) => {
                    map.scale(delta, pinch.centroid.position)
                }
                grabPermissions: PointerHandler.TakeOverForbidden
            }

            DragHandler {
                id: drag
                target: null
                onTranslationChanged: (delta) => map.pan(delta)
            }

            WheelHandler {
                id: wheel
                acceptedDevices: Qt.platform.pluginName === "cocoa" || Qt.platform.pluginName === "wayland"
                                ? PointerDevice.Mouse | PointerDevice.TouchPad
                                : PointerDevice.Mouse
                onWheel: (event) => {
                    map.scale(Math.pow(2.0, event.angleDelta.y / 120), wheel.point.position)
                }
            }
        }
    }

    // ---- Scale indicator --------------------------------------------------------
    Rectangle {
        z: 10
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 8
        color: "#cc000000"
        radius: 4
        width: scaleLabel.implicitWidth + 16
        height: scaleLabel.implicitHeight + 8

        Text {
            id: scaleLabel
            anchors.centerIn: parent
            color: "white"
            font.pixelSize: 14
            text: window.scalePending
                  ? "Scale " + window.selectedScale.toFixed(2) + "x - restart to apply (Ctrl+R)"
                  : "Scale " + window.selectedScale.toFixed(2) + "x   [Ctrl+= / Ctrl+-]"
        }
    }

    // ---- Restart confirmation ---------------------------------------------------
    Rectangle {
        z: 20
        visible: window.confirmOpen && window.scalePending
        anchors.centerIn: parent
        width: 360
        height: 150
        color: "#f2f2f2"
        radius: 8
        border.color: "#888888"

        Column {
            anchors.centerIn: parent
            width: parent.width - 24
            spacing: 16

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Restart to apply " + window.selectedScale.toFixed(2) + "x ?"
                color: "black"
                font.pixelSize: 16
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 16

                Rectangle {
                    width: 120
                    height: 38
                    radius: 4
                    color: "#2d7ef7"
                    Text {
                        anchors.centerIn: parent
                        text: "Restart"
                        color: "white"
                        font.pixelSize: 14
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: window.restartNow()
                    }
                }

                Rectangle {
                    width: 120
                    height: 38
                    radius: 4
                    color: "#d0d0d0"
                    Text {
                        anchors.centerIn: parent
                        text: "Later"
                        color: "black"
                        font.pixelSize: 14
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: window.confirmOpen = false
                    }
                }
            }
        }
    }
}
