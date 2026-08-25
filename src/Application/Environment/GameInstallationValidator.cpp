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
    const Core::Path::FilesystemPath& directory)
{
    auto optResolved = Domain::Game::GameInstallationResolver::resolveSource1(type, directory);
    if (!optResolved.has_value()) {
        return std::nullopt;
    }
    return createInstallationFromResolved(*optResolved);
}

std::optional<GameInstallation> GameInstallationValidator::validateSource2(
    const Core::Path::FilesystemPath& directory,
    Domain::Game::GameType type)
{
    auto optResolved = Domain::Game::GameInstallationResolver::resolveSource2(directory, type);
    if (!optResolved.has_value()) {
        return std::nullopt;
    }
    return createInstallationFromResolved(*optResolved);
}

std::optional<GameInstallation> GameInstallationValidator::inspectGameInfo(
    const Core::Path::FilesystemPath& gameInfoPath)
{
    auto optResolved = Domain::Game::GameInstallationResolver::inspectGameInfo(gameInfoPath);
    if (!optResolved.has_value()) {
        return std::nullopt;
    }
    return createInstallationFromResolved(*optResolved);
}

std::optional<GameInstallation> GameInstallationValidator::validateGameDirectory(
    Domain::Game::GameType type,
    const Core::Path::FilesystemPath& directory)
{
    auto optResolved = Domain::Game::GameInstallationResolver::resolveGameDirectory(type, directory);
    if (!optResolved.has_value()) {
        return std::nullopt;
    }
    return createInstallationFromResolved(*optResolved);
}

} // namespace Application::Environment
