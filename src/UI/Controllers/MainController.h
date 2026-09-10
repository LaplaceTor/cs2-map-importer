#pragma once

#include <QObject>
#include <QString>
#include <memory>

#include "Application/Particle/ParticleImportDTOs.h"
#include "Application/Particle/ParticleImportService.h"

namespace UI::ViewModels {
class LogViewModel;
}

namespace UI::Controllers {

class MainController : public QObject {
    Q_OBJECT

    Q_PROPERTY(int activeTab READ activeTab WRITE setActiveTab NOTIFY activeTabChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(bool isProcessing READ isProcessing NOTIFY isProcessingChanged)
    Q_PROPERTY(bool canStart READ canStart NOTIFY canStartChanged)

public:
    explicit MainController(UI::ViewModels::LogViewModel* logViewModel = nullptr, QObject* parent = nullptr);
    ~MainController() override;

    void setLogViewModel(UI::ViewModels::LogViewModel* logViewModel) noexcept { m_logViewModel = logViewModel; }
    UI::ViewModels::LogViewModel* logViewModel() const noexcept { return m_logViewModel; }

    int activeTab() const noexcept { return m_activeTab; }
    void setActiveTab(int tab);

    QString theme() const noexcept { return m_theme; }
    void setTheme(const QString& themeName);

    QString appVersion() const;

    bool isProcessing() const noexcept { return m_isProcessing; }
    bool canStart() const noexcept { return m_canStart; }

    Q_INVOKABLE void startParticleImport(
        const QString& source1GameDir,
        const QString& cs2BaseDir,
        const QString& addonName,
        const QString& sourcePcfPath,
        bool allowDepthBlend,
        bool disableDiffuse,
        const QString& s1GameType = QString()
    );

    void setParticleImportService(std::unique_ptr<Application::Particle::ParticleImportService> service) noexcept {
        m_particleImportService = std::move(service);
    }
    Application::Particle::ParticleImportService* particleImportService() const noexcept {
        return m_particleImportService.get();
    }

public slots:
    void cycleTheme();
    void startImport();
    void stopImport();
    void checkForUpdates();
    void toggleLogWindow();

signals:
    void activeTabChanged();
    void themeChanged();
    void isProcessingChanged();
    void canStartChanged();
    void alertRequested(const QString& title, const QString& message);
    void logWindowToggleRequested();

private:
    void applyTheme(const QString& themeName);

    int m_activeTab = 0; // 0: Map, 1: Model, 2: Particle
    QString m_theme = QStringLiteral("system");
    bool m_isProcessing = false;
    bool m_canStart = false;
    UI::ViewModels::LogViewModel* m_logViewModel = nullptr;
    std::unique_ptr<Application::Particle::ParticleImportService> m_particleImportService;
};

} // namespace UI::Controllers
