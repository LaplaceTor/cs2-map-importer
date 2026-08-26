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

    static Core::Error::Error unsupportedGame(const QString& message, const QString& details = QString())
    {
        return make(GameErrorCode::UnsupportedGame, message, details, Core::Error::ErrorCode::NotSupported);
    }

    static Core::Error::Error gameInfoNotFound(const QString& message, const QString& details = QString())
    {
        return make(GameErrorCode::GameInfoNotFound, message, details, Core::Error::ErrorCode::FileNotFound);
    }

    static Core::Error::Error gameTypeMismatch(const QString& message, const QString& details = QString())
    {
        return make(GameErrorCode::GameTypeMismatch, message, details, Core::Error::ErrorCode::TypeMismatch);
    }

    static Core::Error::Error steamAppMismatch(const QString& message, const QString& details = QString())
    {
        return make(GameErrorCode::SteamAppMismatch, message, details, Core::Error::ErrorCode::TypeMismatch);
    }

    static Core::Error::Error invalidGameInstallation(const QString& message, const QString& details = QString())
    {
        return make(GameErrorCode::InvalidGameInstallation, message, details, Core::Error::ErrorCode::InvalidState);
    }

    static Core::Error::Error emptyCustomGameInfo(const QString& message, const QString& details = QString())
    {
        return make(GameErrorCode::EmptyCustomGameInfo, message, details, Core::Error::ErrorCode::TypeMismatch);
    }
};

} // namespace Domain::Game

