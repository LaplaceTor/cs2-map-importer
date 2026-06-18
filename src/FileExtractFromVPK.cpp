#include "FileExtractFromVPK.h"
#include "miscellaneous.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

static QStringList GetVpkList(const MapImporter::Options& options) {
    if (options.s1gamename == "css") {
        return {
            "cstrike/cstrike_pak_dir.vpk",
            "hl2/hl2_textures_dir.vpk",
            "hl2/hl2_sound_misc_dir.vpk",
            "hl2/hl2_misc_dir.vpk",
            "hl2/hl2_sound_vo_english_dir.vpk"
        };
    } else {
        return { "csgo/pak01_dir.vpk" };
    }
}

void FileExtractFromVPK::ExtractModel(const QString& filepath, const MapImporter::Options& options) {
    QFileInfo fi(filepath);
    QString basePath = fi.path() + "/" + fi.baseName();
    QString contentPath = QDir(options.s1contentdir).filePath(filepath);
    QString outPath = QDir(options.s1gamedir).filePath(filepath);

    QString baseFolder = QFileInfo(options.s1gamedir).dir().absolutePath();
    QStringList vpkList = GetVpkList(options);
    QString foundVpkPath;

    for (const QString& vpkRel : vpkList) {
        QString vpkPath = QDir(baseFolder).filePath(vpkRel);

        if (QFile::exists(contentPath)) {
            QFile::remove(contentPath);
        }

        QString cmd = "\"bin\\vpkeditcli.exe\" -e \"" + filepath + "\" \"" + vpkPath + "\" -o \"" + contentPath + "\"";
        cmd = cmd.replace("/", "\\");
        Miscellaneous::run_command_sync(cmd);

        if (QFile::exists(contentPath)) {
            QFile::copy(contentPath, outPath);
            foundVpkPath = vpkPath;
            break;
        }
    }

    if (!foundVpkPath.isEmpty()) {
        QStringList extlist = {"vvd","phy","sw.vtx","dx80.vtx","dx90.vtx","ani"};
        for (const QString& ext : extlist) {
            QString target = basePath + "." + ext;
            contentPath = QDir(options.s1contentdir).filePath(target);

            if (QFile::exists(contentPath)) {
                QFile::remove(contentPath);
            }

            QString cmd = "\"bin\\vpkeditcli.exe\" -e \"" + target + "\" \"" + foundVpkPath + "\" -o \"" + contentPath + "\"";
            cmd = cmd.replace("/", "\\");
            Miscellaneous::run_command_sync(cmd);

            outPath = QDir(options.s1gamedir).filePath(target);
            if (QFile::exists(contentPath)) {
                QFile::copy(contentPath, outPath);
            }
        }
    }
}

void FileExtractFromVPK::ExtractParticle(const QString& filepath, const MapImporter::Options& options) {
    QString contentPath = QDir(options.s1contentdir).filePath(filepath);
    QString outPath = QDir(options.s1gamedir).filePath(filepath);
    QString baseFolder = QFileInfo(options.s1gamedir).dir().absolutePath();
    QStringList vpkList = GetVpkList(options);

    for (const QString& vpkRel : vpkList) {
        QString vpkPath = QDir(baseFolder).filePath(vpkRel);

        if (QFile::exists(contentPath)) {
            QFile::remove(contentPath);
        }

        QString cmd = "\"bin\\vpkeditcli.exe\" -e \"" + filepath + "\" \"" + vpkPath + "\" -o \"" + contentPath + "\"";
        cmd = cmd.replace("/", "\\");
        Miscellaneous::run_command_sync(cmd);

        if (QFile::exists(contentPath)) {
            QFileInfo fi(outPath);
            QDir().mkpath(fi.absolutePath());
            if (QFile::exists(outPath)) {
                QFile::remove(outPath);
            }
            QFile::copy(contentPath, outPath);
            break;
        }
    }
}

void FileExtractFromVPK::ExtractSound(const QString& filepath, const MapImporter::Options& options) {
    QString contentPath = QDir(options.s1contentdir).filePath(filepath);
    QString outPath = QDir(options.s1gamedir).filePath(filepath);
    QString baseFolder = QFileInfo(options.s1gamedir).dir().absolutePath();
    QStringList vpkList = GetVpkList(options);

    for (const QString& vpkRel : vpkList) {
        QString vpkPath = QDir(baseFolder).filePath(vpkRel);

        if (QFile::exists(contentPath)) {
            QFile::remove(contentPath);
        }

        QString cmd = "\"bin\\vpkeditcli.exe\" -e \"" + filepath + "\" \"" + vpkPath + "\" -o \"" + contentPath + "\"";
        cmd = cmd.replace("/", "\\");
        Miscellaneous::run_command_sync(cmd);

        if (QFile::exists(contentPath)) {
            QFileInfo fi(outPath);
            QDir().mkpath(fi.absolutePath());
            if (QFile::exists(outPath)) {
                QFile::remove(outPath);
            }
            QFile::copy(contentPath, outPath);
            break;
        }
    }
}
