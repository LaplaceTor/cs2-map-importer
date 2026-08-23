#include "Domain/Game/SearchPathResolver.h"
#include "Core/Path/PathUtils.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

namespace Domain::Game {

std::vector<SearchTarget> SearchPathResolver::resolve(
    const Core::Path::FilesystemPath& modDirectory,
    const Core::Path::FilesystemPath& baseDirectory,
    const Core::KeyValues::KeyValuesNode* searchPathsNode)
{
    std::vector<SearchTarget> targets;

    const QString modDirPath = modDirectory.toString();
    const QString baseDirPath = baseDirectory.toString();

    QSet<QString> addedFolders;
    QSet<QString> addedVpks;

    if (!modDirPath.isEmpty()) {
        addedFolders.insert(modDirPath);
        targets.push_back(SearchTarget::makeDirectory(modDirectory));
    }

    if (!searchPathsNode) {
        return targets;
    }

    const QRegularExpression placeholderRegex(QStringLiteral("\\|[^|]+\\|"));

    for (const auto& child : searchPathsNode->children()) {
        QString val = child.value().trimmed();
        if (val.isEmpty()) {
            continue;
        }

        // Clean placeholder |gameinfo_path|. then |gameinfo_path| and remove any other |...|
        val.replace(QStringLiteral("|gameinfo_path|."), modDirPath, Qt::CaseInsensitive);
        val.replace(QStringLiteral("|gameinfo_path|"), modDirPath, Qt::CaseInsensitive);
        val.replace(placeholderRegex, QString());

        val = QDir::fromNativeSeparators(val.trimmed());
        QString absPath;
        if (QFileInfo(val).isAbsolute()) {
            absPath = Core::Path::PathUtils::normalize(val);
        } else {
            const QString directCandidate = QDir(baseDirPath).filePath(val);
            const QString gameCandidate = QDir(baseDirPath).filePath(QStringLiteral("game/") + val);
            if (!QDir(directCandidate).exists() && !QFile::exists(directCandidate) &&
                (QDir(gameCandidate).exists() || QFile::exists(gameCandidate) || modDirPath.contains(QStringLiteral("/game/"), Qt::CaseInsensitive))) {
                absPath = Core::Path::PathUtils::normalize(gameCandidate);
            } else {
                absPath = Core::Path::PathUtils::normalize(directCandidate);
            }
        }

        if (absPath.endsWith(QLatin1String("/*"))) {
            continue;
        }

        if (absPath.endsWith(QLatin1String(".vpk"), Qt::CaseInsensitive)) {
            if (!absPath.endsWith(QLatin1String("_dir.vpk"), Qt::CaseInsensitive)) {
                absPath.replace(absPath.size() - 4, 4, QStringLiteral("_dir.vpk"));
            }
            if (!addedVpks.contains(absPath)) {
                addedVpks.insert(absPath);
                targets.push_back(SearchTarget::makeVpk(Core::Path::FilesystemPath(absPath)));
            }
        } else {
            if (!addedFolders.contains(absPath)) {
                addedFolders.insert(absPath);
                targets.push_back(SearchTarget::makeDirectory(Core::Path::FilesystemPath(absPath)));
            }
        }
    }

    return targets;
}

} // namespace Domain::Game

