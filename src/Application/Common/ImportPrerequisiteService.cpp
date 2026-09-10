#include "Application/Common/ImportPrerequisiteService.h"

#include <QDir>
#include "Application/Environment/VpkSignatureLeaseService.h"
#include "Domain/Tool/Cs2PathLayout.h"
#include "Core/Error/ErrorCode.h"

namespace Application::Common {

ImportPrerequisiteService::ImportPrerequisiteService(
    std::shared_ptr<Environment::VpkSignatureLeaseService> leaseService)
    : m_leaseService(std::move(leaseService))
{
}

std::shared_ptr<Environment::VpkSignatureLeaseService> ImportPrerequisiteService::leaseService() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_leaseService;
}

void ImportPrerequisiteService::setLeaseService(
    std::shared_ptr<Environment::VpkSignatureLeaseService> leaseService) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_leaseService = std::move(leaseService);
}

Core::Result<ValidatedBaseImport> ImportPrerequisiteService::prepare(
    const BaseImportRequest& request,
    const Workflow::Common::ImportContext& context) const
{
    // Step 1: Cancellation check
    if (context.isCancelled()) {
        return Core::Result<ValidatedBaseImport>::cancelled(
            QStringLiteral("Import cancelled before start"));
    }

    // Step 2: Validate Source 1 directory
    const QString trimmedS1Dir = request.source1GameDir.trimmed();
    if (trimmedS1Dir.isEmpty()) {
        return Core::Result<ValidatedBaseImport>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QStringLiteral("Source 1 game directory cannot be empty"));
    }
    if (!QDir(trimmedS1Dir).exists()) {
        return Core::Result<ValidatedBaseImport>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QStringLiteral("Source 1 game directory does not exist"),
            trimmedS1Dir);
    }

    // Step 3: Validate CS2 base directory
    const QString trimmedCs2Dir = request.cs2BaseDir.trimmed();
    if (trimmedCs2Dir.isEmpty()) {
        return Core::Result<ValidatedBaseImport>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QStringLiteral("CS2 base directory cannot be empty"));
    }
    if (!QDir(trimmedCs2Dir).exists()) {
        return Core::Result<ValidatedBaseImport>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QStringLiteral("CS2 base directory does not exist"),
            trimmedCs2Dir);
    }

    // Step 4: Validate target addon name
    const QString trimmedAddon = request.addonName.trimmed();
    if (trimmedAddon.isEmpty()) {
        return Core::Result<ValidatedBaseImport>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QStringLiteral("Target addon name cannot be empty"));
    }

    // Step 5: Coordinate VpkSignatureLeaseService
    std::shared_ptr<Environment::VpkSignatureLeaseService> leaseService;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        leaseService = m_leaseService;
    }
    if (leaseService) {
        auto leaseRes = leaseService->acquireLease(trimmedCs2Dir);
        if (!leaseRes.isSuccess()) {
            context.warning(QStringLiteral("Could not acquire vpk.signatures lease at '%1': %2")
                .arg(trimmedCs2Dir, leaseRes.message()));
        } else {
            context.info(QStringLiteral("Acquired vpk.signatures exclusive lease"));
        }
    }

    // Tools are fixed relative to the CS2 root directory and automatically generated
    const Core::Path::FilesystemPath cs2Path(trimmedCs2Dir);
    const Core::Path::FilesystemPath s1ImportExe = Domain::Tool::Cs2PathLayout::source1ImportExecutable(cs2Path);
    const Core::Path::FilesystemPath rcExe = Domain::Tool::Cs2PathLayout::resourceCompilerExecutable(cs2Path);

    return Core::Result<ValidatedBaseImport>::success(
        ValidatedBaseImport{trimmedS1Dir, trimmedCs2Dir, trimmedAddon, s1ImportExe, rcExe});
}

} // namespace Application::Common
