#include "Miscellaneous.h"
#include "Ui.h"
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <QDir>
#include <QMetaObject>
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



int Miscellaneous::RunCommandSync(int program, const QStringList& arguments, QStringList* logOutString, bool isMap, bool isCSGO) {
    if (CanceLImport) return -1;

    QString programPath;
    QString appDir = Miscellaneous::GetOptions().appDir;
    QString cs2Basefolder = Miscellaneous::GetOptions().cs2Basefolder;

    switch (program) {
        case PROGRAM_SOURCE1IMPORT: // 1
            programPath = QDir::toNativeSeparators(cs2Basefolder + "/game/bin/win64/source1import.exe");
            break;
        case PROGRAM_CS_MDL_IMPORT: // 2
            programPath = QDir::toNativeSeparators(cs2Basefolder + "/game/bin/win64/cs_mdl_import.exe");
            break;
        case PROGRAM_RESOURCECOMPILER: // 3
            programPath = QDir::toNativeSeparators(cs2Basefolder + "/game/bin/win64/resourcecompiler.exe");
            break;
        case PROGRAM_VPKEDITCLI: // 4
            {
                programPath = QDir::toNativeSeparators(QDir(appDir).filePath("bin/vpkeditcli.exe"));
                if (!QFile::exists(programPath)) {
                    throw AppException("Could not find vpkeditcli executable at " + QDir::toNativeSeparators(programPath));
                }
            }
            break;
        case PROGRAM_VTFCMD: // 5
            {
                programPath = QDir::toNativeSeparators(QDir(appDir).filePath("bin/vtfcmd.exe"));
                if (!QFile::exists(programPath)) {
                    throw AppException("Could not find vtfcmd executable at " + QDir::toNativeSeparators(programPath));
                }
            }
            break;
        case PROGRAM_BSPSRC: // 6
            {
                QString javaExe = "bin/java.exe";
                programPath = QDir::toNativeSeparators(QDir(appDir).filePath("bin/" + javaExe));
                if (!QFile::exists(programPath)) {
                    throw AppException("Could not find java executable at " + QDir::toNativeSeparators(programPath));
                }
            }
            break;
        default:
            throw AppException("Unknown program ID " + QString::number(program));
    }

    QStringList finalArguments = arguments;
    if (program == PROGRAM_BSPSRC) {
        QString jarPath = QDir::toNativeSeparators(QDir(appDir).filePath("bin/bspsrc.jar"));
        if (!QFile::exists(jarPath)) {
            throw AppException("Could not find bspsrc.jar at " + QDir::toNativeSeparators(jarPath));
        }
        finalArguments.prepend(jarPath);
        finalArguments.prepend("-jar");
    }

    bool cmdLogOut = Miscellaneous::GetOptions().cmdLogOut;

    // Log the command program path and arguments in a clear format
    QString loggedCmd = programPath;
    for (const QString& arg : finalArguments) {
        if (arg.contains(' ') || arg.contains('\t') || arg.isEmpty()) {
            loggedCmd += " \"" + arg + "\"";
        } else {
            loggedCmd += " " + arg;
        }
    }
    if (cmdLogOut) {
        Miscellaneous::Log(loggedCmd);
    }

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);

    process.setProgram(programPath);
    process.setArguments(finalArguments);
    if (program == PROGRAM_SOURCE1IMPORT && isMap) {
        QString workingDir = QDir::toNativeSeparators(cs2Basefolder + "/game/csgo/import_scripts");
        process.setWorkingDirectory(workingDir);
    }
    process.start();

    QString lineBuffer;
    bool isSource1Import = (program == PROGRAM_SOURCE1IMPORT);
    bool hasParseEparError = false;

    auto processOutput = [&](const QString& outStr) {
        for (QChar c : outStr) {
            if (c == '\n') {
                if (lineBuffer.endsWith('\r')) lineBuffer.chop(1);
                if (!lineBuffer.isEmpty()) {
                    if (cmdLogOut) {
                        Miscellaneous::Log(lineBuffer);
                    }
                    if (logOutString) {
                        logOutString->append(lineBuffer);
                    }
                    if (isSource1Import && isMap && lineBuffer.contains("ParseEpar: token too long")) {
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
            if (!isCSGO && isSource1Import && process.state() == QProcess::Running) {
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
        if (cmdLogOut) {
            Miscellaneous::Log(lineBuffer);
        }
        if (logOutString) {
            logOutString->append(lineBuffer);
        }
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

bool Miscellaneous::IsCorrectSymlink(const QString& linkPath, const QString& targetPath) {
    QFileInfo linkInfo(linkPath);
    if (linkInfo.isSymLink()) {
        QFileInfo targetInfo(targetPath);
        return (linkInfo.canonicalFilePath().compare(targetInfo.canonicalFilePath(), Qt::CaseInsensitive) == 0);
    }
    return false;
}

bool Miscellaneous::CreateSymlink(const QString& linkPath, const QString& targetPath) {
    QString msgText = QString("To fix texture scale errors, the map importer needs to create a directory symbolic link (symlink) named 'csgo' pointing to your Source 1 game directory:\n%1\n\nThis will allow the importer to treat the game as CS:GO and import it properly.\n\nCreating symbolic links requires Administrator privileges. Would you like to request administrator permission and create the symlink?").arg(targetPath);

    if (!Backend::ShowMessageBox("Administrator Permission Required", msgText, 0, true)) {
        Miscellaneous::Log("User declined administrator elevation. Import process aborted.");
        return false;
    }

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.cbSize = sizeof(sei);
    sei.lpVerb = L"runas";
    sei.lpFile = L"cmd.exe";
    QString params = QString("/c mklink /d \"%1\" \"%2\"").arg(linkPath).arg(targetPath);
    sei.lpParameters = (LPCWSTR)params.utf16();
    sei.nShow = SW_HIDE;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;

    if (ShellExecuteExW(&sei)) {
        if (sei.hProcess != NULL) {
            WaitForSingleObject(sei.hProcess, INFINITE);
            CloseHandle(sei.hProcess);
        }
    }

    return IsCorrectSymlink(linkPath, targetPath);
}
