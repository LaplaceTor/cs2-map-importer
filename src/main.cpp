#include <QApplication>
#include <QProcess>
#include <QMessageBox>
#include <QString>
#include "cs2importer.h"

bool checkPythonInstalled() {
    QProcess process;
    process.start("python", QStringList() << "--version");
    process.waitForFinished();

    QString output = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
    QString error = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();

    QString combined = output + error;
    if (combined.contains("Python 3")) {
        return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    if (!checkPythonInstalled()) {
        QMessageBox::critical(nullptr, "Error", "Python not install! You cannot use this tool.");
        return 1;
    }

    Importer w;
    w.show();
    return a.exec();
}
