#include "FilesystemPath.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <filesystem>
#include <system_error>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>

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

    const QString cleanChildWithSlash = cleanChild.endsWith(QLatin1Char('/'))
        ? cleanChild
        : cleanChild + QLatin1Char('/');

    if (!cleanChildWithSlash.startsWith(cleanBase, Qt::CaseInsensitive)) {
        return false;
    }

    // Physical canonical containment check: verify that symlinks / junctions / reparse points
    // do not escape baseDir's physical boundary, even if baseDir or child elements do not yet exist.
    std::error_code ecBase;
    auto canonBaseFs = std::filesystem::weakly_canonical(cleanBase.toStdWString(), ecBase);
    if (ecBase) {
        // Base directory or its existing ancestor cannot be resolved (e.g. invalid drive / syntax)
        return false;
    }

    std::error_code ecChild;
    auto canonChildFs = std::filesystem::weakly_canonical(cleanChild.toStdWString(), ecChild);
    if (ecChild) {
        // Child path cannot be resolved
        return false;
    }

    QString canonicalBase = QString::fromStdWString(canonBaseFs.wstring());
    if (canonicalBase.startsWith(QStringLiteral("\\\\?\\"))) {
        canonicalBase.remove(0, 4);
    }
    canonicalBase = QDir::fromNativeSeparators(canonicalBase);
    if (!canonicalBase.endsWith(QLatin1Char('/'))) {
        canonicalBase.append(QLatin1Char('/'));
    }

    QString canonicalChild = QString::fromStdWString(canonChildFs.wstring());
    if (canonicalChild.startsWith(QStringLiteral("\\\\?\\"))) {
        canonicalChild.remove(0, 4);
    }
    canonicalChild = QDir::fromNativeSeparators(canonicalChild);
    const QString canonicalChildWithSlash = canonicalChild.endsWith(QLatin1Char('/'))
        ? canonicalChild
        : canonicalChild + QLatin1Char('/');

    return canonicalChildWithSlash.startsWith(canonicalBase, Qt::CaseInsensitive);
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

bool FilesystemPath::verifyHandleWithinBase(void* win32Handle, const FilesystemPath& baseDir) {
    if (!win32Handle || win32Handle == INVALID_HANDLE_VALUE || baseDir.isEmpty()) {
        return false;
    }

    HANDLE hFile = static_cast<HANDLE>(win32Handle);
    std::wstring finalPath(32768, L'\0');
    DWORD len = GetFinalPathNameByHandleW(
        hFile,
        finalPath.data(),
        static_cast<DWORD>(finalPath.size()),
        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS
    );
    if (len == 0 || len >= finalPath.size()) {
        return false;
    }
    finalPath.resize(len);

    QString qFinalPath = QString::fromStdWString(finalPath);
    if (qFinalPath.startsWith(QStringLiteral("\\\\?\\"))) {
        qFinalPath.remove(0, 4);
    }
    qFinalPath = QDir::fromNativeSeparators(qFinalPath);

    std::error_code ec;
    auto canonBaseFs = std::filesystem::weakly_canonical(
        baseDir.absolutePath().toString().toStdWString(), ec);
    if (ec) {
        return false;
    }

    QString canonicalBase = QString::fromStdWString(canonBaseFs.wstring());
    if (canonicalBase.startsWith(QStringLiteral("\\\\?\\"))) {
        canonicalBase.remove(0, 4);
    }
    canonicalBase = QDir::fromNativeSeparators(canonicalBase);
    if (!canonicalBase.endsWith(QLatin1Char('/'))) {
        canonicalBase.append(QLatin1Char('/'));
    }

    const QString finalPathWithSlash = qFinalPath.endsWith(QLatin1Char('/'))
        ? qFinalPath
        : qFinalPath + QLatin1Char('/');

    return finalPathWithSlash.startsWith(canonicalBase, Qt::CaseInsensitive);
}

bool FilesystemPath::verifyFileWithinBase(const QFile& file, const FilesystemPath& baseDir) {
    if (!file.isOpen()) {
        return false;
    }
    int fd = file.handle();
    if (fd < 0) {
        return false;
    }
    intptr_t osf = _get_osfhandle(fd);
    if (osf == -1 || osf == 0) {
        return false;
    }
    return verifyHandleWithinBase(reinterpret_cast<void*>(osf), baseDir);
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
