#include <QCoreApplication>
#include "UI/Controllers/MainController.h"
#include "UI/ViewModels/LogViewModel.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QSet>
#include <QStyleHints>
#include <QPointer>

#ifndef APP_VERSION
#define APP_VERSION "2.0.0-dev"
#endif

namespace UI::Controllers {

MainController::MainController(UI::ViewModels::LogViewModel* logViewModel, QObject* parent)
    : QObject(parent)
    , m_logViewModel(logViewModel)
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QGuiApplication::styleHints()) {
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
            if (m_theme == QStringLiteral("system")) {
                applyTheme(QStringLiteral("system"));
            }
        });
    }
#endif
}

MainController::~MainController() {
    cancelAllOperations();
    m_particleImportService.reset();
    m_materialConvertService.reset();
}

void MainController::cancelAllOperations() {
    if (m_particleImportService && m_particleImportService->isImporting()) {
        m_particleImportService->cancelCurrentImport();
    }
    if (m_materialConvertService && m_materialConvertService->isConverting()) {
        m_materialConvertService->cancelCurrentConversion();
    }
}

QStringList MainController::expandSourcePaths(
    const QStringList& rawPaths,
    const QStringList& allowedExtensions) const
{
    QStringList normalizedExts;
    normalizedExts.reserve(allowedExtensions.size());
    for (const QString& ext : allowedExtensions) {
        const QString trimmed = ext.trimmed().toLower();
        if (!trimmed.isEmpty()) {
            normalizedExts.append(trimmed);
        }
    }

    auto matchesExtension = [&](const QString& filePath) -> bool {
        if (normalizedExts.isEmpty()) {
            return true;
        }
        const QString lower = filePath.toLower();
        for (const QString& ext : normalizedExts) {
            if (lower.endsWith(ext)) {
                return true;
            }
        }
        return false;
    };

    QStringList result;
    QSet<QString> seen;

    auto appendIfUnique = [&](const QString& filePath) {
        const QString nativePath = QDir::toNativeSeparators(QDir::cleanPath(filePath));
        const QString key = nativePath.toLower();
        if (!seen.contains(key)) {
            seen.insert(key);
            result.append(nativePath);
        }
    };

    for (const QString& raw : rawPaths) {
        const QString trimmed = raw.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        const QFileInfo fi(trimmed);
        if (!fi.exists()) {
            continue;
        }
        if (fi.isFile()) {
            if (matchesExtension(fi.absoluteFilePath())) {
                appendIfUnique(fi.absoluteFilePath());
            }
        } else if (fi.isDir()) {
            QStringList dirMatches;
            QDirIterator it(fi.absoluteFilePath(), QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString candidate = it.next();
                if (matchesExtension(candidate)) {
                    dirMatches.append(candidate);
                }
            }
            dirMatches.sort(Qt::CaseInsensitive);
            for (const QString& matched : dirMatches) {
                appendIfUnique(matched);
            }
        }
    }

    return result;
}

void MainController::setActiveTab(int tab) {
    if (m_isProcessing) {
        emit alertRequested(
            QCoreApplication::translate("MainController", "Task in Progress"),
            QCoreApplication::translate("MainController", "An operation is currently executing. You cannot change tabs until the current operation completes.")
        );
        return;
    }

    if (m_activeTab != tab) {
        m_activeTab = tab;
        emit activeTabChanged();
    }
}

void MainController::setTheme(const QString& themeName) {
    if (m_theme != themeName) {
        m_theme = themeName;
        applyTheme(m_theme);
        emit themeChanged();
    }
}

void MainController::cycleTheme() {
    if (m_theme == QStringLiteral("system")) {
        setTheme(QStringLiteral("light"));
    } else if (m_theme == QStringLiteral("light")) {
        setTheme(QStringLiteral("dark"));
    } else {
        setTheme(QStringLiteral("system"));
    }
}

QString MainController::appVersion() const {
    return QStringLiteral(APP_VERSION);
}

void MainController::applyTheme(const QString& themeName) {
    if (auto* hints = QGuiApplication::styleHints()) {
        if (themeName == QStringLiteral("light")) {
            hints->setColorScheme(Qt::ColorScheme::Light);
        } else if (themeName == QStringLiteral("dark")) {
            hints->setColorScheme(Qt::ColorScheme::Dark);
        } else {
            hints->setColorScheme(Qt::ColorScheme::Unknown);
        }
    }
}

void MainController::startImport() {
    if (m_isProcessing) {
        return;
    }
    if (m_logViewModel) {
        m_logViewModel->collapseAll();
    }
    // Placeholder for WorkflowRunner integration in Stage 4
    emit alertRequested(
        QCoreApplication::translate("MainController", "Feature in Development"),
        QCoreApplication::translate("MainController", "Import execution will be connected in the upcoming Workflow integration stage.")
    );
}

