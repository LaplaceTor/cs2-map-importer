#pragma once

#include <QString>

namespace Core::Path {

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

    bool operator==(const AssetPath& other) const;
    bool operator!=(const AssetPath& other) const;

private:
    void processPath(const QString& path);

    QString m_path;
    bool m_isValid{false};
};

} // namespace Core::Path
