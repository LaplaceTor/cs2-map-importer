#include "Miscellaneous.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QProcess>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QByteArray>
#include <QThreadPool>
#include <QRunnable>
#include <QSet>
#include <QThread>

namespace {

class ExtractTask : public QRunnable {
public:
    ExtractTask(const QString& item, bool is_folder, const QString& vpkeditcli_exe, const QString& bspFile, const QString& maps_dir, QAtomicInt& completedCount, int totalCount, QAtomicInt& successFlag)
        : item_(item), is_folder_(is_folder), vpkeditcli_exe_(vpkeditcli_exe), bspFile_(bspFile), maps_dir_(maps_dir), completedCount_(completedCount), totalCount_(totalCount), successFlag_(successFlag) {}

    void run() override {
        if (Miscellaneous::CanceLImport || successFlag_ == 0) return;

        int currentIdx = ++completedCount_;
        if (is_folder_) {
            Miscellaneous::Log(QString("[%1/%2] Extracting subfolder: %3/").arg(currentIdx).arg(totalCount_).arg(item_));
        } else {
            Miscellaneous::Log(QString("[%1/%2] Extracting file: %3").arg(currentIdx).arg(totalCount_).arg(item_));
        }

        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.setProgram(vpkeditcli_exe_);
        QStringList arguments = {
            "-e",
            is_folder_ ? (item_ + "/") : item_,
            "-o",
            QDir::toNativeSeparators(maps_dir_),
            QDir::toNativeSeparators(bspFile_)
        };
        process.setArguments(arguments);
        process.start();
        if (process.waitForStarted()) {
            QString lineBuffer;
            auto processOutput = [&](const QString& outStr) {
                for (QChar c : outStr) {
                    if (c == '\n') {
                        if (lineBuffer.endsWith('\r')) lineBuffer.chop(1);
                        if (!lineBuffer.isEmpty()) {
                            Miscellaneous::Log(QString("[%1/%2] %3").arg(currentIdx).arg(totalCount_).arg(lineBuffer));
                        }
                        lineBuffer.clear();
                    } else {
                        lineBuffer += c;
                    }
                }
            };

            while (process.waitForReadyRead(100) || process.state() == QProcess::Running) {
                if (Miscellaneous::CanceLImport) {
                    process.kill();
                    return;
                }
                QByteArray output = process.readAll();
                if (!output.isEmpty()) {
                    processOutput(QString::fromUtf8(output));
                }
            }
            QByteArray output = process.readAll();
            if (!output.isEmpty()) {
                processOutput(QString::fromUtf8(output));
            }
            if (!lineBuffer.isEmpty()) {
                if (lineBuffer.endsWith('\r')) lineBuffer.chop(1);
                Miscellaneous::Log(QString("[%1/%2] %3").arg(currentIdx).arg(totalCount_).arg(lineBuffer));
            }
        }

        if (process.exitCode() != 0) {
            Miscellaneous::Log(QString("Error: Failed to extract: %1").arg(item_));
            successFlag_ = 0;
        }
    }

private:
    QString item_;
    bool is_folder_;
    QString vpkeditcli_exe_;
    QString bspFile_;
    QString maps_dir_;
    QAtomicInt& completedCount_;
    int totalCount_;
    QAtomicInt& successFlag_;
};

} // namespace

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



