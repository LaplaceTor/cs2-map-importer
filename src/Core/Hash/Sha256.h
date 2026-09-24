#pragma once

#include <QString>
#include <QByteArray>
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"

namespace Core::Hash {

/**
 * @brief Utility for streaming SHA-256 computation on files and data buffers.
 */
class Sha256 {
public:
    /**
     * @brief Computes the lowercase hex-encoded SHA-256 hash of a file in chunks.
     * @param filePath Path to the file.
     * @return Hex string (64 characters) on success, or structured failure.
     */
    static Core::Result<QString> computeFileHash(const Path::FilesystemPath& filePath);

    /**
     * @brief Computes the lowercase hex-encoded SHA-256 hash of an in-memory byte array.
     */
    static QString computeDataHash(const QByteArray& data);
};

} // namespace Core::Hash
