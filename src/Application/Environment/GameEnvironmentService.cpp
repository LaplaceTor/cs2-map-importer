#include "Application/Environment/GameEnvironmentService.h"
#include "Application/Environment/GameDetectService.h"
#include "Application/Environment/GameInstallation.h"
#include "Application/Environment/GameInstallationValidator.h"
#include "Application/Environment/SteamService.h"
#include "Application/Async/AsyncTaskRunner.h"
#include "Domain/Game/GameRegistry.h"
#include "Domain/Game/GameType.h"
#include "Domain/Game/GameInstallationResolver.h"
#include "Core/Path/PathUtils.h"
#include <QDir>
#include <QMetaObject>
#include <QPointer>
#include <QThreadPool>
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
    VpkSignatureLeaseService* leaseService,
    QObject* parent)
    : QObject(parent)
{
    if (leaseService) {
        m_leaseService = leaseService;
    } else {
        m_ownedLeaseService = std::make_unique<VpkSignatureLeaseService>(this);
        m_leaseService = m_ownedLeaseService.get();
    }
    hookLeaseSignals();
}

void GameEnvironmentService::hookLeaseSignals()
{
    if (m_leaseService) {
        connect(m_leaseService, &VpkSignatureLeaseService::leaseStateChanged,
                this, &GameEnvironmentService::vpkLeaseStateChanged, Qt::UniqueConnection);
        connect(m_leaseService, &VpkSignatureLeaseService::leaseStatusChanged,
                this, &GameEnvironmentService::vpkLeaseStatusChanged, Qt::UniqueConnection);
    }
}

