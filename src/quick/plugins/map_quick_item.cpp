// Copyright (C) 2023 MapLibre contributors

// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include "map_quick_item.hpp"
#include "map_quick_item_p.hpp"

#include "texture_node_base_p.hpp"
#ifdef MLN_RENDER_BACKEND_OPENGL
#include "texture_node_opengl_p.hpp"
#endif
#ifdef MLN_RENDER_BACKEND_METAL
#include "texture_node_metal_p.hpp"
#endif
#ifdef MLN_RENDER_BACKEND_VULKAN
#include "texture_node_vulkan_p.hpp"
#endif

#include "style_change_p.hpp"

#include <QMapLibre/Map>

#include <QtCore/QTimer>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGRectangleNode>
#ifdef MLN_RENDER_BACKEND_OPENGL
#include <QtGui/QOpenGLContext>
#endif

#include <memory>
#include <type_traits>
#include <exception>

static_assert(std::is_same_v<std::underlying_type_t<QMapLibre::Map::ProjectionType>,
                             std::underlying_type_t<QMapLibre::MapQuickItem::ProjectionType>>,
              "ProjectionType underlying types must match");
static_assert(static_cast<int>(QMapLibre::Map::Mercator) == static_cast<int>(QMapLibre::MapQuickItem::Mercator) &&
                  static_cast<int>(QMapLibre::Map::Globe) == static_cast<int>(QMapLibre::MapQuickItem::Globe),
              "ProjectionType values must match");

namespace {
constexpr int minSize{64};
constexpr int intervalTime{250};

constexpr double minZoomLevel{0.0};
constexpr double maxZoomLevel{20.0};

QMapLibre::Map::ProjectionType projectionFromMapQuickItem(QMapLibre::MapQuickItem::ProjectionType type) {
    switch (type) {
        case QMapLibre::MapQuickItem::Globe:
            return QMapLibre::Map::Globe;
        case QMapLibre::MapQuickItem::Mercator:
            break;
    }
    return QMapLibre::Map::Mercator;
}
} // namespace

