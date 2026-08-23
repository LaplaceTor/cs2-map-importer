#pragma once

#include "Core/Path/FilesystemPath.h"
#include <QHashFunctions>
#include <QString>
#include <optional>

namespace Domain::Asset {

class AssetPath {
public:
    AssetPath();
    explicit AssetPath(const QString& path);

    bool isEmpty() const;
    bool isValid() const;

    QString fileName() const;
    QString extension() const;
    QString directory() const;
    QString toString() const;

    Core::Path::FilesystemPath toFilesystemPath(const Core::Path::FilesystemPath& baseDir) const;
    Core::Path::FilesystemPath resolve(const Core::Path::FilesystemPath& baseDir) const;

    static std::optional<AssetPath> fromFilesystemPath(const Core::Path::FilesystemPath& baseDir,
                                                      const Core::Path::FilesystemPath& filePath);
    static QString sanitizeAssetName(const QString& assetName, const QString& replacement = QStringLiteral("_"));

    bool operator==(const AssetPath& other) const;
    bool operator!=(const AssetPath& other) const;
    bool operator<(const AssetPath& other) const;

    friend size_t qHash(const AssetPath& key, size_t seed = 0) noexcept {
        return ::qHash(key.m_path, seed);
    }

private:
    void processPath(const QString& path);

    QString m_path;
};

} // namespace Domain::Asset

