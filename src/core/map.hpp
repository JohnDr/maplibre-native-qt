// Copyright (C) 2023 MapLibre contributors
// Copyright (C) 2019 Mapbox, Inc.

// SPDX-License-Identifier: BSD-2-Clause

#ifndef QMAPLIBRE_MAP_H
#define QMAPLIBRE_MAP_H

#include <QMapLibre/Export>
#include <QMapLibre/Settings>
#include <QMapLibre/Types>

#include <QtCore/QMargins>
#include <QtCore/QObject>
#include <QtCore/QPointF>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtGui/QImage>

#include <memory>

#ifdef MLN_RENDER_BACKEND_VULKAN
namespace mln::vulkan {
class Texture2D;
} // namespace mln::vulkan
#endif

namespace QMapLibre {

class MapPrivate;
class MapRenderer;

class Q_MAPLIBRE_CORE_EXPORT Map : public QObject {
    Q_OBJECT
    Q_PROPERTY(double latitude READ latitude WRITE setLatitude)
    Q_PROPERTY(double longitude READ longitude WRITE setLongitude)
    Q_PROPERTY(double zoom READ zoom WRITE setZoom)
    Q_PROPERTY(double bearing READ bearing WRITE setBearing)
    Q_PROPERTY(double pitch READ pitch WRITE setPitch)
    Q_PROPERTY(QString styleJson READ styleJson WRITE setStyleJson)
    Q_PROPERTY(QString styleUrl READ styleUrl WRITE setStyleUrl)
    Q_PROPERTY(double scale READ scale WRITE setScale)
    Q_PROPERTY(QMapLibre::Coordinate coordinate READ coordinate WRITE setCoordinate)
    Q_PROPERTY(QMargins margins READ margins WRITE setMargins)

public:
    enum MapChange {
        MapChangeRegionWillChange = 0,
        MapChangeRegionWillChangeAnimated,
        MapChangeRegionIsChanging,
        MapChangeRegionDidChange,
        MapChangeRegionDidChangeAnimated,
        MapChangeWillStartLoadingMap,
        MapChangeDidFinishLoadingMap,
        MapChangeDidFailLoadingMap,
        MapChangeWillStartRenderingFrame,
        MapChangeDidFinishRenderingFrame,
        MapChangeDidFinishRenderingFrameFullyRendered,
        MapChangeWillStartRenderingMap,
        MapChangeDidFinishRenderingMap,
        MapChangeDidFinishRenderingMapFullyRendered,
        MapChangeDidFinishLoadingStyle,
        MapChangeSourceDidChange
    };

    enum MapLoadingFailure {
        StyleParseFailure,
        StyleLoadFailure,
        NotFoundFailure,
        UnknownFailure
    };

    // Determines the orientation of the map.
    enum NorthOrientation {
        NorthUpwards, // Default
        NorthRightwards,
        NorthDownwards,
        NorthLeftwards,
    };

    // Determines the map projection. Globe is not rendered yet; selecting it
    // records the request until the globe renderer lands. Values must match
    // mln::ProjectionType (enforced in the .cpp) and MapQuickItem::ProjectionType.
    // Determines the map projection. Kept independent of mln::ProjectionType;
    // the two are mapped explicitly in the .cpp.
    enum ProjectionType {
        Mercator = 0, // Default
        Globe = 1,
    };
    Q_ENUM(ProjectionType)

    explicit Map(QObject *parent = nullptr,
                 const Settings &settings = Settings(),
                 const QSize &size = QSize(),
                 qreal pixelRatio = 1);
    ~Map() override;

    [[nodiscard]] QString styleJson() const;
    [[nodiscard]] QString styleUrl() const;

    void setStyleJson(const QString &);
    void setStyleUrl(const QString &);

    [[nodiscard]] double latitude() const;
    void setLatitude(double latitude);

    [[nodiscard]] double longitude() const;
    void setLongitude(double longitude);

    [[nodiscard]] double scale() const;
    void setScale(double scale, const QPointF &center = QPointF());

    [[nodiscard]] double zoom() const;
    void setZoom(double zoom);

    [[nodiscard]] double minimumZoom() const;
    [[nodiscard]] double maximumZoom() const;

    [[nodiscard]] double bearing() const;
    void setBearing(double degrees);
    void setBearing(double degrees, const QPointF &center);

    [[nodiscard]] double pitch() const;
    void setPitch(double pitch);
    void pitchBy(double pitch);

    // Raises/lowers the pitch clamp (default 60 degrees).
    void setMaxPitch(double maxPitch);

    [[nodiscard]] NorthOrientation northOrientation() const;
    void setNorthOrientation(NorthOrientation);

    [[nodiscard]] ProjectionType projectionType() const;
    void setProjectionType(ProjectionType type);

    // Terrain: drapes the elevation of a `raster-dem` source over the map. An
    // empty `source` clears it. Throws (as a std::runtime_error) if the source
    // is missing or is not a raster-dem source.
    void setTerrain(const QString &source, double exaggeration = 1.0);
    void clearTerrain();
    [[nodiscard]] QVariant terrainElevation(const Coordinate &coordinate) const;
    [[nodiscard]] double elevation() const;
    [[nodiscard]] double cameraAltitude() const;