namespace QMapLibre {

MapQuickItem::MapQuickItem(QQuickItem *parent)
    : QQuickItem(parent),
      d_ptr(std::make_unique<MapQuickItemPrivate>(this)) {
    Q_D(MapQuickItem);

    setFlag(ItemHasContents, true);
    // QQuickItem defaults to accepting only the left mouse button, so a
    // right-button press is never delivered to this item and therefore never
    // reaches pointer handlers declared on it (e.g. right-drag zoom). Accept
    // the right button so such gestures are delivered.
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    // TODO: make configurable
    d->m_settings.setCacheDatabasePath(QStringLiteral(":memory:"));
}

MapQuickItem::~MapQuickItem() = default;

QString MapQuickItem::style() const {
    Q_D(const MapQuickItem);

    return d->m_style;
}

void MapQuickItem::setStyle(const QString &style) {
    Q_D(MapQuickItem);

    if (d->m_style == style) {
        return;
    }
    d->m_style = style;

    // Apply immediately when the map already exists. Otherwise the new style
    // would only take effect on the next launch (initialize() reads m_style).
    if (d->m_map != nullptr) {
        if (d->m_styleLoaded) {
            d->m_styleLoaded = false;
            emit styleLoadedChanged();
        }
        d->m_map->setStyleUrl(style);
        update();
    }
}

double MapQuickItem::zoomLevel() const {
    Q_D(const MapQuickItem);

    return d->m_zoomLevel;
}

bool MapQuickItem::styleLoaded() const {
    Q_D(const MapQuickItem);

    return d->m_styleLoaded;
}

MapQuickItem::ProjectionType MapQuickItem::projectionType() const {
    Q_D(const MapQuickItem);

    return d->m_projectionType;
}

void MapQuickItem::setProjectionType(ProjectionType type) {
    Q_D(MapQuickItem);

    if (d->m_projectionType == type) {
        return;
    }

    d->m_projectionType = type;

    // Apply immediately when the map already exists; otherwise initialize()
    // reads m_projectionType when it creates the map.
    if (d->m_map != nullptr) {
        d->m_map->setProjectionType(projectionFromMapQuickItem(type));
    }

    emit projectionTypeChanged();
}

QColor MapQuickItem::backgroundColor() const {
    Q_D(const MapQuickItem);
    return d->m_backgroundColor;
}

void MapQuickItem::setBackgroundColor(const QColor &color) {
    Q_D(MapQuickItem);

    d->m_backgroundColor = color;
    if (d->m_map != nullptr) {
        d->m_map->setBackgroundColor(color);
    }
    emit backgroundColorChanged();
}

QString MapQuickItem::terrainSource() const {
    Q_D(const MapQuickItem);

    return d->m_terrainSource;
}

void MapQuickItem::setTerrainSource(const QString &source) {
    Q_D(MapQuickItem);

    if (d->m_terrainSource == source) {
        return;
    }

    d->m_terrainSource = source;
    d->applyTerrain();
    emit terrainChanged();
}

double MapQuickItem::terrainExaggeration() const {
    Q_D(const MapQuickItem);

    return d->m_terrainExaggeration;
}

void MapQuickItem::setTerrainExaggeration(double exaggeration) {
    Q_D(MapQuickItem);

    if (qFuzzyCompare(d->m_terrainExaggeration, exaggeration)) {
        return;
    }

    d->m_terrainExaggeration = exaggeration;
    d->applyTerrain();
    emit terrainChanged();
}

bool MapQuickItem::skyEnabled() const {
    Q_D(const MapQuickItem);

    return d->m_skyEnabled;
}

void MapQuickItem::setSkyEnabled(bool enabled) {
    Q_D(MapQuickItem);

    d->m_skyExplicit = true; // once set, false means no sky (not "leave the style's sky alone")
    if (d->m_skyEnabled == enabled) {
        return;
    }

    d->m_skyEnabled = enabled;
    d->applySky();
    emit skyChanged();
}

QVariantMap MapQuickItem::sky() const {
    Q_D(const MapQuickItem);

    return d->m_sky;
}

void MapQuickItem::setSky(const QVariantMap &sky) {
    Q_D(MapQuickItem);

    if (d->m_sky == sky) {
        return;
    }

    d->m_sky = sky;
    d->applySky();
    emit skyChanged();
}

void MapQuickItem::setZoomLevel(double zoomLevel) {
    Q_D(MapQuickItem);

    if (zoomLevel < minZoomLevel) {
        zoomLevel = minZoomLevel;
    } else if (zoomLevel > maxZoomLevel) {
        zoomLevel = maxZoomLevel;
    }

    if (d->m_zoomLevel == zoomLevel) {
        return;
    }

    d->m_zoomLevel = zoomLevel;

    if (d->m_map != nullptr) {
        d->m_syncState |= CameraOptionsSync;
        update();
    }

    emit zoomLevelChanged();
}

QVariantList MapQuickItem::coordinate() const {
    Q_D(const MapQuickItem);

    return d->m_coordinate;
}

void MapQuickItem::setCoordinate(const QVariantList &coordinate) {
    Q_D(MapQuickItem);

    if (d->m_coordinate == coordinate || coordinate.size() != 2) {
        return;
    }

    d->m_coordinate = coordinate;

    if (d->m_map != nullptr) {
        d->m_syncState |= CameraOptionsSync;
        update();
    }

    emit coordinateChanged();
}

void MapQuickItem::setCoordinateFromPixel(const QPointF &pixel) {
    Q_D(MapQuickItem);

    if (d->m_map == nullptr) {
        return;
    }

    const Coordinate coordinate = d->m_map->coordinateForPixel(pixel);
    setCoordinate({coordinate.first, coordinate.second});
}

void MapQuickItem::pan(const QPointF &offset) {
    Q_D(MapQuickItem);

    if (d->m_map == nullptr) {
        return;
    }

    d->m_map->moveBy(offset);
    const Coordinate coordinate = d->m_map->coordinate();
    d->m_coordinate = {coordinate.first, coordinate.second};
    update();
    emit coordinateChanged();
}

void MapQuickItem::scale(double scale, const QPointF &center) {
    Q_D(MapQuickItem);

    if (d->m_map == nullptr) {
        return;
    }

    d->m_map->scaleBy(scale, center);
    const Coordinate coordinate = d->m_map->coordinate();
    d->m_coordinate = {coordinate.first, coordinate.second};
    d->m_zoomLevel = d->m_map->zoom();
    update();
    emit coordinateChanged();
    emit zoomLevelChanged();
}

double MapQuickItem::terrainElevation(double latitude, double longitude) const {
    Q_D(const MapQuickItem);

    if (d->m_map == nullptr) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const QVariant elevation = d->m_map->terrainElevation({latitude, longitude});
    return elevation.isValid() ? elevation.toDouble() : std::numeric_limits<double>::quiet_NaN();
}

QVariantMap MapQuickItem::terrainPointAt(const QPointF &pixel) const {
    Q_D(const MapQuickItem);

    if (d->m_map == nullptr) {
        return {};
    }
    const QVariant point = d->m_map->terrainPointForPixel(pixel);
    if (!point.isValid()) {
        return {};
    }
    const auto values = point.value<QList<double>>();
    return {{"latitude", values[0]}, {"longitude", values[1]}, {"elevation", values[2]}};
}

QVariantMap MapQuickItem::coordinateAtElevation(const QPointF &pixel, double elevation) const {
    Q_D(const MapQuickItem);

    if (d->m_map == nullptr) {
        return {};
    }
    const double plane = std::isnan(elevation) ? d->m_map->elevation() : elevation;
    const QVariant coordinate = d->m_map->coordinateForPixelAtElevation(pixel, plane);
    if (!coordinate.isValid()) {
        return {};
    }
    const Coordinate c = coordinate.value<Coordinate>();
    return {{"latitude", c.first}, {"longitude", c.second}};
}

bool MapQuickItem::globeRendering() const {
    Q_D(const MapQuickItem);

    return d->m_map != nullptr && d->m_map->isGlobeRendering();
}

void MapQuickItem::setElevationHold(bool hold) {
    Q_D(MapQuickItem);

    if (d->m_map != nullptr) {
        d->m_map->setElevationHold(hold);
    }
}

bool MapQuickItem::setLocationAtPoint(double latitude, double longitude, const QPointF &pixel, double elevation) {
    Q_D(MapQuickItem);

    if (d->m_map == nullptr) {
        return false;
    }
    if (!d->m_map->setLocationAtPoint({latitude, longitude}, pixel, elevation)) {
        return false;
    }
    const Coordinate coordinate = d->m_map->coordinate();
    d->m_coordinate = {coordinate.first, coordinate.second};
    update();
    emit coordinateChanged();
    return true;
}

double MapQuickItem::elevation() const {
    Q_D(const MapQuickItem);

    return d->m_map != nullptr ? d->m_map->elevation() : 0.0;
}

double MapQuickItem::cameraAltitude() const {
    Q_D(const MapQuickItem);

    return d->m_map != nullptr ? d->m_map->cameraAltitude() : 0.0;
}

void MapQuickItem::easeTo(const QVariantMap &camera, const QVariantMap &animation) {
    Q_D(MapQuickItem);

    if (d->m_map == nullptr) {
        return;
    }

    CameraOptions cameraOptions;
    if (camera.contains("center")) {
        const auto center = camera["center"].toList();
        if (center.size() == 2) {
            cameraOptions.center = QVariant::fromValue(Coordinate{center[0].toDouble(), center[1].toDouble()});
        }
    }
    if (camera.contains("zoom")) {
        cameraOptions.zoom = camera["zoom"].toDouble();
    }
    if (camera.contains("bearing")) {
        cameraOptions.bearing = camera["bearing"].toDouble();
    }
    if (camera.contains("pitch")) {
        cameraOptions.pitch = camera["pitch"].toDouble();
    }
    if (camera.contains("centerAltitude")) {
        cameraOptions.centerAltitude = camera["centerAltitude"].toDouble();
    }

    AnimationOptions animationOptions;
    if (animation.contains("duration")) {
        animationOptions.duration = animation["duration"].toLongLong();
    }
    if (animation.contains("velocity")) {
        animationOptions.velocity = animation["velocity"].toDouble();
    }
    if (animation.contains("minZoom")) {
        animationOptions.minZoom = animation["minZoom"].toDouble();
    }

    d->m_map->easeTo(cameraOptions, animationOptions);

    const Coordinate coordinate = d->m_map->coordinate();
    d->m_coordinate = {coordinate.first, coordinate.second};
    d->m_zoomLevel = d->m_map->zoom();
    update();
    emit coordinateChanged();
    emit zoomLevelChanged();
}

void MapQuickItem::flyTo(const QVariantMap &camera, const QVariantMap &animation) {
    Q_D(MapQuickItem);

    if (d->m_map == nullptr) {
        return;
    }

    CameraOptions cameraOptions;
    if (camera.contains("center")) {
        const auto center = camera["center"].toList();
        if (center.size() == 2) {
            cameraOptions.center = QVariant::fromValue(Coordinate{center[0].toDouble(), center[1].toDouble()});
        }
    }
    if (camera.contains("zoom")) {
        cameraOptions.zoom = camera["zoom"].toDouble();
    }
    if (camera.contains("bearing")) {
        cameraOptions.bearing = camera["bearing"].toDouble();
    }
    if (camera.contains("pitch")) {
        cameraOptions.pitch = camera["pitch"].toDouble();
    }
    if (camera.contains("centerAltitude")) {
        cameraOptions.centerAltitude = camera["centerAltitude"].toDouble();
    }

    AnimationOptions animationOptions;
    if (animation.contains("duration")) {
        animationOptions.duration = animation["duration"].toLongLong();
    }
    if (animation.contains("velocity")) {
        animationOptions.velocity = animation["velocity"].toDouble();
    }
    if (animation.contains("minZoom")) {
        animationOptions.minZoom = animation["minZoom"].toDouble();
    }

    d->m_map->flyTo(cameraOptions, animationOptions);

    const Coordinate coordinate = d->m_map->coordinate();
    d->m_coordinate = {coordinate.first, coordinate.second};
    d->m_zoomLevel = d->m_map->zoom();
    update();
    emit coordinateChanged();
    emit zoomLevelChanged();
}

void MapQuickItem::addStyleParameter(StyleParameter *parameter) {
    Q_D(MapQuickItem);
    d->addStyleParameter(parameter);
}

void MapQuickItem::removeStyleParameter(StyleParameter *parameter) {
    Q_D(MapQuickItem);
    d->removeStyleParameter(parameter);
}

void MapQuickItem::clearStyleParameters() {
    Q_D(MapQuickItem);
    d->clearStyleParameters();
}

void MapQuickItem::componentComplete() {
    QQuickItem::componentComplete();

    QTimer::singleShot(intervalTime, this, &MapQuickItem::initialize);
}

void MapQuickItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    Q_D(MapQuickItem);

