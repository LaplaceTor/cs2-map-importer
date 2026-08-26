#pragma once

namespace Core::Error {

/**
 * @brief Standardized error codes for infrastructure, system, and general operations.
 */
enum class ErrorCode {
    Success = 0,
    Unknown = 1,

    // Arguments and Validation
    InvalidArgument,
    InvalidPath,
    NotSupported,
    TypeMismatch,

    // Filesystem and I/O
    FileNotFound,
    DirectoryNotFound,
    PermissionDenied,
    FileAlreadyExists,
    ReadFailed,
    WriteFailed,
    InvalidFile,
    CorruptedData,

    // External Process
    ProcessFailed,
    ProcessTimeout,
    ProcessCrashed,
    ProcessNotFound,

    // Operations and Execution
    OperationFailed,
    Cancelled,
    Timeout,
    ResourceBusy,
    NetworkError,

    // Domain Specific Fallback
    DomainError
};

} // namespace Core::Error

