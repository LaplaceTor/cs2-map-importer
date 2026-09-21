import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: root

    property QtObject gameViewModel: null
    property QtObject mainController: null
    property var selectedFiles: []
    property string selectedPcfPath: selectedFiles.length > 0 ? selectedFiles[0] : ""

    signal requestBrowseS1()
    signal requestBrowseS2()
    signal requestBrowsePcf()
    signal requestValidateS1()
    signal requestValidateS2()

    function addFiles(newFiles) {
        sourceFilesBox.addFiles(newFiles)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        // Row 1: Game Selectors (Source 1 <-> Source 2)
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            GameSelectorBox {
                id: s1Box
                titleText: qsTr("Source 1 Game")
                gameTypesModel: root.gameViewModel ? root.gameViewModel.s1GameTypes : []
                selectedType: root.gameViewModel ? root.gameViewModel.selectedS1Type : ""
                gamePath: root.gameViewModel ? root.gameViewModel.s1GamePath : ""
                gameTitle: root.gameViewModel ? root.gameViewModel.s1GameTitle : ""
                isValid: root.gameViewModel ? root.gameViewModel.isS1Valid : false
                isProcessing: (root.mainController ? root.mainController.isProcessing : false) || (root.gameViewModel ? root.gameViewModel.isDetecting : false)
                Layout.fillWidth: true
                Layout.preferredWidth: 1

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
                font.pixelSize: 32
                Layout.preferredWidth: 32
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
                isProcessing: (root.mainController ? root.mainController.isProcessing : false) || (root.gameViewModel ? root.gameViewModel.isDetecting : false)
                Layout.fillWidth: true
                Layout.preferredWidth: 1

                onTypeSelected: function(typeName) {
                    if (root.gameViewModel) {
                        root.gameViewModel.setSelectedS2Type(typeName)
                    }
                }
                onBrowseClicked: root.requestBrowseS2()
                onValidateClicked: root.requestValidateS2()
            }
        }

        // Row 2: Addon Name row (full width, left-aligned)
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            spacing: 8

            Label {
                text: qsTr("ADDON NAME:")
                font.bold: true
                color: palette.windowText
                Layout.alignment: Qt.AlignVCenter
            }

            ComboBox {
                id: addonCombo
                objectName: "addonCombo"
                visible: !createNewCheck.checked
                model: root.gameViewModel ? root.gameViewModel.s2AddonsList : []
                currentIndex: Math.max(0, model && root.gameViewModel ? model.indexOf(root.gameViewModel.selectedAddon) : 0)
                enabled: !(root.mainController && root.mainController.isProcessing)
                Layout.fillWidth: true
                Layout.preferredHeight: 28

                contentItem: Text {
                    text: addonCombo.displayText
                    font: addonCombo.font
                    color: addonCombo.palette.text
                    leftPadding: 8
                    rightPadding: addonCombo.indicator ? addonCombo.indicator.width + 8 : 20
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
                visible: createNewCheck.checked
                placeholderText: qsTr("Addon Name")
                text: ""
                font.pixelSize: 12
                leftPadding: 8
                rightPadding: 8
                horizontalAlignment: TextInput.AlignLeft
                verticalAlignment: TextInput.AlignVCenter
                enabled: !(root.mainController && root.mainController.isProcessing)
                Layout.fillWidth: true
                Layout.preferredHeight: 28

                onTextChanged: {
                    if (createNewCheck.checked && root.gameViewModel) {
                        root.gameViewModel.setSelectedAddon(text)
                    }
                }
            }

            StyledCheckBox {
                id: createNewCheck
                text: qsTr("NEW")
                checked: root.gameViewModel ? root.gameViewModel.s2AddonsList.length === 0 : false
                enabled: !(root.mainController && root.mainController.isProcessing)
                Layout.alignment: Qt.AlignVCenter

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

        // Row 3: Files & Options (side-by-side)
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            SourceFileListBox {
                id: sourceFilesBox
                titleText: qsTr("PARTICLE FILES")
                files: root.selectedFiles
                allowedExtensions: [".pcf"]
                enabledState: !(root.mainController && root.mainController.isProcessing)
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1

                onAddRequested: root.requestBrowsePcf()
                onFilesChanged: root.selectedFiles = files
                onFileRemoved: function(idx) { root.selectedFiles = files }
                onClearRequested: root.selectedFiles = []
            }

            GroupBox {
                id: optionsBox
                title: qsTr("OPTIONS")
                enabled: !(root.mainController && root.mainController.isProcessing)
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1

                label: Label {
                    x: optionsBox.leftPadding
                    width: optionsBox.availableWidth
                    text: optionsBox.title
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    color: optionsBox.palette.windowText
                }

                ScrollView {
                    id: optionsScroll
                    anchors.fill: parent
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    Column {
                        width: optionsScroll.availableWidth
                        spacing: 4

                        StyledCheckBox {
                            id: allowDepthBlendCheck
                            objectName: "allowDepthBlendCheck"
                            width: parent.width
                            text: qsTr("Allow Depth Blend (-particle_allow_depth_blend)")
                            checked: false
                            ToolTip.text: qsTr("Respects $DEPTHBLEND in particle materials to smoothly feather and blend smoke, fire, and fog edges with surrounding world geometry.")
                            ToolTip.visible: hovered
                        }

                        StyledCheckBox {
                            id: disableDiffuseCheck
                            objectName: "disableDiffuseCheck"
                            width: parent.width
                            text: qsTr("Disable Diffuse Lighting (-particle_disable_diffuse)")
                            checked: false
                            ToolTip.text: qsTr("Prevents scene lighting from tinting or darkening particle sprites, preserving their intended self-luminous or vivid colors.")
                            ToolTip.visible: hovered
                        }
                    }
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
                objectName: "startBtn"
                text: qsTr("START")
                font.bold: true
                enabled: !(root.mainController && root.mainController.isProcessing) &&
                         (root.gameViewModel && root.gameViewModel.isS1Valid && root.gameViewModel.isS2Valid && root.selectedFiles.length > 0)
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
                        root.mainController.startParticleImport(
                            root.gameViewModel ? root.gameViewModel.s1GamePath : "",
                            root.gameViewModel ? root.gameViewModel.s2GamePath : "",
                            root.gameViewModel ? root.gameViewModel.selectedAddon : "",
                            root.selectedFiles,
                            allowDepthBlendCheck.checked,
                            disableDiffuseCheck.checked,
                            root.gameViewModel ? root.gameViewModel.selectedS1Type : "",
                            root.gameViewModel ? root.gameViewModel.s1GameInfoDir : ""
                        )
                    }
                }
            }

            Button {
                id: stopBtn
                objectName: "stopBtn"
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
