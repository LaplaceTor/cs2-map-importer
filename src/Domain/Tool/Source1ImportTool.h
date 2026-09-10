#pragma once

#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include "Core/Logging/TaskLoggingContext.h"
#include <QString>
#include <QStringList>

namespace Domain::Tool {

/**
 * @brief Options for invoking source1import.exe.
 */
struct Source1ImportOptions {
    Core::Path::FilesystemPath source1GameInfoDir;
    QString addonName;
    Core::Path::FilesystemPath inputPcfPath;
    bool allowDepthBlend = false;
    bool disableDiffuse = false;
    bool isCsgo = false;
    int timeoutMs = 120000;
};

/**
 * @brief High-level tool result from source1import.exe.
 */
struct Source1ImportToolResult {
    bool success = false;
    int importedCount = 0;
    int failedCount = 0;
    int skippedCount = 0;
    QStringList generatedVpcfPaths;
    QString rawOutput;
};

/**
 * @brief Domain CLI wrapper for Valve's source1import.exe tool.
 */
class Source1ImportTool {
public:
    Source1ImportTool() = default;
    explicit Source1ImportTool(Core::Path::FilesystemPath toolBinaryPath)
        : m_toolBinaryPath(std::move(toolBinaryPath)) {}

    static QStringList buildArguments(const Source1ImportOptions& options);

    static Core::Result<Source1ImportToolResult> convertPcf(
        const Core::Path::FilesystemPath& toolBinaryPath,
        const Source1ImportOptions& options,
        Core::Logging::TaskLoggingContext* taskCtx = nullptr);

    Core::Result<Source1ImportToolResult> convertPcf(
        const Source1ImportOptions& options,
        Core::Logging::TaskLoggingContext* taskCtx = nullptr) const
    {
        return convertPcf(m_toolBinaryPath, options, taskCtx);
    }

    const Core::Path::FilesystemPath& toolBinaryPath() const noexcept { return m_toolBinaryPath; }
    void setToolBinaryPath(Core::Path::FilesystemPath path) noexcept { m_toolBinaryPath = std::move(path); }

private:
    Core::Path::FilesystemPath m_toolBinaryPath;
};

} // namespace Domain::Tool
