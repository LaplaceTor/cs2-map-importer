#include "FilesystemPath.h"
#include <QFileInfo>
#include <QDir>

namespace Core::Path {

FilesystemPath::FilesystemPath()
    : m_path() {
}

FilesystemPath::FilesystemPath(const QString& path)
    : m_path(QDir::cleanPath(path)) {
}

bool FilesystemPath::isEmpty() const {
    return m_path.isEmpty();
}

bool FilesystemPath::isValid() const {
    return !m_path.isEmpty();
}

bool FilesystemPath::exists() const {
    if (m_path.isEmpty()) {
        return false;
    }
    return QFileInfo(m_path).exists();
}

bool FilesystemPath::isFile() const {
    if (m_path.isEmpty()) {
        return false;
    }
    return QFileInfo(m_path).isFile();
}

bool FilesystemPath::isDirectory() const {
    if (m_path.isEmpty()) {
        return false;
    }
    return QFileInfo(m_path).isDir();
}

QString FilesystemPath::fileName() const {
    if (m_path.isEmpty()) {
        return QString();
    }
    return QFileInfo(m_path).fileName();
}

QString FilesystemPath::extension() const {
    if (m_path.isEmpty()) {
        return QString();
    }
    return QFileInfo(m_path).suffix();
}

FilesystemPath FilesystemPath::parentPath() const {
    if (m_path.isEmpty()) {
        return FilesystemPath();
    }
    return FilesystemPath(QFileInfo(m_path).path());
}

FilesystemPath FilesystemPath::absolutePath() const {
    if (m_path.isEmpty()) {
        return FilesystemPath();
    }
    return FilesystemPath(QFileInfo(m_path).absoluteFilePath());
}

FilesystemPath FilesystemPath::canonicalPath() const {
    if (m_path.isEmpty()) {
        return FilesystemPath();
    }
    QString canonical = QFileInfo(m_path).canonicalFilePath();
    if (canonical.isEmpty()) {
        return FilesystemPath();
    }
    return FilesystemPath(canonical);
}

FilesystemPath FilesystemPath::join(const QString& subpath) const {
    if (m_path.isEmpty()) {
        return FilesystemPath(subpath);
    }
    if (subpath.isEmpty()) {
        return *this;
    }
    return FilesystemPath(QDir(m_path).filePath(subpath));
}

FilesystemPath FilesystemPath::join(const FilesystemPath& subpath) const {
    return join(subpath.toString());
}

FilesystemPath FilesystemPath::operator/(const QString& subpath) const {
    return join(subpath);
}

FilesystemPath FilesystemPath::operator/(const FilesystemPath& subpath) const {
    return join(subpath.toString());
}

QString FilesystemPath::toString() const {
    return m_path;
}

bool FilesystemPath::operator==(const FilesystemPath& other) const {
    return m_path == other.m_path;
}

bool FilesystemPath::operator!=(const FilesystemPath& other) const {
    return m_path != other.m_path;
}

} // namespace Core::Path