void GameEnvironmentService::setVpkSignatureLeaseService(VpkSignatureLeaseService* service) noexcept
{
    if (m_leaseService) {
        disconnect(m_leaseService, nullptr, this, nullptr);
    }

    if (service) {
        m_ownedLeaseService.reset();
        m_leaseService = service;
    } else if (!m_ownedLeaseService) {
        m_ownedLeaseService = std::make_unique<VpkSignatureLeaseService>(this);
        m_leaseService = m_ownedLeaseService.get();
    }
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

QString GameEnvironmentService::cleanPath(const QString& pathOrUrl) const
{
    if (pathOrUrl.isEmpty()) {
        return QString();
    }
    QUrl url(pathOrUrl);
    QString path = url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
    return Core::Path::PathUtils::normalize(path);
}

void GameEnvironmentService::detectEnvironmentAsync(
    QObject* context,
    std::function<void(const DetectionResult&)> callback,
    const QString& customSteamPath)
{
    GameDetectService::detectEnvironmentAsync(context, std::move(callback), customSteamPath);
}

DetectionResult GameEnvironmentService::detectEnvironment(
    const QString& customSteamPath)
{
    return GameDetectService::detectEnvironment(customSteamPath);
}

void GameEnvironmentService::validateSource1FolderAsync(
    const QString& typeName,
    const QString& pathOrUrl,
    QObject* context,
    std::function<void(const std::optional<GameInstallationInfo>&)> callback)
{
    QString normalizedPath = cleanPath(pathOrUrl);
    Domain::Game::GameType type = resolveGameTypeFromName(typeName);
    QString effectiveName = typeName.trimmed().isEmpty() ? QStringLiteral("Source 1") : typeName.trimmed();
    QString taskName = QStringLiteral("Validate %1").arg(effectiveName);

    Application::Async::AsyncTaskRunner::run<std::optional<GameInstallationInfo>>(
        taskName,
        context,
        [type, normalizedPath, effectiveName](std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx) -> std::optional<GameInstallationInfo> {
            if (logCtx) {
                logCtx->info(QStringLiteral("Starting validation for %1 at: %2").arg(effectiveName, normalizedPath));
            }
            if (normalizedPath.isEmpty()) {
                if (logCtx) {
                    logCtx->warning(QStringLiteral("Target path is empty, validation skipped"));
                }
                return std::nullopt;
            }

            Core::Path::FilesystemPath fsPath(normalizedPath);
            std::optional<GameInstallation> inst;
            if (type == Domain::Game::GameType::Custom) {
                inst = GameInstallationValidator::inspectGameInfo(fsPath, logCtx);
            } else {
                inst = GameInstallationValidator::validateSource1(type, fsPath, logCtx);
            }

            if (inst.has_value()) {
                if (logCtx) {
                    logCtx->info(QStringLiteral("Validation completed successfully: %1").arg(inst->displayName()));
                }
                return inst->toInfo();
            }

            if (logCtx) {
                logCtx->error(QStringLiteral("Validation failed for %1 at: %2").arg(effectiveName, normalizedPath));
            }
            return std::nullopt;
        },
        std::move(callback));
}

std::optional<GameInstallationInfo> GameEnvironmentService::validateSource1Folder(
    const QString& typeName,
    const QString& pathOrUrl)
{
    QString normalizedPath = cleanPath(pathOrUrl);
    if (normalizedPath.isEmpty()) {
        return std::nullopt;
    }

    Core::Path::FilesystemPath fsPath(normalizedPath);
    Domain::Game::GameType type = resolveGameTypeFromName(typeName);
    std::optional<GameInstallation> inst;
    if (type == Domain::Game::GameType::Custom) {
        inst = GameInstallationValidator::inspectGameInfo(fsPath);
    } else {
        inst = GameInstallationValidator::validateSource1(type, fsPath);
    }

    if (!inst.has_value()) {
        return std::nullopt;
    }
    return inst->toInfo();
}

void GameEnvironmentService::validateSource2FolderAsync(
    const QString& pathOrUrl,
    QObject* context,
    std::function<void(const std::optional<GameInstallationInfo>&)> callback)
{
    QString normalizedPath = cleanPath(pathOrUrl);

    Application::Async::AsyncTaskRunner::run<std::optional<GameInstallationInfo>>(
        QStringLiteral("Validate Source 2"),
        context,
        [normalizedPath](std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx) -> std::optional<GameInstallationInfo> {
            if (logCtx) {
                logCtx->info(QStringLiteral("Starting Source 2 validation at: %1").arg(normalizedPath));
            }
            if (normalizedPath.isEmpty()) {
                if (logCtx) {
                    logCtx->warning(QStringLiteral("Target path is empty, validation skipped"));
                }
                return std::nullopt;
            }

            Core::Path::FilesystemPath fsPath(normalizedPath);
            auto inst = GameInstallationValidator::validateSource2(fsPath, Domain::Game::GameType::Unknown, logCtx);
            if (inst.has_value()) {
                if (logCtx) {
                    logCtx->info(QStringLiteral("Validation completed successfully: %1").arg(inst->displayName()));
                }
                return inst->toInfo();
            }

            if (logCtx) {
                logCtx->error(QStringLiteral("Validation failed for Source 2 at: %1").arg(normalizedPath));
            }
            return std::nullopt;
        },
        std::move(callback));
}

std::optional<GameInstallationInfo> GameEnvironmentService::validateSource2Folder(
    const QString& pathOrUrl)
{
    QString normalizedPath = cleanPath(pathOrUrl);
    if (normalizedPath.isEmpty()) {
        return std::nullopt;
    }

    Core::Path::FilesystemPath fsPath(normalizedPath);
    auto inst = GameInstallationValidator::validateSource2(fsPath);
    if (!inst.has_value()) {
        return std::nullopt;
    }
    return inst->toInfo();
}

bool GameEnvironmentService::validateGameInSteam(const QString& typeName)
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

VpkSignatureLeaseResult GameEnvironmentService::updateVpkLease(const QString& s2BasePath)
{
    if (m_leaseService) {
        return m_leaseService->acquireLease(cleanPath(s2BasePath));
    }
    return {VpkSignatureLeaseStatus::Inactive, QString(), QString()};
}

VpkSignatureLeaseResult GameEnvironmentService::updateVpkLease(const GameInstallationInfo& s2Installation)
{
    if (m_leaseService) {
        return m_leaseService->updateInstallation(s2Installation);
    }
    return {VpkSignatureLeaseStatus::Inactive, QString(), QString()};
}

VpkSignatureLeaseResult GameEnvironmentService::retryVpkLease()
{
    if (m_leaseService) {
        return m_leaseService->retryLease();
    }
    return {VpkSignatureLeaseStatus::Inactive, QString(), QString()};
}

} // namespace Application::Environment
