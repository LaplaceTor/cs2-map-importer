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
    signal requestValidateS1()
    signal requestValidateS2()

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        // Row 1: Game Selectors (Source 1 <-> Source 2)
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 15

            GameSelectorBox {
                id: s1Box
                titleText: qsTr("Source 1 Game")
                gameTypesModel: root.gameViewModel ? root.gameViewModel.s1GameTypes : []
                selectedType: root.gameViewModel ? root.gameViewModel.selectedS1Type : ""
                gamePath: root.gameViewModel ? root.gameViewModel.s1GamePath : ""
                gameTitle: root.gameViewModel ? root.gameViewModel.s1GameTitle : ""
                isValid: root.gameViewModel ? root.gameViewModel.isS1Valid : false
                isProcessing: root.mainController ? root.mainController.isProcessing : false

                onTypeSelected: function(typeName) {
                    if (root.gameViewModel) {
                        root.gameViewModel.setSelectedS1Type(typeName)
                    }
                }
                onBrowseClicked: root.requestBrowseS1()
                onValidateClicked: root.requestValidateS1()
            }

            Label {
                text: "➡"
                font.pixelSize: 40
                Layout.preferredWidth: 40
                Layout.alignment: Qt.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                color: palette.text
            }

            GameSelectorBox {
                id: s2Box
                titleText: qsTr("Counter-Strike 2")
                gameTypesModel: root.gameViewModel ? root.gameViewModel.s2GameTypes : []
                selectedType: root.gameViewModel ? root.gameViewModel.selectedS2Type : ""
                gamePath: root.gameViewModel ? root.gameViewModel.s2GamePath : ""
                gameTitle: root.gameViewModel ? root.gameViewModel.s2GameTitle : ""
                isValid: root.gameViewModel ? root.gameViewModel.isS2Valid : false
                isProcessing: root.mainController ? root.mainController.isProcessing : false

                onTypeSelected: function(typeName) {
                    if (root.gameViewModel) {
                        root.gameViewModel.setSelectedS2Type(typeName)
                    }
                }
                onBrowseClicked: root.requestBrowseS2()
                onValidateClicked: root.requestValidateS2()
            }
        }

        // Row 2: Map File & Addon Name
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 15

            Button {
                id: mapBtn
                text: {
                    if (root.selectedMapPath === "") return qsTr("Select VMF/BSP")
                    let path = root.selectedMapPath
                    return path.substring(Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\')) + 1)
                }
                enabled: !(root.mainController && root.mainController.isProcessing)
                Layout.preferredWidth: 165
                Layout.preferredHeight: 40
                Layout.minimumWidth: 165
                Layout.maximumWidth: 165
                Layout.minimumHeight: 40
                Layout.maximumHeight: 40
                implicitWidth: 165
                implicitHeight: 40
                Layout.fillWidth: false

                contentItem: Text {
                    text: mapBtn.text
                    elide: Text.ElideMiddle
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: parent.palette.buttonText
                }

                onClicked: root.requestBrowseMap()
            }

            Label {
                text: "➡"
                font.pixelSize: 40
                Layout.preferredWidth: 40
                Layout.alignment: Qt.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                color: palette.text
            }

            Item {
                Layout.preferredWidth: 165
                Layout.preferredHeight: 40
                Layout.minimumWidth: 165
                Layout.maximumWidth: 165
                Layout.minimumHeight: 40
                Layout.maximumHeight: 40
                implicitWidth: 165
                implicitHeight: 40
                Layout.fillWidth: false

                ComboBox {
                    id: addonCombo
                    anchors.left: parent.left
                    anchors.right: newToggleColumn.left
                    anchors.rightMargin: 4
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    visible: !createNewCheck.checked
                    model: root.gameViewModel ? root.gameViewModel.s2AddonsList : []
                    currentIndex: Math.max(0, model && root.gameViewModel ? model.indexOf(root.gameViewModel.selectedAddon) : 0)
                    enabled: !(root.mainController && root.mainController.isProcessing)

                    contentItem: Text {
                        text: addonCombo.displayText
                        font: addonCombo.font
                        color: addonCombo.palette.text
                        leftPadding: 6
                        rightPadding: addonCombo.indicator ? addonCombo.indicator.width + 4 : 16
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignLeft
                        elide: Text.ElideRight
                    }

                    onActivated: {
                        if (root.gameViewModel) {
                            root.gameViewModel.setSelectedAddon(currentText)
                        }
                    }
                }

                TextField {
                    id: addonField
                    anchors.left: parent.left
                    anchors.right: newToggleColumn.left
                    anchors.rightMargin: 4
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    visible: createNewCheck.checked
                    placeholderText: qsTr("Addon Name")
                    text: ""
                    font.pixelSize: 12
                    leftPadding: 6
                    rightPadding: 6
                    topPadding: 0
                    bottomPadding: 0
                    horizontalAlignment: TextInput.AlignHCenter
                    verticalAlignment: TextInput.AlignVCenter
                    enabled: !(root.mainController && root.mainController.isProcessing)

                    onTextChanged: {
                        if (createNewCheck.checked && root.gameViewModel) {
                            root.gameViewModel.setSelectedAddon(text)
                        }
                    }
                }

                ColumnLayout {
                    id: newToggleColumn
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 36
                    spacing: 0

                    Label {
                        text: qsTr("NEW")
                        font.pixelSize: 9
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                        color: createNewCheck.checked ? palette.highlight : palette.text
                    }

                    StyledCheckBox {
                        id: createNewCheck
                        checked: root.gameViewModel ? root.gameViewModel.s2AddonsList.length === 0 : false
                        enabled: !(root.mainController && root.mainController.isProcessing)
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        padding: 0
                        topPadding: 0
                        bottomPadding: 0
                        leftPadding: 0
                        rightPadding: 0

                        onCheckedChanged: {
                            if (checked) {
                                addonField.text = ""
                                if (root.gameViewModel) {
                                    root.gameViewModel.setSelectedAddon("")
                                }
                            } else {
                                if (root.gameViewModel && addonCombo.currentText) {
                                    root.gameViewModel.setSelectedAddon(addonCombo.currentText)
                                }
                            }
                        }
                    }
                }
            }
        }

        // Row 3: Import Options
        GroupBox {
            id: optionsBox
            title: qsTr("OPTIONS")
            Layout.fillWidth: true
            Layout.fillHeight: true

            label: Label {
                x: optionsBox.leftPadding
                width: optionsBox.availableWidth
                text: optionsBox.title
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                color: optionsBox.palette.windowText
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 4

                StyledCheckBox {
                    id: cleanFacesCheck
                    text: qsTr("Clean Unnecessary Faces (-usebsp)")
                    checked: true
                    ToolTip.text: qsTr("Runs the map through a special VBSP process to generate clean map geometry from brushes, removing hidden faces and stitching edges for easier editing in Hammer.\n• Preserves world (vis) and func_detail brushes for Source 2 compatibility.\n• Merges all func_instances into world geometry.\n• Note: Final geometry will be triangulated.")
                    ToolTip.visible: hovered
                }

                StyledCheckBox {
                    id: keepInstancesCheck
                    text: qsTr("Preserve func_instance Sub-maps (-usebsp_nomergeinstances)")
                    checked: false
                    enabled: cleanFacesCheck.checked
                    ToolTip.text: qsTr("Generates clean map geometry while preserving func_instance sub-maps as separate entities instead of merging them into world geometry.\n• Takes longer as it runs through the import process twice.\n• Final geometry will be triangulated.\n• Requires Clean Unnecessary Faces (-usebsp) to be enabled.")
                    ToolTip.visible: hovered
                }

                StyledCheckBox {
                    id: keepFuncDetailCheck
                    text: qsTr("Keep func_detail as func_brush")
                    checked: false
                    ToolTip.text: qsTr("Converts Source 1 func_detail brushes into separate func_brush entities instead of baking them into static world geometry.")
                    ToolTip.visible: hovered
                }

                StyledCheckBox {
                    id: skipDepsCheck
                    text: qsTr("Skip Dependencies (Map Geometry Only)")
                    checked: false
                    ToolTip.text: qsTr("Generates only the .vmap map structure, skipping model, material, texture, and sound extraction to accelerate conversion for quick testing.")
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
            Layout.preferredHeight: 36
            Layout.minimumHeight: 36
            Layout.maximumHeight: 36
            spacing: 10

            Button {
                id: startBtn
                text: qsTr("START")
                font.bold: true
                enabled: !(root.mainController && root.mainController.isProcessing) &&
                         (root.gameViewModel && root.gameViewModel.isS1Valid && root.gameViewModel.isS2Valid && root.selectedMapPath !== "")
                Layout.fillWidth: true
                Layout.fillHeight: true

                contentItem: Text {
                    text: parent.text
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.bold: true
                    color: parent.palette.buttonText
                }

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

                contentItem: Text {
                    text: parent.text
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.bold: true
                    color: parent.palette.buttonText
                }

                onClicked: {
                    if (root.mainController) {
                        root.mainController.stopImport()
                    }
                }
            }
        }
    }
}

