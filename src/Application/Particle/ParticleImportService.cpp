#include "Application/Particle/ParticleImportService.h"

#include <QFile>
#include <QFileInfo>

#include "Application/Execution/ExecutionGuard.h"
#include "Application/Async/AsyncTaskRunner.h"
#include "Application/Common/ImportPrerequisiteService.h"
#include "Workflow/Particle/ParticleImportWorkflow.h"
#include "Core/Error/ErrorCode.h"

namespace Application::Particle {

namespace {

Workflow::Particle::ParticleImportOptions toWorkflowOptions(
    const ParticleImportRequest& request,
    const Common::ValidatedBaseImport& base,
    const QString& trimmedPcfPath)
{
    Workflow::Particle::ParticleImportOptions options;
    options.source1GameDir = Core::Path::FilesystemPath(base.source1GameDir);
    options.s1GameInfoDir = Core::Path::FilesystemPath(base.s1GameInfoDir);
    options.cs2BaseDir = Core::Path::FilesystemPath(base.cs2BaseDir);
    options.addonName = base.addonName;
    options.sourcePcfPath = Core::Path::FilesystemPath(trimmedPcfPath);
    options.allowDepthBlend = request.allowDepthBlend;
    options.disableDiffuse = request.disableDiffuse;
    options.isCsgo = request.isCsgo;
    options.source1ImportExe = base.source1ImportExe;
    options.resourceCompilerExe = base.resourceCompilerExe;
    return options;
}

ParticleImportResult toApplicationResult(const Workflow::Particle::ParticleImportWorkflowResult& wfVal)
{
    ParticleImportResult appResult;
    appResult.succeeded = true;
    appResult.generatedVpcfFiles = wfVal.generatedVpcfFiles;
    appResult.compiledVpcfCFiles = wfVal.compiledVpcfCFiles;
    appResult.totalConverted = wfVal.totalConverted;
    appResult.totalCompiled = wfVal.totalCompiled;
    return appResult;
}

} // namespace


ParticleImportService::ParticleImportService(
    std::shared_ptr<Common::ImportPrerequisiteService> prerequisiteService,
    QObject* parent)
    : QObject(parent)
    , m_prerequisiteService(prerequisiteService
          ? std::move(prerequisiteService)
          : std::make_shared<Common::ImportPrerequisiteService>())
    , m_workflowRunner(&Workflow::Particle::ParticleImportWorkflow::run)
{
}

ParticleImportService::~ParticleImportService()
{
    cancelCurrentImport();
}

Async::TaskHandle ParticleImportService::importParticlesAsync(
    const ParticleImportRequest& request,
    std::function<void(const Core::Result<ParticleImportResult>&)> callback)
{
    // Reject overlapping imports atomically: a second concurrent import would
    // overwrite m_activeTaskHandle and become uncancellable.
    if (m_activeImportsCount.fetch_add(1, std::memory_order_relaxed) != 0) {
        m_activeImportsCount.fetch_sub(1, std::memory_order_relaxed);
        if (callback) {
            callback(Core::Result<ParticleImportResult>::failure(
                Core::Error::ErrorCode::InvalidState,
                QStringLiteral("Another import operation is already in progress")));
        }
        return Async::TaskHandle{};
    }
    emit isImportingChanged(true);

    // Workers hold a shared_ptr so the service stays alive for the whole task
    // duration even if the owner drops it mid-flight.
    const auto self = shared_from_this();

    const QString pcfFileName = request.sourcePcfPath.trimmed().isEmpty()
        ? QStringLiteral("PCF")
        : QFileInfo(request.sourcePcfPath.trimmed()).fileName();
    const QString pcfBaseName = request.sourcePcfPath.trimmed().isEmpty()
        ? QStringLiteral("pcf")
        : QFileInfo(request.sourcePcfPath.trimmed()).completeBaseName();
    const QString taskName = QStringLiteral("Import Particle: %1").arg(pcfFileName);

    auto worker = [self, request](std::shared_ptr<Core::Logging::TaskLoggingContext> taskCtx,
                                  Core::Async::CancellationToken token) -> Core::Result<ParticleImportResult> {
        Workflow::Common::ImportContext ctx(taskCtx.get(), std::move(token));
        return self->executeImport(request, ctx);
    };

    auto completionCallback = [self, userCallback = std::move(callback)](const Core::Result<ParticleImportResult>& result) {
        if (self->m_activeImportsCount.fetch_sub(1, std::memory_order_relaxed) == 1) {
            {
                std::lock_guard<std::mutex> lock(self->m_mutex);
                self->m_activeTaskHandle = Async::TaskHandle{};
            }
            emit self->isImportingChanged(false);
        }
        if (userCallback) {
            userCallback(result);
        }
    };

    Async::TaskHandle handle = Async::AsyncTaskRunner::runWorkflowTask<ParticleImportResult>(
        taskName,
        pcfBaseName,
        self.get(),
        std::move(worker),
        std::move(completionCallback),
        QThreadPool::globalInstance());

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activeTaskHandle = handle;
    }

