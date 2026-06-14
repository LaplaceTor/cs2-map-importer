#include "appcore.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QProcess>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QByteArray>

QAtomicInt AppCore::cancel_import(0);

bool AppCore::check_java() {
    QProcess process;
    process.start("java", QStringList() << "-version");
    process.waitForFinished();
    QByteArray output = process.readAllStandardError() + process.readAllStandardOutput();
    return output.contains("version");
}

void AppCore::move_vpk_signatures(const QString& cs2_basefolder, bool& vpk_signatures_moved) {
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

void AppCore::restore_vpk_signatures(const QString& cs2_basefolder) {
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

void AppCore::cancel_all() {
    cancel_import = 1;
}



int AppCore::run_command_sync(const QString& cmd, LogCallback logger) {
    if (cancel_import) return -1;
    if (logger) logger(cmd);

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);

    // In QProcess, if we use setNativeArguments, cmd.exe requires the whole string after /c to be quoted if there are inner quotes.
    // However, a simpler cross-platform way is to use QProcess's own parsing if we avoid cmd.exe.
    // Since we need it to behave like CreateProcess, we can just start the command natively if we are not using pipes.
    // For safety with cmd.exe, it expects /c ""command" "arg1" "arg2"". So we wrap in quotes.
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
        for (QChar c : outStr) {
            if (c == '\n') {
                if (lineBuffer.endsWith('\r')) lineBuffer.chop(1);
                if (!lineBuffer.isEmpty() && logger) {
                    logger(lineBuffer);
                }
                if (isSource1Import && !answeredPrompt) {
                    if (lineBuffer.contains("Adding Search Path")) {
                        checkingPrompt = true;
                    } else if (lineBuffer.contains("Building file list...")) {
                        checkingPrompt = false;
                    }
                }
                lineBuffer.clear();
            } else {
                lineBuffer += c;
            }
        }
    };

    while (process.waitForReadyRead(100) || process.state() != QProcess::NotRunning) {
        if (cancel_import) {
            process.kill();
            return -1;
        }
        QByteArray output = process.readAll();
        if (!output.isEmpty()) {
            processOutput(QString(output));
        } else {
            // Timed out waiting for output
            if (isSource1Import && !answeredPrompt && checkingPrompt && process.state() == QProcess::Running) {
                // We're likely stuck at the invisible "Are you sure you want to continue?" prompt
                process.write("y\n");
                answeredPrompt = true;
                checkingPrompt = false; // Stop checking
            }
        }
    }

    QByteArray output = process.readAll();
    if (!output.isEmpty()) {
        processOutput(QString(output));
    }

    if (!lineBuffer.isEmpty() && logger) {
        if (lineBuffer.endsWith('\r')) lineBuffer.chop(1);
        logger(lineBuffer);
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
