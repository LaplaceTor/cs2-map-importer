#include "UI/ViewModels/GameViewModel.h"
#include <QQmlEngine>

namespace UI::ViewModels {

GameViewModel::GameViewModel(Application::Environment::GameEnvironmentService* envService, QObject* parent)
    : QObject(parent)
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
    setEnvironmentService(envService);
}

void GameViewModel::setEnvironmentService(Application::Environment::GameEnvironmentService* envService) {
    if (m_envService == envService && m_envService != nullptr) {
        return;
    }

    if (m_envService) {
        disconnect(m_envService, nullptr, this, nullptr);
    }

    if (envService) {
        m_ownedEnvService.reset();
        m_envService = envService;
    } else {
        m_ownedEnvService = std::make_unique<Application::Environment::GameEnvironmentService>(nullptr, this);
        m_envService = m_ownedEnvService.get();
    }

    if (m_envService) {
        connect(m_envService, &Application::Environment::GameEnvironmentService::vpkLeaseStateChanged,
                this, &GameViewModel::vpkLeaseStateChanged);
        connect(m_envService, &Application::Environment::GameEnvironmentService::vpkLeaseStatusChanged,
                this, &GameViewModel::onVpkLeaseStatusChanged);
    }

    emit vpkLeaseStateChanged();
}

bool GameViewModel::isVpkLeaseHeld() const noexcept {
    return m_envService ? m_envService->isVpkLeaseHeld() : false;
}

QStringList GameViewModel::s1GameTypes() const {
    return m_envService ? m_envService->s1GameTypes() : QStringList();
}

QStringList GameViewModel::s2GameTypes() const {
    return m_envService ? m_envService->s2GameTypes() : QStringList();
}

void GameViewModel::applyS1Installation(const Application::Environment::GameInstallationInfo& inst) {
    m_s1Installation = inst;
    m_s1GamePath = inst.basePath;
    m_s1GameTitle = inst.gameTitle;
    m_isS1Valid = inst.isValid;

    emit s1GamePathChanged();
    emit s1GameTitleChanged();
    emit s1ValidityChanged();
}

void GameViewModel::applyS2Installation(const Application::Environment::GameInstallationInfo& inst) {
    m_s2Installation = inst;
    m_s2GamePath = inst.basePath;
    m_s2GameTitle = inst.gameTitle;
    m_isS2Valid = inst.isValid;

    emit s2GamePathChanged();
    emit s2GameTitleChanged();
    emit s2ValidityChanged();

    refreshS2Addons();

    if (m_envService) {
        m_envService->updateVpkLease(inst);
    }
}

void GameViewModel::retryVpkSignatureLease() {
    if (m_envService) {
        m_envService->retryVpkLease();
    }
}

void GameViewModel::onVpkLeaseStatusChanged(
    Application::Environment::VpkSignatureLeaseStatus status,
    const QString& filePath,
    const QString& systemMessage)
{
    switch (status) {
    case Application::Environment::VpkSignatureLeaseStatus::AlreadyInUse:
        emit vpkSignatureOccupied(
            QStringLiteral("Counter-Strike 2 is Running"),
            QStringLiteral("vpk.signatures is currently in use by Counter-Strike 2 or another application.\n\nPlease close the occupying application and click Retry, or Exit to quit.")
        );
        break;
    case Application::Environment::VpkSignatureLeaseStatus::AccessDenied:
        emit alertRequested(
            QStringLiteral("Access Denied"),
            QStringLiteral("Permission denied when trying to access vpk.signatures:\n%1\n\nPlease check file permissions or run as administrator.").arg(filePath)
        );
        break;
    case Application::Environment::VpkSignatureLeaseStatus::Failed:
        emit alertRequested(
            QStringLiteral("File Lease Failed"),
            QStringLiteral("Failed to acquire exclusive lease on vpk.signatures:\n%1").arg(systemMessage)
        );
        break;
    case Application::Environment::VpkSignatureLeaseStatus::Acquired:
    case Application::Environment::VpkSignatureLeaseStatus::NotFound:
    case Application::Environment::VpkSignatureLeaseStatus::Inactive:
    default:
        break;
    }
}

void GameViewModel::autoDetect() {
    if (m_isDetecting || !m_envService) {
        return;
    }

    m_isDetecting = true;
    emit isDetectingChanged();

    m_envService->detectEnvironmentAsync(
        this,
        [this](const Core::Result<Application::Environment::DetectionResult>& result) {
            if (result.isSuccess()) {
                applyDetectionResult(result.value());
            } else {
                m_isDetecting = false;
                emit isDetectingChanged();
                emit detectionFinished();
            }
        }
    );
}