    QQuickItem::geometryChange(newGeometry, oldGeometry);

    if (d->m_map == nullptr) {
        return;
    }

    if (newGeometry.size() != oldGeometry.size()) {
        const QSize viewportSize{static_cast<int>(newGeometry.width()), static_cast<int>(newGeometry.height())};
        const qreal pixelRatio = window() != nullptr ? window()->devicePixelRatio() : 1.0;
        d->m_map->resize(viewportSize.expandedTo({minSize, minSize}), pixelRatio);
        d->m_syncState |= ViewportSync;
        update();
    }
}

QSGNode *MapQuickItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) {
    Q_D(MapQuickItem);
    Q_UNUSED(data);

    if (!d->m_map) {
        delete oldNode; // NOLINT(cppcoreguidelines-owning-memory)
        return nullptr;
    }

    auto *root = static_cast<QSGRectangleNode *>(oldNode);
    if (root == nullptr) {
        root = window()->createRectangleNode();
    }

    root->setRect(boundingRect());
    // TODO: background color
    // root->setColor(m_color);

    QSGNode *content = root->childCount() > 0 ? root->firstChild() : nullptr;
    content = updateMapNode(content);
    if (content != nullptr && root->childCount() == 0) {
        root->appendChildNode(content);
    }

    return root;
}

