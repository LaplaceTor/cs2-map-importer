#include "Source1ImportLogParser.h"

#include <QDir>
#include <QRegularExpression>
#include <QSet>
#include <QStringTokenizer>
#include <QStringView>

namespace Domain::Tool {

namespace {

void appendUnique(QStringList& target, QSet<QString>& seen, const QString& value)
{
    if (!seen.contains(value)) {
        seen.insert(value);
        target.append(value);
    }
}

} // namespace

Source1ImportLogResult Source1ImportLogParser::parse(
    const QString& stdOut,
    const QString& stdErr,
    int exitCode,
    const QString& workingDirectory)
{
    Source1ImportLogResult result;
    result.rawOutput = stdOut;
    if (!stdErr.isEmpty()) {
        if (!result.rawOutput.isEmpty()) {
            result.rawOutput.append(QLatin1Char('\n'));
        }
        result.rawOutput.append(stdErr);
    }

    // Check for "Found no files matching specifications"
    if (stdOut.contains(QStringLiteral("Found no files matching specification"), Qt::CaseInsensitive) ||
        stdErr.contains(QStringLiteral("Found no files matching specification"), Qt::CaseInsensitive)) {
        result.hasNoMatchingFiles = true;
    }

    // Regex for file writing: Writing file "<path>"
    static const QRegularExpression writingFileRegex(
        QStringLiteral("Writing file \"([^\"]+)\""),
        QRegularExpression::CaseInsensitiveOption);

    // Regex for mapping line: <name> to <dest.vpcf>
    static const QRegularExpression mappingRegex(
        QStringLiteral("^\\s*(\\S+)\\s+to\\s+(.+\\.vpcf)\\s*$"),
        QRegularExpression::CaseInsensitiveOption);

    // Regex for OK summary banner
    static const QRegularExpression okBannerRegex(
        QStringLiteral("OK:\\s*(\\d+)\\s*imported,\\s*(\\d+)\\s*failed,\\s*(\\d+)\\s*skipped(?:,\\s*(\\d+)\\s*unknown)?"),
        QRegularExpression::CaseInsensitiveOption);

    // Regex for ERROR summary banner
    static const QRegularExpression errorBannerRegex(
        QStringLiteral("ERROR:\\s*(\\d+)\\s*imported,\\s*(\\d+)\\s*failed,\\s*(\\d+)\\s*skipped(?:,\\s*(\\d+)\\s*unknown)?"),
        QRegularExpression::CaseInsensitiveOption);

    bool hasOkBanner = false;
    bool hasErrorBanner = false;
    QSet<QString> seenVpcfPaths;
    QSet<QString> seenWarnings;
    QSet<QString> seenErrorMessages;

    auto checkAndAddVpcf = [&result, &seenVpcfPaths, &workingDirectory](QStringView rawPath) {
        if (!rawPath.endsWith(u".vpcf", Qt::CaseInsensitive)) {
            return;
        }
        QString resolved = rawPath.toString();
        if (QDir::isRelativePath(resolved) && !workingDirectory.isEmpty()) {
            resolved = QDir(workingDirectory).absoluteFilePath(resolved);
        }
        const QString clean = QDir::cleanPath(resolved);
        appendUnique(result.generatedVpcfPaths, seenVpcfPaths, clean);
    };

    // Parse standard output line by line
    QStringView lastMeaningfulOutLine;
    for (const QStringView line : QStringView(stdOut).tokenize(u'\n')) {
        const QStringView trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (!trimmed.startsWith(u'-') && !trimmed.startsWith(u'=')) {
            lastMeaningfulOutLine = trimmed;
        }

        // Writing file pattern
        const auto matchWriting = writingFileRegex.matchView(trimmed);
        if (matchWriting.hasMatch()) {
            checkAndAddVpcf(matchWriting.capturedView(1));
        }

        // Mapping pattern
        const auto matchMapping = mappingRegex.matchView(trimmed);
        if (matchMapping.hasMatch()) {
            checkAndAddVpcf(matchMapping.capturedView(2));
        }

        // Summary banners
        const auto matchOk = okBannerRegex.matchView(trimmed);
        if (matchOk.hasMatch()) {
            hasOkBanner = true;
            result.importedCount = matchOk.capturedView(1).toInt();
            result.failedCount = matchOk.capturedView(2).toInt();
            result.skippedCount = matchOk.capturedView(3).toInt();
            if (!matchOk.capturedView(4).isEmpty()) {
                result.unknownCount = matchOk.capturedView(4).toInt();
            }
        }

        const auto matchError = errorBannerRegex.matchView(trimmed);
        if (matchError.hasMatch()) {
            hasErrorBanner = true;
            result.importedCount = matchError.capturedView(1).toInt();
            result.failedCount = matchError.capturedView(2).toInt();
            result.skippedCount = matchError.capturedView(3).toInt();
            if (!matchError.capturedView(4).isEmpty()) {
                result.unknownCount = matchError.capturedView(4).toInt();
            }
        }

        // Warning tracking
        if (trimmed.startsWith(u"WARNING:", Qt::CaseInsensitive)) {
            appendUnique(result.warnings, seenWarnings, trimmed.toString());
        }

        // Error tracking
        if (trimmed.contains(u"*** Error", Qt::CaseInsensitive) ||
            trimmed.startsWith(u"FATAL ERROR:", Qt::CaseInsensitive) ||
            trimmed.startsWith(u"FAILED:", Qt::CaseInsensitive) ||
            trimmed.startsWith(u"Error:", Qt::CaseInsensitive) ||
            trimmed.startsWith(u"Unable to load", Qt::CaseInsensitive) ||
            trimmed.contains(u"Unable to load source 1 mod gameinfo", Qt::CaseInsensitive) ||
            trimmed.contains(u"Failed to make path", Qt::CaseInsensitive) ||
            (trimmed.startsWith(u"Failed to ", Qt::CaseInsensitive) &&
             !trimmed.contains(u"Note this is ok", Qt::CaseInsensitive))) {
            appendUnique(result.errorMessages, seenErrorMessages, trimmed.toString());
        }
    }

    // Inspect stderr. Plain stderr output is NOT treated as an error by itself:
    // source1import may emit ordinary progress/diagnostic lines on stderr. Only
    // explicit WARNING: lines are tracked (with the known "Failed to make path"
    // escalation); all other stderr lines are retained as warnings for diagnostics.
    QStringView lastMeaningfulErrLine;
    if (!stdErr.isEmpty()) {
        for (const QStringView line : QStringView(stdErr).tokenize(u'\n')) {
            const QStringView trimmed = line.trimmed();
            if (trimmed.isEmpty()) {
                continue;
            }
            if (!trimmed.startsWith(u'-') && !trimmed.startsWith(u'=')) {
                lastMeaningfulErrLine = trimmed;
            }
            appendUnique(result.warnings, seenWarnings, trimmed.toString());
            if (trimmed.startsWith(u"WARNING:", Qt::CaseInsensitive) &&
                trimmed.contains(u"Failed to make path", Qt::CaseInsensitive)) {
                appendUnique(result.errorMessages, seenErrorMessages, trimmed.toString());
            }
        }
    }

    // If non-zero exit code but no explicit error messages matched, promote the last
    // meaningful stderr line (then stdout line) so the failure keeps a concrete cause.
    if (exitCode != 0 && result.errorMessages.isEmpty()) {
        if (!lastMeaningfulErrLine.isEmpty()) {
            appendUnique(result.errorMessages, seenErrorMessages, lastMeaningfulErrLine.toString());
        } else if (!lastMeaningfulOutLine.isEmpty()) {
            appendUnique(result.errorMessages, seenErrorMessages, lastMeaningfulOutLine.toString());
        }
    }

    // If 0 files imported and everything was skipped, mark as failure if no error message was extracted
    if (hasOkBanner && result.importedCount == 0 && result.skippedCount > 0 && result.errorMessages.isEmpty()) {
        appendUnique(result.errorMessages, seenErrorMessages,
                     QStringLiteral("No assets imported; %1 asset(s) skipped").arg(result.skippedCount));
    }

    // Determine success:
    // If at least one asset was successfully converted/written, consider it a success (even if some particles failed).
    if (result.hasNoMatchingFiles) {
        result.success = false;
    } else if (!result.generatedVpcfPaths.isEmpty() || result.importedCount > 0) {
        result.success = true;
    } else if (hasOkBanner) {
        result.success = (result.failedCount == 0 && result.importedCount > 0);
    } else {
        // Fallback: no banner, but exitCode == 0, produced files, and no missing files detected
        result.success = (exitCode == 0 && !result.generatedVpcfPaths.isEmpty() && !result.hasNoMatchingFiles);
    }

    return result;
}

} // namespace Domain::Tool
