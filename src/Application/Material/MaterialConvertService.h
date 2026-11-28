#pragma once

#include <QObject>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

#include "Application/Async/TaskHandle.h"
#include "Application/Material/MaterialConvertDTOs.h"
#include "Core/Result/Result.h"

namespace Application::Common {
class ImportPrerequisiteService;
}

namespace Application::Material {

/**
 * @brief Application service orchestrating Source 1 to CS2 material conversion pipelines.
 *
 * Implements the Application layer contract:
 * - Delegates common environment preparation to Application::Common::ImportPrerequisiteService.
 * - Translates Application MaterialConvertRequest into Workflow MaterialConvertOptions.
 * - Bridges asynchronous execution to Worker threads via Application::Async::AsyncTaskRunner.
 * - Enforces single-task concurrency and delivers QObject signals on the object's owning thread.
 * - Returns Application::Async::TaskHandle for unified cooperative cancellation and tracking.
 */
class MaterialConvertService
    : public QObject
    , public std::enable_shared_from_this<MaterialConvertService> {
    Q_OBJECT

    // PassKey idiom: enforces that MaterialConvertService can only be constructed
    // through its static create() factory, guaranteeing std::shared_ptr management.
    struct PassKey {
        explicit PassKey() = default;
    };

public:
    using ConvertCallback = std::function<void(const Core::Result<MaterialConvertResultDTO>&)>;

    /**
     * @brief Factory method: creates a new MaterialConvertService managed by std::shared_ptr.
     *
     * Single Ownership Guarantee:
     * This service strictly disallows Qt parent-child ownership (parent is always nullptr)
     * to eliminate dual-ownership hazards.
     */
    static std::shared_ptr<MaterialConvertService> create(
        std::shared_ptr<Common::ImportPrerequisiteService> prerequisiteService = nullptr);

    explicit MaterialConvertService(
        PassKey,
        std::shared_ptr<Common::ImportPrerequisiteService> prerequisiteService = nullptr);
    ~MaterialConvertService() override;

    /**
     * @brief Asynchronously converts materials, explicit sprites, and skyboxes via AsyncTaskRunner.
     * @return TaskHandle providing task status and cancellation. An invalid handle is returned
     *         (with the failure delivered through @p callback) when a conversion is already in
     *         progress or the service is not managed by std::shared_ptr.
     */
    [[nodiscard]] Async::TaskHandle convertMaterialsAsync(
        const MaterialConvertRequest& request,
        ConvertCallback callback);

    /**
     * @brief Requests cancellation of the currently active conversion operation.
     */
    void cancelCurrentConversion();

    /**
     * @brief Returns true if a material conversion operation is currently in progress.
     */
    bool isConverting() const noexcept;

    /**
     * @brief Access or replace the common prerequisite service.
     */
    std::shared_ptr<Common::ImportPrerequisiteService> prerequisiteService() const;
    void setPrerequisiteService(std::shared_ptr<Common::ImportPrerequisiteService> service);

signals:
    void isConvertingChanged(bool converting);

private:
    std::shared_ptr<Common::ImportPrerequisiteService> m_prerequisiteService;
    mutable std::mutex m_mutex;
    Async::TaskHandle m_activeTaskHandle;
    std::atomic<bool> m_isConverting{false};
};

} // namespace Application::Material
