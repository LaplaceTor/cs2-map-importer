#include "Miscellaneous.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QProcess>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QByteArray>

QAtomicInt Miscellaneous::CanceLImport(0);
Miscellaneous::LogCallback Miscellaneous::GlobaLLogger = nullptr;

void Miscellaneous::Log(const QString& msg) {
    if (GlobaLLogger) {
        GlobaLLogger(msg);
    }
}

bool Miscellaneous::CheckJava() {
    QProcess process;
    process.start("java", QStringList() << "-version");
    process.waitForFinished();
    QByteArray output = process.readAllStandardError() + process.readAllStandardOutput();
    return output.contains("version");
}

void Miscellaneous::MoveVpkSignatures(const QString& cs2Basefolder, bool& vpkSignaturesMoved) {
    if (cs2Basefolder.isEmpty()) return;

    QString binFolder = QDir(cs2Basefolder).filePath("game/bin/win64");
    QString vpkPath = QDir(binFolder).filePath("vpk.signatures");
    QString tempFolder = QDir(binFolder).filePath("temp");
    QString tempVpkPath = QDir(tempFolder).filePath("vpk.signatures");

    if (QFile::exists(vpkPath)) {
        if (!QDir(binFolder).exists("temp")) {
            QDir(binFolder).mkdir("temp");
        }
        if (QFile::exists(tempVpkPath)) {
            QFile::remove(tempVpkPath);
        }
        QFile::rename(vpkPath, tempVpkPath);
        vpkSignaturesMoved = true;
    }
}

void Miscellaneous::RestoreVpkSignatures(const QString& cs2Basefolder) {
    if (cs2Basefolder.isEmpty()) return;

    QString binFolder = QDir(cs2Basefolder).filePath("game/bin/win64");
    QString vpkPath = QDir(binFolder).filePath("vpk.signatures");
    QString tempVpkPath = QDir(binFolder).filePath("temp/vpk.signatures");

    if (QFile::exists(tempVpkPath)) {
        if (QFile::exists(vpkPath)) {
            QFile::remove(vpkPath);
        }
        QFile::rename(tempVpkPath, vpkPath);
    }
}

void Miscellaneous::CancelAll() {
    CanceLImport = 1;
}



int Miscellaneous::RunCommandSync(const QString& cmd) {
    if (CanceLImport) return -1;
    Miscellaneous::Log(cmd);

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
    bool isSource1Import = cmd.contains("source1import.exe");
    bool hasParseEparError = false;

    auto processOutput = [&](const QString& outStr) {
        for (QChar c : outStr) {
            if (c == '\n') {
                if (lineBuffer.endsWith('\r')) lineBuffer.chop(1);
                if (!lineBuffer.isEmpty()) {
                    Miscellaneous::Log(lineBuffer);
                    if (isSource1Import && lineBuffer.contains("ParseEpar: token too long")) {
                        hasParseEparError = true;
                    }
                }
                lineBuffer.clear();
            } else {
                lineBuffer += c;
            }
        }
    };

    while (process.waitForReadyRead(10000) || process.state() != QProcess::NotRunning) {
        if (CanceLImport) {
            process.kill();
            return -1;
        }
        QByteArray output = process.readAll();
        if (!output.isEmpty()) {
            processOutput(QString(output));
            if (hasParseEparError) {
                process.kill();
                Miscellaneous::CancelAll();
                throw AppException("This map geometry is too bad to run the clean up faces process!");
            }
        } else {
            // Timed out waiting for output
            if (isSource1Import && process.state() == QProcess::Running) {
                // We're likely stuck at the invisible "Are you sure you want to continue?" prompt
                process.write("y\n");
            }
        }
    }

    QByteArray output = process.readAll();
    if (!output.isEmpty()) {
        processOutput(QString(output));
    }

    if (!lineBuffer.isEmpty()) {
        if (lineBuffer.endsWith('\r')) lineBuffer.chop(1);
        Miscellaneous::Log(lineBuffer);
    }

    return process.exitCode();
}

bool CopyDirectoryRecursively(const QString &sourceDir, const QString &destinationDir) {
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
        if (!CopyDirectoryRecursively(srcPath, dstPath)) {
            success = false;
        }
    }

    return success;
}
