#include <QCoreApplication>
#include "Application/Environment/GameEnvironmentService.h"
#include "Application/Environment/GameDetectService.h"
#include "Application/Environment/SteamService.h"
#include "Application/Async/AsyncTaskRunner.h"
#include "Domain/Game/GameRegistry.h"
#include "Domain/Game/GameType.h"
#include "Domain/Game/GameInstallationResolver.h"
#include "Core/Path/PathUtils.h"
#include <QUrl>
#include <utility>

namespace Application::Environment {

namespace {

Domain::Game::GameType resolveGameTypeFromName(const QString& typeName)
{
    const QString lower = typeName.trimmed().toLower();
    if (lower == QStringLiteral("cs: global offensive") || lower == QStringLiteral("cs:go") ||
        lower == QStringLiteral("counter-strike: global offensive") ||
        lower == QStringLiteral("counter-strike global offensive") ||
        lower == QStringLiteral("csgo")) {
        return Domain::Game::GameType::CSGO;
    }
    if (lower == QStringLiteral("cs: source") || lower == QStringLiteral("cs:s") ||
        lower == QStringLiteral("counter-strike: source") ||
        lower == QStringLiteral("counter-strike source") ||
        lower == QStringLiteral("css")) {
        return Domain::Game::GameType::CSS;
    }
    if (lower == QStringLiteral("half-life 2") || lower == QStringLiteral("hl2")) {
        return Domain::Game::GameType::HL2;
    }
    if (lower == QStringLiteral("left 4 dead") || lower == QStringLiteral("l4d")) {
        return Domain::Game::GameType::L4D;
    }
    if (lower == QStringLiteral("left 4 dead 2") || lower == QStringLiteral("l4d2")) {
        return Domain::Game::GameType::L4D2;
    }
    if (lower == QStringLiteral("portal")) {
        return Domain::Game::GameType::Portal;
    }
    if (lower == QStringLiteral("portal 2") || lower == QStringLiteral("portal2")) {
        return Domain::Game::GameType::Portal2;
    }
    if (lower == QStringLiteral("team fortress 2") || lower == QStringLiteral("tf2")) {
        return Domain::Game::GameType::TF2;
    }
    if (lower == QStringLiteral("garry's mod") || lower == QStringLiteral("garrysmod") || lower == QStringLiteral("gmod")) {
        return Domain::Game::GameType::GMod;
    }
    if (lower == QStringLiteral("black mesa") || lower == QStringLiteral("blackmesa")) {
        return Domain::Game::GameType::BlackMesa;
    }
    if (lower == QStringLiteral("counter-strike 2") || lower == QStringLiteral("cs2")) {
        return Domain::Game::GameType::CS2;
    }
    if (lower == QStringLiteral("custom") || lower == QStringLiteral("custom game") ||
        lower == QStringLiteral("other") || lower == QStringLiteral("other source 1 game")) {
        return Domain::Game::GameType::Custom;
    }
    return Domain::Game::GameRegistry::stringToGameType(typeName);
}

} // anonymous namespace

GameEnvironmentService::GameEnvironmentService(
    std::shared_ptr<VpkSignatureLeaseService> leaseService,
    QObject* parent)
    : QObject(parent)
    , m_leaseService(leaseService ? std::move(leaseService) : std::make_shared<VpkSignatureLeaseService>())
{
    hookLeaseSignals();
}

void GameEnvironmentService::hookLeaseSignals()
{
    if (m_leaseService) {
        connect(m_leaseService.get(), &VpkSignatureLeaseService::leaseStateChanged,
                this, &GameEnvironmentService::vpkLeaseStateChanged, Qt::UniqueConnection);
        connect(m_leaseService.get(), &VpkSignatureLeaseService::leaseStatusChanged,
                this, &GameEnvironmentService::vpkLeaseStatusChanged, Qt::UniqueConnection);
    }
}

void GameEnvironmentService::setVpkSignatureLeaseService(std::shared_ptr<VpkSignatureLeaseService> service)
{
    if (m_leaseService) {
        disconnect(m_leaseService.get(), nullptr, this, nullptr);
    }

    m_leaseService = service ? std::move(service) : std::make_shared<VpkSignatureLeaseService>();
    hookLeaseSignals();
}

bool GameEnvironmentService::isVpkLeaseHeld() const noexcept
{
    return m_leaseService ? m_leaseService->isLeaseHeld() : false;
}

VpkSignatureLeaseStatus GameEnvironmentService::vpkLeaseStatus() const noexcept
{
    return m_leaseService ? m_leaseService->currentStatus() : VpkSignatureLeaseStatus::Inactive;
}

QString GameEnvironmentService::leasedVpkFilePath() const
{
    return m_leaseService ? m_leaseService->leasedFilePath() : QString();
}

QStringList GameEnvironmentService::s1GameTypes() const
{
    return {
        QStringLiteral("CSGO"),
        QStringLiteral("CS: Source"),
        QStringLiteral("Half-Life 2"),
        QStringLiteral("Left 4 Dead"),
        QStringLiteral("Left 4 Dead 2"),
        QStringLiteral("Portal"),
        QStringLiteral("Portal 2"),
        QStringLiteral("Team Fortress 2"),
        QStringLiteral("Garry's Mod"),
        QStringLiteral("Black Mesa"),
        QStringLiteral("Custom")
    };
}

QStringList GameEnvironmentService::s2GameTypes() const
{
    return {
        QStringLiteral("Counter-Strike 2")
    };
}

QString GameEnvironmentService::cleanPath(const QString& pathOrUrl)
{
    QString trimmed = pathOrUrl.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    if (trimmed.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)) {
        // Normalize malformed Windows file URLs with only 2 slashes, e.g. file://C:/...
        // where index 7 is the drive letter and index 8 is ':'.
        // This avoids QUrl interpreting the drive letter as a hostname.
        if (trimmed.size() >= 9 &&
            (trimmed.at(5) == QLatin1Char('/') || trimmed.at(5) == QLatin1Char('\\')) &&
            (trimmed.at(6) == QLatin1Char('/') || trimmed.at(6) == QLatin1Char('\\')) &&
            trimmed.at(7).isLetter() &&
            trimmed.at(8) == QLatin1Char(':')) {
            trimmed = QStringLiteral("file:///") + trimmed.mid(7);
        }

        QUrl url(trimmed);
        const QString local = url.toLocalFile();
        if (!local.isEmpty()) {
            trimmed = local;
        }
    }
    return Core::Path::PathUtils::normalize(trimmed);
}

