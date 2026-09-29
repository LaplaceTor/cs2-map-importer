#include "FilesystemPath.h"
#include <QFileInfo>
#include <QDir>
#include <filesystem>
#include <system_error>

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

bool FilesystemPath::isAbsolute() const {
    if (m_path.isEmpty()) {
        return false;
    }
    return QFileInfo(m_path).isAbsolute();
}

bool FilesystemPath::isRelative() const {
    if (m_path.isEmpty()) {
        return false;
    }
    return QFileInfo(m_path).isRelative();
}

bool FilesystemPath::isSubpathOf(const FilesystemPath& baseDir) const {
    if (m_path.isEmpty() || baseDir.isEmpty()) {
        return false;
    }
    QString cleanChild = QDir::cleanPath(absolutePath().toString());
    QString cleanBase = QDir::cleanPath(baseDir.absolutePath().toString());
    if (!cleanBase.endsWith(QLatin1Char('/'))) {
        cleanBase.append(QLatin1Char('/'));
    }
    if (!cleanChild.startsWith(cleanBase, Qt::CaseInsensitive)) {
        return false;
    }

    // Physical canonical containment check: verify that symlinks / junctions / reparse points
    // do not escape baseDir's physical boundary
    std::error_code ecBase;
    auto canonBase = std::filesystem::canonical(baseDir.toString().toStdWString(), ecBase);
    if (!ecBase) {
        QString canonicalBase = QString::fromStdWString(canonBase.wstring());
        if (canonicalBase.startsWith(QStringLiteral("\\\\?\\"))) {
            canonicalBase.remove(0, 4);
        }
        canonicalBase = QDir::fromNativeSeparators(canonicalBase);
        if (!canonicalBase.endsWith(QLatin1Char('/'))) {
            canonicalBase.append(QLatin1Char('/'));
        }

        // Find the deepest existing ancestor of this path
        QFileInfo childInfo(toString());
        while (!childInfo.exists()) {
            const QString parent = childInfo.path();
            if (parent.isEmpty() || parent == childInfo.filePath()) {
                break;
            }
            childInfo = QFileInfo(parent);
        }

        if (childInfo.exists()) {
            std::error_code ecChild;
            auto canonChild = std::filesystem::canonical(childInfo.filePath().toStdWString(), ecChild);
            if (ecChild) {
                // Inaccessible or broken symlink / reparse point
                return false;
            }
            QString canonicalChild = QString::fromStdWString(canonChild.wstring());
            if (canonicalChild.startsWith(QStringLiteral("\\\\?\\"))) {
                canonicalChild.remove(0, 4);
            }
            canonicalChild = QDir::fromNativeSeparators(canonicalChild);
            const QString cleanChildWithSlash = canonicalChild.endsWith(QLatin1Char('/'))
                ? canonicalChild
                : canonicalChild + QLatin1Char('/');
            if (!cleanChildWithSlash.startsWith(canonicalBase, Qt::CaseInsensitive) &&
                canonicalChild.compare(canonicalBase.chopped(1), Qt::CaseInsensitive) != 0) {
                return false;
            }
        }
    }

    return true;
}

bool FilesystemPath::contains(const FilesystemPath& childPath) const {
    return childPath.isSubpathOf(*this);
}

std::optional<FilesystemPath> FilesystemPath::resolveBelow(const QString& subpath) const {
    if (m_path.isEmpty() || subpath.isEmpty()) {
        return std::nullopt;
    }
    FilesystemPath sub(subpath);
    if (sub.isAbsolute()) {
        return std::nullopt;
    }
    QString cleanSub = QDir::cleanPath(subpath);
    while (cleanSub.startsWith(u'/') || cleanSub.startsWith(u'\\')) {
        cleanSub.remove(0, 1);
    }
    if (cleanSub.isEmpty() || cleanSub.startsWith(QStringLiteral("../")) || cleanSub == QStringLiteral("..")) {
        return std::nullopt;
    }
    if (cleanSub.contains(u':')) {
        return std::nullopt;
    }
    FilesystemPath resolved = *this / cleanSub;
    if (!resolved.isSubpathOf(*this)) {
        return std::nullopt;
    }
    return resolved;
}

std::optional<FilesystemPath> FilesystemPath::resolveBelow(const FilesystemPath& subpath) const {
    return resolveBelow(subpath.toString());
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
    std::error_code ec;
    auto canon = std::filesystem::canonical(m_path.toStdWString(), ec);
    if (!ec) {
        QString res = QString::fromStdWString(canon.wstring());
        if (res.startsWith(QStringLiteral("\\\\?\\"))) {
            res.remove(0, 4);
        }
        return FilesystemPath(QDir::fromNativeSeparators(res));
    }
    return FilesystemPath();
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
