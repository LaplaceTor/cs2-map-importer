#include "FileExtractFromVPK.h"
#include "Miscellaneous.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

static QStringList GetVpkList() {
    if (Miscellaneous::GetOptions().s1gamename == "css") {
        return {
            "cstrike/cstrike_pak_dir.vpk",
            "hl2/hl2_textures_dir.vpk",
            "hl2/hl2_sound_misc_dir.vpk",
            "hl2/hl2_misc_dir.vpk",
            "hl2/hl2_sound_vo_english_dir.vpk"
        };
    } else if (Miscellaneous::GetOptions().s1gamename == "hl2") {
        return {
            "hl2/hl2_sound_vo_english_dir.vpk",
            "hl2/hl2_pak_dir.vpk",
            "hl2/hl2_textures_dir.vpk",
            "hl2/hl2_sound_misc_dir.vpk",
            "hl2/hl2_misc_dir.vpk"
        };
    } else if (Miscellaneous::GetOptions().s1gamename == "l4d") {
        return {
            "left4dead/pak01_dir.vpk",
            "left4dead_dlc3/pak01_dir.vpk"
        };
    } else if (Miscellaneous::GetOptions().s1gamename == "l4d2") {
        return {
            "left4dead2/pak01_dir.vpk",
            "left4dead2_dlc1/pak01_dir.vpk",
            "left4dead2_dlc2/pak01_dir.vpk",
            "left4dead2_dlc3/pak01_dir.vpk",
            "update/pak01_dir.vpk"
        };
    } else if (Miscellaneous::GetOptions().s1gamename == "portal") {
        return {
            "portal/portal_pak_dir.vpk",
            "hl2/hl2_sound_vo_english_dir.vpk",
            "hl2/hl2_textures_dir.vpk",
            "hl2/hl2_sound_misc_dir.vpk",
            "hl2/hl2_misc_dir.vpk"
        };
    } else if (Miscellaneous::GetOptions().s1gamename == "portal2") {
        return {
            "portal2/pak01_dir.vpk",
            "portal2_dlc1/pak01_dir.vpk",
            "portal2_dlc2/pak01_dir.vpk"
        };
    } else if (Miscellaneous::GetOptions().s1gamename == "tf2") {
        return {
            "tf/tf2_textures_dir.vpk",
            "tf/tf2_sound_vo_english_dir.vpk",
            "tf/tf2_sound_misc_dir.vpk",
            "tf/tf2_misc_dir.vpk",
            "hl2/hl2_sound_vo_english_dir.vpk",
            "hl2/hl2_textures_dir.vpk",
            "hl2/hl2_sound_misc_dir.vpk",
            "hl2/hl2_misc_dir.vpk"
        };
    } else if (Miscellaneous::GetOptions().s1gamename == "gmod") {
        return {
            "garrysmod/garrysmod_dir.vpk",
            "sourceengine/hl2_textures_dir.vpk",
            "sourceengine/hl2_sound_vo_english_dir.vpk",
            "sourceengine/hl2_sound_misc_dir.vpk",
            "sourceengine/hl2_misc_dir.vpk"
        };
    } else {
        return { "csgo/pak01_dir.vpk" };
    }
}

void FileExtractFromVPK::ExtractModel(const QString& filepath) {
    QFileInfo fi(filepath);
    QString basePath = fi.path() + "/" + fi.baseName();
    QString contentPath = QDir(Miscellaneous::GetOptions().s1contentdir).filePath(filepath);
    QString outPath = QDir(Miscellaneous::GetOptions().s1gamedir).filePath(filepath);

    QString baseFolder = QFileInfo(Miscellaneous::GetOptions().s1gamedir).dir().absolutePath();
    QStringList vpkList = GetVpkList();
    QString foundVpkPath;

    for (const QString& vpkRel : vpkList) {
        QString vpkPath = QDir(baseFolder).filePath(vpkRel);

        if (QFile::exists(contentPath)) {
            QFile::remove(contentPath);
        }

        QString cmd = "\"bin\\vpkeditcli.exe\" -e \"" + filepath + "\" \"" + vpkPath + "\" -o \"" + contentPath + "\"";
        cmd = cmd.replace("/", "\\");
        Miscellaneous::RunCommandSync(cmd);

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
            contentPath = QDir(Miscellaneous::GetOptions().s1contentdir).filePath(target);

            if (QFile::exists(contentPath)) {
                QFile::remove(contentPath);
            }

            QString cmd = "\"bin\\vpkeditcli.exe\" -e \"" + target + "\" \"" + foundVpkPath + "\" -o \"" + contentPath + "\"";
            cmd = cmd.replace("/", "\\");
            Miscellaneous::RunCommandSync(cmd);

            outPath = QDir(Miscellaneous::GetOptions().s1gamedir).filePath(target);
            if (QFile::exists(contentPath)) {
                QFile::copy(contentPath, outPath);
            }
        }
    }
}

void FileExtractFromVPK::ExtractParticle(const QString& filepath) {
    QString contentPath = QDir(Miscellaneous::GetOptions().s1contentdir).filePath(filepath);
    QString outPath = QDir(Miscellaneous::GetOptions().s1gamedir).filePath(filepath);
    QString baseFolder = QFileInfo(Miscellaneous::GetOptions().s1gamedir).dir().absolutePath();
    QStringList vpkList = GetVpkList();

    for (const QString& vpkRel : vpkList) {
        QString vpkPath = QDir(baseFolder).filePath(vpkRel);

        if (QFile::exists(contentPath)) {
            QFile::remove(contentPath);
        }

        QString cmd = "\"bin\\vpkeditcli.exe\" -e \"" + filepath + "\" \"" + vpkPath + "\" -o \"" + contentPath + "\"";
        cmd = cmd.replace("/", "\\");
        Miscellaneous::RunCommandSync(cmd);

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

void FileExtractFromVPK::ExtractSound(const QString& filepath) {
    QString contentPath = QDir(Miscellaneous::GetOptions().s1contentdir).filePath(filepath);
    QString outPath = QDir(Miscellaneous::GetOptions().s1gamedir).filePath(filepath);
    QString baseFolder = QFileInfo(Miscellaneous::GetOptions().s1gamedir).dir().absolutePath();
    QStringList vpkList = GetVpkList();

    for (const QString& vpkRel : vpkList) {
        QString vpkPath = QDir(baseFolder).filePath(vpkRel);

        if (QFile::exists(contentPath)) {
            QFile::remove(contentPath);
        }

        QString cmd = "\"bin\\vpkeditcli.exe\" -e \"" + filepath + "\" \"" + vpkPath + "\" -o \"" + contentPath + "\"";
        cmd = cmd.replace("/", "\\");
        Miscellaneous::RunCommandSync(cmd);

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