    // Backdrop colour (space) drawn behind the map - relevant when rendering the globe.
    void setBackgroundColor(const QColor &color);

    [[nodiscard]] Coordinate coordinate() const;
    void setCoordinate(const Coordinate &coordinate);
    void setCoordinateZoom(const Coordinate &coordinate, double zoom);

    void jumpTo(const CameraOptions &);
    void easeTo(const CameraOptions &camera, const AnimationOptions &animation);
    void flyTo(const CameraOptions &camera, const AnimationOptions &animation);

    void setGestureInProgress(bool inProgress);

    void setTransitionOptions(qint64 duration, qint64 delay = 0);

    void addAnnotationIcon(const QString &name, const QImage &sprite);

    AnnotationID addAnnotation(const Annotation &annotation);
    void updateAnnotation(AnnotationID id, const Annotation &annotation);
    void removeAnnotation(AnnotationID id);

    bool setLayoutProperty(const QString &layerId, const QString &propertyName, const QVariant &value);
    bool setPaintProperty(const QString &layerId, const QString &propertyName, const QVariant &value);

    [[nodiscard]] bool isFullyLoaded() const;

    void moveBy(const QPointF &offset);
    void scaleBy(double scale, const QPointF &center = QPointF());
    void rotateBy(const QPointF &first, const QPointF &second);

    void resize(const QSize &size, qreal pixelRatio = 0);

    [[nodiscard]] QPointF pixelForCoordinate(const Coordinate &coordinate) const;
    [[nodiscard]] Coordinate coordinateForPixel(const QPointF &pixel) const;

    [[nodiscard]] CoordinateZoom coordinateZoomForBounds(const Coordinate &sw, const Coordinate &ne) const;
    [[nodiscard]] CoordinateZoom coordinateZoomForBounds(const Coordinate &sw,
                                                         const Coordinate &ne,
                                                         double bearing,
                                                         double pitch);

    void setMargins(const QMargins &margins);
    [[nodiscard]] QMargins margins() const;

    void addSource(const QString &id, const QVariantMap &params);
    bool sourceExists(const QString &id);
    void updateSource(const QString &id, const QVariantMap &params);
    void removeSource(const QString &id);

    void addImage(const QString &id, const QImage &sprite);
    void removeImage(const QString &id);

    void addCustomLayer(const QString &id,
                        std::unique_ptr<CustomLayerHostInterface> host,
                        const QString &before = QString());
    void addLayer(const QString &id, const QVariantMap &params, const QString &before = QString());
    bool layerExists(const QString &id);
    void removeLayer(const QString &id);

    [[nodiscard]] QVector<QString> layerIds() const;

    void setFilter(const QString &layerId, const QVariant &filter);
    [[nodiscard]] QVariant getFilter(const QString &layerId) const;
    // When rendering on a different thread,
    // should be called on the render thread.
    void createRenderer(void *nativeTargetPtr);
#ifdef MLN_RENDER_BACKEND_VULKAN
    // Vulkan-specific: create the renderer using Qt's Vulkan device
    void createRendererWithQtVulkanDevice(void *windowPtr,
                                          void *physicalDevice,
                                          void *device,
                                          uint32_t graphicsQueueIndex);
#endif
    void updateRenderer(const QSize &size, qreal pixelRatio, quint32 fbo = 0);
    void destroyRenderer();

    void setCurrentDrawable(void *texturePtr);
    void setExternalDrawable(void *texturePtr, const QSize &textureSize);

#ifdef MLN_RENDER_BACKEND_VULKAN
    // Vulkan-specific: get the Vulkan texture object.
    mln::vulkan::Texture2D *getVulkanTexture() const;

    // Vulkan-specific: read image data from the Vulkan texture.
    // std::shared_ptr<mln::PremultipliedImage> readVulkanImageData() const;
#endif

#ifdef MLN_RENDER_BACKEND_OPENGL
    // OpenGL-specific: get the OpenGL framebuffer texture ID for direct texture sharing.
    [[nodiscard]] unsigned int getFramebufferTextureId() const;
#endif

public slots:
    void render();
    void setConnectionEstablished();
    void triggerRepaint();

    // Commit changes, load all the resources
    // and renders the map when completed.
    void startStaticRender();

    [[nodiscard]] void *nativeColorTexture() const;

signals:
    void needsRendering();
    void mapChanged(Map::MapChange);
    void mapLoadingFailed(Map::MapLoadingFailure, const QString &reason);
    void copyrightsChanged(const QString &copyrightsHtml);

    void projectionTypeChanged(ProjectionType type);

    void staticRenderFinished(const QString &error);

private:
    Q_DISABLE_COPY(Map)

    std::unique_ptr<MapPrivate> d_ptr;
};

} // namespace QMapLibre

Q_DECLARE_METATYPE(QMapLibre::Map::MapChange);
Q_DECLARE_METATYPE(QMapLibre::Map::MapLoadingFailure);
Q_DECLARE_METATYPE(QMapLibre::Map::ProjectionType);

#endif // QMAPLIBRE_MAP_H
