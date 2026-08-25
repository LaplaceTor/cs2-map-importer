#include "Application/Environment/GameEnvironmentService.h"
#include "Domain/Game/GameRegistry.h"
#include "Domain/Game/GameType.h"
#include "Core/Path/PathUtils.h"
#include <QDir>
#include <QMetaObject>
#include <QPointer>
#include <QThreadPool>
#include <QUrl>
#include <utility>

namespace Application::Environment {

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
}

void GameEnvironmentService::setVpkSignatureLeaseService(VpkSignatureLeaseService* service) noexcept
{
    if (service) {
        m_ownedLeaseService.reset();
        m_leaseService = service;
    } else if (!m_ownedLeaseService) {
        m_ownedLeaseService = std::make_unique<VpkSignatureLeaseService>(this);
        m_leaseService = m_ownedLeaseService.get();
    }
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

Domain::Game::GameType GameEnvironmentService::resolveGameType(const QString& typeName) const
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
    const Core::Path::FilesystemPath& customSteamPath)
{
    GameDetectService::detectEnvironmentAsync(context, std::move(callback), customSteamPath);
}

DetectionResult GameEnvironmentService::detectEnvironment(
    const Core::Path::FilesystemPath& customSteamPath)
{
    return GameDetectService::detectEnvironment(customSteamPath);
}

void GameEnvironmentService::validateSource1FolderAsync(
    const QString& typeName,
    const QString& pathOrUrl,
    QObject* context,
    std::function<void(const std::optional<GameInstallation>&)> callback)
{
    QPointer<QObject> contextGuard(context);
    QString normalizedPath = cleanPath(pathOrUrl);
    Domain::Game::GameType type = resolveGameType(typeName);

    QThreadPool::globalInstance()->start([this, contextGuard, callback = std::move(callback), type, normalizedPath]() {
        std::optional<GameInstallation> result;
        if (!normalizedPath.isEmpty()) {
            Core::Path::FilesystemPath fsPath(normalizedPath);
            if (type == Domain::Game::GameType::Custom) {
                result = GameInstallationValidator::inspectGameInfo(fsPath);
            } else {
                result = GameInstallationValidator::validateSource1(type, fsPath);
            }
        }

        if (!contextGuard) {
            return;
        }

        QMetaObject::invokeMethod(contextGuard.data(), [contextGuard, callback = std::move(callback), res = std::move(result)]() {
            if (contextGuard && callback) {
                callback(res);
            }
        }, Qt::QueuedConnection);
    });
}

std::optional<GameInstallation> GameEnvironmentService::validateSource1Folder(
    const QString& typeName,
    const QString& pathOrUrl)
{
    QString normalizedPath = cleanPath(pathOrUrl);
    if (normalizedPath.isEmpty()) {
        return std::nullopt;
    }

    Core::Path::FilesystemPath fsPath(normalizedPath);
    Domain::Game::GameType type = resolveGameType(typeName);
    if (type == Domain::Game::GameType::Custom) {
        return GameInstallationValidator::inspectGameInfo(fsPath);
    }
    return GameInstallationValidator::validateSource1(type, fsPath);
}

void GameEnvironmentService::validateSource2FolderAsync(
    const QString& pathOrUrl,
    QObject* context,
    std::function<void(const std::optional<GameInstallation>&)> callback)
{
    QPointer<QObject> contextGuard(context);
    QString normalizedPath = cleanPath(pathOrUrl);

    QThreadPool::globalInstance()->start([contextGuard, callback = std::move(callback), normalizedPath]() {
        std::optional<GameInstallation> result;
        if (!normalizedPath.isEmpty()) {
            Core::Path::FilesystemPath fsPath(normalizedPath);
            result = GameInstallationValidator::validateSource2(fsPath);
        }

        if (!contextGuard) {
            return;
        }

        QMetaObject::invokeMethod(contextGuard.data(), [contextGuard, callback = std::move(callback), res = std::move(result)]() {
            if (contextGuard && callback) {
                callback(res);
            }
        }, Qt::QueuedConnection);
    });
}

std::optional<GameInstallation> GameEnvironmentService::validateSource2Folder(
    const QString& pathOrUrl)
{
    QString normalizedPath = cleanPath(pathOrUrl);
    if (normalizedPath.isEmpty()) {
        return std::nullopt;
    }

    Core::Path::FilesystemPath fsPath(normalizedPath);
    return GameInstallationValidator::validateSource2(fsPath);
}

bool GameEnvironmentService::validateGameInSteam(const QString& typeName)
{
    Domain::Game::GameType type = resolveGameType(typeName);
    return SteamService::validateGameFiles(type);
}

QStringList GameEnvironmentService::listSource2Addons(const GameInstallation& s2Installation) const
{
    QStringList addons;
    if (s2Installation.isValid()) {
        Core::Path::FilesystemPath addonsDir = s2Installation.addonGameDirectory();
        if (addonsDir.isValid() && addonsDir.exists() && addonsDir.isDirectory()) {
            QDir dir(addonsDir.toString());
            const QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const auto& entry : entries) {
                addons.append(entry);
            }
        }
    }
    return addons;
}

VpkSignatureLeaseResult GameEnvironmentService::updateVpkLease(const GameInstallation& s2Installation)
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

