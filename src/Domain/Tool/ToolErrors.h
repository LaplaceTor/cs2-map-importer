#pragma once

#include <QCoreApplication>
#include "Core/Error/Error.h"
#include "Core/Error/ErrorCode.h"
#include <QString>

namespace Domain::Tool {

/**
 * @brief Fine-grained business error codes for external CLI tools and parsers.
 */
enum class ToolErrorCode {
    None = 0,
    ExecutableNotFound,
    ExecutionFailed,
    Timeout,
    Crashed,
    NoMatchingFiles,
    ImportFailed,
    CompilationFailed,
    ParseError
};

/**
 * @brief Factory helper for creating Domain::Tool structured Error objects.
 */
class ToolErrors {
public:
    static inline const QString DomainName = QStringLiteral("Domain::Tool");

    /** @brief Type-safe matcher for a Tool domain error code. */
    static bool is(const Core::Error::Error& error, ToolErrorCode code) noexcept
    {
        return error.is(DomainName, code);
    }

    static Core::Error::Error make(
        ToolErrorCode code,
        const QString& message,
        const QString& details = QString(),
        Core::Error::ErrorCode highLevelCode = Core::Error::ErrorCode::DomainError)
    {
        return Core::Error::Error::domain(DomainName, code, message, details, highLevelCode);
    }

    static Core::Error::Error executableNotFound(
        const QString& path,
        const QString& details = QString())
    {
        return make(ToolErrorCode::ExecutableNotFound,
                    QCoreApplication::translate("ToolErrors", "Executable not found: %1").arg(path),
                    details,
                    Core::Error::ErrorCode::FileNotFound);
    }

    static Core::Error::Error executionFailed(
        const QString& toolName,
        int exitCode,
        const QString& details = QString())
    {
        return make(ToolErrorCode::ExecutionFailed,
                    QCoreApplication::translate("ToolErrors", "%1 execution failed with exit code %2").arg(toolName).arg(exitCode),
                    details,
                    Core::Error::ErrorCode::ProcessFailed);
    }

    static Core::Error::Error timeout(
        const QString& toolName,
        const QString& details = QString())
    {
        return make(ToolErrorCode::Timeout,
                    QCoreApplication::translate("ToolErrors", "%1 execution timed out").arg(toolName),
                    details,
                    Core::Error::ErrorCode::ProcessTimeout);
    }

    static Core::Error::Error crashed(
        const QString& toolName,
        const QString& details = QString())
    {
        return make(ToolErrorCode::Crashed,
                    QCoreApplication::translate("ToolErrors", "%1 crashed during execution").arg(toolName),
                    details,
                    Core::Error::ErrorCode::ProcessCrashed);
    }

    static Core::Error::Error noMatchingFiles(
        const QString& pattern,
        const QString& details = QString())
    {
        return make(ToolErrorCode::NoMatchingFiles,
                    QCoreApplication::translate("ToolErrors", "Found no files matching specification: %1").arg(pattern),
                    details,
                    Core::Error::ErrorCode::FileNotFound);
    }

    static Core::Error::Error importFailed(
        const QString& message = QCoreApplication::translate("ToolErrors", "Source1Import failed"),
        const QString& details = QString())
    {
        return make(ToolErrorCode::ImportFailed,
                    message.isEmpty() ? QCoreApplication::translate("ToolErrors", "Source1Import failed") : message,
                    details,
                    Core::Error::ErrorCode::OperationFailed);
    }

    static Core::Error::Error compilationFailed(
        const QString& message = QCoreApplication::translate("ToolErrors", "ResourceCompiler failed"),
        const QString& details = QString())
    {
        return make(ToolErrorCode::CompilationFailed,
                    message.isEmpty() ? QCoreApplication::translate("ToolErrors", "ResourceCompiler failed") : message,
                    details,
                    Core::Error::ErrorCode::OperationFailed);
    }

    static Core::Error::Error parseError(
        const QString& message = QCoreApplication::translate("ToolErrors", "Failed to parse tool output"),
        const QString& details = QString())
    {
        return make(ToolErrorCode::ParseError,
                    message.isEmpty() ? QCoreApplication::translate("ToolErrors", "Failed to parse tool output") : message,
                    details,
                    Core::Error::ErrorCode::CorruptedData);
    }
};

} // namespace Domain::Tool
