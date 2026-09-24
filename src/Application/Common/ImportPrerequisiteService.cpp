#include <QCoreApplication>
#include "Application/Common/ImportPrerequisiteService.h"

#include <QDir>
#include "Application/Environment/VpkSignatureLeaseService.h"
#include "Application/Package/VpkIndexService.h"
#include "Domain/Game/GameInstallationResolver.h"
#include "Domain/Tool/Cs2PathLayout.h"
#include "Core/Error/ErrorCode.h"

namespace Application::Common {

ImportPrerequisiteService::ImportPrerequisiteService(
    std::shared_ptr<Environment::VpkSignatureLeaseService> leaseService,
    std::shared_ptr<Package::VpkIndexService> vpkIndexService)
    : m_leaseService(std::move(leaseService))
    , m_vpkIndexService(std::move(vpkIndexService))
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

std::shared_ptr<Package::VpkIndexService> ImportPrerequisiteService::vpkIndexService() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_vpkIndexService;
}

void ImportPrerequisiteService::setVpkIndexService(
    std::shared_ptr<Package::VpkIndexService> vpkIndexService) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_vpkIndexService = std::move(vpkIndexService);
}

Core::Result<ValidatedBaseImport> ImportPrerequisiteService::prepare(
    const BaseImportRequest& request,
    const Workflow::Common::ImportContext& context) const
{
    // Step 1: Cancellation check
    if (context.isCancelled()) {
        return Core::Result<ValidatedBaseImport>::cancelled(
            QCoreApplication::translate("ImportPrerequisiteService", "Import cancelled before start"));
    }

    // Step 2: Validate Source 1 directory
    const QString trimmedS1Dir = request.source1GameDir.trimmed();
    if (trimmedS1Dir.isEmpty()) {
        return Core::Result<ValidatedBaseImport>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("ImportPrerequisiteService", "Source 1 game directory cannot be empty"));
    }
    if (!QDir(trimmedS1Dir).exists()) {
        return Core::Result<ValidatedBaseImport>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("ImportPrerequisiteService", "Source 1 game directory does not exist"),
            trimmedS1Dir);
    }

    // Resolve Source 1 gameinfo directory
    QString resolvedS1GameInfoDir;
    const QString trimmedReqGiDir = request.s1GameInfoDir.trimmed();
    if (!trimmedReqGiDir.isEmpty() && QDir(trimmedReqGiDir).exists()) {
        resolvedS1GameInfoDir = trimmedReqGiDir;
    } else {
        const Core::Path::FilesystemPath s1Path(trimmedS1Dir);
        if ((s1Path / QStringLiteral("gameinfo.txt")).exists()) {
            resolvedS1GameInfoDir = trimmedS1Dir;
        } else {
            auto inspectRes = Domain::Game::GameInstallationResolver::inspectGameInfo(s1Path);
            if (inspectRes.isSuccess() && inspectRes.value().modDirectory().isValid() && inspectRes.value().modDirectory().exists()) {
                resolvedS1GameInfoDir = inspectRes.value().modDirectory().toString();
            } else {
                resolvedS1GameInfoDir = trimmedS1Dir;
            }
        }
    }

    // Step 3: Validate CS2 base directory
    const QString trimmedCs2Dir = request.cs2BaseDir.trimmed();
    if (trimmedCs2Dir.isEmpty()) {
        return Core::Result<ValidatedBaseImport>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("ImportPrerequisiteService", "CS2 base directory cannot be empty"));
    }
    if (!QDir(trimmedCs2Dir).exists()) {
        return Core::Result<ValidatedBaseImport>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("ImportPrerequisiteService", "CS2 base directory does not exist"),
            trimmedCs2Dir);
    }

    // Step 4: Validate target addon name
    const QString trimmedAddon = request.addonName.trimmed();
    if (trimmedAddon.isEmpty()) {
        return Core::Result<ValidatedBaseImport>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("ImportPrerequisiteService", "Target addon name cannot be empty"));
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
            context.warning(QCoreApplication::translate("ImportPrerequisiteService", "Could not acquire vpk.signatures lease at '%1': %2")
                .arg(trimmedCs2Dir, leaseRes.message()));
        } else {
            context.info(QCoreApplication::translate("ImportPrerequisiteService", "Acquired vpk.signatures exclusive lease"));
        }
    }

    // Step 6: Coordinate VpkIndexService
    std::shared_ptr<Package::VpkIndexService> vpkService;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        vpkService = m_vpkIndexService;
    }

    std::shared_ptr<const Domain::Package::VpkIndex> vpkIndex;
    std::shared_ptr<const Domain::Package::VpkIndex> cs2Index;

    const Core::Path::FilesystemPath cs2Path(trimmedCs2Dir);

    if (vpkService) {
        context.info(QCoreApplication::translate("ImportPrerequisiteService", "Ensuring VPK asset indices..."));

        // 1. Ensure CS2 Index
        auto cs2IndexRes = vpkService->ensureCs2IndexFromGameInfoSync(cs2Path, context.token());
        if (cs2IndexRes.isSuccess()) {
            cs2Index = cs2IndexRes.value();
        } else {
            context.warning(QCoreApplication::translate("ImportPrerequisiteService", "Could not load CS2 VPK index: %1")
                                .arg(cs2IndexRes.message()));
        }

        // 2. Ensure Source 1 Index
        QString s1GameId = request.gameId.trimmed();
        if (s1GameId.isEmpty()) {
            s1GameId = vpkService->activeSource1GameId();
        }
        if (s1GameId.isEmpty()) {
            s1GameId = QStringLiteral("custom");
        }

        auto s1IndexRes = vpkService->ensureSource1IndexFromGameInfoSync(
            s1GameId, Core::Path::FilesystemPath(resolvedS1GameInfoDir), context.token());
        if (s1IndexRes.isSuccess()) {
            vpkIndex = s1IndexRes.value();
        } else {
            context.warning(QCoreApplication::translate("ImportPrerequisiteService", "Could not load Source 1 VPK index: %1")
                                .arg(s1IndexRes.message()));
        }
    }

    // Tools are fixed relative to the CS2 root directory and automatically generated
    const Core::Path::FilesystemPath s1ImportExe = Domain::Tool::Cs2PathLayout::source1ImportExecutable(cs2Path);
    const Core::Path::FilesystemPath rcExe = Domain::Tool::Cs2PathLayout::resourceCompilerExecutable(cs2Path);

    return Core::Result<ValidatedBaseImport>::success(
        ValidatedBaseImport{trimmedS1Dir, resolvedS1GameInfoDir, trimmedCs2Dir, trimmedAddon, s1ImportExe, rcExe, std::move(vpkIndex), std::move(cs2Index)});
}

} // namespace Application::Common
