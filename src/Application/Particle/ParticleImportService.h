#pragma once

#include <QObject>
#include <QString>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <cstdint>

#include "Application/Particle/ParticleImportDTOs.h"
#include "Application/Async/TaskHandle.h"
#include "Workflow/Common/ImportContext.h"
#include "Workflow/Particle/ParticleImportOptions.h"
#include "Core/Result/Result.h"
#include "Core/Logging/TaskLoggingContext.h"

namespace Application::Common {
class ImportPrerequisiteService;
}

namespace Application::Particle {

/**
 * @brief Application service orchestrating particle import and compilation pipelines.
 *
 * Implements the Application layer contract:
 * - Delegates common environment preparation to Application::Common::ImportPrerequisiteService.
 * - Translates UI/Application ParticleImportRequest into Workflow ParticleImportOptions.
 * - Wraps execution boundaries with Application::Execution::ExecutionGuard.
 * - Bridges asynchronous execution to Worker threads via Application::Async::AsyncTaskRunner.
 * - Manages dual-plane lifecycle: TaskState in LogManager vs typed Result<ParticleImportResult>.
 * - Returns Application::Async::TaskHandle for unified cooperative cancellation and tracking.
 */
class ParticleImportService : public QObject, public std::enable_shared_from_this<ParticleImportService> {
    Q_OBJECT

    // PassKey idiom: enforces that ParticleImportService can only be constructed
    // through its static create() factory, guaranteeing std::shared_ptr management.
    struct PassKey {
        explicit PassKey() = default;
    };

public:
    using WorkflowRunner = std::function<Core::Result<Workflow::Particle::ParticleImportWorkflowResult>(
        const Workflow::Particle::ParticleImportOptions&,
        const Workflow::Common::ImportContext&)>;

    /**
     * @brief Factory method: creates a new ParticleImportService managed by std::shared_ptr.
     *
     * Asynchronous execution and std::enable_shared_from_this require that instances
     * are strictly managed by std::shared_ptr. Direct stack allocation, raw new, or
     * std::make_unique are disallowed by constructor constraints.
     */
    static std::shared_ptr<ParticleImportService> create(
        std::shared_ptr<Common::ImportPrerequisiteService> prerequisiteService = nullptr,
        QObject* parent = nullptr);

    explicit ParticleImportService(
        PassKey,
        std::shared_ptr<Common::ImportPrerequisiteService> prerequisiteService = nullptr,
        QObject* parent = nullptr);
    ~ParticleImportService() override;

    /**
     * @brief Asynchronously imports particles using AsyncTaskRunner.
     * @return TaskHandle providing task status and cancellation. An invalid handle
     *         is returned (with the failure delivered through @p callback) when an
     *         import is already in progress.
     */
    Async::TaskHandle importParticlesAsync(
        const ParticleImportRequest& request,
        std::function<void(const Core::Result<ParticleImportResult>&)> callback);

    /**
     * @brief Requests cancellation of the currently active import operation.
     */
    void cancelCurrentImport();

    /**
     * @brief Returns true if an import operation is currently in progress.
     */
    bool isImporting() const;

    /**
     * @brief Injects a custom workflow runner for testing.
     */
    void setWorkflowRunner(WorkflowRunner runner) noexcept;

    /**
     * @brief Access the common prerequisite service.
     */
    std::shared_ptr<Common::ImportPrerequisiteService> prerequisiteService() const noexcept;
    void setPrerequisiteService(std::shared_ptr<Common::ImportPrerequisiteService> service) noexcept;

signals:
    void isImportingChanged(bool isImporting);

private:
    Core::Result<ParticleImportResult> executeImport(
        const ParticleImportRequest& request,
        const Workflow::Common::ImportContext& context);

    std::shared_ptr<Common::ImportPrerequisiteService> m_prerequisiteService;
    WorkflowRunner m_workflowRunner;
    mutable std::mutex m_mutex;
    Async::TaskHandle m_activeTaskHandle;
    std::atomic<int> m_activeImportsCount{0};
};

} // namespace Application::Particle
