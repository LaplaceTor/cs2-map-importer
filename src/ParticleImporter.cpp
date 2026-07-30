#include "ParticleImporter.h"
#include "Miscellaneous.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>

bool ParticleImporter::Run(const QString& pcfPath) {
    if (Miscellaneous::CanceLImport) return false;
    Miscellaneous::Log("Starting standalone Particle Import process.");

    QString fullPcfPath = QDir::toNativeSeparators(pcfPath);

    const auto& opts = Miscellaneous::GetOptions();
    QString filename = QFileInfo(fullPcfPath).fileName();

    // Create destination particles directory in s1gamedir
    QString destDir = QDir::toNativeSeparators(opts.s1gamedir + "/particles");
    QDir().mkpath(destDir);

    QString destPcfPath = QDir::toNativeSeparators(QDir(destDir).filePath(filename));

    bool alreadyInDest = false;
    QString lowerPcfPath = QDir::fromNativeSeparators(fullPcfPath).toLower();
    QString lowerDestDir = QDir::fromNativeSeparators(destDir).toLower();
    if (!lowerDestDir.endsWith('/')) {
        lowerDestDir += '/';
    }

    if (lowerPcfPath.startsWith(lowerDestDir)) {
        alreadyInDest = true;
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
    QStringList arguments = {
        "-retail",
        "-nop4",
        "-nop4sync",
        "-src1gameinfodir",
        opts.s1gamedir,
        "-s2addon",
        opts.addonName,
        "-game",
        "csgo"
    };
    if (opts.particleAllowDepthBlend) {
        arguments << "-particle_allow_depth_blend";
    }
    if (opts.particleDisableDiffuse) {
        arguments << "-particle_disable_diffuse";
    }
    arguments << destPcfPath;

    Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_SOURCE1IMPORT, arguments, false, nullptr, false, Miscellaneous::GetOptions().s1GameType == "csgo");

    if (Miscellaneous::CanceLImport) return false;

    Miscellaneous::Log("Particle Import process complete.");
    return true;
}
