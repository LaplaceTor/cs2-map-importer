#pragma once

#include <QString>
#include "PathUtils.h"

namespace Core::Path {

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
    bool isEmpty() const;
    bool isValid() const;

private:
    QString m_rawPath;
};

} // namespace Core::Path
