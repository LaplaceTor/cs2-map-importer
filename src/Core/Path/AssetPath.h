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
    QString absolutePath() const;
    QString fileName() const;
    QString extension() const;
    bool exists() const;
    AssetType type() const;
    bool isEmpty() const;
    bool isValid() const;

private:
    static AssetType detectType(const QString& ext);

    QString m_rawPath;
};

} // namespace Core::Path
