#include "Application/Material/MaterialConvertService.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMetaObject>
#include <QScopeGuard>
#include <QSet>
#include <QThreadPool>

#include "Application/Async/AsyncTaskRunner.h"
#include "Application/Common/ImportPrerequisiteService.h"
#include "Application/Environment/GameEnvironmentService.h"
#include "Application/Package/VpkIndexService.h"
#include "Core/Error/ErrorCode.h"
#include "Core/Logging/TaskLoggingContext.h"
#include "Domain/Game/GameInstallationResolver.h"
#include "Workflow/Common/ImportContext.h"
#include "Workflow/Material/MaterialConvertOptions.h"
#include "Workflow/Material/MaterialConvertWorkflow.h"

namespace Application::Material {

namespace {

QStringList cleanRelativeList(const QStringList& rawItems)
{
    QStringList cleaned;
    cleaned.reserve(rawItems.size());
    for (const QString& item : rawItems) {
        const QString trimmed = item.trimmed();
        if (!trimmed.isEmpty()) {
            cleaned.append(trimmed);
        }
    }
    return cleaned;
}

QStringList cleanLocalPathList(const QStringList& rawItems)
{
    QStringList cleaned;
    QSet<QString> seen;
    cleaned.reserve(rawItems.size());
    for (const QString& item : rawItems) {
        const QString path = Environment::GameEnvironmentService::cleanPath(item);
        if (path.isEmpty()) {
            continue;
        }
        const QString key = path.toLower();
        if (!seen.contains(key)) {
            seen.insert(key);
            cleaned.append(path);
        }
    }
    return cleaned;
}

MaterialConvertRequest sanitizeRequest(const MaterialConvertRequest& request)
{
    MaterialConvertRequest cleaned = request;
    cleaned.source1GameDir = Environment::GameEnvironmentService::cleanPath(request.source1GameDir);
    cleaned.cs2BaseDir = Environment::GameEnvironmentService::cleanPath(request.cs2BaseDir);
    cleaned.s1GameInfoDir = Environment::GameEnvironmentService::cleanPath(request.s1GameInfoDir);
    cleaned.addonName = request.addonName.trimmed();
    cleaned.gameId = request.gameId.trimmed();
    cleaned.sourceVmtPaths = cleanLocalPathList(request.sourceVmtPaths);
    cleaned.materialRelativePaths = cleanRelativeList(request.materialRelativePaths);
    cleaned.explicitSpriteVmtPaths = cleanRelativeList(request.explicitSpriteVmtPaths);
    cleaned.skyboxNames = cleanRelativeList(request.skyboxNames);
    return cleaned;
}

void resolveLocalVmtInputs(
    const QStringList& sourceVmtPaths,
    Workflow::Material::MaterialConvertOptions& options,
    const Workflow::Common::ImportContext& context)
{
    if (sourceVmtPaths.isEmpty()) {
        return;
    }

    QStringList expandedFiles;
    QSet<QString> seenFiles;
    auto pushFile = [&](const QString& filePath) {
        const QString cleaned = QDir::cleanPath(filePath);
        const QString lower = cleaned.toLower();
        if (!lower.endsWith(QStringLiteral(".vmt"))) {
            return;
        }
        if (!seenFiles.contains(lower)) {
            seenFiles.insert(lower);
            expandedFiles.append(cleaned);
        }
    };

    for (const QString& rawPath : sourceVmtPaths) {
        const QFileInfo fi(rawPath);
        if (!fi.exists()) {
            context.warning(MaterialConvertService::tr("Source VMT path does not exist: %1").arg(rawPath));
            continue;
        }
        if (fi.isDir()) {
            QStringList dirMatches;
            QDirIterator it(fi.absoluteFilePath(), QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString f = it.next();
                if (f.endsWith(QStringLiteral(".vmt"), Qt::CaseInsensitive)) {
                    dirMatches.append(f);
                }
            }
            dirMatches.sort(Qt::CaseInsensitive);
            for (const QString& f : dirMatches) {
                pushFile(f);
            }
        } else if (fi.isFile()) {
            if (!fi.fileName().endsWith(QStringLiteral(".vmt"), Qt::CaseInsensitive)) {
                context.warning(MaterialConvertService::tr("Ignoring non-VMT source file: %1").arg(rawPath));
                continue;
            }
            pushFile(fi.absoluteFilePath());
        }
    }

    std::vector<Domain::Game::SearchTarget> highPriorityTargets;
    QSet<QString> seenHighPriorityRoots;
    QSet<QString> seenVirtualRoots;
    QSet<QString> seenMaterialRelPaths;
    for (const QString& existing : options.materialRelativePaths) {
        seenMaterialRelPaths.insert(existing.trimmed().toLower());
    }

    for (const QString& absPath : expandedFiles) {
        QString slashPath = absPath;
        slashPath.replace(u'\\', u'/');
        const QStringList parts = slashPath.split(u'/', Qt::KeepEmptyParts);

        int materialsSegIdx = -1;
        for (int i = parts.size() - 2; i >= 0; --i) {
            if (parts.at(i).compare(QStringLiteral("materials"), Qt::CaseInsensitive) == 0) {
                materialsSegIdx = i;
                break;
            }
        }

        if (materialsSegIdx > 0) {
            const QString searchRoot = parts.mid(0, materialsSegIdx).join(u'/');
            const QString relPath = parts.mid(materialsSegIdx).join(u'/').toLower();
            const QString rootKey = searchRoot.toLower();
            if (!searchRoot.isEmpty() && !seenHighPriorityRoots.contains(rootKey)) {
                seenHighPriorityRoots.insert(rootKey);
                highPriorityTargets.push_back(
                    Domain::Game::SearchTarget::makeDirectory(Core::Path::FilesystemPath(searchRoot)));
            }
            if (!seenMaterialRelPaths.contains(relPath)) {
                seenMaterialRelPaths.insert(relPath);
                options.materialRelativePaths.append(relPath);
            }
        } else {
            // Fallback for loose VMTs located outside any "materials/" directory hierarchy.
            const QFileInfo fi(absPath);
            const QString parentDir = fi.absolutePath();
            const QString parentKey = parentDir.toLower();
            if (!parentDir.isEmpty() && !seenVirtualRoots.contains(parentKey)) {
                seenVirtualRoots.insert(parentKey);
                options.virtualMaterialRoots.emplace_back(parentDir);
            }

            const QString lowerName = fi.fileName().toLower();
            QString relPath = QStringLiteral("materials/%1").arg(lowerName);
            if (options.directAssetOverrides.contains(relPath)
                && options.directAssetOverrides.value(relPath).toString().compare(absPath, Qt::CaseInsensitive) != 0) {
                const QString folderTag = QFileInfo(parentDir).fileName().toLower();
                const QString basePrefix = folderTag.isEmpty()
                    ? QStringLiteral("materials")
                    : QStringLiteral("materials/%1").arg(folderTag);
                relPath = QStringLiteral("%1/%2").arg(basePrefix, lowerName);
                int suffix = 1;
                while (options.directAssetOverrides.contains(relPath)) {
                    relPath = QStringLiteral("%1/%2_%3.vmt")
                                  .arg(basePrefix, fi.completeBaseName().toLower())
                                  .arg(suffix++);
                }
            }

            options.directAssetOverrides.insert(relPath, Core::Path::FilesystemPath(absPath));
            if (!seenMaterialRelPaths.contains(relPath)) {
                seenMaterialRelPaths.insert(relPath);
                options.materialRelativePaths.append(relPath);
            }
            context.warning(
                MaterialConvertService::tr(
                    "VMT '%1' is outside a 'materials/' folder; mapping as '%2' and treating its directory as a virtual materials root")
                    .arg(absPath, relPath));
        }
    }

    if (!highPriorityTargets.empty()) {
        options.searchTargets.insert(
            options.searchTargets.begin(),
            std::make_move_iterator(highPriorityTargets.begin()),
            std::make_move_iterator(highPriorityTargets.end()));
    }
}

std::vector<Domain::Game::SearchTarget> resolveSearchTargets(const Common::ValidatedBaseImport& base)
{
    const Core::Path::FilesystemPath gameInfoDir(base.s1GameInfoDir);
    auto inspectResult = Domain::Game::GameInstallationResolver::inspectGameInfo(gameInfoDir);
    if (inspectResult.isSuccess() && !inspectResult.value().gameInfo.searchTargets().empty()) {
        return inspectResult.value().gameInfo.searchTargets();
    }

    std::vector<Domain::Game::SearchTarget> fallbackTargets;
    if (gameInfoDir.isValid()) {
        fallbackTargets.push_back(Domain::Game::SearchTarget::makeDirectory(gameInfoDir));
    }
    const Core::Path::FilesystemPath source1Dir(base.source1GameDir);
    if (source1Dir.isValid() && source1Dir.toString().compare(gameInfoDir.toString(), Qt::CaseInsensitive) != 0) {
        fallbackTargets.push_back(Domain::Game::SearchTarget::makeDirectory(source1Dir));
    }
    return fallbackTargets;
}

MaterialConvertResultDTO toApplicationResult(const Workflow::Material::MaterialConvertResult& wfResult)
{
    MaterialConvertResultDTO dto;
    dto.totalRequested = wfResult.totalRequested;
    dto.convertedMaterials = wfResult.convertedMaterials;
    dto.convertedSprites = wfResult.convertedSprites;
    dto.convertedSkyboxes = wfResult.convertedSkyboxes;
    dto.skippedNative = wfResult.skippedNative;
    dto.skippedUnsupported = wfResult.skippedUnsupported;
    dto.failedCount = wfResult.failedCount;
    dto.compiledCount = wfResult.compiledCount;
    dto.generatedFiles = wfResult.generatedFiles;
    dto.compiledAssetPaths = wfResult.compiledAssetPaths;
    dto.warnings = wfResult.warnings;
    dto.errors = wfResult.errors;
    return dto;
}

Core::Result<MaterialConvertResultDTO> executeConversionInternal(
    const std::shared_ptr<Common::ImportPrerequisiteService>& prereqService,
    const MaterialConvertRequest& request,
    const Workflow::Common::ImportContext& context)
{
    if (!prereqService) {
        return Core::Result<MaterialConvertResultDTO>::failure(
            Core::Error::ErrorCode::InvalidState,
            MaterialConvertService::tr("Prerequisite service is unavailable"));
    }

    auto prereqResult = prereqService->prepare(request, context);
    if (!prereqResult.isSuccess()) {
        if (prereqResult.isCancelled()) {
            return Core::Result<MaterialConvertResultDTO>::cancelled(prereqResult.message());
        }
        if (prereqResult.isSkipped()) {
            return Core::Result<MaterialConvertResultDTO>::skipped(prereqResult.message());
        }
        return Core::Result<MaterialConvertResultDTO>::failure(prereqResult.error(), prereqResult.message());
    }

    const auto& base = prereqResult.value();
    const Core::Path::FilesystemPath cs2BasePath(base.cs2BaseDir);
    const Core::Path::FilesystemPath addonsRoot = cs2BasePath / QStringLiteral("content/csgo_addons");
    const auto resolvedContentDir = addonsRoot.resolveBelow(base.addonName);
    if (!resolvedContentDir.has_value()) {
        return Core::Result<MaterialConvertResultDTO>::failure(
            Core::Error::ErrorCode::InvalidPath,
            MaterialConvertService::tr("Invalid target addon name: %1").arg(base.addonName));
    }

    Workflow::Material::MaterialConvertOptions options;
    options.searchTargets = resolveSearchTargets(base);
    options.destContentDir = *resolvedContentDir;
    options.cs2BaseDir = cs2BasePath;
    options.vpkIndex = base.vpkIndex;
    options.cs2Index = base.cs2Index;
    options.archivePool = nullptr;
    options.materialRelativePaths = request.materialRelativePaths;
    if (!request.regularMaterialsOnly) {
        options.explicitSpriteVmtPaths = request.explicitSpriteVmtPaths;
        options.skyboxNames = request.skyboxNames;
    }
    options.translatePolicy.detailMode = request.preserveDetailViaComplex
        ? Domain::Material::DetailHandlingMode::PreserveViaComplex
        : Domain::Material::DetailHandlingMode::BakeToEnvironment;
    options.overwriteNativeAssets = request.overwriteNativeAssets;
    options.compileResources = request.compileResources;
    options.cleanupOnFailure = true;
    options.regularMaterialsOnly = request.regularMaterialsOnly;

    resolveLocalVmtInputs(request.sourceVmtPaths, options, context);

    const int totalItems = options.materialRelativePaths.size()
        + options.explicitSpriteVmtPaths.size()
        + options.skyboxNames.size();
    if (totalItems == 0) {
        return Core::Result<MaterialConvertResultDTO>::failure(
            Core::Error::ErrorCode::InvalidPath,
            MaterialConvertService::tr("No valid source VMT files specified"));
    }

    context.info(MaterialConvertService::tr("Starting material conversion for addon '%1' (%2 item(s))")
                     .arg(base.addonName)
                     .arg(totalItems));

    Workflow::Material::MaterialConvertWorkflow workflow;
    auto wfResult = workflow.execute(options, context);

    if (wfResult.isCancelled()) {
        context.info(MaterialConvertService::tr("Material conversion workflow was cancelled"));
        return Core::Result<MaterialConvertResultDTO>::cancelled(wfResult.message());
    }

    if (wfResult.isSkipped()) {
        context.info(MaterialConvertService::tr("Material conversion workflow was skipped: %1")
                         .arg(wfResult.message()));
        return Core::Result<MaterialConvertResultDTO>::skipped(wfResult.message());
    }

    if (wfResult.isSuccess()) {
        return Core::Result<MaterialConvertResultDTO>::success(
            toApplicationResult(wfResult.value()),
            wfResult.message());
    }

    if (wfResult.hasValue()) {
        return Core::Result<MaterialConvertResultDTO>::failure(
            wfResult.error(),
            wfResult.message(),
            toApplicationResult(wfResult.value()));
    }

    return Core::Result<MaterialConvertResultDTO>::failure(wfResult.error(), wfResult.message());
}

} // namespace

std::shared_ptr<MaterialConvertService> MaterialConvertService::create(
    std::shared_ptr<Common::ImportPrerequisiteService> prerequisiteService)
{
    return std::make_shared<MaterialConvertService>(PassKey{}, std::move(prerequisiteService));
}

MaterialConvertService::MaterialConvertService(
    PassKey,
    std::shared_ptr<Common::ImportPrerequisiteService> prerequisiteService)
    : QObject(nullptr)
    , m_prerequisiteService(prerequisiteService
          ? std::move(prerequisiteService)
          : std::make_shared<Common::ImportPrerequisiteService>(nullptr, Package::VpkIndexService::create()))
{
}

MaterialConvertService::~MaterialConvertService()
{
    cancelCurrentConversion();
}

Async::TaskHandle MaterialConvertService::convertMaterialsAsync(
    const MaterialConvertRequest& request,
    ConvertCallback callback)
{
    // 1. Safe shared ownership check BEFORE modifying any state:
    auto self = weak_from_this().lock();
    if (!self) {
        if (callback) {
            callback(Core::Result<MaterialConvertResultDTO>::failure(
                Core::Error::ErrorCode::InvalidState,
                tr("MaterialConvertService instance must be managed by std::shared_ptr to start async operations")));
        }
        return Async::TaskHandle{};
    }

    // 2. Reject overlapping conversions atomically:
    bool expected = false;
    if (!m_isConverting.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        if (callback) {
            callback(Core::Result<MaterialConvertResultDTO>::failure(
                Core::Error::ErrorCode::InvalidState,
                tr("Another material conversion operation is already in progress")));
        }
        return Async::TaskHandle{};
    }

    QMetaObject::invokeMethod(
        self.get(),
        [self]() { emit self->isConvertingChanged(true); },
        Qt::QueuedConnection);

    // RAII scope guard rolls back state and notifies on QObject thread if dispatch throws.
    auto launchGuard = qScopeGuard([self]() {
        if (self->m_isConverting.exchange(false, std::memory_order_acq_rel)) {
            {
                std::lock_guard<std::mutex> lock(self->m_mutex);
                self->m_activeTaskHandle = Async::TaskHandle{};
            }
            QMetaObject::invokeMethod(
                self.get(),
                [self]() { emit self->isConvertingChanged(false); },
                Qt::QueuedConnection);
        }
    });

    try {
        const MaterialConvertRequest cleanedRequest = sanitizeRequest(request);
        const int totalItems = cleanedRequest.sourceVmtPaths.size()
            + cleanedRequest.materialRelativePaths.size()
            + (cleanedRequest.regularMaterialsOnly ? 0 : cleanedRequest.explicitSpriteVmtPaths.size())
            + (cleanedRequest.regularMaterialsOnly ? 0 : cleanedRequest.skyboxNames.size());

        QString logBaseName = QStringLiteral("materials_batch");
        QString taskName;
        if (totalItems == 1) {
            QString singleItem;
            if (!cleanedRequest.sourceVmtPaths.isEmpty()) {
                singleItem = cleanedRequest.sourceVmtPaths.first();
            } else if (!cleanedRequest.materialRelativePaths.isEmpty()) {
                singleItem = cleanedRequest.materialRelativePaths.first();
            } else if (!cleanedRequest.explicitSpriteVmtPaths.isEmpty()) {
                singleItem = cleanedRequest.explicitSpriteVmtPaths.first();
            } else if (!cleanedRequest.skyboxNames.isEmpty()) {
                singleItem = cleanedRequest.skyboxNames.first();
            }
            const QFileInfo fi(singleItem);
            const QString displayName = fi.fileName().isEmpty() ? singleItem : fi.fileName();
            if (!fi.completeBaseName().isEmpty()) {
                logBaseName = fi.completeBaseName();
            }
            taskName = tr("Convert Material: %1").arg(displayName);
        } else {
            taskName = tr("Convert Materials (%1 items)").arg(totalItems);
        }

        auto worker = [self, cleanedRequest](
                          std::shared_ptr<Core::Logging::TaskLoggingContext> taskCtx,
                          Core::Async::CancellationToken token) -> Core::Result<MaterialConvertResultDTO> {
            Workflow::Common::ImportContext ctx(taskCtx.get(), std::move(token));
            std::shared_ptr<Common::ImportPrerequisiteService> prereq;
            {
                std::lock_guard<std::mutex> lock(self->m_mutex);
                prereq = self->m_prerequisiteService;
            }
            return executeConversionInternal(prereq, cleanedRequest, ctx);
        };

        auto completionCallback = [self, cb = callback](
                                      const Core::Result<MaterialConvertResultDTO>& result) {
            if (self->m_isConverting.exchange(false, std::memory_order_acq_rel)) {
                {
                    std::lock_guard<std::mutex> lock(self->m_mutex);
                    self->m_activeTaskHandle = Async::TaskHandle{};
                }
                QMetaObject::invokeMethod(
                    self.get(),
                    [self]() { emit self->isConvertingChanged(false); },
                    Qt::QueuedConnection);
            }
            if (cb) {
                cb(result);
            }
        };

        Async::TaskHandle handle = Async::AsyncTaskRunner::runWorkflowTask<MaterialConvertResultDTO>(
            taskName,
            logBaseName,
            self.get(),
            std::move(worker),
            std::move(completionCallback),
            QThreadPool::globalInstance());

        // AsyncTaskRunner now guarantees completionCallback invocation, so dismiss launchGuard.
        launchGuard.dismiss();

        if (handle.isValid()) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_isConverting.load(std::memory_order_acquire)) {
                m_activeTaskHandle = handle;
            }
        }

