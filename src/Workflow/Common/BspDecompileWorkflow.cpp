#include <QCoreApplication>

#include "Workflow/Common/BspDecompileWorkflow.h"

#include <utility>

#include "Core/Logging/TaskLoggingContext.h"

namespace Workflow::Common {

Core::Result<Domain::Bsp::BspDecompileResult> BspDecompileWorkflow::decompile(
    const Core::Path::FilesystemPath& bspPath,
    const Core::Path::FilesystemPath& outputVmfPath,
    ImportContext& context,
    Domain::Bsp::BspDecompileConfig config,
    const Core::Path::FilesystemPath& expectedBaseDir)
{
    config.token = context.token();

    Core::Logging::TaskLoggingContext* taskLog = context.loggingContext();
    auto existingOnLog = std::move(config.onLog);
    if (taskLog || existingOnLog) {
        config.onLog = [taskLog, existingOnLog = std::move(existingOnLog)](
                           Domain::Bsp::BspLogLevel level, const QString& message) {
            if (taskLog) {
                switch (level) {
                case Domain::Bsp::BspLogLevel::Debug:
                    taskLog->debug(message);
                    break;
                case Domain::Bsp::BspLogLevel::Info:
                    taskLog->info(message);
                    break;
                case Domain::Bsp::BspLogLevel::Warning:
                    taskLog->warning(message);
                    break;
                case Domain::Bsp::BspLogLevel::Error:
                    taskLog->error(message);
                    break;
                }
            }
            if (existingOnLog) {
                existingOnLog(level, message);
            }
        };
    }

    if (taskLog) {
        taskLog->info(QCoreApplication::translate("BspDecompileWorkflow", "Decompiling BSP '%1' to '%2'")
                          .arg(bspPath.toString(), outputVmfPath.toString()));
    }

    auto result = Domain::Bsp::BspDecompiler::decompile(
        bspPath, outputVmfPath, config, expectedBaseDir);

    if (taskLog) {
        if (result.isSuccess()) {
            const auto& val = result.value();
            const QString gameLabel = val.detectedGameName.isEmpty()
                ? QString::number(val.detectedAppId)
                : val.detectedGameName;
            if (val.isProtected) {
                taskLog->warning(QCoreApplication::translate(
                                     "BspDecompileWorkflow",
                                     "Decompiled protected BSP '%1' (v%2, %3, methods: %4) to '%5'")
                                     .arg(bspPath.toString())
                                     .arg(val.bspVersion)
                                     .arg(gameLabel,
                                          val.protectionMethods.join(QStringLiteral(", ")),
                                          val.outputVmfPath.toString()));
            } else {
                taskLog->info(QCoreApplication::translate(
                                  "BspDecompileWorkflow",
                                  "Decompiled BSP '%1' (v%2, %3) to '%4'")
                                  .arg(bspPath.toString())
                                  .arg(val.bspVersion)
                                  .arg(gameLabel, val.outputVmfPath.toString()));
            }
        } else if (result.isCancelled()) {
            taskLog->warning(QCoreApplication::translate("BspDecompileWorkflow", "BSP decompilation cancelled"));
        } else if (result.isFailure()) {
            taskLog->error(QCoreApplication::translate("BspDecompileWorkflow", "Failed to decompile BSP '%1': %2")
                               .arg(bspPath.toString(), result.message()));
        }
    }

    return result;
}

} // namespace Workflow::Common