void MainController::startParticleImport(
    const QString& source1GameDir,
    const QString& cs2BaseDir,
    const QString& addonName,
    const QStringList& sourcePcfPaths,
    bool allowDepthBlend,
    bool disableDiffuse,
    const QString& s1GameType,
    const QString& s1GameInfoDir
) {
    if (m_isProcessing) {
        return;
    }

    if (m_logViewModel) {
        m_logViewModel->collapseAll();
    }

    Application::Particle::ParticleImportRequest request;
    request.source1GameDir = source1GameDir;
    request.s1GameInfoDir = s1GameInfoDir;
    request.cs2BaseDir = cs2BaseDir;
    request.addonName = addonName;
    request.sourcePcfPaths = sourcePcfPaths;
    request.allowDepthBlend = allowDepthBlend;
    request.disableDiffuse = disableDiffuse;
    request.gameId = s1GameType;

    if (!m_particleImportService) {
        const QString warningMsg = QCoreApplication::translate("MainController", "Particle import service is not available.");
        emit particleImportFailed(warningMsg);
        emit alertRequested(
            QCoreApplication::translate("MainController", "Particle Import Failed"),
            warningMsg);
        return;
    }

    m_isProcessing = true;
    emit isProcessingChanged();

    QPointer<MainController> self(this);
    // The handle's cancellation path is routed through the service (stopImport);
    // the returned handle is intentionally not retained here.
    (void)m_particleImportService->importParticlesAsync(
        request,
        [self](const Core::Result<Application::Particle::ParticleImportResult>& result) {
            if (!self) {
                return;
            }
            self->m_isProcessing = false;
            emit self->isProcessingChanged();

            if (result.isSuccess()) {
                const auto& data = result.valueOr(Application::Particle::ParticleImportResult{});
                const QString title = (data.totalFailed > 0)
                    ? QCoreApplication::translate("MainController", "Particle Import Completed with Warnings")
                    : QCoreApplication::translate("MainController", "Particle Import Complete");
                emit self->alertRequested(
                    title,
                    result.message().isEmpty()
                        ? QCoreApplication::translate("MainController", "All particles were successfully imported and compiled.")
                        : result.message()
                );
            } else if (result.isCancelled()) {
                emit self->alertRequested(
                    QCoreApplication::translate("MainController", "Import Cancelled"),
                    QCoreApplication::translate("MainController", "Particle import was cancelled by user.")
                );
            } else {
                emit self->alertRequested(
                    QCoreApplication::translate("MainController", "Particle Import Failed"),
                    result.message().isEmpty()
                        ? QCoreApplication::translate("MainController", "An error occurred during particle import.")
                        : result.message()
                );
            }
        }
    );
}

void MainController::startMaterialConversion(
    const QString& source1GameDir,
    const QString& cs2BaseDir,
    const QString& addonName,
    const QStringList& sourceVmtPaths,
    const QString& s1GameType,
    const QString& s1GameInfoDir
) {
    if (m_isProcessing) {
        return;
    }

    if (m_logViewModel) {
        m_logViewModel->collapseAll();
    }

    Application::Material::MaterialConvertRequest request;
    request.source1GameDir = source1GameDir;
    request.s1GameInfoDir = s1GameInfoDir;
    request.cs2BaseDir = cs2BaseDir;
    request.addonName = addonName;
    request.gameId = s1GameType;
    request.sourceVmtPaths = sourceVmtPaths;
    request.preserveDetailViaComplex = true;
    request.overwriteNativeAssets = false;
    request.compileResources = true;
    request.regularMaterialsOnly = true;

    if (!m_materialConvertService) {
        const QString warningMsg = QCoreApplication::translate(
            "MainController",
            "Material conversion service is not available.");
        emit materialConversionFailed(warningMsg);
        emit alertRequested(
            QCoreApplication::translate("MainController", "Material Conversion Failed"),
            warningMsg);
        return;
    }

    m_isProcessing = true;
    emit isProcessingChanged();

    QPointer<MainController> self(this);
    (void)m_materialConvertService->convertMaterialsAsync(
        request,
        [self](const Core::Result<Application::Material::MaterialConvertResultDTO>& result) {
            if (!self) {
                return;
            }
            self->m_isProcessing = false;
            emit self->isProcessingChanged();

            if (result.isSuccess()) {
                const auto& data = result.valueOr(Application::Material::MaterialConvertResultDTO{});
                const bool hasIssues = (data.failedCount > 0 || data.skippedUnsupported > 0);
                const QString title = hasIssues
                    ? QCoreApplication::translate("MainController", "Material Conversion Completed with Warnings")
                    : QCoreApplication::translate("MainController", "Material Conversion Complete");
                const QString summary = QCoreApplication::translate(
                    "MainController",
                    "Converted: %1\nCompiled: %2\nSkipped (native CS2): %3\nSkipped (unsupported): %4\nFailed: %5")
                    .arg(data.convertedMaterials)
                    .arg(data.compiledCount)
                    .arg(data.skippedNative)
                    .arg(data.skippedUnsupported)
                    .arg(data.failedCount);
                emit self->alertRequested(title, summary);
            } else if (result.isCancelled()) {
                emit self->alertRequested(
                    QCoreApplication::translate("MainController", "Conversion Cancelled"),
                    QCoreApplication::translate("MainController", "Material conversion was cancelled by user."));
            } else {
                const QString errMsg = result.message().isEmpty()
                    ? QCoreApplication::translate("MainController", "An error occurred during material conversion.")
                    : result.message();
                emit self->materialConversionFailed(errMsg);
                emit self->alertRequested(
                    QCoreApplication::translate("MainController", "Material Conversion Failed"),
                    errMsg);
            }
        }
    );
}

void MainController::stopImport() {
    if (m_particleImportService && m_particleImportService->isImporting()) {
        m_particleImportService->cancelCurrentImport();
    }
    if (m_materialConvertService && m_materialConvertService->isConverting()) {
        m_materialConvertService->cancelCurrentConversion();
    }
}

void MainController::checkForUpdates() {
    // Placeholder for UpdateService integration in Stage 4
    emit alertRequested(
        QCoreApplication::translate("MainController", "Check for Updates"),
        QCoreApplication::translate("MainController", "You are currently running the latest development version (v%1).").arg(appVersion())
    );
}

void MainController::toggleLogWindow() {
    emit logWindowToggleRequested();
}

} // namespace UI::Controllers

