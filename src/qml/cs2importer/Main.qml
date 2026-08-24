import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "tabs"

ApplicationWindow {
    id: window
    width: 640
    height: 720
    minimumWidth: 560
    minimumHeight: 620
    visible: true
    title: qsTr("CS2 IMPORTER")

    property var gameViewModel: gameViewModelInstance
    property var logViewModel: logViewModelInstance
    property var mainController: mainControllerInstance

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
    }

    // Dialogs
    MessageDialog {
        id: alertDialog
        title: qsTr("Alert")
        buttons: MessageDialog.Ok
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
        logViewModel: window.logViewModel
    }

    // Main UI Layout
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

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
                text: qsTr("Map Import")
            }
            TabButton {
                text: qsTr("Model Import")
            }
            TabButton {
                text: qsTr("Particle Import")
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
                    if (window.gameViewModel && (window.gameViewModel.selectedS1Type === "other" || window.gameViewModel.selectedS1Type === "Other Source 1 game")) {
                        s1GameInfoDialog.open()
                    } else {
                        s1FolderDialog.open()
                    }
                }
                onRequestBrowseS2: s2FolderDialog.open()
                onRequestBrowseMap: mapFileDialog.open()
            }

            ModelTab {
                gameViewModel: window.gameViewModel
                mainController: window.mainController
                selectedMdlPath: window.selectedMdlFileName

                onRequestBrowseS1: {
                    if (window.gameViewModel && (window.gameViewModel.selectedS1Type === "other" || window.gameViewModel.selectedS1Type === "Other Source 1 game")) {
                        s1GameInfoDialog.open()
                    } else {
                        s1FolderDialog.open()
                    }
                }
                onRequestBrowseS2: s2FolderDialog.open()
                onRequestBrowseMdl: mdlFileDialog.open()
            }

            ParticleTab {
                gameViewModel: window.gameViewModel
                mainController: window.mainController
                selectedPcfPath: window.selectedPcfFileName

                onRequestBrowseS1: {
                    if (window.gameViewModel && (window.gameViewModel.selectedS1Type === "other" || window.gameViewModel.selectedS1Type === "Other Source 1 game")) {
                        s1GameInfoDialog.open()
                    } else {
                        s1FolderDialog.open()
                    }
                }
                onRequestBrowseS2: s2FolderDialog.open()
                onRequestBrowsePcf: pcfFileDialog.open()
            }
        }

        // Bottom Footer Bar
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            spacing: 10

            Label {
                text: qsTr("Theme:")
                Layout.alignment: Qt.AlignVCenter
            }

            Button {
                text: {
                    if (!window.mainController) return qsTr("System")
                    if (window.mainController.theme === "light") return qsTr("Light")
                    if (window.mainController.theme === "dark") return qsTr("Dark")
                    return qsTr("System")
                }
                Layout.alignment: Qt.AlignVCenter
                onClicked: {
                    if (window.mainController) {
                        window.mainController.cycleTheme()
                    }
                }
            }

            Button {
                text: qsTr("Check Update")
                enabled: !(window.mainController && window.mainController.isProcessing)
                Layout.alignment: Qt.AlignVCenter
                onClicked: {
                    if (window.mainController) {
                        window.mainController.checkForUpdates()
                    }
                }
            }

            Label {
                text: qsTr("Version: %1").arg(window.mainController ? window.mainController.appVersion : "1.0.0")
                Layout.alignment: Qt.AlignVCenter
                color: palette.placeholderText
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                id: logButton
                text: qsTr("LOG")
                font.bold: true
                Layout.alignment: Qt.AlignVCenter
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
