#pragma once

#include <QString>
#include <QStringList>

namespace Domain::Tool {

/**
 * @brief Structured result parsed from source1import.exe standard output/error.
 */
struct Source1ImportLogResult {
    bool success = false;
    bool hasNoMatchingFiles = false;
    int importedCount = 0;
    int failedCount = 0;
    int skippedCount = 0;
    int unknownCount = 0;
    QStringList generatedVpcfPaths;
    QStringList warnings;
    QStringList errorMessages;
    QString rawOutput;
};

/**
 * @brief Parser for source1import.exe CLI output logs.
 */
class Source1ImportLogParser {
public:
    static Source1ImportLogResult parse(
        const QString& stdOut,
        const QString& stdErr = QString(),
        int exitCode = 0);
};

} // namespace Domain::Tool
