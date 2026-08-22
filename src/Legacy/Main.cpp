#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QPalette>
#include <QtPlugin>
#include "Ui.h"

Q_IMPORT_PLUGIN(cs2importerPlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/icon.png")); // Optional if we had one
    QQuickStyle::setStyle("Fusion");

    Backend backend;
    QObject::connect(&app, &QGuiApplication::aboutToQuit, &backend, &Backend::AppAboutToQuit);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backendObject", &backend);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [](QObject *obj, const QUrl &objUrl) {
        if (!obj)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.loadFromModule("cs2importer", "Main");

    return app.exec();
}