void GameEnvironmentService::detectEnvironmentAsync(
    QObject* context,
    std::function<void(const Core::Result<DetectionResult>&)> callback,
    const QString& customSteamPath)
{
    GameDetectService::detectEnvironmentAsync(context, std::move(callback), customSteamPath);
}

Core::Result<DetectionResult> GameEnvironmentService::detectEnvironment(
    const QString& customSteamPath)
{
    return GameDetectService::detectEnvironment(customSteamPath);
}

void GameEnvironmentService::validateSource1FolderAsync(
    const QString& typeName,
    const QString& pathOrUrl,
    QObject* context,
    std::function<void(const Core::Result<GameInstallationInfo>&)> callback)
{
    QString normalizedPath = cleanPath(pathOrUrl);
    Domain::Game::GameType type = resolveGameTypeFromName(typeName);
    QString effectiveName = typeName.trimmed().isEmpty() ? QStringLiteral("Source 1") : typeName.trimmed();
    QString taskName = QCoreApplication::translate("GameEnvironmentService", "Validate %1").arg(effectiveName);

    (void)Application::Async::AsyncTaskRunner::runSystemTask<GameInstallationInfo>(
        taskName,
        context,
        [type, normalizedPath, effectiveName](const Application::Async::SystemTaskLog& sysLog) -> Core::Result<GameInstallationInfo> {
            sysLog.info(QStringLiteral("Starting validation for %1 at: %2").arg(effectiveName, normalizedPath));
            if (normalizedPath.isEmpty()) {
                const QString errMsg = QStringLiteral("Target path is empty, validation skipped");
                sysLog.warning(errMsg);
                return Core::Result<GameInstallationInfo>::skipped(errMsg);
            }

            Core::Path::FilesystemPath fsPath(normalizedPath);
            auto resolvedResult = (type == Domain::Game::GameType::Custom)
                ? Domain::Game::GameInstallationResolver::inspectGameInfo(fsPath)
                : Domain::Game::GameInstallationResolver::resolveSource1(type, fsPath);

            if (resolvedResult.isSuccess()) {
                auto info = toInstallationInfo(resolvedResult.value());
                sysLog.info(QStringLiteral("Validation completed successfully: %1").arg(info.displayName));
                return Core::Result<GameInstallationInfo>::success(std::move(info));
            }

            sysLog.error(QStringLiteral("Validation failed for %1 at: %2: %3").arg(effectiveName, normalizedPath, resolvedResult.message()));
            return Core::Result<GameInstallationInfo>::failure(resolvedResult.error(), resolvedResult.message());
        },
        std::move(callback));
}