QSGNode *MapQuickItem::updateMapNode(QSGNode *node) {
    Q_D(MapQuickItem);

    const QSize viewportSize{static_cast<int>(width()), static_cast<int>(height())};

    if (node == nullptr) {
#ifdef MLN_RENDERER_DEBUGGING
        qDebug() << "MapQuickItem::updatePaintNode() - Creating new node for size" << viewportSize;
#endif

#ifdef MLN_RENDER_BACKEND_OPENGL
        // OpenGL context check
        const QOpenGLContext *glContext = QOpenGLContext::currentContext();
        if (glContext == nullptr) {
            qWarning("QOpenGLContext is NULL!");
            // qWarning() << "You are running on QSG backend " << QSGContext::backend();
            qWarning("The MapLibre plugin works with both Desktop and ES 2.0+ OpenGL versions.");
            qWarning("Verify that your Qt is built with OpenGL, and what kind of OpenGL.");
            qWarning(
                "To force using a specific OpenGL version, check QSurfaceFormat::setRenderableType and "
                "QSurfaceFormat::setDefaultFormat");

            return node;
        }

        std::unique_ptr<TextureNodeBase> mbglNode = std::make_unique<TextureNodeOpenGL>(
            d->m_map, viewportSize, window()->devicePixelRatio());
#elif defined(MLN_RENDER_BACKEND_METAL)
        std::unique_ptr<TextureNodeBase> mbglNode = std::make_unique<TextureNodeMetal>(
            d->m_map, viewportSize, window()->devicePixelRatio());
#elif defined(MLN_RENDER_BACKEND_VULKAN)
        std::unique_ptr<TextureNodeBase> mbglNode = std::make_unique<TextureNodeVulkan>(
            d->m_map, viewportSize, window()->devicePixelRatio());
#endif

        d->m_syncState = ViewportSync | CameraOptionsSync;

        node = mbglNode.release();

#ifdef MLN_RENDERER_DEBUGGING
        qDebug() << "MapQuickItem::updatePaintNode() - Created new node" << node;
#endif
    }

    if ((d->m_syncState & CameraOptionsSync) != 0) {
        d->m_map->setCoordinateZoom({d->m_coordinate.size() == 2 ? d->m_coordinate[0].toDouble() : 0.0,
                                     d->m_coordinate.size() == 2 ? d->m_coordinate[1].toDouble() : 0.0},
                                    d->m_zoomLevel);
    }

    if ((d->m_syncState & ViewportSync) != 0) {
        static_cast<TextureNodeBase *>(node)->resize(viewportSize, window()->devicePixelRatio(), window());
    }

    if (d->m_styleLoaded) {
        d->syncStyleChanges();
    }

    static_cast<TextureNodeBase *>(node)->render(window());

    d->m_syncState = NoSync;

    return node;
}

