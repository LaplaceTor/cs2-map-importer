#include "miscellaneous.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QProcess>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QByteArray>
#include <QEventLoop>
#include <QTimer>
#include <QTemporaryFile>

QAtomicInt Miscellaneous::cancel_import(0);
Miscellaneous::LogCallback Miscellaneous::global_logger = nullptr;

void Miscellaneous::log(const QString& msg) {
    if (global_logger) {
        global_logger(msg);
    }
}

bool Miscellaneous::check_java() {
    QProcess process;
    process.start("java", QStringList() << "-version");
    process.waitForFinished();
    QByteArray output = process.readAllStandardError() + process.readAllStandardOutput();
    return output.contains("version");
}

void Miscellaneous::move_vpk_signatures(const QString& cs2_basefolder, bool& vpk_signatures_moved) {
    if (cs2_basefolder.isEmpty()) return;

    QString bin_folder = QDir(cs2_basefolder).filePath("game/bin/win64");
    QString vpk_path = QDir(bin_folder).filePath("vpk.signatures");
    QString temp_folder = QDir(bin_folder).filePath("temp");
    QString temp_vpk_path = QDir(temp_folder).filePath("vpk.signatures");

    if (QFile::exists(vpk_path)) {
        if (!QDir(bin_folder).exists("temp")) {
            QDir(bin_folder).mkdir("temp");
        }
        if (QFile::exists(temp_vpk_path)) {
            QFile::remove(temp_vpk_path);
        }
        QFile::rename(vpk_path, temp_vpk_path);
        vpk_signatures_moved = true;
    }
}

void Miscellaneous::restore_vpk_signatures(const QString& cs2_basefolder) {
    if (cs2_basefolder.isEmpty()) return;

    QString bin_folder = QDir(cs2_basefolder).filePath("game/bin/win64");
    QString vpk_path = QDir(bin_folder).filePath("vpk.signatures");
    QString temp_vpk_path = QDir(bin_folder).filePath("temp/vpk.signatures");

    if (QFile::exists(temp_vpk_path)) {
        if (QFile::exists(vpk_path)) {
            QFile::remove(vpk_path);
        }
        QFile::rename(temp_vpk_path, vpk_path);
    }
}

void Miscellaneous::cancel_all() {
    cancel_import = 1;
}



int Miscellaneous::run_command_sync(const QString& cmd) {
    if (cancel_import) return -1;
    Miscellaneous::log(cmd);

    QTemporaryFile tempLogFile;
    tempLogFile.setAutoRemove(false);
    if (!tempLogFile.open()) {
        Miscellaneous::log("Failed to create temporary log file.");
        return -1;
    }
    QString tempLogFilePath = tempLogFile.fileName();
    tempLogFile.close();

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.setStandardOutputFile(tempLogFilePath, QIODevice::Append);

    process.setProgram("cmd.exe");
#ifdef Q_OS_WIN
    process.setNativeArguments("/S /C \"" + cmd + "\"");
#else
    process.setArguments({"/c", cmd});
#endif
    process.start();

    QString lineBuffer;
    bool answeredPrompt = false;
    bool isSource1Import = cmd.contains("source1import.exe");
    bool checkingPrompt = false;

    auto processOutput = [&](const QString& outStr) {
        int i = 0;
        int len = outStr.length();
        while (i < len) {
            int nextNewline = outStr.indexOf('\n', i);
            if (nextNewline == -1) {
                lineBuffer += outStr.mid(i);
                break;
            } else {
                lineBuffer += outStr.mid(i, nextNewline - i);
                if (lineBuffer.endsWith('\r')) lineBuffer.chop(1);
                if (!lineBuffer.isEmpty()) {
                    Miscellaneous::log(lineBuffer);
                }
                if (isSource1Import && !answeredPrompt) {
                    if (lineBuffer.contains("Adding Search Path")) {
                        checkingPrompt = true;
                    } else if (lineBuffer.contains("Building file list...")) {
                        checkingPrompt = false;
                    }
                }
                lineBuffer.clear();
                i = nextNewline + 1;
            }
        }
    };

    QFile fileReader(tempLogFilePath);
    if (!fileReader.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Miscellaneous::log("Failed to open temp log file for reading.");
    }
    QTextStream inStream(&fileReader);

    QEventLoop loop;

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        if (cancel_import) {
            process.kill();
            loop.quit();
            return;
        }

        // Read newly appended data from the file
        if (fileReader.isOpen()) {
            QString newOutput = inStream.readAll();
            if (!newOutput.isEmpty()) {
                processOutput(newOutput);
            }
        }

        if (isSource1Import && !answeredPrompt && checkingPrompt && process.state() == QProcess::Running) {
            // We're likely stuck at the invisible "Are you sure you want to continue?" prompt
            process.write("y\n");
            answeredPrompt = true;
            checkingPrompt = false; // Stop checking
        }
    });
    timer.start(100);

    QObject::connect(&process, &QProcess::finished, &loop, &QEventLoop::quit);

    if (process.state() == QProcess::Running || process.state() == QProcess::Starting) {
        loop.exec();
    }

    timer.stop();

    if (fileReader.isOpen()) {
        QString newOutput = inStream.readAll();
        if (!newOutput.isEmpty()) {
            processOutput(newOutput);
        }
        fileReader.close();
    }

    if (!lineBuffer.isEmpty()) {
        if (lineBuffer.endsWith('\r')) lineBuffer.chop(1);
        Miscellaneous::log(lineBuffer);
    }

    if (QFile::exists(tempLogFilePath)) {
        QFile::remove(tempLogFilePath);
    }

    return process.exitCode();
}

bool copyDirectoryRecursively(const QString &sourceDir, const QString &destinationDir) {
    QDir source(sourceDir);
    if (!source.exists()) {
        return false;
    }

    QDir destination(destinationDir);
    if (!destination.exists()) {
        destination.mkpath(destinationDir);
    }

    bool success = true;

    QStringList files = source.entryList(QDir::Files);
    for (const QString &file : files) {
        QString srcPath = source.filePath(file);
        QString dstPath = destination.filePath(file);
        if (QFile::exists(dstPath)) {
            QFile::remove(dstPath);
        }
        if (!QFile::copy(srcPath, dstPath)) {
            success = false;
        }
    }

    QStringList dirs = source.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &dir : dirs) {
        QString srcPath = source.filePath(dir);
        QString dstPath = destination.filePath(dir);
        if (!copyDirectoryRecursively(srcPath, dstPath)) {
            success = false;
        }
    }

    return success;
}
