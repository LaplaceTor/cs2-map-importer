#include "Application/Environment/GameInstallationValidator.h"
#include "Domain/Game/GameErrors.h"

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

Core::Async::TaskResult<GameInstallation> GameInstallationValidator::validateSource1(
    Domain::Game::GameType type,
    const Core::Path::FilesystemPath& directory,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    QString typeStr = Domain::Game::GameRegistry::gameTypeToString(type);
    if (logCtx) {
        logCtx->debug(QStringLiteral("Checking Source 1 directory structure for '%1': %2")
            .arg(typeStr, directory.toString()));
    }

    if (!directory.isValid() || directory.isEmpty()) {
        if (logCtx) {
            logCtx->debug(QStringLiteral("Target directory path is empty or invalid: %1").arg(directory.toString()));
        }
        return Core::Async::TaskResult<GameInstallation>::failure(
            Core::Error::Error::invalidPath(
                QStringLiteral("Target directory path is empty or invalid"),
                directory.toString()),
            QStringLiteral("Source 1 validation failed"));
    }
    if (!directory.exists()) {
        if (logCtx) {
            logCtx->debug(QStringLiteral("Target directory does not exist: %1").arg(directory.toString()));
        }
        return Core::Async::TaskResult<GameInstallation>::failure(
            Core::Error::Error::directoryNotFound(
                QStringLiteral("Target directory does not exist"),
                directory.toString()),
            QStringLiteral("Source 1 validation failed"));
    }

    auto res = Domain::Game::GameInstallationResolver::resolveSource1(type, directory);
    if (!res.isSuccess()) {
        if (logCtx) {
            logCtx->debug(res.message());
        }
        return Core::Async::TaskResult<GameInstallation>::failure(
            res.error(),
            QStringLiteral("Source 1 validation failed"));
    }

    auto inst = createInstallationFromResolved(res.value());
    if (inst.has_value()) {
        if (logCtx) {
            logCtx->debug(QStringLiteral("Verified Source 1 gameinfo at: %1").arg(inst->gameInfoPath().toString()));
        }
        return Core::Async::TaskResult<GameInstallation>::success(std::move(*inst));
    }

    if (logCtx) {
        logCtx->debug(QStringLiteral("Failed to create installation from resolved Source 1 path: %1").arg(directory.toString()));
    }
    return Core::Async::TaskResult<GameInstallation>::failure(
        Domain::Game::GameErrors::invalidGameInstallation(
            QStringLiteral("Failed to create installation from resolved Source 1 path"),
            directory.toString()),
        QStringLiteral("Source 1 validation failed"));
}

Core::Async::TaskResult<GameInstallation> GameInstallationValidator::validateSource2(
    const Core::Path::FilesystemPath& directory,
    Domain::Game::GameType type,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    if (logCtx) {
        logCtx->debug(QStringLiteral("Checking Source 2 directory structure: %1").arg(directory.toString()));
    }

    if (!directory.isValid() || directory.isEmpty()) {
        if (logCtx) {
            logCtx->debug(QStringLiteral("Target directory path is empty or invalid: %1").arg(directory.toString()));
        }
        return Core::Async::TaskResult<GameInstallation>::failure(
            Core::Error::Error::invalidPath(
                QStringLiteral("Target directory path is empty or invalid"),
                directory.toString()),
            QStringLiteral("Source 2 validation failed"));
    }
    if (!directory.exists()) {
        if (logCtx) {
            logCtx->debug(QStringLiteral("Target directory does not exist: %1").arg(directory.toString()));
        }
        return Core::Async::TaskResult<GameInstallation>::failure(
            Core::Error::Error::directoryNotFound(
                QStringLiteral("Target directory does not exist"),
                directory.toString()),
            QStringLiteral("Source 2 validation failed"));
    }

    auto res = Domain::Game::GameInstallationResolver::resolveSource2(directory, type);
    if (!res.isSuccess()) {
        if (logCtx) {
            logCtx->debug(res.message());
        }
        return Core::Async::TaskResult<GameInstallation>::failure(
            res.error(),
            QStringLiteral("Source 2 validation failed"));
    }

    auto inst = createInstallationFromResolved(res.value());
    if (inst.has_value()) {
        if (logCtx) {
            logCtx->debug(QStringLiteral("Verified Source 2 gameinfo.gi at: %1").arg(inst->gameInfoPath().toString()));
        }
        return Core::Async::TaskResult<GameInstallation>::success(std::move(*inst));
    }

    if (logCtx) {
        logCtx->debug(QStringLiteral("Failed to create installation from resolved Source 2 path: %1").arg(directory.toString()));
    }
    return Core::Async::TaskResult<GameInstallation>::failure(
        Domain::Game::GameErrors::invalidGameInstallation(
            QStringLiteral("Failed to create installation from resolved Source 2 path"),
            directory.toString()),
        QStringLiteral("Source 2 validation failed"));
}

