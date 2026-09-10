#pragma once

#include <memory>
#include <mutex>
#include <QString>

#include "Application/Common/BaseImportDTOs.h"
#include "Workflow/Common/ImportContext.h"
#include "Core/Result/Result.h"

namespace Application::Environment {
class VpkSignatureLeaseService;
}

namespace Application::Common {

/**
 * @brief Common application service responsible for verifying environment preconditions
 *        and acquiring system resources (e.g. vpk.signatures lease) prior to asset import.
 *
 * Shared across Map, Model, and Particle import pipelines.
 */
class ImportPrerequisiteService {
public:
    explicit ImportPrerequisiteService(
        std::shared_ptr<Environment::VpkSignatureLeaseService> leaseService = nullptr);
    virtual ~ImportPrerequisiteService() = default;

    /**
     * @brief Validates common base import parameters and acquires required leases.
     *
     * Performs:
     * 1. Cancellation check on context.
     * 2. Path validation on source1GameDir and cs2BaseDir (non-empty, exists on disk).
     * 3. Addon name validation (non-empty).
     * 4. Exclusive lease acquisition on cs2BaseDir/vpk.signatures if leaseService is configured.
     *
     * @return Success with ValidatedBaseImport, or structured Failure/Cancelled result.
     */
    Core::Result<ValidatedBaseImport> prepare(
        const BaseImportRequest& request,
        const Workflow::Common::ImportContext& context = Workflow::Common::ImportContext{}) const;

    std::shared_ptr<Environment::VpkSignatureLeaseService> leaseService() const noexcept;
    void setLeaseService(std::shared_ptr<Environment::VpkSignatureLeaseService> leaseService) noexcept;

private:
    mutable std::mutex m_mutex;
    std::shared_ptr<Environment::VpkSignatureLeaseService> m_leaseService;
};

} // namespace Application::Common
