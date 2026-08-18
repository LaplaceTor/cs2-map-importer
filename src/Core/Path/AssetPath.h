#pragma once

#include <QString>
#include "PathUtils.h"

namespace Core::Path {

enum class AssetType {
    Unknown,
    Model,
    Particle,
    Material,
    Map
};

class AssetPath {
public:
    AssetPath();
    explicit AssetPath(const QString& path);

    // Getters
    QString rawPath() const;
    QString RawPath() const { return rawPath(); }

    QString absolutePath() const;
    QString AbsolutePath() const { return absolutePath(); }

    QString fileName() const;
    QString FileName() const { return fileName(); }

    QString extension() const;
    QString Extension() const { return extension(); }

    bool exists() const;
    bool Exists() const { return exists(); }

    AssetType type() const;
    AssetType Type() const { return type(); }

    bool isEmpty() const;
    bool IsEmpty() const { return isEmpty(); }

    bool isValid() const;
    bool IsValid() const { return isValid(); }

private:
    static AssetType detectType(const QString& ext);

    QString m_rawPath;
};

} // namespace Core::Path

namespace Core {
    using Path::AssetType;
    using Path::AssetPath;
}

using Core::Path::AssetType;
using Core::Path::AssetPath;
