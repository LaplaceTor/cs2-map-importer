#include "Source1ImportLogParser.h"

#include <QDir>
#include <QRegularExpression>

namespace Domain::Tool {

Source1ImportLogResult Source1ImportLogParser::parse(
    const QString& stdOut,
    const QString& stdErr,
    int exitCode)
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

    auto checkAndAddVpcf = [&result](const QString& rawPath) {
        if (rawPath.endsWith(QStringLiteral(".vpcf"), Qt::CaseInsensitive)) {
            QString clean = QDir::cleanPath(rawPath);
            if (!result.generatedVpcfPaths.contains(clean)) {
                result.generatedVpcfPaths.append(clean);
            }
        }
    };

    // Parse standard output line by line
    const QStringList outLines = stdOut.split(QLatin1Char('\n'));
    for (const QString& line : outLines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        // Writing file pattern
        auto matchWriting = writingFileRegex.match(trimmed);
        if (matchWriting.hasMatch()) {
            checkAndAddVpcf(matchWriting.captured(1));
        }

        // Mapping pattern
        auto matchMapping = mappingRegex.match(trimmed);
        if (matchMapping.hasMatch()) {
            checkAndAddVpcf(matchMapping.captured(2));
        }

        // Summary banners
        auto matchOk = okBannerRegex.match(trimmed);
        if (matchOk.hasMatch()) {
            hasOkBanner = true;
            result.importedCount = matchOk.captured(1).toInt();
            result.failedCount = matchOk.captured(2).toInt();
            result.skippedCount = matchOk.captured(3).toInt();
            if (!matchOk.captured(4).isEmpty()) {
                result.unknownCount = matchOk.captured(4).toInt();
            }
        }

        auto matchError = errorBannerRegex.match(trimmed);
        if (matchError.hasMatch()) {
            hasErrorBanner = true;
            result.importedCount = matchError.captured(1).toInt();
            result.failedCount = matchError.captured(2).toInt();
            result.skippedCount = matchError.captured(3).toInt();
            if (!matchError.captured(4).isEmpty()) {
                result.unknownCount = matchError.captured(4).toInt();
            }
        }

        // Warning tracking
        if (trimmed.startsWith(QStringLiteral("WARNING:"), Qt::CaseInsensitive)) {
            result.warnings.append(trimmed);
        }

        // Error tracking
        if (trimmed.contains(QStringLiteral("*** Error"), Qt::CaseInsensitive) ||
            trimmed.startsWith(QStringLiteral("FATAL ERROR:"), Qt::CaseInsensitive) ||
            trimmed.startsWith(QStringLiteral("FAILED:"), Qt::CaseInsensitive)) {
            result.errorMessages.append(trimmed);
        }
    }

    // Also inspect stderr for errors and warnings
    if (!stdErr.isEmpty()) {
        const QStringList errLines = stdErr.split(QLatin1Char('\n'));
        for (const QString& line : errLines) {
            QString trimmed = line.trimmed();
            if (trimmed.isEmpty()) {
                continue;
            }
            if (trimmed.startsWith(QStringLiteral("WARNING:"), Qt::CaseInsensitive)) {
                result.warnings.append(trimmed);
            } else {
                result.errorMessages.append(trimmed);
            }
        }
    }

    // Determine success
    if (result.hasNoMatchingFiles || exitCode != 0 || hasErrorBanner || result.failedCount > 0) {
        result.success = false;
    } else if (hasOkBanner) {
        result.success = (result.failedCount == 0 && (result.importedCount > 0 || result.skippedCount > 0));
    } else {
        // Fallback: no banner, but exitCode == 0, produced files, and no missing files detected
        result.success = (exitCode == 0 && !result.generatedVpcfPaths.isEmpty() && !result.hasNoMatchingFiles);
    }

    return result;
}

} // namespace Domain::Tool
