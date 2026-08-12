#pragma once

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