        return handle;
    } catch (const std::exception& ex) {
        if (callback) {
            callback(Core::Result<MaterialConvertResultDTO>::failure(
                Core::Error::ErrorCode::OperationFailed,
                tr("Failed to start async material conversion task: %1").arg(QString::fromUtf8(ex.what()))));
        }
        return Async::TaskHandle{};
    } catch (...) {
        if (callback) {
            callback(Core::Result<MaterialConvertResultDTO>::failure(
                Core::Error::ErrorCode::Unknown,
                tr("Failed to start async material conversion task due to an unknown exception")));
        }
        return Async::TaskHandle{};
    }
}

void MaterialConvertService::cancelCurrentConversion()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_activeTaskHandle.isValid()) {
        m_activeTaskHandle.cancel();
    }
}

bool MaterialConvertService::isConverting() const noexcept
{
    return m_isConverting.load(std::memory_order_acquire);
}

std::shared_ptr<Common::ImportPrerequisiteService> MaterialConvertService::prerequisiteService() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_prerequisiteService;
}

void MaterialConvertService::setPrerequisiteService(
    std::shared_ptr<Common::ImportPrerequisiteService> service)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_prerequisiteService = service
        ? std::move(service)
        : std::make_shared<Common::ImportPrerequisiteService>();
}

} // namespace Application::Material
