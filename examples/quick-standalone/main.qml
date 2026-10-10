// SPDX-License-Identifier: MIT

import QtQuick 6.5
import QtQuick.Controls 6.5
import QtQuick.Controls.Material 6.5
import QtQuick.Layouts 6.5

import MapLibre 4.0

Window {
    id: window
    visible: true
    // Normal window (not maximized) filled entirely by the map.
    width: Qt.platform.os === "android" ? Screen.width : 1024
    height: Qt.platform.os === "android" ? Screen.height : 768
    visibility: Qt.platform.os === "android" ? Window.FullScreen : Window.Windowed

    // Controls style (dialogs, buttons, text fields)
    property bool darkMode: false
    Material.theme: darkMode ? Material.Dark : Material.Light
    Material.accent: "#2d7ef7"

    // ---- Application (UI) scale -------------------------------------------------
    // Scales the whole application, including the map and its labels, via
    // QT_SCALE_FACTOR. Applied on startup, so a restart is required to change it.
    property double appliedScale: appState.appliedScale()
    property double selectedScale: appliedScale
    readonly property var scalePresets: [0.75, 1.0, 1.25, 1.5, 2.0]
    readonly property bool scalePending: Math.abs(selectedScale - appliedScale) > 0.001

    // 2D flat (pitch 0) vs 3D tilted (pitch 60) view.
    property bool globe3D: false
    readonly property real tilt3DPitch: 60

    // 3D buildings opacity: Liberty's "building-3d" layer is 0.8 by default.
    property bool buildingsOpaque: false
    // The 3D-buildings opacity override only applies to styles that actually
    // contain a "building-3d" fill-extrusion layer (the OpenFreeMap styles).
    // It is added/removed dynamically so styles without it (e.g. Open Waters)
    // don't log "Unable to add layer building-3d" / "Layer not found".
    property var buildingLayerParam: null

    // True while any modal dialog is open. The map's gesture handlers are
    // disabled then, so touch is handled by the dialog (otherwise dragging a
    // list inside a dialog scrolled the map behind it).
    readonly property bool dialogOpen:
        presetsDialog.visible || settingsDialog.visible || mapDialog.visible
        || nameDialog.visible || addMapDialog.visible

    // ---- Camera presets ---------------------------------------------------------
    // Each entry: { name: string, center: [lat, lon], zoom: real, globe3D: bool }
    property var presets: []
    property int presetIndex: -1

    // ---- Map styles (style URLs) ------------------------------------------------
    // Each entry: { name: string, url: string }
    property string styleUrl: "https://tiles.openfreemap.org/styles/liberty"
    property var mapStyles: []
    property int mapIndex: -1
    readonly property var defaultMapStyles: [
        { "name": "Liberty (OSM)", "url": "https://tiles.openfreemap.org/styles/liberty" },
        { "name": "Bright (OSM)", "url": "https://tiles.openfreemap.org/styles/bright" },
        { "name": "Positron (OSM)", "url": "https://tiles.openfreemap.org/styles/positron" },
        { "name": "Open Waters", "url": "https://tiles.openwaters.io/seamap/style.json" }
    ]

    // QSettings returns stored booleans as strings ("true"/"false").
    function asBool(v) {
        return v === true || v === "true" || v === 1;
    }

    function nearestScalePresetIndex(value) {
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
        let index = nearestScalePresetIndex(selectedScale) + direction;
        index = Math.max(0, Math.min(scalePresets.length - 1, index));
        selectedScale = scalePresets[index];
    }

    function restartNow() {
        saveState();
        appState.applyAndRestart(selectedScale);
    }

    // ---- 2D / 3D toggle ---------------------------------------------------------
    function applyTilt(animated) {
        map.easeTo({ pitch: window.globe3D ? window.tilt3DPitch : 0 },
                    { duration: animated ? 600 : 0 });
    }

    function toggle3D() {
        window.globe3D = !window.globe3D;
        applyTilt(true);
    }

    // Zoom in/out buttons (anchored at the map centre).
    function zoomBy(steps) {
        map.scale(Math.pow(2, steps), Qt.point(map.width / 2, map.height / 2));
    }

    // Adds the 3D-buildings opacity override for styles that have the layer,
    // removes it otherwise.
    function syncBuildingOverride() {
        const supported = window.styleUrl.indexOf("openfreemap.org") !== -1;
        if (supported && window.buildingLayerParam === null) {
            window.buildingLayerParam = buildingLayerComponent.createObject(map);
            map.addStyleParameter(window.buildingLayerParam);
        } else if (!supported && window.buildingLayerParam !== null) {
            map.removeStyleParameter(window.buildingLayerParam);
            window.buildingLayerParam.destroy();
            window.buildingLayerParam = null;
        }
    }

    Component {
        id: buildingLayerComponent
        LayerParameter {
            styleId: "building-3d"
            type: "fill-extrusion"
            paint: ({ "fill-extrusion-opacity": window.buildingsOpaque ? 1.0 : 0.8 })
        }
    }

    // ---- Camera presets ---------------------------------------------------------
    function savePresets() {
        appState.setValue("presets/list", JSON.stringify(presets));
    }

    function loadPresets() {
        const raw = appState.value("presets/list");
        if (raw === undefined || raw === null || raw === "") {
            presets = [];
            return;
        }
        try {
            const parsed = JSON.parse(raw);
            presets = Array.isArray(parsed) ? parsed : [];
        } catch (e) {
            presets = [];
        }
    }

    function addPreset(name) {
        const c = map.coordinate;
        const entry = {
            "name": name,
            "center": [c[0], c[1]],
            "zoom": map.zoomLevel,
            "globe3D": window.globe3D
        };
        window.presets = window.presets.concat([entry]);
        window.savePresets();
        window.presetIndex = window.presets.length - 1;
    }

    function removePresetAt(i) {
        if (i < 0 || i >= window.presets.length) {
            return;
        }
        const arr = window.presets.slice();
        arr.splice(i, 1);
        window.presets = arr;
        window.savePresets();
        window.presetIndex = arr.length > 0 ? Math.min(i, arr.length - 1) : -1;
    }

    function flyToPreset(i) {
        if (i < 0 || i >= window.presets.length) {
            return;
        }
        window.presetIndex = i;
        const p = window.presets[i];
        window.globe3D = p.globe3D === true;
        map.flyTo({ center: p.center, zoom: p.zoom,
                    pitch: window.globe3D ? window.tilt3DPitch : 0 },
                  { duration: 1500 });
    }

    // ---- Map styles -------------------------------------------------------------
    function saveMapStyles() {
        appState.setValue("maps/list", JSON.stringify(mapStyles));
    }

    function loadMapStyles() {
        const raw = appState.value("maps/list");
        let parsed = null;
        if (raw !== undefined && raw !== null && raw !== "") {
            try {
                const arr = JSON.parse(raw);
                if (Array.isArray(arr)) {
                    parsed = arr;
                }
            } catch (e) {
                parsed = null;
            }
        }
        if (parsed !== null && parsed.length > 0) {
            // Migrate the old "MapLibre Demo" (demotiles) entry to Open Waters.
            mapStyles = parsed.map(function(m) {
                if (m && m.url === "https://demotiles.maplibre.org/style.json") {
                    return { "name": "Open Waters", "url": "https://tiles.openwaters.io/seamap/style.json" };
                }
                return m;
            });
            saveMapStyles();
        } else {
            mapStyles = defaultMapStyles.slice();
            saveMapStyles();
        }
    }

    function addMapStyle(name, url) {
        const entry = { "name": name, "url": url };
        window.mapStyles = window.mapStyles.concat([entry]);
        window.saveMapStyles();
        window.mapIndex = window.mapStyles.length - 1;
    }

    function removeMapAt(i) {
        if (i < 0 || i >= window.mapStyles.length) {
            return;
        }
        const arr = window.mapStyles.slice();
        arr.splice(i, 1);
        window.mapStyles = arr;
        window.saveMapStyles();
        window.mapIndex = arr.length > 0 ? Math.min(i, arr.length - 1) : -1;
    }

    function selectMap(i) {
        if (i < 0 || i >= window.mapStyles.length) {
            return;
        }
        window.mapIndex = i;
        window.styleUrl = window.mapStyles[i].url;
        appState.setValue("map/styleUrl", window.styleUrl);
    }

    // ---- Persisted state --------------------------------------------------------
    function saveState() {
        // On Android the window is full-screen; don't persist window geometry.
        if (Qt.platform.os !== "android") {
            appState.setValue("window/width", window.width);
            appState.setValue("window/height", window.height);
            appState.setValue("window/x", window.x);
            appState.setValue("window/y", window.y);
        }
        appState.setValue("map/coordinate", map.coordinate);
        appState.setValue("map/zoomLevel", map.zoomLevel);
        appState.setValue("map/globe3D", window.globe3D);
        appState.setValue("map/styleUrl", window.styleUrl);
        appState.setValue("ui/darkMode", window.darkMode);
        savePresets();
        saveMapStyles();
    }

    function restoreState() {
        // On Android the window is full-screen; don't restore window geometry
        // (a stale persisted size would leave the map not filling the screen).
        if (Qt.platform.os !== "android") {
            const w = appState.value("window/width");
            const h = appState.value("window/height");
            if (w !== undefined && h !== undefined && w > 0 && h > 0) {
                window.width = w;
                window.height = h;
            }

            const x = appState.value("window/x");
            const y = appState.value("window/y");
            if (x !== undefined && y !== undefined && isOnScreen(x, y, window.width, window.height)) {
                window.x = x;
                window.y = y;
            }
        }

        const coordinate = appState.value("map/coordinate");
        if (coordinate !== undefined && coordinate.length === 2) {
            map.coordinate = coordinate;
        }

        const zoom = appState.value("map/zoomLevel");
        if (zoom !== undefined && zoom > 0) {
            map.zoomLevel = zoom;
        }

        if (asBool(appState.value("map/globe3D"))) {
            window.globe3D = true;
            applyTilt(false);
        }

        const storedStyle = appState.value("map/styleUrl");
        if (storedStyle !== undefined && storedStyle !== null && storedStyle !== "") {
            window.styleUrl = storedStyle;
        }
        if (window.styleUrl === "https://demotiles.maplibre.org/style.json") {
            // follow the MapLibre Demo -> Open Waters change
            window.styleUrl = "https://tiles.openwaters.io/seamap/style.json";
            appState.setValue("map/styleUrl", window.styleUrl);
        }

        window.darkMode = asBool(appState.value("ui/darkMode"));

        loadPresets();
        window.presetIndex = -1;
        loadMapStyles();
        window.mapIndex = -1;
    }

    function isOnScreen(x, y, w, h) {
        const screens = Qt.application.screens;
        for (let i = 0; i < screens.length; ++i) {
            const s = screens[i];
            if (x < s.virtualX + s.width && x + w > s.virtualX
                    && y < s.virtualY + s.height && y + h > s.virtualY) {
                return true;
            }
        }
        return false;
    }

    Component.onCompleted: {
        restoreState();
        syncBuildingOverride();
    }
    onClosing: saveState()
    onStyleUrlChanged: syncBuildingOverride()

    // Persist the scale as soon as the user changes it, so it survives an abrupt
    // exit too (the value is applied on the next start). The untouched default
    // of 1.0 is left unset so it does not override native/OS scaling.
    onSelectedScaleChanged: {
        if (appState.hasExplicitScale() || Math.abs(selectedScale - 1.0) > 0.001) {
            appState.setValue("ui/scaleFactor", selectedScale);
        }
        if (scaleSlider) {
            scaleSlider.value = selectedScale;
        }
    }

    // Re-apply the tilt when the style (re)loads, as a style load resets the camera.
    Connections {
        target: map
        function onStyleLoadedChanged() {
            if (map.styleLoaded && window.globe3D) {
                window.applyTilt(false);
            }
        }
    }

    Shortcut { sequence: "Ctrl+="; onActivated: { window.stepScale(1); settingsDialog.open(); } }
    Shortcut { sequence: "Ctrl+Shift+="; onActivated: { window.stepScale(1); settingsDialog.open(); } }
    Shortcut { sequence: "Ctrl+-"; onActivated: { window.stepScale(-1); settingsDialog.open(); } }
    Shortcut { sequence: "Ctrl+0"; onActivated: { window.selectedScale = 1.0; settingsDialog.open(); } }

    MapLibre {
        id: map
        anchors.fill: parent
        focus: true

        style: window.styleUrl
        zoomLevel: 4
        coordinate: [59.91, 10.75]

        // The 3D-buildings opacity override is added dynamically in C++/QML via
        // syncBuildingOverride() (see the Component at the top level).

        // Two-finger pinch: zoom about the centroid (the point between the
        // fingers stays put). Scale-only, like the upstream example.
        PinchHandler {
            id: pinch
            target: null
            acceptedDevices: PointerDevice.TouchScreen
            enabled: !window.dialogOpen
            onScaleChanged: (delta) => {
                if (delta > 0.25 && delta < 4.0) {
                    map.scale(delta, pinch.centroid.position)
                }
            }
        }

        // Single-finger / left-mouse pan, implemented as a one-point
        // PinchHandler. Unlike a DragHandler it resets its translation baseline
        // each time it (re)activates, so when a pinch ends it does not apply a
        // stale translation to the remaining finger (which made the map jump).
        PinchHandler {
            id: panHandler
            target: null
            minimumPointCount: 1
            maximumPointCount: 1
            enabled: !window.dialogOpen
            onTranslationChanged: (delta) => {
                if (Math.abs(delta.x) < 200 && Math.abs(delta.y) < 200) {
                    map.pan(Qt.point(delta.x, delta.y))
                }
            }
        }

        // Mouse / trackpad: right drag zooms smoothly toward the cursor.
        // Wheel clicks remain the stepped zoom below.
        PinchHandler {
            id: zoomDrag
            target: null
            acceptedButtons: Qt.RightButton
            minimumPointCount: 1
            maximumPointCount: 1
            enabled: !window.dialogOpen
            onTranslationChanged: (delta) => {
                map.scale(Math.pow(2.0, -delta.y / 250.0), zoomDrag.centroid.position)
            }
        }

        WheelHandler {
            id: wheel
            enabled: !window.dialogOpen
            acceptedDevices: Qt.platform.pluginName === "cocoa" || Qt.platform.pluginName === "wayland"
                            ? PointerDevice.Mouse | PointerDevice.TouchPad
                            : PointerDevice.Mouse
            onWheel: (event) => {
                map.scale(Math.pow(2.0, event.angleDelta.y / 120), wheel.point.position)
            }
        }
    }

    // ---- Top-left control cluster: Presets, Maps, Settings, light/dark switch ----
    Rectangle {
        id: controlCluster
        z: 10
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 8
        color: "#cc000000"
        radius: 8
        width: clusterRow.implicitWidth + 20
        height: Math.max(44, clusterRow.implicitHeight) + 12

        Row {
            id: clusterRow
            anchors.centerIn: parent
            spacing: 8

            // Presets (Material bookmark icon)
            Item {
                width: 44
                height: 44
                anchors.verticalCenter: parent.verticalCenter
                Canvas {
                    anchors.centerIn: parent
                    width: 22
                    height: 22
                    onPaint: {
                        const ctx = getContext("2d");
                        ctx.reset();
                        const x0 = width * 0.25;
                        const x1 = width * 0.75;
                        const y0 = height * 0.15;
                        const y1 = height * 0.85;
                        ctx.strokeStyle = "white";
                        ctx.lineWidth = 2;
                        ctx.lineJoin = "miter";
                        ctx.beginPath();
                        ctx.moveTo(x0, y0);
                        ctx.lineTo(x1, y0);
                        ctx.lineTo(x1, y1);
                        ctx.lineTo((x0 + x1) / 2, y1 - height * 0.22);
                        ctx.lineTo(x0, y1);
                        ctx.closePath();
                        ctx.stroke();
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: presetsDialog.open()
                }
            }

            // Maps (Material folded-map icon)
            Item {
                width: 44
                height: 44
                anchors.verticalCenter: parent.verticalCenter
                Canvas {
                    anchors.centerIn: parent
                    width: 24
                    height: 24
                    onPaint: {
                        const ctx = getContext("2d");
                        ctx.reset();
                        const x0 = width * 0.15;
                        const x1 = width * 0.5;
                        const x2 = width * 0.85;
                        const y0 = height * 0.2;
                        const y1 = height * 0.8;
                        ctx.strokeStyle = "white";
                        ctx.lineWidth = 2;
                        ctx.lineJoin = "round";
                        ctx.beginPath();
                        ctx.moveTo(x0, y0 + 2);
                        ctx.lineTo(x1, y0 - 2);
                        ctx.lineTo(x2, y0 + 2);
                        ctx.lineTo(x2, y1);
                        ctx.lineTo(x1, y1 - 2);
                        ctx.lineTo(x0, y1);
                        ctx.closePath();
                        ctx.stroke();
                        ctx.beginPath();
                        ctx.moveTo(x1, y0 - 2);
                        ctx.lineTo(x1, y1 - 2);
                        ctx.stroke();
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: mapDialog.open()
                }
            }

            // Settings (Material tune icon)
            Item {
                width: 44
                height: 44
                anchors.verticalCenter: parent.verticalCenter
                Canvas {
                    anchors.centerIn: parent
                    width: 24
                    height: 24
                    onPaint: {
                        const ctx = getContext("2d");
                        ctx.reset();
                        const x0 = width * 0.15;
                        const x1 = width * 0.85;
                        const ys = [0.25, 0.5, 0.75];
                        const xs = [0.35, 0.7, 0.45];
                        ctx.strokeStyle = "white";
                        ctx.lineWidth = 2;
                        ctx.lineCap = "round";
                        for (let i = 0; i < 3; ++i) {
                            const y = height * ys[i];
                            ctx.beginPath();
                            ctx.moveTo(x0, y);
                            ctx.lineTo(x1, y);
                            ctx.stroke();
                        }
                        for (let i = 0; i < 3; ++i) {
                            const kx = width * xs[i];
                            const y = height * ys[i];
                            ctx.beginPath();
                            ctx.moveTo(kx, y - 4);
                            ctx.lineTo(kx, y + 4);
                            ctx.stroke();
                        }
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: settingsDialog.open()
                }
            }

            // Light / dark switch
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Dark"
                    color: "white"
                    font.pixelSize: 14
                }

                Switch {
                    anchors.verticalCenter: parent.verticalCenter
                    checked: window.darkMode
                    onToggled: window.darkMode = checked
                }
            }
        }
    }

    // ---- Settings dialog (App scale) --------------------------------------------
    Dialog {
        id: settingsDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: Math.min(420, Overlay.overlay.width - 40)
        title: "Settings"

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "App scale"
                    color: Material.foreground
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: window.selectedScale.toFixed(2) + "x"
                    color: Material.foreground
                }
            }

            Slider {
                id: scaleSlider
                Layout.fillWidth: true
                from: 0.5
                to: 3.0
                stepSize: 0.05
                Component.onCompleted: value = window.selectedScale
                onMoved: window.selectedScale = value
            }

            Text {
                Layout.fillWidth: true
                visible: window.scalePending
                text: "Restart required to apply the new scale."
                wrapMode: Text.WordWrap
                color: Material.foreground
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: "Restart to apply"
                    enabled: window.scalePending
                    onClicked: {
                        settingsDialog.close();
                        window.restartNow();
                    }
                }
            }
        }
    }

    // ---- Presets dialog (list / add / remove) -----------------------------------
    Dialog {
        id: presetsDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: Math.min(380, Overlay.overlay.width - 40)
        height: Math.min(440, Overlay.overlay.height - 40)
        title: "Presets"

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            Text {
                visible: window.presets.length === 0
                Layout.fillWidth: true
                text: "No presets yet. Move the map, then press Add."
                wrapMode: Text.WordWrap
                color: Material.foreground
            }

            ListView {
                id: presetList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: window.presets

                delegate: ItemDelegate {
                    width: presetList.width
                    text: modelData && modelData.name ? modelData.name : ""
                    highlighted: index === window.presetIndex
                    onClicked: {
                        window.flyToPreset(index);
                        presetsDialog.close();
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    text: "Add"
                    onClicked: {
                        presetsDialog.close();
                        nameDialog.open();
                    }
                }

                Button {
                    text: "Remove"
                    enabled: window.presetIndex >= 0
                    onClicked: window.removePresetAt(window.presetIndex)
                }

                Item { Layout.fillWidth: true }
            }
        }
    }

    // Name prompt for preset "Add"
    Dialog {
        id: nameDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: Math.min(320, Overlay.overlay.width - 40)
        title: "Add preset"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: {
            nameField.text = "Preset " + (window.presets.length + 1);
            nameField.selectAll();
            nameField.forceActiveFocus();
        }
        onAccepted: {
            const n = nameField.text.trim();
            window.addPreset(n.length > 0 ? n : ("Preset " + (window.presets.length + 1)));
        }

        TextField {
            id: nameField
            width: parent.width
            placeholderText: "Preset name"
            onAccepted: nameDialog.accept()
        }
    }

    // ---- Maps dialog (list / add / remove) --------------------------------------
    Dialog {
        id: mapDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: Math.min(440, Overlay.overlay.width - 40)
        height: Math.min(480, Overlay.overlay.height - 40)
        title: "Maps"

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            ListView {
                id: mapList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: window.mapStyles

                delegate: ItemDelegate {
                    width: mapList.width
                    text: modelData && modelData.name ? modelData.name : ""
                    highlighted: modelData && modelData.url === window.styleUrl
                    onClicked: {
                        window.selectMap(index);
                        mapDialog.close();
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    text: "Add"
                    onClicked: {
                        mapDialog.close();
                        addMapDialog.open();
                    }
                }

                Button {
                    text: "Remove"
                    enabled: window.mapIndex >= 0
                    onClicked: window.removeMapAt(window.mapIndex)
                }

                Item { Layout.fillWidth: true }
            }
        }
    }

    // Add-map prompt (name + style URL)
    Dialog {
        id: addMapDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: Math.min(420, Overlay.overlay.width - 40)
        title: "Add map"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: {
            mapNameField.text = "Map " + (window.mapStyles.length + 1);
            mapUrlField.text = "";
            mapNameField.forceActiveFocus();
        }
        onAccepted: {
            const n = mapNameField.text.trim();
            const u = mapUrlField.text.trim();
            if (u.length > 0) {
                window.addMapStyle(n.length > 0 ? n : u, u);
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            TextField {
                id: mapNameField
                Layout.fillWidth: true
                placeholderText: "Name"
                onAccepted: addMapDialog.accept()
            }

            TextField {
                id: mapUrlField
                Layout.fillWidth: true
                placeholderText: "Style URL, e.g. https://tiles.openfreemap.org/styles/liberty"
                onAccepted: addMapDialog.accept()
            }
        }
    }

    // ---- 2D / 3D toggle button (Material globe icon) ----------------------------
    Rectangle {
        id: toggle3DButton
        z: 10
        width: 44
        height: 44
        radius: width / 2
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 8
        color: window.globe3D ? "#2d7ef7" : "#cc000000"

        Canvas {
            id: globeIcon
            anchors.centerIn: parent
            width: 26
            height: 26
            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                const cx = width / 2;
                const cy = height / 2;
                const r = width / 2 - 2;
                const rx = r * 0.55;   // meridian horizontal radius
                const ry = r * 0.55;   // parallel vertical radius
                const k = 4 / 3;       // cubic-bezier ellipse constant
                ctx.strokeStyle = "white";
                ctx.lineWidth = 2;
                ctx.lineCap = "round";

                // Outer circle
                ctx.beginPath();
                ctx.arc(cx, cy, r, 0, 2 * Math.PI);
                ctx.stroke();

                // Vertical meridian (a tall ellipse)
                ctx.beginPath();
                ctx.moveTo(cx, cy - r);
                ctx.bezierCurveTo(cx + k * rx, cy - r, cx + k * rx, cy + r, cx, cy + r);
                ctx.bezierCurveTo(cx - k * rx, cy - r, cx - k * rx, cy + r, cx, cy - r);
                ctx.stroke();

                // Horizontal parallel (a wide ellipse)
                ctx.beginPath();
                ctx.moveTo(cx - r, cy);
                ctx.bezierCurveTo(cx - r, cy + k * ry, cx + r, cy + k * ry, cx + r, cy);
                ctx.bezierCurveTo(cx + r, cy - k * ry, cx - r, cy - k * ry, cx - r, cy);
                ctx.stroke();
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: window.toggle3D()
        }
    }

    // ---- Zoom in / out buttons --------------------------------------------------
    Column {
        id: zoomButtons
        z: 10
        anchors.top: toggle3DButton.bottom
        anchors.topMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        spacing: 8

        Rectangle {
            width: 44
            height: 44
            radius: width / 2
            color: "#cc000000"
            Text {
                anchors.centerIn: parent
                text: "+"
                color: "white"
                font.pixelSize: 26
                font.bold: true
            }
            MouseArea {
                anchors.fill: parent
                onClicked: window.zoomBy(1)
            }
        }

        Rectangle {
            width: 44
            height: 44
            radius: width / 2
            color: "#cc000000"
            Text {
                anchors.centerIn: parent
                text: "\u2212"
                color: "white"
                font.pixelSize: 26
                font.bold: true
            }
            MouseArea {
                anchors.fill: parent
                onClicked: window.zoomBy(-1)
            }
        }
    }

    // ---- 3D buildings opacity toggle (Material buildings icon) ------------------
    Rectangle {
        id: buildingsButton
        z: 10
        width: 44
        height: 44
        radius: width / 2
        anchors.top: zoomButtons.bottom
        anchors.topMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        color: window.buildingsOpaque ? "#2d7ef7" : "#cc000000"

        Canvas {
            anchors.centerIn: parent
            width: 24
            height: 24
            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                const bx = width * 0.28;
                const by = height * 0.12;
                const bw = width * 0.44;
                const bh = height * 0.76;
                ctx.strokeStyle = "white";
                ctx.lineWidth = 2;
                ctx.lineJoin = "round";
                ctx.strokeRect(bx, by, bw, bh);
                ctx.fillStyle = "white";
                for (let r = 0; r < 3; ++r) {
                    for (let c = 0; c < 2; ++c) {
                        ctx.fillRect(bx + bw * (0.22 + c * 0.36),
                                     by + bh * (0.12 + r * 0.28),
                                     bw * 0.2, bh * 0.12);
                    }
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: window.buildingsOpaque = !window.buildingsOpaque
        }
    }
}
