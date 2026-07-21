#include "ParticleImporter.h"
#include "Miscellaneous.h"
#include <QFileInfo>
#include <QDir>

bool ParticleImporter::Run(const QString& pcfPath) {
    if (Miscellaneous::CanceLImport) return false;
    Miscellaneous::Log("Starting standalone Particle Import process.");

    QString fullPcfPath = pcfPath;
    fullPcfPath.replace('/', '\\');

    Miscellaneous::Log("Input PCF path: " + fullPcfPath);

    // Build options for source1import.exe
    QString extraOpts;
    const auto& opts = Miscellaneous::GetOptions();
    if (opts.particleAllowDepthBlend) {
        extraOpts += " -particle_allow_depth_blend";
    }
    if (opts.particleDisableDiffuse) {
        extraOpts += " -particle_disable_diffuse";
    }

    // Command: source1import.exe -retail -nop4 -nop4sync -src1gameinfodir <s1gamedir> -s2addon <addonName> -game csgo <extraOpts> <pcfPath>
    QString importCmd = "\"" + opts.cs2Basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"" + opts.s1gamedir + "\" -s2addon " + opts.addonName + " -game csgo" + extraOpts + " \"" + fullPcfPath + "\"";

    Miscellaneous::RunCommandSync(importCmd);

    if (Miscellaneous::CanceLImport) return false;

    Miscellaneous::Log("Particle Import process complete.");
    return true;
}
