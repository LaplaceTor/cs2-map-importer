import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "tabs"

ApplicationWindow {
    id: window
    width: 420
    height: 700
    minimumWidth: 420
    maximumWidth: 420
    minimumHeight: 700
    maximumHeight: 700
    flags: Qt.Window | Qt.MSWindowsFixedSizeDialogHint | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowMinimizeButtonHint | Qt.WindowCloseButtonHint
    visible: true
    title: qsTr("CS2 IMPORTER")

    onClosing: function(closeEvent) {
        if (logWindow) {
            logWindow.close()
        }
        Qt.quit()
    }

    property QtObject gameViewModel: gameViewModelInstance
    property QtObject logViewModel: logViewModelInstance
    property QtObject mainController: mainControllerInstance

    property string selectedMapFileName: ""
    property string selectedMdlFileName: ""
    property string selectedPcfFileName: ""

    // Connections to C++ signals
    Connections {
        target: mainController
        function onAlertRequested(title, message) {
            alertDialog.title = title
            alertDialog.text = message
            alertDialog.open()
        }
        function onLogWindowToggleRequested() {
            logWindow.visible = !logWindow.visible
            if (logWindow.visible) {
                logWindow.raise()
                logWindow.requestActivate()
            }
        }
    }

    Connections {
        target: gameViewModel
        function onAlertRequested(title, message) {
            alertDialog.title = title
            alertDialog.text = message
            alertDialog.open()
        }
        function onVpkSignatureOccupied(title, message) {
            vpkConflictDialog.title = title
            vpkConflictDialog.text = message
            vpkConflictDialog.open()
        }
    }

    // Dialogs
    MessageDialog {
        id: alertDialog
        title: qsTr("Alert")
        buttons: MessageDialog.Ok
    }

    MessageDialog {
        id: vpkConflictDialog
        title: qsTr("Counter-Strike 2 is Running")
        text: qsTr("vpk.signatures is currently in use by Counter-Strike 2 or another application.\n\nPlease close Counter-Strike 2 before using CS2 Importer.")
        buttons: MessageDialog.Retry | MessageDialog.Close

        onButtonClicked: function(button, role) {
            if (button === MessageDialog.Retry) {
                if (window.gameViewModel) {
                    window.gameViewModel.retryVpkSignatureLease()
                }
            } else {
                Qt.quit()
            }
        }
        onRejected: {
            Qt.quit()
        }
    }

    MessageDialog {
        id: validateConfirmDialog
        title: qsTr("Confirm Game Validation")
        text: qsTr("Validating game files through Steam is only needed if you are certain there are corrupted or missing game files.\n\nDo you want to proceed?")
        buttons: MessageDialog.Yes | MessageDialog.No

        property var pendingCallback: null

        onButtonClicked: function(button, role) {
            if (button === MessageDialog.Yes && pendingCallback) {
                pendingCallback()
            }
            pendingCallback = null
        }
    }

    function confirmAndValidateS1() {
        validateConfirmDialog.pendingCallback = function() {
            if (window.gameViewModel) {
                window.gameViewModel.validateS1InSteam()
            }
        }
        validateConfirmDialog.open()
    }

    function confirmAndValidateS2() {
        validateConfirmDialog.pendingCallback = function() {
            if (window.gameViewModel) {
                window.gameViewModel.validateS2InSteam()
            }
        }
        validateConfirmDialog.open()
    }

    FolderDialog {
        id: s1FolderDialog
        title: qsTr("Select Source 1 Game Folder")
        onAccepted: {
            if (window.gameViewModel) {
                window.gameViewModel.selectS1Folder(selectedFolder)
            }
        }
    }

    FileDialog {
        id: s1GameInfoDialog
        title: qsTr("Select gameinfo.txt")
        nameFilters: ["gameinfo.txt"]
        onAccepted: {
            if (window.gameViewModel) {
                window.gameViewModel.selectS1Folder(selectedFile)
            }
        }
    }

    FolderDialog {
        id: s2FolderDialog
        title: qsTr("Select Source 2 Game Folder")
        onAccepted: {
            if (window.gameViewModel) {
                window.gameViewModel.selectS2Folder(selectedFolder)
            }
        }
    }

    FileDialog {
        id: mapFileDialog
        title: qsTr("Select VMF or BSP Map File")
        nameFilters: ["Map files (*.vmf *.bsp)"]
        onAccepted: {
            selectedMapFileName = selectedFile.toString()
        }
    }

    FileDialog {
        id: mdlFileDialog
        title: qsTr("Select Source 1 MDL Model File")
        nameFilters: ["Model files (*.mdl)"]
        onAccepted: {
            selectedMdlFileName = selectedFile.toString()
        }
    }

    FileDialog {
        id: pcfFileDialog
        title: qsTr("Select Source 1 PCF Particle File")
        nameFilters: ["Particle files (*.pcf)"]
        onAccepted: {
            selectedPcfFileName = selectedFile.toString()
        }
    }

    // Standalone Log Window instance
    LogWindow {
        id: logWindow
        transientParent: null
        logViewModel: window.logViewModel
    }

    // Main UI Layout
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        // Top Navigation TabBar
        TabBar {
            id: navTabBar
            Layout.fillWidth: true
            enabled: !(window.mainController && window.mainController.isProcessing)
            currentIndex: window.mainController ? window.mainController.activeTab : 0

            onCurrentIndexChanged: {
                if (window.mainController) {
                    window.mainController.setActiveTab(currentIndex)
                }
            }

            TabButton {
                text: qsTr("Map")
                font.pixelSize: 13
                implicitHeight: 25
                topPadding: 6
                bottomPadding: 6
            }
            TabButton {
                text: qsTr("Model")
                font.pixelSize: 13
                implicitHeight: 25
                topPadding: 6
                bottomPadding: 6
            }
            TabButton {
                text: qsTr("Particle")
                font.pixelSize: 13
                implicitHeight: 25
                topPadding: 6
                bottomPadding: 6
            }
        }

        // Center Content Stack
        StackLayout {
            id: contentStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: navTabBar.currentIndex

            MapTab {
                gameViewModel: window.gameViewModel
                mainController: window.mainController
                selectedMapPath: window.selectedMapFileName

                onRequestBrowseS1: {
                    if (window.gameViewModel && (window.gameViewModel.selectedS1Type.toLowerCase() === "custom" || window.gameViewModel.selectedS1Type.toLowerCase() === "other")) {
                        s1GameInfoDialog.open()
                    } else {
                        s1FolderDialog.open()
                    }
                }
                onRequestBrowseS2: s2FolderDialog.open()
                onRequestBrowseMap: mapFileDialog.open()
                onRequestValidateS1: window.confirmAndValidateS1()
                onRequestValidateS2: window.confirmAndValidateS2()
            }

            ModelTab {
                gameViewModel: window.gameViewModel
                mainController: window.mainController
                selectedMdlPath: window.selectedMdlFileName

                onRequestBrowseS1: {
                    if (window.gameViewModel && (window.gameViewModel.selectedS1Type.toLowerCase() === "custom" || window.gameViewModel.selectedS1Type.toLowerCase() === "other")) {
                        s1GameInfoDialog.open()
                    } else {
                        s1FolderDialog.open()
                    }
                }
                onRequestBrowseS2: s2FolderDialog.open()
                onRequestBrowseMdl: mdlFileDialog.open()
                onRequestValidateS1: window.confirmAndValidateS1()
                onRequestValidateS2: window.confirmAndValidateS2()
            }

            ParticleTab {
                gameViewModel: window.gameViewModel
                mainController: window.mainController
                selectedPcfPath: window.selectedPcfFileName

                onRequestBrowseS1: {
                    if (window.gameViewModel && (window.gameViewModel.selectedS1Type.toLowerCase() === "custom" || window.gameViewModel.selectedS1Type.toLowerCase() === "other")) {
                        s1GameInfoDialog.open()
                    } else {
                        s1FolderDialog.open()
                    }
                }
                onRequestBrowseS2: s2FolderDialog.open()
                onRequestBrowsePcf: pcfFileDialog.open()
                onRequestValidateS1: window.confirmAndValidateS1()
                onRequestValidateS2: window.confirmAndValidateS2()
            }
        }

        // Bottom Footer Bar
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 24

            // Left: Fixed-width Theme Button
            Button {
                id: themeButton
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: 96
                height: 24
                implicitHeight: 24
                font.pixelSize: 11
                text: {
                    if (!window.mainController) return qsTr("Theme:System")
                    if (window.mainController.theme === "light") return qsTr("Theme:Light")
                    if (window.mainController.theme === "dark") return qsTr("Theme:Dark")
                    return qsTr("Theme:System")
                }
                onClicked: {
                    if (window.mainController) {
                        window.mainController.cycleTheme()
                    }
                }
            }

            // Center: Check Update Button strictly centered
            Button {
                id: checkUpdateButton
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Check Update")
                enabled: !(window.mainController && window.mainController.isProcessing)
                height: 24
                implicitHeight: 24
                font.pixelSize: 11
                onClicked: {
                    if (window.mainController) {
                        window.mainController.checkForUpdates()
                    }
                }
            }

            // Version Label follows immediately after Check Update
            Label {
                id: versionLabel
                anchors.left: checkUpdateButton.right
                anchors.leftMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("v%1").arg(window.mainController ? window.mainController.appVersion : "1.0.0")
                font.pixelSize: 11
                color: palette.placeholderText
            }

            // Right: Fixed-width LOG Button
            Button {
                id: logButton
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 55
                height: 24
                implicitHeight: 24
                font.pixelSize: 11
                text: qsTr("LOG")
                highlighted: logWindow.visible

                onClicked: {
                    logWindow.visible = !logWindow.visible
                    if (logWindow.visible) {
                        logWindow.raise()
                        logWindow.requestActivate()
                    }
                }
            }
        }
    }
}
