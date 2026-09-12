#include "ResourceCompilerLogParser.h"

#include <QDir>
#include <QRegularExpression>

namespace Domain::Tool {

ResourceCompilerLogResult ResourceCompilerLogParser::parse(
    const QString& stdOut,
    const QString& stdErr,
    int exitCode,
    const QString& workingDirectory)
{
    ResourceCompilerLogResult result;
    result.rawOutput = stdOut;
    if (!stdErr.isEmpty()) {
        if (!result.rawOutput.isEmpty()) {
            result.rawOutput.append(QLatin1Char('\n'));
        }
        result.rawOutput.append(stdErr);
    }

    // Regex for written file: " - Wrote to: <path>"
    static const QRegularExpression wroteToRegex(
        QStringLiteral("^\\s*-\\s*Wrote to:\\s*(.+)$"),
        QRegularExpression::CaseInsensitiveOption);

    // Regex for resource compile error
    static const QRegularExpression compileErrorRegex(
        QStringLiteral("RESOURCE COMPILE ERROR:\\s*(.+)$"),
        QRegularExpression::CaseInsensitiveOption);

    // Regex for OK summary banner
    static const QRegularExpression okBannerRegex(
        QStringLiteral("OK:\\s*(\\d+)\\s*compiled,\\s*(\\d+)\\s*failed,\\s*(\\d+)\\s*skipped"),
        QRegularExpression::CaseInsensitiveOption);

    // Regex for ERROR summary banner
    static const QRegularExpression errorBannerRegex(
        QStringLiteral("ERROR:\\s*(\\d+)\\s*compiled,\\s*(\\d+)\\s*failed,\\s*(\\d+)\\s*skipped"),
        QRegularExpression::CaseInsensitiveOption);

    bool hasOkBanner = false;
    bool hasErrorBanner = false;

    auto checkAndAddVpcfC = [&result, &workingDirectory](const QString& rawPath) {
        QString resolved = rawPath;
        if (QDir::isRelativePath(resolved) && !workingDirectory.isEmpty()) {
            resolved = QDir(workingDirectory).absoluteFilePath(resolved);
        }
        QString clean = QDir::cleanPath(resolved);
        if (!result.compiledVpcfCPaths.contains(clean)) {
            result.compiledVpcfCPaths.append(clean);
        }
    };

    const QStringList outLines = stdOut.split(QLatin1Char('\n'));
    for (const QString& line : outLines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        // Wrote to pattern
        auto matchWrote = wroteToRegex.match(trimmed);
        if (matchWrote.hasMatch()) {
            QString path = matchWrote.captured(1).trimmed();
            if (path.endsWith(QStringLiteral(".vpcf_c"), Qt::CaseInsensitive)) {
                checkAndAddVpcfC(path);
            }
        }

        // Compile error pattern
        if (!trimmed.contains(QStringLiteral("Look for"), Qt::CaseInsensitive)) {
            auto matchErr = compileErrorRegex.match(trimmed);
            if (matchErr.hasMatch()) {
                QString errStr = matchErr.captured(1).trimmed();
                if (!result.compileErrors.contains(errStr)) {
                    result.compileErrors.append(errStr);
                }
            }
        }

        // Summary banners
        auto matchOk = okBannerRegex.match(trimmed);
        if (matchOk.hasMatch()) {
            hasOkBanner = true;
            result.compiledCount = matchOk.captured(1).toInt();
            result.failedCount = matchOk.captured(2).toInt();
            result.skippedCount = matchOk.captured(3).toInt();
        }

        auto matchError = errorBannerRegex.match(trimmed);
        if (matchError.hasMatch()) {
            hasErrorBanner = true;
            result.compiledCount = matchError.captured(1).toInt();
            result.failedCount = matchError.captured(2).toInt();
            result.skippedCount = matchError.captured(3).toInt();
        }

        // Warning tracking
        if (trimmed.startsWith(QStringLiteral("WARNING:"), Qt::CaseInsensitive)) {
            result.warnings.append(trimmed);
        }
    }

    // Check stderr as well. Plain stderr output is NOT treated as a compile error by
    // itself: resourcecompiler may emit ordinary progress/diagnostic lines on stderr.
    // Only explicit WARNING: lines are tracked; all other stderr lines are retained
    // as warnings for diagnostics.
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
                result.warnings.append(trimmed);
            }
        }
    }

    // If non-zero exit code but no explicit compile errors matched, promote the last
    // meaningful stderr line (then stdout line) so the failure keeps a concrete cause.
    if (exitCode != 0 && result.compileErrors.isEmpty()) {
        const QStringList errLines = stdErr.split(QLatin1Char('\n'));
        for (auto it = errLines.crbegin(); it != errLines.crend(); ++it) {
            QString trimmed = it->trimmed();
            if (!trimmed.isEmpty() &&
                !trimmed.startsWith(QLatin1Char('-')) &&
                !trimmed.startsWith(QLatin1Char('='))) {
                result.compileErrors.append(trimmed);
                break;
            }
        }
    }
    if (exitCode != 0 && result.compileErrors.isEmpty()) {
        for (auto it = outLines.crbegin(); it != outLines.crend(); ++it) {
            QString trimmed = it->trimmed();
            if (!trimmed.isEmpty() &&
                !trimmed.startsWith(QLatin1Char('-')) &&
                !trimmed.startsWith(QLatin1Char('='))) {
                result.compileErrors.append(trimmed);
                break;
            }
        }
    }

    // Determine success
    if (exitCode != 0 || hasErrorBanner || result.failedCount > 0 || !result.compileErrors.isEmpty()) {
        result.success = false;
    } else if (hasOkBanner) {
        result.success = (result.failedCount == 0 && (result.compiledCount > 0 || result.skippedCount > 0));
    } else {
        // Fallback: no banner, exitCode 0, produced compiled files, no compile errors
        result.success = (exitCode == 0 && !result.compiledVpcfCPaths.isEmpty() && result.compileErrors.isEmpty());
    }

    return result;
}

} // namespace Domain::Tool