int Miscellaneous::RunCommandSync(int program, const QStringList& arguments, bool logOut, QStringList* logOutString, bool isMap, bool isCSGO) {
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
            if (appDir.isEmpty()) {
                programPath = QDir::toNativeSeparators("bin/vpkeditcli.exe");
            } else {
                programPath = QDir::toNativeSeparators(QDir(appDir).filePath("bin/vpkeditcli.exe"));
            }
            break;
        case PROGRAM_VTFCMD: // 5
            if (appDir.isEmpty()) {
                programPath = QDir::toNativeSeparators("bin/vtfcmd.exe");
            } else {
                programPath = QDir::toNativeSeparators(QDir(appDir).filePath("bin/vtfcmd.exe"));
            }
            break;
        case PROGRAM_BSPSRC: // 6
            {
                QString javaExe = "bin/java.exe";
                if (appDir.isEmpty()) {
                    programPath = QDir::toNativeSeparators("bin/" + javaExe);
                } else {
                    programPath = QDir::toNativeSeparators(QDir(appDir).filePath("bin/" + javaExe));
                }

                if (!QFile::exists(programPath)) {
                    throw AppException("Could not find java executable at " + QDir::toNativeSeparators(programPath));
                }

                QString jarPath;
                if (appDir.isEmpty()) {
                    jarPath = QDir::toNativeSeparators("bin/bspsrc.jar");
                } else {
                    jarPath = QDir::toNativeSeparators(QDir(appDir).filePath("bin/bspsrc.jar"));
                }

                if (!QFile::exists(jarPath)) {
                    throw AppException("Could not find bspsrc.jar at " + QDir::toNativeSeparators(jarPath));
                }
            }
            break;
        default:
            throw AppException("Unknown program ID " + QString::number(program));
    }

    QStringList finalArguments = arguments;
    if (program == PROGRAM_BSPSRC) {
        QString jarPath;
        if (appDir.isEmpty()) {
            jarPath = QDir::toNativeSeparators("bin/bspsrc.jar");
        } else {
            jarPath = QDir::toNativeSeparators(QDir(appDir).filePath("bin/bspsrc.jar"));
        }
        finalArguments.prepend(jarPath);
        finalArguments.prepend("-jar");
    }

    if (program == PROGRAM_VPKEDITCLI && isMap) {
        QString maps_dir;
        QString bspFile;
        for (int i = 0; i < arguments.size(); ++i) {
            if (arguments[i] == "-o" && i + 1 < arguments.size()) {
                maps_dir = arguments[i + 1];
            }
        }
        if (!arguments.isEmpty()) {
            bspFile = arguments.last();
        }

        if (maps_dir.isEmpty() || bspFile.isEmpty()) {
            throw AppException("Failed to parse output directory or BSP file path for parallel extraction.");
        }

        Miscellaneous::Log("Reading BSP file tree using vpkeditcli...");
        QProcess treeProcess;
        treeProcess.setProgram(programPath);
        treeProcess.setArguments({"--file-tree", QDir::toNativeSeparators(bspFile)});
        treeProcess.start();
        if (!treeProcess.waitForStarted()) {
            throw AppException("Failed to start vpkeditcli to read the BSP file tree.");
        }
        if (!treeProcess.waitForFinished(30000)) { // 30 seconds timeout
            treeProcess.kill();
            throw AppException("vpkeditcli timed out reading the BSP file tree.");
        }
        if (treeProcess.exitCode() != 0) {
            throw AppException("vpkeditcli failed with exit code " + QString::number(treeProcess.exitCode()) + " while reading the BSP file tree.");
        }
        QString treeOutput = QString::fromUtf8(treeProcess.readAllStandardOutput());
        if (treeOutput.trimmed().isEmpty()) {
            throw AppException("vpkeditcli returned an empty BSP file tree.");
        }

        QStringList lines = treeOutput.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
        QMap<int, QString> dir_path_by_level;
        QSet<QString> unique_parents;
        struct FileInfo {
            QString path;
            QString parent;
        };
        QList<FileInfo> all_files;

        QRegularExpression sizeRegex("\\s*-\\s*\\d+(\\.\\d+)?\\s*\\w*\\s*$");

        for (const QString& rawLine : lines) {
            if (CanceLImport) return -1;
            QString line = rawLine;
            int name_start_idx = 0;
            while (name_start_idx < line.length()) {
                QChar c = line[name_start_idx];
                if (c.isSpace() ||
                    (c.unicode() >= 0x2500 && c.unicode() <= 0x257F) ||
                    c == '|' || c == '+' || c == '-' || c == '\\') {
                    name_start_idx++;
                } else {
                    break;
                }
            }

            if (name_start_idx >= line.length()) {
                continue;
            }

            QString name_part = line.mid(name_start_idx).trimmed();
            if (name_part.isEmpty()) continue;

            int level = 0;
            if (name_start_idx > 0) {
                level = (name_start_idx - 3) / 3;
            }

            if (level == 0) {
                continue;
            }

            QRegularExpressionMatch sizeMatch = sizeRegex.match(name_part);
            bool is_file = sizeMatch.hasMatch();

            if (is_file) {
                QString filename = name_part.left(sizeMatch.capturedStart()).trimmed();
                QString parent_path = "";
                for (int l = 1; l < level; ++l) {
                    if (dir_path_by_level.contains(l)) {
                        if (!parent_path.isEmpty()) parent_path += "/";
                        parent_path += dir_path_by_level[l];
                    }
                }
                QString full_path = parent_path.isEmpty() ? filename : (parent_path + "/" + filename);
                all_files.append({full_path, parent_path});
                if (!parent_path.isEmpty()) {
                    unique_parents.insert(parent_path);
                }
            } else {
                QString dir_name = name_part;
                dir_path_by_level[level] = dir_name;
                QList<int> keys = dir_path_by_level.keys();
                for (int k : keys) {
                    if (k > level) {
                        dir_path_by_level.remove(k);
                    }
                }
            }
        }

        QStringList leaf_folders;
        for (const QString& parent : unique_parents) {
            if (CanceLImport) return -1;
            bool is_leaf = true;
            for (const QString& other : unique_parents) {
                if (other != parent && other.startsWith(parent + "/")) {
                    is_leaf = false;
                    break;
                }
            }
            if (is_leaf) {
                leaf_folders.append(parent);
            }
        }

        QStringList individual_files;
        for (const FileInfo& file : all_files) {
            if (CanceLImport) return -1;
            if (file.parent.isEmpty() || !leaf_folders.contains(file.parent)) {
                individual_files.append(file.path);
            }
        }

        int totalCount = leaf_folders.size() + individual_files.size();
        Miscellaneous::Log(QString("Found %1 subfolders and %2 individual files to extract in parallel...").arg(leaf_folders.size()).arg(individual_files.size()));

        QThreadPool pool;
        pool.setMaxThreadCount(QThread::idealThreadCount());

        QAtomicInt completedCount(0);
        QAtomicInt successFlag(1);

        for (const QString& folder : leaf_folders) {
            if (CanceLImport) break;
            pool.start(new ExtractTask(folder, true, programPath, bspFile, maps_dir, completedCount, totalCount, successFlag));
        }

        for (const QString& file : individual_files) {
            if (CanceLImport) break;
            pool.start(new ExtractTask(file, false, programPath, bspFile, maps_dir, completedCount, totalCount, successFlag));
        }

        pool.waitForDone();

        if (CanceLImport) return -1;

        if (successFlag == 0) {
            throw AppException("Failed to extract some embedded files from BSP.");
        }

        return 0; // Return exit code 0 on success
    }

    // Log the command program path and arguments in a clear format
    QString loggedCmd = programPath;
    for (const QString& arg : finalArguments) {
        if (arg.contains(' ') || arg.contains('\t') || arg.isEmpty()) {
            loggedCmd += " \"" + arg + "\"";
        } else {
            loggedCmd += " " + arg;
        }
    }
    Miscellaneous::Log(loggedCmd);

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
                    Miscellaneous::Log(lineBuffer);
                    if (logOut && logOutString) {
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
        Miscellaneous::Log(lineBuffer);
        if (logOut && logOutString) {
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
