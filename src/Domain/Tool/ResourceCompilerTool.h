#pragma once

#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include "Core/Logging/TaskLoggingContext.h"
#include <QString>
#include <QStringList>

#include "Core/Async/CancellationToken.h"

namespace Domain::Tool {

/**
 * @brief Options for invoking resourcecompiler.exe.
 */
struct ResourceCompilerOptions {
    Core::Path::FilesystemPath gameDir; // e.g. <cs2Base>/game/csgo
    QStringList inputFiles;             // list of .vpcf files
    bool forceCompile = true;
    bool verbose = true;
    int timeoutMs = 120000;
    Core::Async::CancellationToken cancellationToken;
};

/**
 * @brief High-level tool result from resourcecompiler.exe.
 */
struct ResourceCompilerToolResult {
    bool success = false;
    int compiledCount = 0;
    int failedCount = 0;
    int skippedCount = 0;
    QStringList compiledVpcfCPaths;
    QStringList compileErrors;
    QString rawOutput;
};

/**
 * @brief Domain CLI wrapper for Valve's resourcecompiler.exe tool.
 */
class ResourceCompilerTool {
public:
    ResourceCompilerTool() = default;
    explicit ResourceCompilerTool(Core::Path::FilesystemPath toolBinaryPath)
        : m_toolBinaryPath(std::move(toolBinaryPath)) {}

    static QStringList buildArguments(const ResourceCompilerOptions& options);

    static Core::Result<ResourceCompilerToolResult> compileResources(
        const Core::Path::FilesystemPath& toolBinaryPath,
        const ResourceCompilerOptions& options,
        Core::Logging::TaskLoggingContext* taskCtx = nullptr);

    Core::Result<ResourceCompilerToolResult> compileResources(
        const ResourceCompilerOptions& options,
        Core::Logging::TaskLoggingContext* taskCtx = nullptr) const
    {
        return compileResources(m_toolBinaryPath, options, taskCtx);
    }

    const Core::Path::FilesystemPath& toolBinaryPath() const noexcept { return m_toolBinaryPath; }
    void setToolBinaryPath(Core::Path::FilesystemPath path) noexcept { m_toolBinaryPath = std::move(path); }

private:
    Core::Path::FilesystemPath m_toolBinaryPath;
};

} // namespace Domain::Tool
