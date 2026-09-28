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
        if (ec == std::errc::read_only_file_system || ec == std::errc::io_error) {
            return ErrorCode::WriteFailed;
        }
        return ErrorCode::ReadFailed;
    }

    if (dynamic_cast<const std::invalid_argument*>(&ex) || dynamic_cast<const std::out_of_range*>(&ex)) {
        return ErrorCode::InvalidArgument;
    }

    if (dynamic_cast<const std::bad_alloc*>(&ex)) {
        return ErrorCode::ResourceBusy;
    }

    QString msg = QString::fromUtf8(ex.what()).toLower();
    if (msg.contains(QStringLiteral("cancel"))) {
        return ErrorCode::Cancelled;
    }
    if (msg.contains(QStringLiteral("entry"))) {
        return ErrorCode::EntryNotFound;
    }
    if (msg.contains(QStringLiteral("archive")) || msg.contains(QStringLiteral("vpk")) || msg.contains(QStringLiteral("bsp"))) {
        return ErrorCode::ArchiveOpenFailed;
    }
    if (msg.contains(QStringLiteral("not found")) || msg.contains(QStringLiteral("no such file"))) {
        return ErrorCode::FileNotFound;
    }
    if (msg.contains(QStringLiteral("permission")) || msg.contains(QStringLiteral("access denied"))) {
        return ErrorCode::PermissionDenied;
    }
    if (msg.contains(QStringLiteral("write")) || msg.contains(QStringLiteral("disk"))) {
        return ErrorCode::WriteFailed;
    }
    if (msg.contains(QStringLiteral("read"))) {
        return ErrorCode::ReadFailed;
    }
    if (msg.contains(QStringLiteral("corrupt"))) {
        return ErrorCode::CorruptedData;
    }

    return ErrorCode::OperationFailed;
}

} // namespace Core::Error
