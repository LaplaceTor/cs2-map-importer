#include "FileExtractFromVPK.h"
#include "Miscellaneous.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

struct SearchResult {
    bool found = false;
    bool isVpk = false;
    QString path;
};

static SearchResult SearchAndExtractFileEx(const QString& filepath, const QString& contentPath, const QString& outPath) {
    SearchResult res;
    const auto& opts = Miscellaneous::GetOptions();
    const auto& targets = opts.searchTargets;

    for (const auto& target : targets) {
        if (Miscellaneous::CanceLImport) return res;

        if (target.isVpk) {
            QString vpkPath = target.path;
            if (!QFile::exists(vpkPath)) continue;

            if (QFile::exists(contentPath)) {
                QFile::remove(contentPath);
            }

            QStringList arguments = {
                "-e",
                filepath,
                QDir::toNativeSeparators(vpkPath),
                "-o",
                QDir::toNativeSeparators(contentPath)
            };
            int ret = Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_VPKEDITCLI, arguments);

            if (ret == 100) {
                QFileInfo fi(outPath);
                QDir().mkpath(fi.absolutePath());
                if (QFile::exists(outPath)) {
                    QFile::remove(outPath);
                }
                if (QFile::copy(contentPath, outPath)) {
                    res.found = true;
                    res.isVpk = true;
                    res.path = vpkPath;
                    return res; // Found and extracted successfully!
                } else {
                    if (opts.cmdLogOut) {
                        Miscellaneous::Log("Failed to copy from " + contentPath + " to " + outPath);
                    }
                }
            }
        } else {
            // Folder target
            // 1. Check raw folder first
            QString folderPath = target.path;
            QString rawFilePath = QDir(folderPath).filePath(filepath);
            if (QFile::exists(rawFilePath)) {
                QFileInfo fiContent(contentPath);
                QDir().mkpath(fiContent.absolutePath());
                if (QFile::exists(contentPath)) {
                    QFile::remove(contentPath);
                }
                if (QFile::copy(rawFilePath, contentPath)) {
                    QFileInfo fiOut(outPath);
                    QDir().mkpath(fiOut.absolutePath());
                    if (QFile::exists(outPath)) {
                        QFile::remove(outPath);
                    }
                    if (QFile::copy(contentPath, outPath)) {
                        res.found = true;
                        res.isVpk = false;
                        res.path = folderPath;
                        return res; // Found and copied successfully!
                    } else {
                        if (opts.cmdLogOut) {
                            Miscellaneous::Log("Failed to copy from " + contentPath + " to " + outPath);
                        }
                    }
                } else {
                    if (opts.cmdLogOut) {
                        Miscellaneous::Log("Failed to copy from " + rawFilePath + " to " + contentPath);
                    }
                }
            }

            // 2. Check pak01_dir.vpk in that folder
            QString pakVpkPath = QDir(folderPath).filePath("pak01_dir.vpk");
            if (QFile::exists(pakVpkPath)) {
                if (QFile::exists(contentPath)) {
                    QFile::remove(contentPath);
                }

                QStringList arguments = {
                    "-e",
                    filepath,
                    QDir::toNativeSeparators(pakVpkPath),
                    "-o",
                    QDir::toNativeSeparators(contentPath)
                };
                int ret = Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_VPKEDITCLI, arguments);

                if (ret == 100) {
                    QFileInfo fi(outPath);
                    QDir().mkpath(fi.absolutePath());
                    if (QFile::exists(outPath)) {
                        QFile::remove(outPath);
                    }
                    if (QFile::copy(contentPath, outPath)) {
                        res.found = true;
                        res.isVpk = true;
                        res.path = pakVpkPath;
                        return res; // Found and extracted successfully!
                    } else {
                        if (opts.cmdLogOut) {
                            Miscellaneous::Log("Failed to copy from " + contentPath + " to " + outPath);
                        }
                    }
                }
            }
        }
    }
    return res;
}

static bool SearchAndExtractFile(const QString& filepath, const QString& contentPath, const QString& outPath) {
    return SearchAndExtractFileEx(filepath, contentPath, outPath).found;
}

