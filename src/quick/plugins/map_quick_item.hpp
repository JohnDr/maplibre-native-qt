// Copyright (C) 2023 MapLibre contributors

// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <QMapLibre/Map>
#include <QMapLibre/StyleParameter>

#include <QtQuick/QQuickItem>
#include <QtQuick/QSGNode>

#include <memory>

namespace QMapLibre {

class MapQuickItemPrivate;
class StyleChange;

class MapQuickItem : public QQuickItem {
    Q_OBJECT
    Q_DECLARE_PRIVATE(MapQuickItem)
    QML_NAMED_ELEMENT(MapLibre)
    QML_ADDED_IN_VERSION(3, 0)

    Q_PROPERTY(QString style READ style WRITE setStyle)
    Q_PROPERTY(QVariantList coordinate READ coordinate WRITE setCoordinate NOTIFY coordinateChanged)
    Q_PROPERTY(double zoomLevel READ zoomLevel WRITE setZoomLevel NOTIFY zoomLevelChanged)
    // CFID addition: exposes the native style-load state so QML can gate
    // addStyleParameter() calls on it directly, instead of an indirect
    // proxy (e.g. a "GPU ready" heuristic) that can race ahead of the
    // actual native style-load completion - see onMapChanged().
    Q_PROPERTY(bool styleLoaded READ styleLoaded NOTIFY styleLoadedChanged)
    // Globe is not rendered yet; selecting it records the request until the
    // globe renderer lands. Exposed on this element so QML can write
    // `projectionType: MapLibre.Globe`.
    Q_PROPERTY(ProjectionType projectionType READ projectionType WRITE setProjectionType NOTIFY projectionTypeChanged)
    // CFID addition: backdrop colour (space) drawn behind the map (visible around the globe).
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged)
    // CFID addition: terrain - the id of a raster-dem source draped over the map as a 3D
    // surface. An empty source disables terrain (flat map).
    Q_PROPERTY(QString terrainSource READ terrainSource WRITE setTerrainSource NOTIFY terrainChanged)
    Q_PROPERTY(double terrainExaggeration READ terrainExaggeration WRITE setTerrainExaggeration NOTIFY terrainChanged)
    // The sky (GL JS `map.setSky`). While \c skyEnabled, \c sky (style-spec `sky` properties, e.g.
    // `{"sky-color": "#199EF3", "fog-ground-blend": 0.5}`; empty = the style's own sky if it has one, else the
    // default sky) is applied. Once \c skyEnabled has been set to false there is no sky, even if the style defines
    // one. Never set: the style's own sky (if any) is left alone.
    Q_PROPERTY(bool skyEnabled READ skyEnabled WRITE setSkyEnabled NOTIFY skyChanged)
    Q_PROPERTY(QVariantMap sky READ sky WRITE setSky NOTIFY skyChanged)

public:
    enum SyncState : int {
        NoSync = 0,
        ViewportSync = 1 << 0,
        CameraOptionsSync = 1 << 1,
    };
    Q_DECLARE_FLAGS(SyncStates, SyncState);

    // Values must match QMapLibre::Map::ProjectionType (enforced in the .cpp).
    enum ProjectionType {
        Mercator = 0, // Default
        Globe = 1,
    };
    Q_ENUM(ProjectionType)

    explicit MapQuickItem(QQuickItem *parent = nullptr);
    ~MapQuickItem() override;

    [[nodiscard]] QString style() const;
    void setStyle(const QString &style);

    [[nodiscard]] double zoomLevel() const;
    void setZoomLevel(double zoomLevel);

    [[nodiscard]] QVariantList coordinate() const;
    void setCoordinate(const QVariantList &coordinate);
    Q_INVOKABLE void setCoordinateFromPixel(const QPointF &pixel);

    [[nodiscard]] bool styleLoaded() const;

    [[nodiscard]] ProjectionType projectionType() const;
    void setProjectionType(ProjectionType type);

    [[nodiscard]] QColor backgroundColor() const;
    void setBackgroundColor(const QColor &color);

    [[nodiscard]] QString terrainSource() const;
    void setTerrainSource(const QString &source);

    [[nodiscard]] double terrainExaggeration() const;
    void setTerrainExaggeration(double exaggeration);

    [[nodiscard]] bool skyEnabled() const;
    void setSkyEnabled(bool enabled);

    [[nodiscard]] QVariantMap sky() const;
    void setSky(const QVariantMap &sky);

    Q_INVOKABLE void pan(const QPointF &offset);
    Q_INVOKABLE void scale(double scale, const QPointF &center);
    Q_INVOKABLE void easeTo(const QVariantMap &camera, const QVariantMap &animation = QVariantMap());
    Q_INVOKABLE void flyTo(const QVariantMap &camera, const QVariantMap &animation = QVariantMap());
    // Terrain height in metres at a position, without exaggeration; NaN while its elevation tile is loading.
    Q_INVOKABLE double terrainElevation(double latitude, double longitude) const;
    // Number of frames the map has rendered since creation (for a frames-per-second readout).
    Q_INVOKABLE int renderedFrames() const;
    // Average CPU time (encoding + rendering) per rendered frame in milliseconds since the previous call; NaN if no
    // frame was rendered since then.
    Q_INVOKABLE double averageFrameTimeMs();
    // Camera centre and camera altitude in metres above sea level.
    Q_INVOKABLE double elevation() const;
    Q_INVOKABLE double cameraAltitude() const;

    Q_INVOKABLE void addStyleParameter(StyleParameter *parameter);
    Q_INVOKABLE void removeStyleParameter(StyleParameter *parameter);
    Q_INVOKABLE void clearStyleParameters();

signals:
    void coordinateChanged();
    void zoomLevelChanged();
    void styleLoadedChanged();
    void projectionTypeChanged();
    void backgroundColorChanged();
    void terrainChanged();
    void skyChanged();
    // The map finished loading (style, sprites) / became fully rendered (nothing left to load or draw): the
    // equivalents of MapLibre GL JS' `load` and `idle` events. Emitted every time, not only the first.
    void mapLoaded();
    void mapIdle();

protected:
    void componentComplete() override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;

private slots:
    void initialize();
    void onMapChanged(Map::MapChange change);
    void onStyleParameterUpdated(StyleParameter *parameter);

private:
    QSGNode *updateMapNode(QSGNode *node);

    std::unique_ptr<MapQuickItemPrivate> d_ptr;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MapQuickItem::SyncStates)

} // namespace QMapLibre
