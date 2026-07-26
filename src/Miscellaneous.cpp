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

static Miscellaneous::Options globalOptions;

const Miscellaneous::Options& Miscellaneous::GetOptions() {
    return globalOptions;
}

void Miscellaneous::SetOptions(const Miscellaneous::Options& options) {
    globalOptions = options;
}

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



int Miscellaneous::RunCommandSync(const QString& program, const QStringList& arguments) {
    if (CanceLImport) return -1;

    // Log the command program and arguments in a clear format
    QString loggedCmd = program;
    for (const QString& arg : arguments) {
        if (arg.contains(' ') || arg.contains('\t') || arg.isEmpty()) {
            loggedCmd += " \"" + arg + "\"";
        } else {
            loggedCmd += " " + arg;
        }
    }
    Miscellaneous::Log(loggedCmd);

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);

    process.setProgram(program);
    process.setArguments(arguments);
    process.start();

    QString lineBuffer;
    bool isSource1Import = program.contains("source1import.exe");
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

QString Miscellaneous::CleanRefPath(QString input) {
    int filePos = input.indexOf("\"file\"");
    if (filePos != -1) {
        input = input.mid(filePos + 6);
    }

    QRegularExpression reLeading("^\\s*\"");
    QRegularExpressionMatch matchLeading = reLeading.match(input);
    if (matchLeading.hasMatch()) {
        input = input.mid(matchLeading.capturedLength());
    } else {
        int start = input.indexOf(QRegularExpression("[^ \\t]"));
        if (start != -1) {
            input = input.mid(start);
        } else {
            return "";
        }
    }

    QRegularExpression reTrailing("\"\\s*$");
    QRegularExpressionMatch matchTrailing = reTrailing.match(input);
    if (matchTrailing.hasMatch()) {
        input = input.left(input.size() - matchTrailing.capturedLength());
    } else {
        int end = input.lastIndexOf(QRegularExpression("[^ \\t]"));
        if (end != -1) {
            input = input.left(end + 1);
        }
    }

    if (input == "importfilelist" || input == "{" || input == "}") return "";
    return input;
}

QStringList Miscellaneous::ReadTextFile(const QString& filepath) {
    QStringList lines;
    QFile file(filepath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (!line.isEmpty() && line.endsWith('\r')) {
                line.chop(1);
            }
            lines.append(line);
        }
        file.close();
    }
    return lines;
}

void Miscellaneous::EnsureFileWritable(const QString& filepath) {
    QFileInfo p(filepath);
    if (p.exists()) {
        QFile::setPermissions(filepath, QFileDevice::WriteOwner | QFileDevice::WriteUser | QFileDevice::WriteGroup | QFileDevice::WriteOther | QFile::permissions(filepath));
    } else {
        QDir dir = p.dir();
        if (!dir.exists()) {
            dir.mkpath(".");
        }
    }
}
