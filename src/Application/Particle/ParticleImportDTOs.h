#pragma once

#include <QString>
#include <QStringList>
#include "Application/Common/BaseImportDTOs.h"

namespace Application::Particle {

/**
 * @brief User-level request DTO for particle import and compilation.
 */
struct ParticleImportRequest : public Common::BaseImportRequest {
    QStringList sourcePcfPaths;
    bool allowDepthBlend = false;
    bool disableDiffuse = false;
    bool isCsgo = false;

    QString sourcePcfPath() const {
        return sourcePcfPaths.isEmpty() ? QString() : sourcePcfPaths.first();
    }

    bool operator==(const ParticleImportRequest& other) const = default;
};

/**
 * @brief User-level outcome DTO of a particle import and compilation operation.
 */
struct ParticleImportResult {
    bool succeeded = false;
    QStringList generatedVpcfFiles;
    QStringList compiledVpcfCFiles;
    QStringList failedPcfFiles;
    int totalConverted = 0;
    int totalCompiled = 0;
    int totalFailed = 0;

    bool operator==(const ParticleImportResult& other) const = default;
};

} // namespace Application::Particle
