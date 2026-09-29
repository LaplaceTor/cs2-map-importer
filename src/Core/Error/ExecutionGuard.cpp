#include "Core/Error/ExecutionGuard.h"

#include <filesystem>
#include <new>
#include <stdexcept>
#include <QStringList>

namespace Core::Error {

QString ExecutionContext::formatDetails(const QString& rawDetails) const
{
    QStringList parts;
    if (!stage.isEmpty()) {
        parts << QStringLiteral("Stage: %1").arg(stage);
    }
    if (!resourcePath.isEmpty()) {
        parts << QStringLiteral("Resource: %1").arg(resourcePath);
    }
    if (!targetPath.isEmpty()) {
        parts << QStringLiteral("Target: %1").arg(targetPath);
    }
    if (!rawDetails.isEmpty()) {
        parts << rawDetails;
    }
    return parts.join(QStringLiteral("\n"));
}

ErrorCode ExecutionGuard::classifyStdException(const std::exception& ex)
{
    if (const auto* fsErr = dynamic_cast<const std::filesystem::filesystem_error*>(&ex)) {
        const auto& ec = fsErr->code();
        if (ec == std::errc::no_such_file_or_directory) {
            return ErrorCode::FileNotFound;
        }
        if (ec == std::errc::permission_denied) {
            return ErrorCode::PermissionDenied;
        }
        if (ec == std::errc::file_exists) {
            return ErrorCode::FileAlreadyExists;
        }
        if (ec == std::errc::invalid_argument) {
            return ErrorCode::InvalidArgument;
        }
        if (ec == std::errc::read_only_file_system) {
            return ErrorCode::WriteFailed;
        }
        return ErrorCode::OperationFailed;
    }

    if (dynamic_cast<const std::invalid_argument*>(&ex) || dynamic_cast<const std::out_of_range*>(&ex)) {
        return ErrorCode::InvalidArgument;
    }

    if (dynamic_cast<const std::bad_alloc*>(&ex)) {
        return ErrorCode::OutOfMemory;
    }

    return ErrorCode::OperationFailed;
}

} // namespace Core::Error
