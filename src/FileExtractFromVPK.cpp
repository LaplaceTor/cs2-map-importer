#include "FileExtractFromVPK.h"
#include "miscellaneous.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>

void FileExtractFromVPK::ExtractModelFromVPK(const QString& filepath, const MapImporter::Options& options) {
    QFileInfo fi(filepath);
    QString basePath = fi.path() + "/" + fi.baseName();
    QString vpkName = (options.s1gamename == "css") ? "cstrike_pak_dir.vpk" : "pak01_dir.vpk";
    QString vpkPath = QDir(options.s1gamedir).filePath(vpkName);
    QString contentPath = QDir(options.s1contentdir).filePath(filepath);
    QString outPath = QDir(options.s1gamedir).filePath(filepath);

    QString cmd = "\"bin\\vpkeditcli.exe\" -e \"" + filepath + "\" \"" + vpkPath + "\" -o \"" + contentPath + "\"";
    cmd = cmd.replace("/", "\\");
    Miscellaneous::run_command_sync(cmd);

    if (QFile::exists(contentPath)) {
        QFile::copy(contentPath, outPath);

        QStringList extlist = {"vvd","phy","sw.vtx","dx80.vtx","dx90.vtx","ani"};
        for (const QString& ext : extlist) {
            QString target = basePath + "." + ext;
            contentPath = QDir(options.s1contentdir).filePath(target);
            cmd = "\"bin\\vpkeditcli.exe\" -e \"" + target + "\" \"" + vpkPath + "\" -o \"" + contentPath + "\"";
            cmd = cmd.replace("/", "\\");
            Miscellaneous::run_command_sync(cmd);

            outPath = QDir(options.s1gamedir).filePath(target);
            if (QFile::exists(contentPath)) {
                QFile::copy(contentPath, outPath);
            }
        }
    }
}

void FileExtractFromVPK::ExtractParticleFromVPK(const QString& filepath, const MapImporter::Options& options) {
    QString vpkName = (options.s1gamename == "css") ? "cstrike_pak_dir.vpk" : "pak01_dir.vpk";
    QString vpkPath = QDir(options.s1gamedir).filePath(vpkName);
    QString contentPath = QDir(options.s1contentdir).filePath(filepath);
    QString outPath = QDir(options.s1gamedir).filePath(filepath);

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
    }
}

void FileExtractFromVPK::ExtractSoundFromVPK(const QString& filepath, const MapImporter::Options& options) {
    QString vpkName = (options.s1gamename == "css") ? "cstrike_pak_dir.vpk" : "pak01_dir.vpk";
    QString vpkPath = QDir(options.s1gamedir).filePath(vpkName);
    QString contentPath = QDir(options.s1contentdir).filePath(filepath);
    QString outPath = QDir(options.s1gamedir).filePath(filepath);

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
    }
}
