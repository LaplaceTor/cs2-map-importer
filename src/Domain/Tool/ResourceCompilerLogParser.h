#pragma once

#include <QString>
#include <QStringList>

namespace Domain::Tool {

/**
 * @brief Structured result parsed from resourcecompiler.exe standard output/error.
 */
struct ResourceCompilerLogResult {
    bool success = false;
    int compiledCount = 0;
    int failedCount = 0;
    int skippedCount = 0;
    QStringList compiledVpcfCPaths;
    QStringList compileErrors;
    QStringList warnings;
    QString rawOutput;
};

/**
 * @brief Parser for resourcecompiler.exe CLI output logs.
 */
class ResourceCompilerLogParser {
public:
    static ResourceCompilerLogResult parse(
        const QString& stdOut,
        const QString& stdErr = QString(),
        int exitCode = 0);
};

} // namespace Domain::Tool