    return handle;
}

void ParticleImportService::cancelCurrentImport()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_activeTaskHandle.isValid()) {
        m_activeTaskHandle.cancel();
    }
}

bool ParticleImportService::isImporting() const
{
    return m_activeImportsCount.load(std::memory_order_relaxed) > 0;
}

void ParticleImportService::setWorkflowRunner(WorkflowRunner runner) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (runner) {
        m_workflowRunner = std::move(runner);
    } else {
        m_workflowRunner = &Workflow::Particle::ParticleImportWorkflow::run;
    }
}

std::shared_ptr<Common::ImportPrerequisiteService> ParticleImportService::prerequisiteService() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_prerequisiteService;
}

void ParticleImportService::setPrerequisiteService(
    std::shared_ptr<Common::ImportPrerequisiteService> service) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_prerequisiteService = service
        ? std::move(service)
        : std::make_shared<Common::ImportPrerequisiteService>();
}

Core::Result<ParticleImportResult> ParticleImportService::executeImport(
    const ParticleImportRequest& request,
    const Workflow::Common::ImportContext& context)
{
    return Execution::ExecutionGuard::guard<ParticleImportResult>([&]() -> Core::Result<ParticleImportResult> {
        // Step 1: Common prerequisite preparation (validation & lease acquisition)
        std::shared_ptr<Common::ImportPrerequisiteService> prereqService;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            prereqService = m_prerequisiteService;
        }

        auto prereqResult = prereqService->prepare(request, context);
        if (!prereqResult.isSuccess()) {
            if (prereqResult.isCancelled()) {
                return Core::Result<ParticleImportResult>::cancelled(prereqResult.message());
            }
            if (prereqResult.isSkipped()) {
                return Core::Result<ParticleImportResult>::skipped(prereqResult.message());
            }
            return Core::Result<ParticleImportResult>::failure(prereqResult.error(), prereqResult.message());
        }

        const auto& base = prereqResult.value();

        // Step 2: Validate particle-specific request parameters
        const QString trimmedPcfPath = request.sourcePcfPath.trimmed();
        if (trimmedPcfPath.isEmpty()) {
            return Core::Result<ParticleImportResult>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QStringLiteral("Source PCF path cannot be empty"));
        }
        if (!QFile::exists(trimmedPcfPath)) {
            return Core::Result<ParticleImportResult>::failure(
                Core::Error::ErrorCode::FileNotFound,
                QStringLiteral("Source PCF file does not exist"),
                trimmedPcfPath);
        }

        context.info(QStringLiteral("Starting particle import for addon '%1' with PCF '%2'")
            .arg(base.addonName, trimmedPcfPath));

        // Step 3: Build Workflow Options & Invoke Workflow
        const auto options = toWorkflowOptions(request, base, trimmedPcfPath);

        WorkflowRunner runner;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            runner = m_workflowRunner;
        }
        auto wfResult = runner(options, context);

        if (wfResult.isCancelled()) {
            context.info(QStringLiteral("Particle import workflow was cancelled"));
            return Core::Result<ParticleImportResult>::cancelled(wfResult.message());
        }

        if (wfResult.isFailure()) {
            context.error(QStringLiteral("Particle import workflow failed: %1").arg(wfResult.message()));
            return Core::Result<ParticleImportResult>::failure(wfResult.error(), wfResult.message());
        }

        if (wfResult.isSkipped()) {
            context.info(QStringLiteral("Particle import workflow was skipped: %1").arg(wfResult.message()));
            return Core::Result<ParticleImportResult>::skipped(wfResult.message());
        }

        // Step 4: Map Workflow Result to Application Result DTO
        const auto appResult = toApplicationResult(wfResult.value());

        context.info(QStringLiteral("Particle import workflow finished successfully: %1 converted, %2 compiled")
            .arg(appResult.totalConverted).arg(appResult.totalCompiled));

        return Core::Result<ParticleImportResult>::success(appResult, wfResult.message());
    }, QStringLiteral("Particle import failed"));
}

} // namespace Application::Particle
