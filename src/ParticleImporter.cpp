#include "ParticleImporter.h"
#include "Miscellaneous.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>

bool ParticleImporter::Run(const QString& pcfPath) {
    if (Miscellaneous::CanceLImport) return false;
    Miscellaneous::Log("Starting standalone Particle Import process.");

    QString fullPcfPath = pcfPath;
    fullPcfPath.replace('/', '\\');

    const auto& opts = Miscellaneous::GetOptions();
    QString filename = QFileInfo(fullPcfPath).fileName();

    // Create destination particles directory in s1gamedir
    QString destDir = opts.s1gamedir + "\\particles";
    destDir.replace('/', '\\');
    QDir().mkpath(destDir);

    QString destPcfPath = destDir + "\\" + filename;
    destPcfPath.replace('/', '\\');

    QFileInfo sourceInfo(fullPcfPath);
    QFileInfo destInfo(destPcfPath);

    bool alreadyInDest = false;
    if (sourceInfo.exists() && destInfo.exists()) {
        if (sourceInfo.canonicalFilePath().toLower() == destInfo.canonicalFilePath().toLower()) {
            alreadyInDest = true;
        }
    } else {
        if (QDir::toNativeSeparators(fullPcfPath).toLower() == QDir::toNativeSeparators(destPcfPath).toLower()) {
            alreadyInDest = true;
        }
    }

    if (alreadyInDest) {
        Miscellaneous::Log("PCF file is already inside Source 1 particles folder. Skipping copy progress.");
    } else {
        Miscellaneous::Log("Copying PCF to Source 1 particles folder...");
        Miscellaneous::Log("From: " + fullPcfPath);
        Miscellaneous::Log("To: " + destPcfPath);

        if (QFile::exists(destPcfPath)) {
            QFile::remove(destPcfPath);
        }
        if (!QFile::copy(fullPcfPath, destPcfPath)) {
            Miscellaneous::Log("Error: Failed to copy PCF file to Source 1 particles folder!");
            return false;
        }
    }

    // Build options for source1import.exe
    QString extraOpts;
    if (opts.particleAllowDepthBlend) {
        extraOpts += " -particle_allow_depth_blend";
    }
    if (opts.particleDisableDiffuse) {
        extraOpts += " -particle_disable_diffuse";
    }

    // Command: source1import.exe -retail -nop4 -nop4sync -src1gameinfodir <s1gamedir> -s2addon <addonName> -game csgo <extraOpts> <destPcfPath>
    QString importCmd = "\"" + opts.cs2Basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"" + opts.s1gamedir + "\" -s2addon " + opts.addonName + " -game csgo" + extraOpts + " \"" + destPcfPath + "\"";

    Miscellaneous::RunCommandSync(importCmd);

    if (Miscellaneous::CanceLImport) return false;

    Miscellaneous::Log("Particle Import process complete.");
    return true;
}