Core::Async::TaskResult<GameInstallation> GameInstallationValidator::inspectGameInfo(
    const Core::Path::FilesystemPath& gameInfoPath,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    if (logCtx) {
        logCtx->debug(QStringLiteral("Inspecting custom GameInfo at: %1").arg(gameInfoPath.toString()));
    }

    if (!gameInfoPath.isValid() || gameInfoPath.isEmpty()) {
        if (logCtx) {
            logCtx->debug(QStringLiteral("GameInfo path is empty or invalid: %1").arg(gameInfoPath.toString()));
        }
        return Core::Async::TaskResult<GameInstallation>::failure(
            Core::Error::Error::invalidPath(
                QStringLiteral("GameInfo path is empty or invalid"),
                gameInfoPath.toString()),
            QStringLiteral("GameInfo inspection failed"));
    }
    if (!gameInfoPath.exists()) {
        if (logCtx) {
            logCtx->debug(QStringLiteral("GameInfo path does not exist: %1").arg(gameInfoPath.toString()));
        }
        return Core::Async::TaskResult<GameInstallation>::failure(
            Core::Error::Error::fileNotFound(
                QStringLiteral("GameInfo path does not exist"),
                gameInfoPath.toString()),
            QStringLiteral("GameInfo inspection failed"));
    }

    auto res = Domain::Game::GameInstallationResolver::inspectGameInfo(gameInfoPath);
    if (!res.isSuccess()) {
        if (logCtx) {
            logCtx->debug(res.message());
        }
        return Core::Async::TaskResult<GameInstallation>::failure(
            res.error(),
            QStringLiteral("GameInfo inspection failed"));
    }

    auto inst = createInstallationFromResolved(res.value());
    if (inst.has_value()) {
        if (logCtx) {
            logCtx->debug(QStringLiteral("Parsed custom GameInfo title: %1").arg(inst->displayName()));
        }
        return Core::Async::TaskResult<GameInstallation>::success(std::move(*inst));
    }

    if (logCtx) {
        logCtx->debug(QStringLiteral("Failed to create installation from inspected GameInfo: %1").arg(gameInfoPath.toString()));
    }
    return Core::Async::TaskResult<GameInstallation>::failure(
        Domain::Game::GameErrors::invalidGameInstallation(
            QStringLiteral("Failed to create installation from inspected GameInfo"),
            gameInfoPath.toString()),
        QStringLiteral("GameInfo inspection failed"));
}

Core::Async::TaskResult<GameInstallation> GameInstallationValidator::validateGameDirectory(
    Domain::Game::GameType type,
    const Core::Path::FilesystemPath& directory,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    if (logCtx) {
        logCtx->debug(QStringLiteral("Resolving game directory for type '%1' at: %2")
            .arg(Domain::Game::GameRegistry::gameTypeToString(type), directory.toString()));
    }

    auto res = Domain::Game::GameInstallationResolver::resolveGameDirectory(type, directory);
    if (!res.isSuccess()) {
        if (logCtx) {
            logCtx->debug(res.message());
        }
        return Core::Async::TaskResult<GameInstallation>::failure(
            res.error(),
            QStringLiteral("Game directory validation failed"));
    }

    auto inst = createInstallationFromResolved(res.value());
    if (inst.has_value()) {
        return Core::Async::TaskResult<GameInstallation>::success(std::move(*inst));
    }

    return Core::Async::TaskResult<GameInstallation>::failure(
        Domain::Game::GameErrors::invalidGameInstallation(
            QStringLiteral("Failed to create installation from resolved game directory"),
            directory.toString()),
        QStringLiteral("Game directory validation failed"));
}

} // namespace Application::Environment