void MapQuickItem::initialize() {
    Q_D(MapQuickItem);

    d->initialize();
}

double MapQuickItem::averageFrameTimeMs() {
    Q_D(MapQuickItem);

    const double average = d->m_frameTimeCount > 0 ? d->m_frameTimeSumMs / d->m_frameTimeCount
                                                   : std::numeric_limits<double>::quiet_NaN();
    d->m_frameTimeSumMs = 0.0;
    d->m_frameTimeCount = 0;
    return average;
}

int MapQuickItem::renderedFrames() const {
    Q_D(const MapQuickItem);

    return d->m_renderedFrames;
}

void MapQuickItem::onMapChanged(Map::MapChange change) {
    Q_D(MapQuickItem);

    if (change == Map::MapChangeDidFinishRenderingFrameFullyRendered) {
        ++d->m_renderedFrames;
    }

    if (change == Map::MapChangeDidFinishLoadingStyle || change == Map::MapChangeDidFailLoadingMap) {
        if (!d->m_styleLoaded) {
            d->m_styleLoaded = true;
            emit styleLoadedChanged();
        }
        // Loading a style resets the projection to the style's root property, so
        // re-apply the projection selected through this item.
        d->m_map->setProjectionType(projectionFromMapQuickItem(d->m_projectionType));
        // Terrain also lives on the style, so re-apply it after a (re)load.
        d->applyTerrain();
        // The style's own sky replaced any sky this item added.
        d->applySky();
    } else if (change == Map::MapChangeWillStartLoadingMap) {
        if (d->m_styleLoaded) {
            d->m_styleLoaded = false;
            emit styleLoadedChanged();
        }
        d->m_styleChanges.clear();

        for (const StyleParameter *parameter : d->m_mapParameters) {
            std::vector<std::unique_ptr<StyleChange>> changes = StyleChange::addParameter(parameter,
                                                                                          d->m_mapItemsBefore);
            std::ranges::move(changes, std::back_inserter(d->m_styleChanges));
        }
    } else if (change == Map::MapChangeDidFinishLoadingMap) {
        // TODO: make it more elegant
        QTimer::singleShot(intervalTime, this, &QQuickItem::update);
        emit mapLoaded();
    } else if (change == Map::MapChangeDidFinishRenderingMapFullyRendered) {
        emit mapIdle();
    } else if (change == Map::MapChangeDidFinishRenderingFrame) {
        ++d->m_renderedFrames;
        // The map rendered a partial frame (e.g. tiles arriving after the initial load). Rendering is
        // driven by Qt scene-graph repaints, so without scheduling another update the freshly rendered
        // frame is never shown until some other input forces a repaint. Keep repainting until the map
        // reports a fully rendered frame.
        QTimer::singleShot(0, this, &QQuickItem::update);
    }
}

