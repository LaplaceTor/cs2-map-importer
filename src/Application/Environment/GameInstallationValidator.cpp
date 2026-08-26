#include "Application/Environment/GameInstallationValidator.h"

namespace Application::Environment {

std::optional<GameInstallation> GameInstallationValidator::createInstallationFromResolved(
    const Domain::Game::ResolvedGameInstallation& resolved)
{
    if (!resolved.isValid) {
        return std::nullopt;
    }

    GameInstallation inst;
    inst.setType(resolved.type);
    inst.setGameId(Domain::Game::GameRegistry::gameTypeToString(resolved.type));

    const auto* def = Domain::Game::GameRegistry::findByType(resolved.type);
    if (def && resolved.type != Domain::Game::GameType::Custom && resolved.type != Domain::Game::GameType::Unknown) {
        inst.setDisplayName(def->displayName);
        inst.setSource2(def->isSource2());
        inst.setAppId(resolved.gameInfo.steamAppId() > 0 ? resolved.gameInfo.steamAppId() : def->primaryAppId);
    } else {
        inst.setDisplayName(resolved.gameInfo.game().isEmpty() ? resolved.gameInfo.title() : resolved.gameInfo.game());
        inst.setSource2(resolved.isSource2);
        inst.setAppId(resolved.gameInfo.steamAppId());
    }

    inst.setGameTitle(resolved.gameInfo.game().isEmpty() ? resolved.gameInfo.title() : resolved.gameInfo.game());
    inst.setBaseDirectory(resolved.baseDirectory);
    inst.setGameInfoPath(resolved.gameInfoPath);
    inst.setValid(true);
    inst.setGameInfo(resolved.gameInfo);

    return inst;
}

std::optional<GameInstallation> GameInstallationValidator::createInstallationFromGameInfo(
    Domain::Game::GameType type,
    const Core::Path::FilesystemPath& baseDir,
    const Domain::Game::GameInfo& info)
{
    auto optResolved = Domain::Game::GameInstallationResolver::createResolved(type, baseDir, info);
    if (!optResolved.has_value()) {
        return std::nullopt;
    }
    return createInstallationFromResolved(*optResolved);
}

std::optional<GameInstallation> GameInstallationValidator::validateSource1(
    Domain::Game::GameType type,
    const Core::Path::FilesystemPath& directory,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    QString typeStr = Domain::Game::GameRegistry::gameTypeToString(type);
    if (logCtx) {
        logCtx->debug(QStringLiteral("Checking Source 1 directory structure for '%1': %2")
            .arg(typeStr, directory.toString()));
    }

    if (!directory.isValid() || !directory.exists()) {
        if (logCtx) {
            logCtx->error(QStringLiteral("Target directory does not exist or is invalid: %1").arg(directory.toString()));
        }
        return std::nullopt;
    }

    auto optResolved = Domain::Game::GameInstallationResolver::resolveSource1(type, directory);
    if (!optResolved.has_value()) {
        if (logCtx) {
            logCtx->debug(QStringLiteral("Source 1 gameinfo resolution failed for '%1' in: %2")
                .arg(typeStr, directory.toString()));
        }
        return std::nullopt;
    }

    auto inst = createInstallationFromResolved(*optResolved);
    if (inst.has_value() && logCtx) {
        logCtx->debug(QStringLiteral("Verified Source 1 gameinfo at: %1").arg(inst->gameInfoPath().toString()));
    }
    return inst;
}

std::optional<GameInstallation> GameInstallationValidator::validateSource2(
    const Core::Path::FilesystemPath& directory,
    Domain::Game::GameType type,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    if (logCtx) {
        logCtx->debug(QStringLiteral("Checking Source 2 directory structure: %1").arg(directory.toString()));
    }

    if (!directory.isValid() || !directory.exists()) {
        if (logCtx) {
            logCtx->error(QStringLiteral("Target directory does not exist or is invalid: %1").arg(directory.toString()));
        }
        return std::nullopt;
    }

    auto optResolved = Domain::Game::GameInstallationResolver::resolveSource2(directory, type);
    if (!optResolved.has_value()) {
        if (logCtx) {
            logCtx->debug(QStringLiteral("Source 2 gameinfo.gi resolution failed in: %1").arg(directory.toString()));
        }
        return std::nullopt;
    }

    auto inst = createInstallationFromResolved(*optResolved);
    if (inst.has_value() && logCtx) {
        logCtx->debug(QStringLiteral("Verified Source 2 gameinfo.gi at: %1").arg(inst->gameInfoPath().toString()));
    }
    return inst;
}

std::optional<GameInstallation> GameInstallationValidator::inspectGameInfo(
    const Core::Path::FilesystemPath& gameInfoPath,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    if (logCtx) {
        logCtx->debug(QStringLiteral("Inspecting custom GameInfo at: %1").arg(gameInfoPath.toString()));
    }

    if (!gameInfoPath.isValid() || !gameInfoPath.exists()) {
        if (logCtx) {
            logCtx->error(QStringLiteral("GameInfo path does not exist: %1").arg(gameInfoPath.toString()));
        }
        return std::nullopt;
    }

    auto optResolved = Domain::Game::GameInstallationResolver::inspectGameInfo(gameInfoPath);
    if (!optResolved.has_value()) {
        if (logCtx) {
            logCtx->debug(QStringLiteral("Failed to parse or resolve GameInfo at: %1").arg(gameInfoPath.toString()));
        }
        return std::nullopt;
    }

    auto inst = createInstallationFromResolved(*optResolved);
    if (inst.has_value() && logCtx) {
        logCtx->debug(QStringLiteral("Parsed custom GameInfo title: %1").arg(inst->displayName()));
    }
    return inst;
}

std::optional<GameInstallation> GameInstallationValidator::validateGameDirectory(
    Domain::Game::GameType type,
    const Core::Path::FilesystemPath& directory,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    if (logCtx) {
        logCtx->debug(QStringLiteral("Resolving game directory for type '%1' at: %2")
            .arg(Domain::Game::GameRegistry::gameTypeToString(type), directory.toString()));
    }

    auto optResolved = Domain::Game::GameInstallationResolver::resolveGameDirectory(type, directory);
    if (!optResolved.has_value()) {
        if (logCtx) {
            logCtx->debug(QStringLiteral("Resolution failed for '%1' at: %2")
                .arg(Domain::Game::GameRegistry::gameTypeToString(type), directory.toString()));
        }
        return std::nullopt;
    }
    return createInstallationFromResolved(*optResolved);
}

} // namespace Application::Environment
