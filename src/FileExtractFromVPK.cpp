#include "FileExtractFromVPK.h"
#include "Miscellaneous.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

static bool SearchAndExtractFile(const QString& filepath, const QString& contentPath, const QString& outPath) {
    const auto& opts = Miscellaneous::GetOptions();
    const auto& targets = opts.searchTargets;

    for (const auto& target : targets) {
        if (Miscellaneous::CanceLImport) return false;

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
                QFile::copy(contentPath, outPath);
                return true; // Found and extracted successfully!
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
                    QFile::copy(contentPath, outPath);
                    return true; // Found and copied successfully!
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
                    QFile::copy(contentPath, outPath);
                    return true; // Found and extracted successfully!
                }
            }
        }
    }
    return false;
}

bool FileExtractFromVPK::ExtractModel(const QString& filepath) {
    QFileInfo fi(filepath);
    QString basePath = fi.path() + "/" + fi.baseName();
    QString contentPath = QDir(Miscellaneous::GetOptions().s1contentdir).filePath(filepath);
    QString outPath = QDir(Miscellaneous::GetOptions().s1gamedir).filePath(filepath);

    const auto& opts = Miscellaneous::GetOptions();
    const auto& targets = opts.searchTargets;

    bool found = false;
    bool foundInVpk = false;
    QString foundVpkPath;
    QString foundFolderPath;

    for (const auto& target : targets) {
        if (Miscellaneous::CanceLImport) return;

        if (target.isVpk) {
            QString vpkPath = target.path;
            if (!QFile::exists(vpkPath)) continue;

            if (QFile::exists(contentPath)) {
                QFile::remove(contentPath);
            }

            // Extract main .mdl file
            QStringList arguments = {
                "-e",
                filepath,
                QDir::toNativeSeparators(vpkPath),
                "-o",
                QDir::toNativeSeparators(contentPath)
            };
            int ret = Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_VPKEDITCLI, arguments);

            if (ret == 100) {
                QFileInfo fiOut(outPath);
                QDir().mkpath(fiOut.absolutePath());
                if (QFile::exists(outPath)) {
                    QFile::remove(outPath);
                }
                QFile::copy(contentPath, outPath);
                found = true;
                foundInVpk = true;
                foundVpkPath = vpkPath;
                break;
            }
        } else {
            // Folder target
            // 1. Raw folder
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
                    QFile::copy(contentPath, outPath);
                    found = true;
                    foundFolderPath = folderPath;
                    break;
                }
            }

            // 2. pak01_dir.vpk inside raw folder
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
                    QFileInfo fiOut(outPath);
                    QDir().mkpath(fiOut.absolutePath());
                    if (QFile::exists(outPath)) {
                        QFile::remove(outPath);
                    }
                    QFile::copy(contentPath, outPath);
                    found = true;
                    foundInVpk = true;
                    foundVpkPath = pakVpkPath;
                    break;
                }
            }
        }
    }

    if (found) {
        QStringList extlist = {"vvd", "phy", "sw.vtx", "dx80.vtx", "dx90.vtx", "ani"};
        for (const QString& ext : extlist) {
            if (Miscellaneous::CanceLImport) return;
            QString targetFile = basePath + "." + ext;
            contentPath = QDir(opts.s1contentdir).filePath(targetFile);
            outPath = QDir(opts.s1gamedir).filePath(targetFile);

            if (foundInVpk) {
                if (QFile::exists(contentPath)) {
                    QFile::remove(contentPath);
                }

                QStringList arguments = {
                    "-e",
                    targetFile,
                    QDir::toNativeSeparators(foundVpkPath),
                    "-o",
                    QDir::toNativeSeparators(contentPath)
                };
                int ret = Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_VPKEDITCLI, arguments);

                if (ret == 100) {
                    QFileInfo fiOut(outPath);
                    QDir().mkpath(fiOut.absolutePath());
                    if (QFile::exists(outPath)) {
                        QFile::remove(outPath);
                    }
                    QFile::copy(contentPath, outPath);
                }
            } else {
                // Copy from found raw folder
                QString rawFilePath = QDir(foundFolderPath).filePath(targetFile);
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
                        QFile::copy(contentPath, outPath);
                    }
                }
            }
        }
    }else {
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