void MapQuickItem::onStyleParameterUpdated(StyleParameter *parameter) {
    Q_D(MapQuickItem);

    std::vector<std::unique_ptr<StyleChange>> changes = StyleChange::addParameter(parameter, d->m_mapItemsBefore);
    std::ranges::move(changes, std::back_inserter(d->m_styleChanges));

    update();
}

// private implementation

MapQuickItemPrivate::MapQuickItemPrivate(MapQuickItem *q)
    : q_ptr(q) {}

void MapQuickItemPrivate::applySky() {
    if (m_map == nullptr || !m_styleLoaded) {
        return;
    }

    if (m_skyEnabled) {
        if (m_sky.isEmpty() && m_map->hasSky()) {
            return; // enabled with no properties of our own: the style's own sky already is the sky
        }
        m_map->setSky(m_sky);
    } else if (m_skyExplicit && m_map->hasSky()) {
        // Explicitly switched off: no sky at all, even if the style defines one.
        m_map->clearSky();
    }
}

void MapQuickItemPrivate::applyTerrain() {
    if (m_map == nullptr || !m_styleLoaded) {
        return;
    }

    try {
        if (m_terrainSource.isEmpty()) {
            m_map->clearTerrain();
        } else {
            m_map->setTerrain(m_terrainSource, m_terrainExaggeration);
        }
    } catch (const std::exception &error) {
        qWarning("MapLibre terrain: %s", error.what());
    }
}

