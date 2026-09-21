#pragma once

#include <vector>
#include "Core/Path/FilesystemPath.h"
#include <QString>
#include <QStringList>

namespace Workflow::Particle {

/**
 * @brief Input options for running the particle import and compilation workflow.
 */
struct ParticleImportOptions {
    Core::Path::FilesystemPath source1GameDir;
    Core::Path::FilesystemPath s1GameInfoDir;
    Core::Path::FilesystemPath cs2BaseDir;
    QString addonName;
    std::vector<Core::Path::FilesystemPath> sourcePcfPaths;
    bool allowDepthBlend = false;
    bool disableDiffuse = false;
    bool isCsgo = false;
    Core::Path::FilesystemPath source1ImportExe;
    Core::Path::FilesystemPath resourceCompilerExe;
    // Per-tool timeout override in milliseconds; 0 keeps the tool default (120 s).
    int toolTimeoutMs = 0;

    Core::Path::FilesystemPath sourcePcfPath() const {
        return sourcePcfPaths.empty() ? Core::Path::FilesystemPath{} : sourcePcfPaths.front();
    }

    bool operator==(const ParticleImportOptions& other) const = default;
};

/**
 * @brief Aggregated outcome of a ParticleImportWorkflow execution.
 */
struct ParticleImportWorkflowResult {
    QStringList generatedVpcfFiles;
    QStringList compiledVpcfCFiles;
    QStringList failedPcfFiles;
    int totalConverted = 0;
    int totalCompiled = 0;
    int totalFailed = 0;

    bool operator==(const ParticleImportWorkflowResult& other) const = default;
};

} // namespace Workflow::Particle