Core::Result<GameInstallationInfo> GameEnvironmentService::validateSource1Folder(
    const QString& typeName,
    const QString& pathOrUrl)
{
    QString normalizedPath = cleanPath(pathOrUrl);
    if (normalizedPath.isEmpty()) {
        return Core::Result<GameInstallationInfo>::failure(Core::Error::ErrorCode::InvalidArgument, QCoreApplication::translate("GameEnvironmentService", "Target path is empty"));
    }

    Core::Path::FilesystemPath fsPath(normalizedPath);
    Domain::Game::GameType type = resolveGameTypeFromName(typeName);
    auto resolvedResult = (type == Domain::Game::GameType::Custom)
        ? Domain::Game::GameInstallationResolver::inspectGameInfo(fsPath)
        : Domain::Game::GameInstallationResolver::resolveSource1(type, fsPath);

    if (!resolvedResult.isSuccess()) {
        return Core::Result<GameInstallationInfo>::failure(resolvedResult.error(), QCoreApplication::translate("GameEnvironmentService", "Source 1 validation failed"));
    }
    return Core::Result<GameInstallationInfo>::success(toInstallationInfo(resolvedResult.value()));
}

void GameEnvironmentService::validateSource2FolderAsync(
    const QString& pathOrUrl,
    QObject* context,
    std::function<void(const Core::Result<GameInstallationInfo>&)> callback)
{
    QString normalizedPath = cleanPath(pathOrUrl);

    (void)Application::Async::AsyncTaskRunner::runSystemTask<GameInstallationInfo>(
        QCoreApplication::translate("GameEnvironmentService", "Validate Source 2"),
        context,
        [normalizedPath](const Application::Async::SystemTaskLog& sysLog) -> Core::Result<GameInstallationInfo> {
            sysLog.info(QStringLiteral("Starting Source 2 validation at: %1").arg(normalizedPath));
            if (normalizedPath.isEmpty()) {
                const QString errMsg = QStringLiteral("Target path is empty, validation skipped");
                sysLog.warning(errMsg);
                return Core::Result<GameInstallationInfo>::skipped(errMsg);
            }

            Core::Path::FilesystemPath fsPath(normalizedPath);
            auto resolvedResult = Domain::Game::GameInstallationResolver::resolveSource2(fsPath, Domain::Game::GameType::Unknown);
            if (resolvedResult.isSuccess()) {
                auto info = toInstallationInfo(resolvedResult.value());
                sysLog.info(QStringLiteral("Validation completed successfully: %1").arg(info.displayName));
                return Core::Result<GameInstallationInfo>::success(std::move(info));
            }

            sysLog.error(QStringLiteral("Validation failed for Source 2 at: %1: %2").arg(normalizedPath, resolvedResult.message()));
            return Core::Result<GameInstallationInfo>::failure(resolvedResult.error(), resolvedResult.message());
        },
        std::move(callback));
}

