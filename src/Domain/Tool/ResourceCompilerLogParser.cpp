#include "ResourceCompilerLogParser.h"

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
    QSet<QString> seenVpcfCPaths;
    QSet<QString> seenCompileErrors;
    QSet<QString> seenWarnings;

    auto checkAndAddVpcfC = [&result, &seenVpcfCPaths, &workingDirectory](QStringView rawPath) {
        QString resolved = rawPath.toString();
        if (QDir::isRelativePath(resolved) && !workingDirectory.isEmpty()) {
            resolved = QDir(workingDirectory).absoluteFilePath(resolved);
        }
        const QString clean = QDir::cleanPath(resolved);
        appendUnique(result.compiledVpcfCPaths, seenVpcfCPaths, clean);
    };

    QStringView lastMeaningfulOutLine;
    for (const QStringView line : QStringView(stdOut).tokenize(u'\n')) {
        const QStringView trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (!trimmed.startsWith(u'-') && !trimmed.startsWith(u'=')) {
            lastMeaningfulOutLine = trimmed;
        }

        // Wrote to pattern
        const auto matchWrote = wroteToRegex.matchView(trimmed);
        if (matchWrote.hasMatch()) {
            const QStringView path = matchWrote.capturedView(1).trimmed();
            if (path.endsWith(u".vpcf_c", Qt::CaseInsensitive)) {
                checkAndAddVpcfC(path);
            }
        }

        // Compile error pattern
        if (!trimmed.contains(u"Look for", Qt::CaseInsensitive)) {
            const auto matchErr = compileErrorRegex.matchView(trimmed);
            if (matchErr.hasMatch()) {
                appendUnique(result.compileErrors, seenCompileErrors, matchErr.capturedView(1).trimmed().toString());
            }
        }

        // Summary banners
        const auto matchOk = okBannerRegex.matchView(trimmed);
        if (matchOk.hasMatch()) {
            hasOkBanner = true;
            result.compiledCount = matchOk.capturedView(1).toInt();
            result.failedCount = matchOk.capturedView(2).toInt();
            result.skippedCount = matchOk.capturedView(3).toInt();
        }

        const auto matchError = errorBannerRegex.matchView(trimmed);
        if (matchError.hasMatch()) {
            hasErrorBanner = true;
            result.compiledCount = matchError.capturedView(1).toInt();
            result.failedCount = matchError.capturedView(2).toInt();
            result.skippedCount = matchError.capturedView(3).toInt();
        }

        // Warning tracking
        if (trimmed.startsWith(u"WARNING:", Qt::CaseInsensitive)) {
            appendUnique(result.warnings, seenWarnings, trimmed.toString());
        }
    }

    // Check stderr as well. Plain stderr output is NOT treated as a compile error by
    // itself: resourcecompiler may emit ordinary progress/diagnostic lines on stderr.
    // All non-empty stderr lines are retained as warnings for diagnostics while tracking
    // the last meaningful line for non-zero exit fallback.
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
        }
    }

    // If non-zero exit code but no explicit compile errors matched, promote the last
    // meaningful stderr line (then stdout line) so the failure keeps a concrete cause.
    if (exitCode != 0 && result.compileErrors.isEmpty()) {
        if (!lastMeaningfulErrLine.isEmpty()) {
            appendUnique(result.compileErrors, seenCompileErrors, lastMeaningfulErrLine.toString());
        } else if (!lastMeaningfulOutLine.isEmpty()) {
            appendUnique(result.compileErrors, seenCompileErrors, lastMeaningfulOutLine.toString());
        }
    }

    // Determine success:
    // If at least one file was compiled to .vpcf_c, or compiledCount > 0, consider it a success (even if some failed).
    if (!result.compiledVpcfCPaths.isEmpty() || result.compiledCount > 0) {
        result.success = true;
    } else if (hasOkBanner) {
        result.success = (result.failedCount == 0 && (result.compiledCount > 0 || result.skippedCount > 0));
    } else {
        // Fallback: no banner, exitCode 0, produced compiled files, no compile errors
        result.success = (exitCode == 0 && !result.compiledVpcfCPaths.isEmpty() && result.compileErrors.isEmpty());
    }

    return result;
}

} // namespace Domain::Tool
