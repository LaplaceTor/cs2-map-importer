import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: root

    property QtObject gameViewModel: null
    property QtObject mainController: null
    property string selectedMdlPath: ""

    signal requestBrowseS1()
    signal requestBrowseS2()
    signal requestBrowseMdl()

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

        // Row 2: Model File & Target Addon
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                text: root.selectedMdlPath === "" ? qsTr("Select Source 1 MDL File") : root.selectedMdlPath
                enabled: !(root.mainController && root.mainController.isProcessing)
                Layout.fillWidth: true
                Layout.preferredHeight: 38

                contentItem: Text {
                    text: parent.text
                    elide: Text.ElideLeft
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: parent.palette.buttonText
                }

                onClicked: root.requestBrowseMdl()
            }

            Label {
                text: "➔"
                font.pixelSize: 26
                Layout.alignment: Qt.AlignVCenter
                color: palette.text
            }

            ComboBox {
                id: addonCombo
                model: root.gameViewModel ? root.gameViewModel.s2AddonsList : []
                currentIndex: Math.max(0, model && root.gameViewModel ? model.indexOf(root.gameViewModel.selectedAddon) : 0)
                enabled: !(root.mainController && root.mainController.isProcessing)
                Layout.fillWidth: true
                Layout.preferredHeight: 38

                contentItem: Text {
                    text: addonCombo.displayText
                    font: addonCombo.font
                    color: addonCombo.palette.text
                    leftPadding: 10
                    rightPadding: addonCombo.indicator ? addonCombo.indicator.width + 10 : 20
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideLeft
                }

                onActivated: {
                    if (root.gameViewModel) {
                        root.gameViewModel.setSelectedAddon(currentText)
                    }
                }
            }
        }

        // Row 3: Model Options
        GroupBox {
            title: qsTr("Model Options")
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 6

                CheckBox {
                    text: qsTr("Skip animation import (-skipcommondmxwrite)")
                    checked: false
                }

                CheckBox {
                    text: qsTr("Change bindpose from Yup to Zup (-YupToZup)")
                    checked: false
                }

                CheckBox {
                    text: qsTr("Override \"lean\" sequence (-overridelean)")
                    checked: false
                }

                CheckBox {
                    text: qsTr("Import mdl hull bounds from studiohdr (-header_hull_bounds)")
                    checked: false
                }

                CheckBox {
                    text: qsTr("Import all LODs (-lods)")
                    checked: false
                }

                CheckBox {
                    text: qsTr("Write weapon sequences & weightlists into prefab (-write_weapon_anim_prefab)")
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
                text: qsTr("START IMPORT")
                font.bold: true
                enabled: !(root.mainController && root.mainController.isProcessing) &&
                         (root.gameViewModel && root.gameViewModel.isS1Valid && root.gameViewModel.isS2Valid && root.selectedMdlPath !== "")
                Layout.fillWidth: true
                Layout.fillHeight: true

                onClicked: {
                    if (root.mainController) {
                        root.mainController.startImport()
                    }
                }
            }

            Button {
                text: qsTr("STOP")
                font.bold: true
                enabled: root.mainController && root.mainController.isProcessing
                Layout.fillWidth: true
                Layout.fillHeight: true

                onClicked: {
                    if (root.mainController) {
                        root.mainController.stopImport()
                    }
                }
            }
        }
    }
}

