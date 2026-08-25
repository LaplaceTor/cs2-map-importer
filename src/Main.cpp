#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QtPlugin>
#include <memory>

#include "Application/Environment/GameEnvironmentService.h"
#include "Application/Environment/VpkSignatureLeaseService.h"
#include "UI/ViewModels/GameViewModel.h"
#include "UI/ViewModels/LogViewModel.h"
#include "UI/Controllers/MainController.h"

Q_IMPORT_PLUGIN(cs2importerPlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CS2 Importer"));
    app.setOrganizationName(QStringLiteral("LaplaceTor"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/icon.png")));

    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    auto gameEnvService = std::make_unique<Application::Environment::GameEnvironmentService>();
    auto gameViewModel = std::make_unique<UI::ViewModels::GameViewModel>(gameEnvService.get());
    auto logViewModel = std::make_shared<UI::ViewModels::LogViewModel>();
    auto mainController = std::make_unique<UI::Controllers::MainController>();

    logViewModel->registerWithLogManager();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("gameViewModelInstance"), gameViewModel.get());
    engine.rootContext()->setContextProperty(QStringLiteral("logViewModelInstance"), logViewModel.get());
    engine.rootContext()->setContextProperty(QStringLiteral("mainControllerInstance"), mainController.get());

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [](QObject *obj, const QUrl &objUrl) {
        if (!obj) {
            QCoreApplication::exit(-1);
        }
    }, Qt::QueuedConnection);

    engine.loadFromModule(QStringLiteral("cs2importer"), QStringLiteral("Main"));

    // Trigger non-blocking asynchronous game detection in the background
    gameViewModel->autoDetect();

    return app.exec();
}

