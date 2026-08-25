#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <memory>
#include "Application/Environment/GameInstallation.h"
#include "Application/Environment/GameDetectService.h"
#include "Application/Environment/SteamService.h"
#include "Application/Environment/VpkSignatureLeaseService.h"
#include "Domain/Game/GameType.h"
#include "Domain/Game/GameRegistry.h"
#include "Core/Path/FilesystemPath.h"

namespace UI::ViewModels {

class GameViewModel : public QObject {
    Q_OBJECT

    // Source 1 Properties
    Q_PROPERTY(QStringList s1GameTypes READ s1GameTypes CONSTANT)
    Q_PROPERTY(QString selectedS1Type READ selectedS1Type WRITE setSelectedS1Type NOTIFY selectedS1TypeChanged)
    Q_PROPERTY(QString s1GamePath READ s1GamePath NOTIFY s1GamePathChanged)
    Q_PROPERTY(QString s1GameTitle READ s1GameTitle NOTIFY s1GameTitleChanged)
    Q_PROPERTY(bool isS1Valid READ isS1Valid NOTIFY s1ValidityChanged)

    // Source 2 Properties
    Q_PROPERTY(QStringList s2GameTypes READ s2GameTypes CONSTANT)
    Q_PROPERTY(QString selectedS2Type READ selectedS2Type WRITE setSelectedS2Type NOTIFY selectedS2TypeChanged)
    Q_PROPERTY(QString s2GamePath READ s2GamePath NOTIFY s2GamePathChanged)
    Q_PROPERTY(QString s2GameTitle READ s2GameTitle NOTIFY s2GameTitleChanged)
    Q_PROPERTY(bool isS2Valid READ isS2Valid NOTIFY s2ValidityChanged)
    Q_PROPERTY(QStringList s2AddonsList READ s2AddonsList NOTIFY s2AddonsListChanged)
    Q_PROPERTY(QString selectedAddon READ selectedAddon WRITE setSelectedAddon NOTIFY selectedAddonChanged)
    Q_PROPERTY(bool isVpkLeaseHeld READ isVpkLeaseHeld NOTIFY vpkLeaseStateChanged)

    // Detection State
    Q_PROPERTY(bool isDetecting READ isDetecting NOTIFY isDetectingChanged)

public:
    explicit GameViewModel(Application::Environment::VpkSignatureLeaseService* vpkLeaseService = nullptr, QObject* parent = nullptr);
    ~GameViewModel() override = default;

    bool isDetecting() const noexcept { return m_isDetecting; }

    QStringList s1GameTypes() const;
    QString selectedS1Type() const noexcept { return m_selectedS1Type; }
    QString s1GamePath() const noexcept { return m_s1GamePath; }
    QString s1GameTitle() const noexcept { return m_s1GameTitle; }
    bool isS1Valid() const noexcept { return m_isS1Valid; }

    QStringList s2GameTypes() const;
    QString selectedS2Type() const noexcept { return m_selectedS2Type; }
    QString s2GamePath() const noexcept { return m_s2GamePath; }
    QString s2GameTitle() const noexcept { return m_s2GameTitle; }
    bool isS2Valid() const noexcept { return m_isS2Valid; }
    QStringList s2AddonsList() const noexcept { return m_s2AddonsList; }
    QString selectedAddon() const noexcept { return m_selectedAddon; }
    bool isVpkLeaseHeld() const noexcept;

    void setVpkSignatureLeaseService(Application::Environment::VpkSignatureLeaseService* service);
    Application::Environment::VpkSignatureLeaseService* vpkSignatureLeaseService() const noexcept { return m_vpkLeaseService; }

    const Application::Environment::GameInstallation& s1Installation() const noexcept { return m_s1Installation; }
    const Application::Environment::GameInstallation& s2Installation() const noexcept { return m_s2Installation; }

public slots:
    void autoDetect();
    void setSelectedS1Type(const QString& typeId);
    void setSelectedS2Type(const QString& typeId);
    void selectS1Folder(const QString& pathOrUrl);
    void selectS2Folder(const QString& pathOrUrl);
    void validateS1InSteam();
    void validateS2InSteam();
    void setSelectedAddon(const QString& addon);
    void refreshS2Addons();
    void retryVpkSignatureLease();

signals:
    void selectedS1TypeChanged();
    void s1GamePathChanged();
    void s1GameTitleChanged();
    void s1ValidityChanged();

    void selectedS2TypeChanged();
    void s2GamePathChanged();
    void s2GameTitleChanged();
    void s2ValidityChanged();
    void s2AddonsListChanged();
    void selectedAddonChanged();

    void isDetectingChanged();
    void detectionFinished();

    void vpkLeaseStateChanged();
    void alertRequested(const QString& title, const QString& message);
    void vpkSignatureOccupied(const QString& title, const QString& message);

private:
    Domain::Game::GameType parseS1Type(const QString& typeStr) const;
    Domain::Game::GameType parseS2Type(const QString& typeStr) const;
    QString cleanInputPath(const QString& pathOrUrl) const;
    void applyDetectionResult(const Application::Environment::DetectionResult& result);
    void applyS1Installation(const Application::Environment::GameInstallation& inst);
    void applyS2Installation(const Application::Environment::GameInstallation& inst);
    void onVpkLeaseStatusChanged(Application::Environment::VpkSignatureLeaseStatus status, const QString& filePath, const QString& systemMessage);

    bool m_isDetecting = false;

    QString m_selectedS1Type = QStringLiteral("CSGO");
    QString m_s1GamePath;
    QString m_s1GameTitle;
    bool m_isS1Valid = false;
    Application::Environment::GameInstallation m_s1Installation;

    QString m_selectedS2Type = QStringLiteral("cs2");
    QString m_s2GamePath;
    QString m_s2GameTitle;
    bool m_isS2Valid = false;
    QStringList m_s2AddonsList;
    QString m_selectedAddon;
    Application::Environment::GameInstallation m_s2Installation;
    Application::Environment::VpkSignatureLeaseService* m_vpkLeaseService = nullptr;

    // Cache for detected installations across Steam libraries: [GameType -> GameInstallation]
    QHash<Domain::Game::GameType, Application::Environment::GameInstallation> m_detectedGames;
};

} // namespace UI::ViewModels

