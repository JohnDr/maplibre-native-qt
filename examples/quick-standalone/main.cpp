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

// Small QML helper that owns the persisted application state:
//   - the application scale (applied through QT_SCALE_FACTOR before
//     QGuiApplication is constructed, so changing it requires a restart), and
//   - arbitrary key/value state (window geometry, map camera) backed by
//     QSettings.
class AppStateController : public QObject {
    Q_OBJECT
public:
    AppStateController(double appliedScale, bool explicitScale, QObject *parent = nullptr)
        : QObject(parent), m_appliedScale(appliedScale), m_explicitScale(explicitScale) {}

    [[nodiscard]] Q_INVOKABLE double appliedScale() const { return m_appliedScale; }

    // True when the applied scale was explicitly set (via --scale, a persisted
    // value or the in-app control), as opposed to the untouched default of 1.0.
    [[nodiscard]] Q_INVOKABLE bool hasExplicitScale() const { return m_explicitScale; }

    Q_INVOKABLE QVariant value(const QString &key) const {
        return QSettings().value(key);
    }

    Q_INVOKABLE void setValue(const QString &key, const QVariant &valueToStore) {
        QSettings settings;
        settings.setValue(key, valueToStore);
    }

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
    bool m_explicitScale;
};

int main(int argc, char *argv[]) {
    QCoreApplication::setOrganizationName(QStringLiteral("MapLibre"));
    QCoreApplication::setApplicationName(QStringLiteral("QMapLibreExampleQuickStandalone"));

    // The application scale must be known before QGuiApplication exists, because
    // Qt reads QT_SCALE_FACTOR (and the high-DPI policy) during its construction.
    // Precedence: --scale <v>  ->  persisted QSettings  ->  leave OS/native scaling.
    double requestedScale = 0.0;
    bool scaleFromCommandLine = false;
    for (int i = 1; i + 1 < argc; ++i) {
        if (qstrcmp(argv[i], "--scale") == 0) {
            bool ok = false;
            const double value = QString::fromUtf8(argv[i + 1]).toDouble(&ok);
            if (ok && value > 0.0) {
                requestedScale = value;
                scaleFromCommandLine = true;
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
    if (scaleFromCommandLine) {
        // Explicit command-line request: remember it for future runs.
        QSettings().setValue(QStringLiteral("ui/scaleFactor"), requestedScale);
    }
    if (requestedScale > 0.0) {
        qputenv("QT_SCALE_FACTOR", QByteArray::number(requestedScale, 'f', 2));
    }
    const double appliedScale = requestedScale > 0.0 ? requestedScale : 1.0;
    const bool explicitScale = requestedScale > 0.0;

    // Enable verbose logging for debugging
    QLoggingCategory::setFilterRules(
        "qt.location.*.debug=true\n"
        "qt.positioning.*.debug=true\n"
        "maplibre.*.debug=true");
    // Set up graphics API and instance for each platform
    const QMapLibre::RendererType rendererType = QMapLibre::supportedRendererType();
    auto graphicsApi = static_cast<QSGRendererInterface::GraphicsApi>(rendererType);
    QQuickWindow::setGraphicsApi(graphicsApi);

    const QGuiApplication app(argc, argv);

    qDebug() << "Platform:" << QGuiApplication::platformName();
    qDebug() << "Graphics API:" << QQuickWindow::graphicsApi();
    qDebug() << "Renderer Type:" << rendererType;
    qDebug() << "Application scale:" << appliedScale;

    AppStateController appState(appliedScale, explicitScale);

    // Check if location services are available
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appState"), &appState);

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
