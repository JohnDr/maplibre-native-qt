// Copyright (C) 2023 MapLibre contributors

// SPDX-License-Identifier: MIT

#include <QtQuick/qquickitem.h>
#include <QDebug>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QtQuick/QSGRendererInterface>
#include <QCoreApplication>
#include <QProcess>
#include <QSettings>
#if QT_CONFIG(vulkan)
#include <QVulkanInstance>
#endif

#include <QMapLibre/Utils>

// Small QML helper that owns the persisted application scale and can relaunch
// the process. The scale itself is applied through QT_SCALE_FACTOR before
// QGuiApplication is constructed, so changing it requires a restart.
class AppScaleController : public QObject {
    Q_OBJECT
public:
    explicit AppScaleController(double appliedScale, QObject *parent = nullptr)
        : QObject(parent), m_appliedScale(appliedScale) {}

    Q_INVOKABLE double appliedScale() const { return m_appliedScale; }

    Q_INVOKABLE void applyAndRestart(double scale) {
        if (scale <= 0.0) {
            return;
        }

        QSettings settings;
        settings.setValue(QStringLiteral("ui/scaleFactor"), scale);
        settings.sync();

        QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                {QStringLiteral("--scale"), QString::number(scale, 'f', 2)});
        QCoreApplication::quit();
    }

private:
    double m_appliedScale;
};

int main(int argc, char *argv[]) {
    QCoreApplication::setOrganizationName(QStringLiteral("MapLibre"));
    QCoreApplication::setApplicationName(QStringLiteral("QMapLibreExampleQuickStandalone"));

    // The application scale must be known before QGuiApplication exists, because
    // Qt reads QT_SCALE_FACTOR (and the high-DPI policy) during its construction.
    // Precedence: --scale <v>  ->  persisted QSettings  ->  leave OS/native scaling.
    double requestedScale = 0.0;
    for (int i = 1; i + 1 < argc; ++i) {
        if (qstrcmp(argv[i], "--scale") == 0) {
            bool ok = false;
            const double value = QString::fromUtf8(argv[i + 1]).toDouble(&ok);
            if (ok && value > 0.0) {
                requestedScale = value;
            }
        }
    }
    if (requestedScale <= 0.0) {
        const QVariant stored = QSettings().value(QStringLiteral("ui/scaleFactor"));
        if (stored.isValid()) {
            bool ok = false;
            const double value = stored.toDouble(&ok);
            if (ok && value > 0.0) {
                requestedScale = value;
            }
        }
    }
    if (requestedScale > 0.0) {
        qputenv("QT_SCALE_FACTOR", QByteArray::number(requestedScale, 'f', 2));
    }
    const double appliedScale = requestedScale > 0.0 ? requestedScale : 1.0;

    // Enable verbose logging for debugging
    QLoggingCategory::setFilterRules(
        "qt.location.*.debug=true\n"
        "qt.positioning.*.debug=true\n"
        "maplibre.*.debug=true");
    // Set up graphics API and instance for each platform
    const QMapLibre::RendererType rendererType = QMapLibre::supportedRendererType();
    auto graphicsApi = static_cast<QSGRendererInterface::GraphicsApi>(rendererType);
    QQuickWindow::setGraphicsApi(graphicsApi);

    if (rendererType == QMapLibre::Vulkan) {
        // Enable Vulkan debug output
        qputenv("QT_VULKAN_DEBUG_OUTPUT", "1");
    }

    const QGuiApplication app(argc, argv);

    qDebug() << "Platform:" << QGuiApplication::platformName();
    qDebug() << "Graphics API:" << QQuickWindow::graphicsApi();
    qDebug() << "Renderer Type:" << rendererType;
    qDebug() << "Application scale:" << appliedScale;

    AppScaleController appScale(appliedScale);

    // Check if location services are available
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appScale"), &appScale);

    qDebug() << "Starting QML engine...";

    // Try to load test file if provided as argument
    if (argc > 1 && argv[1][0] != '-') {
        qDebug() << "Loading file:" << argv[1];
        engine.load(QUrl::fromLocalFile(argv[1]));
    } else {
        qDebug() << "Loading QML from resources...";
        engine.load(QUrl(QStringLiteral("qrc:/Example/main.qml")));
    }

    qDebug() << "Checking root objects...";
    if (engine.rootObjects().isEmpty()) {
        qDebug() << "ERROR: No root objects found!";

        // Check for errors
        const auto errors = engine.outputWarningsToStandardError();
        qDebug() << "QML Loading failed. Check above for errors.";
        return -1;
    }
    qDebug() << "Root objects loaded successfully";

    return app.exec();
}

#include "main.moc"
