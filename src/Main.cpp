#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QPalette>
#include "Ui.h"

int main(int argc, char *argv[])
{
    // Enable high DPI scaling


    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/icon.png")); // Optional if we had one
    QQuickStyle::setStyle("Fusion");

    Backend backend;
    QObject::connect(&app, &QGuiApplication::aboutToQuit, &backend, &Backend::AppAboutToQuit);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backendObject", &backend);

    const QUrl url(QStringLiteral("qrc:/qt/qml/cs2importer/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