Core::Result<GameInstallationInfo> GameEnvironmentService::validateSource2Folder(
    const QString& pathOrUrl)
{
    QString normalizedPath = cleanPath(pathOrUrl);
    if (normalizedPath.isEmpty()) {
        return Core::Result<GameInstallationInfo>::failure(Core::Error::ErrorCode::InvalidArgument, QCoreApplication::translate("GameEnvironmentService", "Target path is empty"));
    }

    Core::Path::FilesystemPath fsPath(normalizedPath);
    auto resolvedResult = Domain::Game::GameInstallationResolver::resolveSource2(fsPath);
    if (!resolvedResult.isSuccess()) {
        return Core::Result<GameInstallationInfo>::failure(resolvedResult.error(), QCoreApplication::translate("GameEnvironmentService", "Source 2 validation failed"));
    }
    return Core::Result<GameInstallationInfo>::success(toInstallationInfo(resolvedResult.value()));
}

Core::Result<void> GameEnvironmentService::validateGameInSteam(const QString& typeName)
{
    Domain::Game::GameType type = resolveGameTypeFromName(typeName);
    return SteamService::validateGameFiles(type);
}

QStringList GameEnvironmentService::listSource2Addons(const QString& s2BasePath) const
{
    if (s2BasePath.isEmpty()) {
        return QStringList();
    }
    Core::Path::FilesystemPath fsPath(cleanPath(s2BasePath));
    return Domain::Game::GameInstallationResolver::listSource2Addons(fsPath);
}

QStringList GameEnvironmentService::listSource2Addons(const GameInstallationInfo& s2Installation) const
{
    return listSource2Addons(s2Installation.basePath);
}

void GameEnvironmentService::listSource2AddonsAsync(
    const GameInstallationInfo& s2Installation,
    QObject* context,
    std::function<void(Core::Result<QStringList>)> callback)
{
    const QString basePath = cleanPath(s2Installation.basePath);

    (void)Application::Async::AsyncTaskRunner::runSystemTask<QStringList>(
        QCoreApplication::translate("GameEnvironmentService", "List Source 2 Addons"),
        context,
        [basePath](const Application::Async::SystemTaskLog& sysLog) -> Core::Result<QStringList> {
            if (basePath.isEmpty()) {
                const QString errMsg = QStringLiteral("Source 2 base path is empty, addon listing skipped");
                sysLog.warning(errMsg);
                return Core::Result<QStringList>::skipped(errMsg);
            }
            sysLog.info(QStringLiteral("Scanning addons under: %1").arg(basePath));
            Core::Path::FilesystemPath fsPath(basePath);
            return Core::Result<QStringList>::success(
                Domain::Game::GameInstallationResolver::listSource2Addons(fsPath));
        },
        std::move(callback));
}

Core::Result<VpkSignatureLeaseResult> GameEnvironmentService::updateVpkLease(const QString& s2BasePath)
{
    if (!m_leaseService) {
        VpkSignatureLeaseResult res{VpkSignatureLeaseStatus::Inactive, QCoreApplication::translate("GameEnvironmentService", "VPK signature lease service is unavailable"), QString()};
        return Core::Result<VpkSignatureLeaseResult>::failure(Core::Error::ErrorCode::InvalidState, res.systemMessage, QString(), res);
    }
    return m_leaseService->acquireLease(cleanPath(s2BasePath));
}

Core::Result<VpkSignatureLeaseResult> GameEnvironmentService::updateVpkLease(const GameInstallationInfo& s2Installation)
{
    if (!m_leaseService) {
        VpkSignatureLeaseResult res{VpkSignatureLeaseStatus::Inactive, QCoreApplication::translate("GameEnvironmentService", "VPK signature lease service is unavailable"), QString()};
        return Core::Result<VpkSignatureLeaseResult>::failure(Core::Error::ErrorCode::InvalidState, res.systemMessage, QString(), res);
    }
    return m_leaseService->updateInstallation(s2Installation);
}

Core::Result<VpkSignatureLeaseResult> GameEnvironmentService::retryVpkLease()
{
    if (!m_leaseService) {
        VpkSignatureLeaseResult res{VpkSignatureLeaseStatus::Inactive, QCoreApplication::translate("GameEnvironmentService", "VPK signature lease service is unavailable"), QString()};
        return Core::Result<VpkSignatureLeaseResult>::failure(Core::Error::ErrorCode::InvalidState, res.systemMessage, QString(), res);
    }
    return m_leaseService->retryLease();
}

} // namespace Application::Environment
