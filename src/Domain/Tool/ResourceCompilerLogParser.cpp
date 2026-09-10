#include "ResourceCompilerLogParser.h"

#include <QDir>
#include <QRegularExpression>

namespace Domain::Tool {

ResourceCompilerLogResult ResourceCompilerLogParser::parse(
    const QString& stdOut,
    const QString& stdErr,
    int exitCode)
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
                QString clean = QDir::cleanPath(path);
                if (!result.compiledVpcfCPaths.contains(clean)) {
                    result.compiledVpcfCPaths.append(clean);
                }
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

    // Check stderr as well
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
                if (!result.compileErrors.contains(trimmed)) {
                    result.compileErrors.append(trimmed);
                }
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
