#pragma once

namespace Core::Error {

enum class ImportErrorCode {
    Unknown,
    FileNotFound,
    InvalidPath,
    PermissionDenied,
    InvalidFile,
    DirectoryNotFound,
    ProcessFailed,
    ProcessTimeout,
    OperationFailed
};

} // namespace Core::Error
