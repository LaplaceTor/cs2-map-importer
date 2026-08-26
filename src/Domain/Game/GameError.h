#pragma once
#include "Core/Error/Error.h"
#include "Core/Error/ErrorCode.h"
#include <QString>

namespace Domain::Game {

/**
 * @brief Fine-grained business error codes for the Game domain.
 */
enum class GameErrorCode {
    None = 0,
    UnsupportedGame,
    GameInfoNotFound,
    GameTypeMismatch,
    SteamAppMismatch,
    InvalidGameInstallation,
    EmptyCustomGameInfo
};

/**
 * @brief Factory helper for creating Domain::Game structured Error objects.
 */
class GameError {
public:
    static inline const QString DomainName = QStringLiteral("Domain::Game");

    static Core::Error::Error make(
        GameErrorCode code,
        const QString& message,
        const QString& details = QString(),
        Core::Error::ErrorCode highLevelCode = Core::Error::ErrorCode::DomainError)
    {
        return Core::Error::Error::domain(DomainName, code, message, details, highLevelCode);
    }

    static Core::Error::Error unsupportedGame(
        const QString& message = QStringLiteral("Unsupported or unrecognised game type"),
        const QString& details = QString())
    {
        return make(GameErrorCode::UnsupportedGame,
                    message.isEmpty() ? QStringLiteral("Unsupported or unrecognised game type") : message,
                    details,
                    Core::Error::ErrorCode::NotSupported);
    }

    static Core::Error::Error gameInfoNotFound(
        const QString& message = QStringLiteral("GameInfo file was not found"),
        const QString& details = QString())
    {
        return make(GameErrorCode::GameInfoNotFound,
                    message.isEmpty() ? QStringLiteral("GameInfo file was not found") : message,
                    details,
                    Core::Error::ErrorCode::FileNotFound);
    }

    static Core::Error::Error gameTypeMismatch(
        const QString& message = QStringLiteral("Game configuration does not match expected game type"),
        const QString& details = QString())
    {
        return make(GameErrorCode::GameTypeMismatch,
                    message.isEmpty() ? QStringLiteral("Game configuration does not match expected game type") : message,
                    details,
                    Core::Error::ErrorCode::TypeMismatch);
    }

    static Core::Error::Error steamAppMismatch(
        const QString& message = QStringLiteral("Steam AppID does not match expected game"),
        const QString& details = QString())
    {
        return make(GameErrorCode::SteamAppMismatch,
                    message.isEmpty() ? QStringLiteral("Steam AppID does not match expected game") : message,
                    details,
                    Core::Error::ErrorCode::TypeMismatch);
    }

    static Core::Error::Error invalidGameInstallation(
        const QString& message = QStringLiteral("Invalid game installation structure"),
        const QString& details = QString())
    {
        return make(GameErrorCode::InvalidGameInstallation,
                    message.isEmpty() ? QStringLiteral("Invalid game installation structure") : message,
                    details,
                    Core::Error::ErrorCode::InvalidState);
    }

    static Core::Error::Error emptyCustomGameInfo(
        const QString& message = QStringLiteral("Custom GameInfo is empty and has no valid gameinfo file path"),
        const QString& details = QString())
    {
        return make(GameErrorCode::EmptyCustomGameInfo,
                    message.isEmpty() ? QStringLiteral("Custom GameInfo is empty and has no valid gameinfo file path") : message,
                    details,
                    Core::Error::ErrorCode::TypeMismatch);
    }
};

} // namespace Domain::Game

