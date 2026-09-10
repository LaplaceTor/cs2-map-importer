#include "UI/Controllers/MainController.h"
#include "UI/ViewModels/LogViewModel.h"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QStyleHints>
#include <QUrl>
#include <QMetaObject>
#include <QPointer>

#ifndef APP_VERSION
#define APP_VERSION "1.0.0"
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
    if (m_particleImportService && m_particleImportService->isImporting()) {
        m_particleImportService->cancelCurrentImport();
    }
    m_particleImportService.reset();
}

void MainController::setActiveTab(int tab) {
    if (m_isProcessing) {
        emit alertRequested(
            QStringLiteral("Task in Progress"),
            QStringLiteral("An operation is currently executing. You cannot change tabs until the current operation completes.")
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
        QStringLiteral("Feature in Development"),
        QStringLiteral("Import execution will be connected in the upcoming Workflow integration stage.")
    );
}

void MainController::startParticleImport(
    const QString& source1GameDir,
    const QString& cs2BaseDir,
    const QString& addonName,
    const QString& sourcePcfPath,
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

    const bool isCsgo = (s1GameType.compare(QStringLiteral("CSGO"), Qt::CaseInsensitive) == 0);

    QString sanitizedPcfPath = sourcePcfPath;
    if (sanitizedPcfPath.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)) {
        // Normalize malformed Windows file URLs with only 2 slashes, e.g. file://C:/...
        // where index 7 is the drive letter and index 8 is ':'.
        // This avoids QUrl interpreting the drive letter as a hostname.
        if (sanitizedPcfPath.size() >= 9 &&
            (sanitizedPcfPath.at(5) == QLatin1Char('/') || sanitizedPcfPath.at(5) == QLatin1Char('\\')) &&
            (sanitizedPcfPath.at(6) == QLatin1Char('/') || sanitizedPcfPath.at(6) == QLatin1Char('\\')) &&
            sanitizedPcfPath.at(7).isLetter() &&
            sanitizedPcfPath.at(8) == QLatin1Char(':')) {
            sanitizedPcfPath = QStringLiteral("file:///") + sanitizedPcfPath.mid(7);
        }

        QUrl pcfUrl(sanitizedPcfPath);
        QString local = pcfUrl.toLocalFile();
        if (!local.isEmpty()) {
            sanitizedPcfPath = local;
        }
    }

    Application::Particle::ParticleImportRequest request;
    request.source1GameDir = source1GameDir;
    request.s1GameInfoDir = s1GameInfoDir;
    request.cs2BaseDir = cs2BaseDir;
    request.addonName = addonName;
    request.sourcePcfPath = sanitizedPcfPath;
    request.allowDepthBlend = allowDepthBlend;
    request.disableDiffuse = disableDiffuse;
    request.isCsgo = isCsgo;

    m_isProcessing = true;
    emit isProcessingChanged();

    if (!m_particleImportService) {
        m_particleImportService = std::make_unique<Application::Particle::ParticleImportService>();
    }

    QPointer<MainController> self(this);
    m_particleImportService->importParticlesAsync(
        request,
        nullptr,
        [self](const Core::Result<Application::Particle::ParticleImportResult>& result) {
            if (!self) {
                return;
            }
            QMetaObject::invokeMethod(self, [self, result]() {
                if (!self) {
                    return;
                }
                self->m_isProcessing = false;
                emit self->isProcessingChanged();

                if (result.isSuccess()) {
                    emit self->alertRequested(
                        QStringLiteral("Particle Import Complete"),
                        result.message().isEmpty()
                            ? QStringLiteral("All particles were successfully imported and compiled.")
                            : result.message()
                    );
                } else if (result.isCancelled()) {
                    emit self->alertRequested(
                        QStringLiteral("Import Cancelled"),
                        QStringLiteral("Particle import was cancelled by user.")
                    );
                } else {
                    emit self->alertRequested(
                        QStringLiteral("Particle Import Failed"),
                        result.message().isEmpty()
                            ? QStringLiteral("An error occurred during particle import.")
                            : result.message()
                    );
                }
            }, Qt::QueuedConnection);
        }
    );
}

void MainController::stopImport() {
    if (m_particleImportService && m_particleImportService->isImporting()) {
        m_particleImportService->cancelCurrentImport();
    }
}

void MainController::checkForUpdates() {
    // Placeholder for UpdateService integration in Stage 4
    emit alertRequested(
        QStringLiteral("Check for Updates"),
        QStringLiteral("You are currently running the latest development version (v%1).").arg(appVersion())
    );
}

void MainController::toggleLogWindow() {
    emit logWindowToggleRequested();
}

} // namespace UI::Controllers