void GameViewModel::applyDetectionResult(const Application::Environment::DetectionResult& result) {
    m_detectedGames.clear();

    for (const auto& game : result.installations) {
        if (game.isValid) {
            if (!game.gameId.isEmpty()) {
                m_detectedGames.insert(game.gameId.toLower(), game);
            }
            if (!game.displayName.isEmpty()) {
                m_detectedGames.insert(game.displayName.toLower(), game);
            }
        }
    }

    // Auto-select CS2 if found
    auto itCs2 = m_detectedGames.find(QStringLiteral("cs2"));
    if (itCs2 == m_detectedGames.end()) {
        itCs2 = m_detectedGames.find(QStringLiteral("counter-strike 2"));
    }
    if (itCs2 != m_detectedGames.end()) {
        applyS2Installation(itCs2.value());
    }

    // Auto-select current S1 game if found
    const QString currentS1Key = m_selectedS1Type.trimmed().toLower();
    auto itS1 = m_detectedGames.find(currentS1Key);
    if (itS1 != m_detectedGames.end()) {
        applyS1Installation(itS1.value());
    } else if (currentS1Key == QStringLiteral("csgo") || currentS1Key == QStringLiteral("cs:go")) {
        // Fallback checks
        for (const QString& fallbackKey : {QStringLiteral("css"), QStringLiteral("cs: source"), QStringLiteral("hl2"), QStringLiteral("half-life 2"), QStringLiteral("l4d2"), QStringLiteral("left 4 dead 2"), QStringLiteral("tf2"), QStringLiteral("team fortress 2")}) {
            auto itFallback = m_detectedGames.find(fallbackKey);
            if (itFallback != m_detectedGames.end()) {
                m_selectedS1Type = itFallback.value().displayName;
                emit selectedS1TypeChanged();
                applyS1Installation(itFallback.value());
                break;
            }
        }
    }

    m_isDetecting = false;
    emit isDetectingChanged();
    emit detectionFinished();
}

void GameViewModel::setSelectedS1Type(const QString& typeId) {
    if (m_selectedS1Type == typeId) {
        return;
    }

    m_selectedS1Type = typeId;
    emit selectedS1TypeChanged();

    const QString key = typeId.trimmed().toLower();
    auto it = m_detectedGames.find(key);
    if (it != m_detectedGames.end()) {
        applyS1Installation(it.value());
        return;
    }

    // Check if existing path is valid for this newly selected type asynchronously off UI thread
    if (!m_s1GamePath.isEmpty() && key != QStringLiteral("custom") && m_envService) {
        const QString currentPath = m_s1GamePath;
        const QString requestedType = typeId;
        m_envService->validateSource1FolderAsync(
            requestedType,
            currentPath,
            this,
            [this, requestedType, key](const Core::Result<Application::Environment::GameInstallationInfo>& validated) {
                if (m_selectedS1Type == requestedType && validated.isSuccess() && validated.value().isValid) {
                    m_detectedGames.insert(key, validated.value());
                    applyS1Installation(validated.value());
                } else if (m_selectedS1Type == requestedType) {
                    m_s1Installation = Application::Environment::GameInstallationInfo();
                    m_s1GamePath.clear();
                    m_s1GameTitle.clear();
                    m_isS1Valid = false;

                    emit s1GamePathChanged();
                    emit s1GameTitleChanged();
                    emit s1ValidityChanged();
                }
            }
        );
        return;
    }

    // Reset validation for newly selected game
    m_s1Installation = Application::Environment::GameInstallationInfo();
    m_s1GamePath.clear();
    m_s1GameTitle.clear();
    m_isS1Valid = false;

    emit s1GamePathChanged();
    emit s1GameTitleChanged();
    emit s1ValidityChanged();
}

void GameViewModel::setSelectedS2Type(const QString& typeId) {
    if (m_selectedS2Type == typeId) {
        return;
    }

    m_selectedS2Type = typeId;
    emit selectedS2TypeChanged();

    const QString key = typeId.trimmed().toLower();
    auto it = m_detectedGames.find(key);
    if (it != m_detectedGames.end()) {
        applyS2Installation(it.value());
    } else {
        m_s2Installation = Application::Environment::GameInstallationInfo();
        if (m_envService) {
            m_envService->updateVpkLease(m_s2Installation);
        }
        m_s2GamePath.clear();
        m_s2GameTitle.clear();
        m_isS2Valid = false;
        m_s2AddonsList.clear();
        m_selectedAddon.clear();

        emit s2GamePathChanged();
        emit s2GameTitleChanged();
        emit s2ValidityChanged();
        emit s2AddonsListChanged();
        emit selectedAddonChanged();
    }
}

