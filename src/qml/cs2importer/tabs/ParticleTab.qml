import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: root

    property var gameViewModel: null
    property var mainController: null
    property string selectedPcfPath: ""

    signal requestBrowseS1()
    signal requestBrowseS2()
    signal requestBrowsePcf()

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        // Row 1: Game Selectors (Source 1 <-> Source 2)
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            GameSelectorBox {
                id: s1Box
                titleText: qsTr("Source 1 Game")
                gameTypesModel: root.gameViewModel ? root.gameViewModel.s1GameTypes : []
                selectedType: root.gameViewModel ? root.gameViewModel.selectedS1Type : ""
                gamePath: root.gameViewModel ? root.gameViewModel.s1GamePath : ""
                gameTitle: root.gameViewModel ? root.gameViewModel.s1GameTitle : ""
                isValid: root.gameViewModel ? root.gameViewModel.isS1Valid : false
                isCustomGame: root.selectedType === "Other Source 1 game" || root.selectedType === "other"
                isProcessing: root.mainController ? root.mainController.isProcessing : false

                onTypeSelected: function(typeName) {
                    if (root.gameViewModel) {
                        root.gameViewModel.setSelectedS1Type(typeName)
                    }
                }
                onBrowseClicked: root.requestBrowseS1()
                onValidateClicked: {
                    if (root.gameViewModel) {
                        root.gameViewModel.validateS1InSteam()
                    }
                }
            }

            Label {
                text: "➔"
                font.pixelSize: 26
                Layout.alignment: Qt.AlignVCenter
                color: palette.text
            }

            GameSelectorBox {
                id: s2Box
                titleText: qsTr("Source 2 Game")
                gameTypesModel: root.gameViewModel ? root.gameViewModel.s2GameTypes : []
                selectedType: root.gameViewModel ? root.gameViewModel.selectedS2Type : ""
                gamePath: root.gameViewModel ? root.gameViewModel.s2GamePath : ""
                gameTitle: root.gameViewModel ? root.gameViewModel.s2GameTitle : ""
                isValid: root.gameViewModel ? root.gameViewModel.isS2Valid : false
                isCustomGame: false
                isProcessing: root.mainController ? root.mainController.isProcessing : false

                onTypeSelected: function(typeName) {
                    if (root.gameViewModel) {
                        root.gameViewModel.setSelectedS2Type(typeName)
                    }
                }
                onBrowseClicked: root.requestBrowseS2()
                onValidateClicked: {
                    if (root.gameViewModel) {
                        root.gameViewModel.validateS2InSteam()
                    }
                }
            }
        }

        // Row 2: Particle File & Target Addon
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                enabled: !(root.mainController && root.mainController.isProcessing)
                text: root.selectedPcfPath === "" ? qsTr("Select Source 1 PCF File") : root.selectedPcfPath

                contentItem: Text {
                    text: parent.text
                    elide: Text.ElideMiddle
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: parent.palette.buttonText
                }

                onClicked: root.requestBrowsePcf()
            }

            Label {
                text: "➔"
                font.pixelSize: 26
                Layout.alignment: Qt.AlignVCenter
                color: palette.text
            }

            ComboBox {
                id: addonCombo
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                enabled: !(root.mainController && root.mainController.isProcessing)
                model: root.gameViewModel ? root.gameViewModel.s2AddonsList : []
                currentIndex: Math.max(0, model && root.gameViewModel ? model.indexOf(root.gameViewModel.selectedAddon) : 0)

                onActivated: {
                    if (root.gameViewModel) {
                        root.gameViewModel.setSelectedAddon(currentText)
                    }
                }
            }
        }

        // Row 3: Particle Options
        GroupBox {
            title: qsTr("Particle Options")
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 6

                CheckBox {
                    text: qsTr("Respect $DEPTHBLEND in particle materials (-particle_allow_depth_blend)")
                    checked: false
                }

                CheckBox {
                    text: qsTr("Disable diffuse lighting on imported particle systems (-particle_disable_diffuse)")
                    checked: false
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }

        // Row 4: Action Buttons (START / STOP)
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            spacing: 12

            Button {
                Layout.fillWidth: true
                Layout.fillHeight: true
                enabled: !(root.mainController && root.mainController.isProcessing) &&
                         (root.gameViewModel && root.gameViewModel.isS1Valid && root.gameViewModel.isS2Valid && root.selectedPcfPath !== "")
                text: qsTr("START IMPORT")
                font.bold: true

                onClicked: {
                    if (root.mainController) {
                        root.mainController.startImport()
                    }
                }
            }

            Button {
                Layout.fillWidth: true
                Layout.fillHeight: true
                enabled: root.mainController && root.mainController.isProcessing
                text: qsTr("STOP")
                font.bold: true

                onClicked: {
                    if (root.mainController) {
                        root.mainController.stopImport()
                    }
                }
            }
        }
    }
}

