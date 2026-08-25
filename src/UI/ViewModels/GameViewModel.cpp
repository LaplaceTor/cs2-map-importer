#include "UI/ViewModels/GameViewModel.h"
#include "Core/Path/PathUtils.h"
#include <QDir>
#include <QUrl>

namespace UI::ViewModels {

GameViewModel::GameViewModel(Application::Environment::VpkSignatureLeaseService* vpkLeaseService, QObject* parent)
    : QObject(parent)
{
    setVpkSignatureLeaseService(vpkLeaseService);
}

void GameViewModel::setVpkSignatureLeaseService(Application::Environment::VpkSignatureLeaseService* service) {
    if (m_vpkLeaseService == service) {
        return;
    }
    if (m_vpkLeaseService) {
        disconnect(m_vpkLeaseService, nullptr, this, nullptr);
    }
    m_vpkLeaseService = service;
    if (m_vpkLeaseService) {
        connect(m_vpkLeaseService, &Application::Environment::VpkSignatureLeaseService::leaseStateChanged,
                this, &GameViewModel::vpkLeaseStateChanged);
    }
    emit vpkLeaseStateChanged();
}

bool GameViewModel::isVpkLeaseHeld() const noexcept {
    return m_vpkLeaseService ? m_vpkLeaseService->isLeaseHeld() : false;
}

QStringList GameViewModel::s1GameTypes() const {
    return {
        QStringLiteral("CSGO"),
        QStringLiteral("CS: Source"),
        QStringLiteral("Half-Life 2"),
        QStringLiteral("Left 4 Dead"),
        QStringLiteral("Left 4 Dead 2"),
        QStringLiteral("Portal"),
        QStringLiteral("Portal 2"),
        QStringLiteral("Team Fortress 2"),
        QStringLiteral("Garry's Mod"),
        QStringLiteral("Black Mesa"),
        QStringLiteral("Custom")
    };
}

QStringList GameViewModel::s2GameTypes() const {
    return {
        QStringLiteral("Counter-Strike 2")
    };
}

Domain::Game::GameType GameViewModel::parseS1Type(const QString& typeStr) const {
    const QString lower = typeStr.trimmed().toLower();
    if (lower == QStringLiteral("cs: global offensive") || lower == QStringLiteral("cs:go") || lower == QStringLiteral("counter-strike: global offensive") || lower == QStringLiteral("counter-strike global offensive") || lower == QStringLiteral("csgo")) {
        return Domain::Game::GameType::CSGO;
    }
    if (lower == QStringLiteral("cs: source") || lower == QStringLiteral("cs:s") || lower == QStringLiteral("counter-strike: source") || lower == QStringLiteral("counter-strike source") || lower == QStringLiteral("css")) {
        return Domain::Game::GameType::CSS;
    }
    if (lower == QStringLiteral("half-life 2") || lower == QStringLiteral("hl2")) {
        return Domain::Game::GameType::HL2;
    }
    if (lower == QStringLiteral("left 4 dead") || lower == QStringLiteral("l4d")) {
        return Domain::Game::GameType::L4D;
    }
    if (lower == QStringLiteral("left 4 dead 2") || lower == QStringLiteral("l4d2")) {
        return Domain::Game::GameType::L4D2;
    }
    if (lower == QStringLiteral("portal")) {
        return Domain::Game::GameType::Portal;
    }
    if (lower == QStringLiteral("portal 2") || lower == QStringLiteral("portal2")) {
        return Domain::Game::GameType::Portal2;
    }
    if (lower == QStringLiteral("team fortress 2") || lower == QStringLiteral("tf2")) {
        return Domain::Game::GameType::TF2;
    }
    if (lower == QStringLiteral("garry's mod") || lower == QStringLiteral("garrysmod") || lower == QStringLiteral("gmod")) {
        return Domain::Game::GameType::GMod;
    }
    if (lower == QStringLiteral("black mesa") || lower == QStringLiteral("blackmesa")) {
        return Domain::Game::GameType::BlackMesa;
    }
    if (lower == QStringLiteral("custom") || lower == QStringLiteral("custom game") || lower == QStringLiteral("other") || lower == QStringLiteral("other source 1 game")) {
        return Domain::Game::GameType::Custom;
    }
    return Domain::Game::GameRegistry::stringToGameType(lower);
}

Domain::Game::GameType GameViewModel::parseS2Type(const QString& typeStr) const {
    return Domain::Game::GameRegistry::stringToGameType(typeStr);
}

QString GameViewModel::cleanInputPath(const QString& pathOrUrl) const {
    if (pathOrUrl.isEmpty()) {
        return QString();
    }
    QUrl url(pathOrUrl);
    QString path = url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
    return Core::Path::PathUtils::normalize(path);
}

void GameViewModel::applyS1Installation(const Application::Environment::GameInstallation& inst) {
    m_s1Installation = inst;
    m_s1GamePath = inst.baseDirectory().toString();
    m_s1GameTitle = inst.gameTitle();
    m_isS1Valid = inst.isValid();

    emit s1GamePathChanged();
    emit s1GameTitleChanged();
    emit s1ValidityChanged();
}

void GameViewModel::applyS2Installation(const Application::Environment::GameInstallation& inst) {
    m_s2Installation = inst;
    m_s2GamePath = inst.baseDirectory().toString();
    m_s2GameTitle = inst.gameTitle();
    m_isS2Valid = inst.isValid();

    emit s2GamePathChanged();
    emit s2GameTitleChanged();
    emit s2ValidityChanged();

    refreshS2Addons();

    // Delegate vpk.signatures lease lifecycle entirely to the Application service
    if (m_vpkLeaseService) {
        m_vpkLeaseService->updateInstallation(inst);
    }
}

void GameViewModel::retryVpkSignatureLease() {
    if (m_vpkLeaseService) {
        m_vpkLeaseService->retryLease();
    }
}

void GameViewModel::onVpkLeaseStatusChanged(Application::Environment::VpkSignatureLeaseStatus status, const QString& filePath, const QString& systemMessage) {
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
    if (m_isDetecting) {
        return;
    }

    m_isDetecting = true;
    emit isDetectingChanged();

    Application::Environment::GameDetectService::detectEnvironmentAsync(
        this,
        [this](const Application::Environment::DetectionResult& result) {
            applyDetectionResult(result);
        }
    );
}

void GameViewModel::applyDetectionResult(const Application::Environment::DetectionResult& result) {
    m_detectedGames.clear();

    for (const auto& game : result.installations) {
        if (game.isValid()) {
            m_detectedGames.insert(game.type(), game);
        }
    }

    // Auto-select CS2 if found
    auto itCs2 = m_detectedGames.find(Domain::Game::GameType::CS2);
    if (itCs2 != m_detectedGames.end()) {
        applyS2Installation(itCs2.value());
    }

    // Auto-select current S1 game if found
    Domain::Game::GameType activeType = parseS1Type(m_selectedS1Type);
    auto itS1 = m_detectedGames.find(activeType);
    if (itS1 != m_detectedGames.end()) {
        applyS1Installation(itS1.value());
    } else if (activeType == Domain::Game::GameType::CSGO) {
        // If CSGO is default and not found, check CSS or HL2
        for (const auto& fallbackType : {Domain::Game::GameType::CSS, Domain::Game::GameType::HL2, Domain::Game::GameType::L4D2, Domain::Game::GameType::TF2}) {
            auto itFallback = m_detectedGames.find(fallbackType);
            if (itFallback != m_detectedGames.end()) {
                const auto* def = Domain::Game::GameRegistry::findByType(fallbackType);
                if (def) {
                    m_selectedS1Type = def->displayName;
                    emit selectedS1TypeChanged();
                }
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
    if (m_selectedS1Type != typeId) {
        m_selectedS1Type = typeId;
        emit selectedS1TypeChanged();

        Domain::Game::GameType type = parseS1Type(typeId);
        auto it = m_detectedGames.find(type);
        if (it != m_detectedGames.end()) {
            applyS1Installation(it.value());
        } else {
            // Check if existing path is valid for this newly selected type
            if (!m_s1GamePath.isEmpty() && type != Domain::Game::GameType::Custom) {
                auto validated = Application::Environment::GameDetectService::validateSource1(type, Core::Path::FilesystemPath(m_s1GamePath));
                if (validated.has_value() && validated->isValid()) {
                    m_detectedGames.insert(type, *validated);
                    applyS1Installation(*validated);
                    return;
                }
            }
            // Reset validation for newly selected game
            m_s1Installation = Application::Environment::GameInstallation();
            m_s1GamePath.clear();
            m_s1GameTitle.clear();
            m_isS1Valid = false;

            emit s1GamePathChanged();
            emit s1GameTitleChanged();
            emit s1ValidityChanged();
        }
    }
}

void GameViewModel::setSelectedS2Type(const QString& typeId) {
    if (m_selectedS2Type != typeId) {
        m_selectedS2Type = typeId;
        emit selectedS2TypeChanged();

        Domain::Game::GameType type = parseS2Type(typeId);
        auto it = m_detectedGames.find(type);
        if (it != m_detectedGames.end()) {
            applyS2Installation(it.value());
        } else {
            m_s2Installation = Application::Environment::GameInstallation();
            if (m_vpkLeaseService) {
                m_vpkLeaseService->updateInstallation(m_s2Installation);
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
}

void GameViewModel::selectS1Folder(const QString& pathOrUrl) {
    QString rawPath = cleanInputPath(pathOrUrl);
    if (rawPath.isEmpty()) {
        return;
    }

    Core::Path::FilesystemPath fsPath(rawPath);
    Domain::Game::GameType activeType = parseS1Type(m_selectedS1Type);

    std::optional<Application::Environment::GameInstallation> validated = std::nullopt;
    if (activeType == Domain::Game::GameType::Custom) {
        validated = Application::Environment::GameDetectService::inspectGameInfo(fsPath);
    } else {
        validated = Application::Environment::GameDetectService::validateSource1(activeType, fsPath);
    }

    if (validated.has_value() && validated->isValid()) {
        m_detectedGames.insert(activeType, *validated);
        applyS1Installation(*validated);
    } else {
        m_isS1Valid = false;
        emit s1ValidityChanged();

        emit alertRequested(
            QStringLiteral("Invalid Source 1 Installation"),
            QStringLiteral("The selected directory is not a valid installation for the selected game.\nPlease verify that it contains the expected game files and gameinfo.txt.")
        );
    }
}

void GameViewModel::selectS2Folder(const QString& pathOrUrl) {
    QString rawPath = cleanInputPath(pathOrUrl);
    if (rawPath.isEmpty()) {
        return;
    }

    Core::Path::FilesystemPath fsPath(rawPath);
    auto validated = Application::Environment::GameDetectService::validateSource2(fsPath);

    if (validated.has_value() && validated->isValid()) {
        m_detectedGames.insert(validated->type(), *validated);
        applyS2Installation(*validated);
    } else {
        m_s2Installation = Application::Environment::GameInstallation();
        if (m_vpkLeaseService) {
            m_vpkLeaseService->updateInstallation(m_s2Installation);
        }
        m_isS2Valid = false;
        emit s2ValidityChanged();

        emit alertRequested(
            QStringLiteral("Invalid Source 2 Installation"),
            QStringLiteral("The selected folder is not a valid Source 2 installation.\nPlease ensure it contains game/csgo/gameinfo.gi or a valid Source 2 game layout.")
        );
    }
}

void GameViewModel::validateS1InSteam() {
    Domain::Game::GameType type = m_s1Installation.isValid() ? m_s1Installation.type() : parseS1Type(m_selectedS1Type);
    bool ok = Application::Environment::SteamService::validateGameFiles(type);
    if (!ok) {
        emit alertRequested(
            QStringLiteral("Steam Validation Unavailable"),
            QStringLiteral("Could not initiate Steam validation. Make sure Steam is running and the game is installed.")
        );
    }
}

void GameViewModel::validateS2InSteam() {
    Domain::Game::GameType type = m_s2Installation.isValid() ? m_s2Installation.type() : parseS2Type(m_selectedS2Type);
    bool ok = Application::Environment::SteamService::validateGameFiles(type);
    if (!ok) {
        emit alertRequested(
            QStringLiteral("Steam Validation Unavailable"),
            QStringLiteral("Could not initiate Steam validation for Source 2. Make sure Steam is running and Counter-Strike 2 is installed.")
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
    if (m_s2Installation.isValid()) {
        Core::Path::FilesystemPath addonsDir = m_s2Installation.addonGameDirectory();
        if (addonsDir.isValid() && addonsDir.exists() && addonsDir.isDirectory()) {
            QDir dir(addonsDir.toString());
            const QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const auto& entry : entries) {
                addons.append(entry);
            }
        }
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

