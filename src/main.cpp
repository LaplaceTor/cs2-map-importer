#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QPalette>
#include <QCoreApplication>
#include <QProcess>
#include <QLocalSocket>
#include <QStringList>
#include "ui.h"

int run_client(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QStringList args = app.arguments();
    if (args.size() < 4) {
        return 1;
    }

    QString server_name = args[2];
    QString cmd = args[3];

    QLocalSocket socket;
    socket.connectToServer(server_name);
    if (!socket.waitForConnected(5000)) {
        return 1;
    }

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.setProgram("cmd.exe");
#ifdef Q_OS_WIN
    process.setNativeArguments("/S /C \"" + cmd + "\"");
#else
    process.setArguments({"/c", cmd});
#endif

    QObject::connect(&process, &QProcess::errorOccurred, [&](QProcess::ProcessError error) {
        QString errorMsg = "Process error occurred: " + process.errorString() + "\n";
        socket.write(errorMsg.toUtf8());
        socket.flush();
        socket.waitForBytesWritten(3000);
        socket.disconnectFromServer();
        socket.waitForDisconnected(1000);
        app.exit(1);
    });

    process.start();

    QString lineBuffer;
    bool answeredPrompt = false;

    QObject::connect(&process, &QProcess::readyRead, [&]() {
        QByteArray output = process.readAll();
        socket.write(output);
        socket.flush();

        QString outStr(output);
        for (QChar c : outStr) {
            if (c == '\n') {
                if (lineBuffer.endsWith('\r')) lineBuffer.chop(1);
                lineBuffer.clear();
            } else {
                lineBuffer += c;
                if (!answeredPrompt && lineBuffer.contains("Are you sure you want to continue? ('y')")) {
                    lineBuffer.clear();
                    process.write("y\n");
                    answeredPrompt = true;
                }
            }
        }
    });

    QObject::connect(&process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [&](int exitCode, QProcess::ExitStatus) {
        socket.flush();
        socket.waitForBytesWritten(3000);
        socket.disconnectFromServer();
        socket.waitForDisconnected(1000);
        app.exit(exitCode);
    });

    return app.exec();
}

int main(int argc, char *argv[])
{
    if (argc >= 2 && QString(argv[1]) == "--client") {
        return run_client(argc, argv);
    }

    // Enable high DPI scaling


    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/icon.png")); // Optional if we had one
    QQuickStyle::setStyle("Fusion");

    Backend backend;
    QObject::connect(&app, &QGuiApplication::aboutToQuit, &backend, &Backend::appAboutToQuit);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backendObject", &backend);

    const QUrl url(QStringLiteral("qrc:/cs2importer/src/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
