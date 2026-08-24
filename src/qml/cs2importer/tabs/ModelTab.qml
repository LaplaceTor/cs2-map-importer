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

        // Row 2: Model File & Target Addon
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 15

            Button {
                id: mdlBtn
                text: {
                    if (root.selectedMdlPath === "") return qsTr("SELECT MDL")
                    let path = root.selectedMdlPath
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
                    text: mdlBtn.text
                    elide: Text.ElideMiddle
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: parent.palette.buttonText
                }

                onClicked: root.requestBrowseMdl()
            }

            Label {
                text: "➡"
                font.pixelSize: 40
                Layout.preferredWidth: 40
                Layout.alignment: Qt.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                color: palette.text
            }

            ComboBox {
                id: addonCombo
                model: root.gameViewModel ? root.gameViewModel.s2AddonsList : []
                currentIndex: Math.max(0, model && root.gameViewModel ? model.indexOf(root.gameViewModel.selectedAddon) : 0)
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
                    text: qsTr("Skip Animation Import (-skipcommondmxwrite)")
                    checked: false
                    ToolTip.text: qsTr("Converts only the static 3D mesh model without extracting skeletal animations (.dmx files), significantly accelerating conversion.")
                    ToolTip.visible: hovered
                }

                StyledCheckBox {
                    text: qsTr("Convert Coordinate (Y-Up to Z-Up) (-YupToZup)")
                    checked: false
                    ToolTip.text: qsTr("Transforms the model's base pose from Y-axis Up (Source 1/Maya) to Z-axis Up (Source 2 standard) to fix lying-down or rotated models.")
                    ToolTip.visible: hovered
                }

                StyledCheckBox {
                    text: qsTr("Override \"lean\" Sequence (-overridelean)")
                    checked: false
                    ToolTip.text: qsTr("Overrides directional leaning animation sequences for characters or weapons with standard default poses.")
                    ToolTip.visible: hovered
                }

                StyledCheckBox {
                    text: qsTr("Use Studiohdr Bounds (-header_hull_bounds)")
                    checked: false
                    ToolTip.text: qsTr("Uses the bounding box dimensions defined in the MDL studio header directly, rather than calculating boundaries from collision physics hulls.")
                    ToolTip.visible: hovered
                }

                StyledCheckBox {
                    text: qsTr("Import All LODs (-lods)")
                    checked: false
                    ToolTip.text: qsTr("Imports all distance-based Level-of-Detail meshes (LOD 0, 1, 2...). When unchecked, only the highest detail LOD 0 is imported.")
                    ToolTip.visible: hovered
                }

                StyledCheckBox {
                    text: qsTr("Export Weapon Anim Prefab (-write_weapon_anim_prefab)")
                    checked: false
                    ToolTip.text: qsTr("Writes weapon animation sequences and bone weightlists into a reusable prefab file, prefixing each entry with the weapon filename.")
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
                         (root.gameViewModel && root.gameViewModel.isS1Valid && root.gameViewModel.isS2Valid && root.selectedMdlPath !== "")
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