bool FileExtractFromVPK::ExtractModel(const QString& filepath) {
    QFileInfo fi(filepath);
    QString basePath = fi.path() + "/" + fi.baseName();
    QString contentPath = QDir(Miscellaneous::GetOptions().s1contentdir).filePath(filepath);
    QString outPath = QDir(Miscellaneous::GetOptions().s1gamedir).filePath(filepath);

    const auto& opts = Miscellaneous::GetOptions();

    SearchResult res = SearchAndExtractFileEx(filepath, contentPath, outPath);

    if (res.found) {
        QStringList extlist = {"vvd", "phy", "sw.vtx", "dx80.vtx", "dx90.vtx", "ani"};
        for (const QString& ext : extlist) {
            if (Miscellaneous::CanceLImport) return false;
            QString targetFile = basePath + "." + ext;
            QString subContentPath = QDir(opts.s1contentdir).filePath(targetFile);
            QString subOutPath = QDir(opts.s1gamedir).filePath(targetFile);

            if (res.isVpk) {
                if (QFile::exists(subContentPath)) {
                    QFile::remove(subContentPath);
                }

                QStringList arguments = {
                    "-e",
                    targetFile,
                    QDir::toNativeSeparators(res.path),
                    "-o",
                    QDir::toNativeSeparators(subContentPath)
                };
                int ret = Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_VPKEDITCLI, arguments);

                if (ret == 100) {
                    QFileInfo fiOut(subOutPath);
                    QDir().mkpath(fiOut.absolutePath());
                    if (QFile::exists(subOutPath)) {
                        QFile::remove(subOutPath);
                    }
                    if (!QFile::copy(subContentPath, subOutPath)) {
                        if (opts.cmdLogOut) {
                            Miscellaneous::Log("Failed to copy from " + subContentPath + " to " + subOutPath);
                        }
                    }
                }
            } else {
                // Copy from found raw folder
                QString rawFilePath = QDir(res.path).filePath(targetFile);
                if (QFile::exists(rawFilePath)) {
                    QFileInfo fiContent(subContentPath);
                    QDir().mkpath(fiContent.absolutePath());
                    if (QFile::exists(subContentPath)) {
                        QFile::remove(subContentPath);
                    }
                    if (QFile::copy(rawFilePath, subContentPath)) {
                        QFileInfo fiOut(subOutPath);
                        QDir().mkpath(fiOut.absolutePath());
                        if (QFile::exists(subOutPath)) {
                            QFile::remove(subOutPath);
                        }
                        if (!QFile::copy(subContentPath, subOutPath)) {
                            if (opts.cmdLogOut) {
                                Miscellaneous::Log("Failed to copy from " + subContentPath + " to " + subOutPath);
                            }
                        }
                    } else {
                        if (opts.cmdLogOut) {
                            Miscellaneous::Log("Failed to copy from " + rawFilePath + " to " + subContentPath);
                        }
                    }
                }
            }
        }
    } else {
        return false; // Model not found in any target
    }
    return true; // Model and associated files extracted/copied successfully
}

bool FileExtractFromVPK::ExtractMaterial(const QString& filepath) {
    QString contentPath = QDir(Miscellaneous::GetOptions().s1contentdir).filePath(filepath);
    QString outPath = QDir(Miscellaneous::GetOptions().s1gamedir).filePath(filepath);
    return SearchAndExtractFile(filepath, contentPath, outPath);
}

bool FileExtractFromVPK::ExtractParticle(const QString& filepath) {
    QString contentPath = QDir(Miscellaneous::GetOptions().s1contentdir).filePath(filepath);
    QString outPath = QDir(Miscellaneous::GetOptions().s1gamedir).filePath(filepath);
    return SearchAndExtractFile(filepath, contentPath, outPath);
}

bool FileExtractFromVPK::ExtractSound(const QString& filepath) {
    QString contentPath = QDir(Miscellaneous::GetOptions().s1contentdir).filePath(filepath);
    QString outPath = QDir(Miscellaneous::GetOptions().s1gamedir).filePath(filepath);
    return SearchAndExtractFile(filepath, contentPath, outPath);
}
