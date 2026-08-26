#pragma once

#include <QString>

namespace Core::Path {

class FilesystemPath {
public:
    FilesystemPath();
    explicit FilesystemPath(const QString& path);

    bool isEmpty() const;
    bool isValid() const;
    bool exists() const;
    bool isFile() const;
    bool isDirectory() const;

    QString fileName() const;
    QString extension() const;
    FilesystemPath parentPath() const;
    FilesystemPath absolutePath() const;
    FilesystemPath canonicalPath() const;
    FilesystemPath join(const QString& subpath) const;
    FilesystemPath join(const FilesystemPath& subpath) const;
    QString toString() const;

    FilesystemPath operator/(const QString& subpath) const;
    FilesystemPath operator/(const FilesystemPath& subpath) const;

    bool operator==(const FilesystemPath& other) const;
    bool operator!=(const FilesystemPath& other) const;

private:
    QString m_path;
};

} // namespace Core::Path
