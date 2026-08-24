import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: root

    property QtObject gameViewModel: null
    property QtObject mainController: null
    property string selectedMapPath: ""

    signal requestBrowseS1()
    signal requestBrowseS2()
    signal requestBrowseMap()

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

        // Row 2: Map File & Addon Name
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                text: root.selectedMapPath === "" ? qsTr("Select VMF / BSP Map File") : root.selectedMapPath
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

                onClicked: root.requestBrowseMap()
            }

            Label {
                text: "➔"
                font.pixelSize: 26
                Layout.alignment: Qt.AlignVCenter
                color: palette.text
            }

            TextField {
                id: addonField
                placeholderText: qsTr("Addon Name in Source 2")
                text: root.gameViewModel ? root.gameViewModel.selectedAddon : ""
                horizontalAlignment: TextInput.AlignHCenter
                verticalAlignment: TextInput.AlignVCenter
                enabled: !(root.mainController && root.mainController.isProcessing)
                Layout.fillWidth: true
                Layout.preferredHeight: 38

                onTextChanged: {
                    if (root.gameViewModel) {
                        root.gameViewModel.setSelectedAddon(text)
                    }
                }
            }
        }

        // Row 3: Import Options
        GroupBox {
            title: qsTr("Options")
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 6

                CheckBox {
                    id: cleanFacesCheck
                    text: qsTr("Clean Unnecessary Faces (vbsp brush cleanup)")
                    checked: true
                    ToolTip.text: qsTr("Runs map brushes through special processing to generate clean map geometry")
                    ToolTip.visible: hovered
                }

                CheckBox {
                    id: keepInstancesCheck
                    text: qsTr("Keep func_instance as separate part")
                    checked: false
                    enabled: cleanFacesCheck.checked
                    ToolTip.text: qsTr("Preserves func_instance entities without merging them into world geometry")
                    ToolTip.visible: hovered
                }

                CheckBox {
                    id: keepFuncDetailCheck
                    text: qsTr("Keep func_detail as brush")
                    checked: false
                    ToolTip.text: qsTr("Converts func_detail entities to func_brush instead of merging with world geometry")
                    ToolTip.visible: hovered
                }

                CheckBox {
                    id: skipDepsCheck
                    text: qsTr("Skip References Import")
                    checked: false
                    ToolTip.text: qsTr("Skips importing referenced dependencies/content and generates only vmap files")
                    ToolTip.visible: hovered
                }

                CheckBox {
                    id: fullLogCheck
                    text: qsTr("Detailed / Verbose external tools logging")
                    checked: false
                    ToolTip.text: qsTr("Outputs complete stdout/stderr logs from external command tools")
                    ToolTip.visible: hovered
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
                id: startBtn
                text: qsTr("START IMPORT")
                font.bold: true
                enabled: !(root.mainController && root.mainController.isProcessing) &&
                         (root.gameViewModel && root.gameViewModel.isS1Valid && root.gameViewModel.isS2Valid && root.selectedMapPath !== "")
                Layout.fillWidth: true
                Layout.fillHeight: true

                onClicked: {
                    if (root.mainController) {
                        root.mainController.startImport()
                    }
                }
            }

            Button {
                id: stopBtn
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