void MapQuickItemPrivate::initialize() {
    Q_Q(MapQuickItem);

    if (m_map != nullptr) {
        return;
    }

    const QSize viewportSize{static_cast<int>(q->width()), static_cast<int>(q->height())};
    const qreal pixelRatio = q->window() != nullptr ? q->window()->devicePixelRatio() : 1.0;
    m_map = std::make_unique<Map>(nullptr, m_settings, viewportSize, pixelRatio);
    m_map->setConnectionEstablished();

    // CFID: allow the map to tilt further than the MapLibre default of 60 degrees.
    m_map->setMaxPitch(85);

    if (m_backgroundColor.isValid()) {
        m_map->setBackgroundColor(m_backgroundColor);
    }

    // Connect before the style load is started below. Connecting later (e.g. when the scene graph
    // node is first created) races the load: a style that resolves quickly, such as one already in
    // the cache, emits MapChangeDidFinishLoadingStyle with nothing listening and the event is lost.
    QObject::connect(m_map.get(), &Map::needsRendering, q, &QQuickItem::update);
    QObject::connect(m_map.get(), &Map::mapChanged, q, &MapQuickItem::onMapChanged);
    QObject::connect(m_map.get(), &Map::frameRendered, q, [this](double encodingMs, double renderingMs) {
        m_frameTimeSumMs += encodingMs + renderingMs;
        ++m_frameTimeCount;
    });

    m_map->setProjectionType(projectionFromMapQuickItem(m_projectionType));

    // Set default style
    if (!m_style.isEmpty()) {
        m_map->setStyleUrl(m_style);
    } else if (!m_settings.styles().empty()) {
        m_map->setStyleUrl(m_settings.styles().front().url);
    } else if (!m_settings.providerStyles().empty()) {
        m_map->setStyleUrl(m_settings.providerStyles().front().url);
    }

    for (const StyleParameter *parameter : m_mapParameters) {
        std::vector<std::unique_ptr<StyleChange>> changes = StyleChange::addParameter(parameter, m_mapItemsBefore);
        std::ranges::move(changes, std::back_inserter(m_styleChanges));
    }

    q->update();
}

void MapQuickItemPrivate::addStyleParameter(StyleParameter *parameter) {
    Q_Q(MapQuickItem);

    if (m_mapParameters.contains(parameter)) {
        return;
    }

    m_mapParameters << parameter;

    QObject::connect(parameter, &StyleParameter::updated, q, &MapQuickItem::onStyleParameterUpdated);

    if (m_styleLoaded) {
        std::vector<std::unique_ptr<StyleChange>> changes = StyleChange::addParameter(parameter, m_mapItemsBefore);
        std::ranges::move(changes, std::back_inserter(m_styleChanges));
        q->update();
    }
}

void MapQuickItemPrivate::removeStyleParameter(StyleParameter *parameter) {
    Q_Q(MapQuickItem);

    q->disconnect(parameter);

    if (m_styleLoaded) {
        std::vector<std::unique_ptr<StyleChange>> changes = StyleChange::removeParameter(parameter);
        std::ranges::move(changes, std::back_inserter(m_styleChanges));
        q->update();
    }

    m_mapParameters.removeOne(parameter);
}

void MapQuickItemPrivate::clearStyleParameters() {
    for (StyleParameter *parameter : m_mapParameters) {
        removeStyleParameter(parameter);
    }
}

void MapQuickItemPrivate::syncStyleChanges() {
    for (const auto &change : m_styleChanges) {
        if (change->isValid()) {
            change->apply(m_map.get());
        }
    }

    m_styleChanges.clear();
}

} // namespace QMapLibre