void GameViewModel::selectS1Folder(const QString& pathOrUrl) {
    if (pathOrUrl.isEmpty() || !m_envService) {
        return;
    }

    const QString capturedType = m_selectedS1Type;

    m_envService->validateSource1FolderAsync(
        capturedType,
        pathOrUrl,
        this,
        [this, capturedType](const Core::Result<Application::Environment::GameInstallationInfo>& validated) {
            if (validated.isSuccess() && validated.value().isValid) {
                const QString key = capturedType.trimmed().toLower();
                m_detectedGames.insert(key, validated.value());
                if (!validated.value().gameId.isEmpty()) {
                    m_detectedGames.insert(validated.value().gameId.toLower(), validated.value());
                }
                applyS1Installation(validated.value());
            } else {
                m_isS1Valid = false;
                emit s1ValidityChanged();

                QString errorDetail = validated.message().isEmpty()
                    ? QStringLiteral("The selected directory is not a valid installation for the selected game.\nPlease verify that it contains the expected game files and gameinfo.txt.")
                    : validated.message();

                emit alertRequested(
                    QStringLiteral("Invalid Source 1 Installation"),
                    errorDetail
                );
            }
        }
    );
}

void GameViewModel::selectS2Folder(const QString& pathOrUrl) {
    if (pathOrUrl.isEmpty() || !m_envService) {
        return;
    }

    m_envService->validateSource2FolderAsync(
        pathOrUrl,
        this,
        [this](const Core::Result<Application::Environment::GameInstallationInfo>& validated) {
            if (validated.isSuccess() && validated.value().isValid) {
                if (!validated.value().gameId.isEmpty()) {
                    m_detectedGames.insert(validated.value().gameId.toLower(), validated.value());
                }
                applyS2Installation(validated.value());
            } else {
                m_s2Installation = Application::Environment::GameInstallationInfo();
                if (m_envService) {
                    m_envService->updateVpkLease(m_s2Installation);
                }
                m_isS2Valid = false;
                emit s2ValidityChanged();

                QString errorDetail = validated.message().isEmpty()
                    ? QStringLiteral("The selected folder is not a valid Source 2 installation.\nPlease ensure it contains game/csgo/gameinfo.gi or a valid Source 2 game layout.")
                    : validated.message();

                emit alertRequested(
                    QStringLiteral("Invalid Source 2 Installation"),
                    errorDetail
                );
            }
        }
    );
}

void GameViewModel::validateS1InSteam() {
    if (!m_envService) {
        return;
    }
    const QString target = m_s1Installation.isValid ? m_s1Installation.gameId : m_selectedS1Type;
    auto res = m_envService->validateGameInSteam(target);
    if (!res.isSuccess()) {
        emit alertRequested(
            QStringLiteral("Steam Validation Unavailable"),
            res.message().isEmpty()
                ? QStringLiteral("Could not initiate Steam validation. Make sure Steam is running and the game is installed.")
                : res.message()
        );
    }
}

void GameViewModel::validateS2InSteam() {
    if (!m_envService) {
        return;
    }
    const QString target = m_s2Installation.isValid ? m_s2Installation.gameId : m_selectedS2Type;
    auto res = m_envService->validateGameInSteam(target);
    if (!res.isSuccess()) {
        emit alertRequested(
            QStringLiteral("Steam Validation Unavailable"),
            res.message().isEmpty()
                ? QStringLiteral("Could not initiate Steam validation for Source 2. Make sure Steam is running and Counter-Strike 2 is installed.")
                : res.message()
        );
    }
}

void GameViewModel::setSelectedAddon(const QString& addon) {
    if (m_selectedAddon != addon) {
        m_selectedAddon = addon;
        emit selectedAddonChanged();
    }
}

void GameViewModel::refreshS2Addons() {
    QStringList addons;
    if (m_envService && m_s2Installation.isValid) {
        addons = m_envService->listSource2Addons(m_s2Installation);
    }

    m_s2AddonsList = addons;
    emit s2AddonsListChanged();

    if (!m_s2AddonsList.isEmpty()) {
        if (m_selectedAddon.isEmpty() || !m_s2AddonsList.contains(m_selectedAddon)) {
            setSelectedAddon(m_s2AddonsList.first());
        }
    } else {
        setSelectedAddon(QString());
    }
}

} // namespace UI::ViewModels
