import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GroupBox {
    id: root

    property string titleText: qsTr("Game")
    property var gameTypesModel: []
    property string selectedType: ""
    property string gamePath: ""
    property string gameTitle: ""
    property bool isValid: false
    property bool isCustomGame: selectedType.toLowerCase() === "custom" || selectedType.toLowerCase() === "other"
    property bool isProcessing: false

    signal typeSelected(string typeName)
    signal browseClicked()
    signal validateClicked()

    title: titleText
    label: Label {
        x: root.leftPadding
        width: root.availableWidth
        text: root.titleText
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        color: root.palette.windowText
    }
    implicitWidth: 165
    implicitHeight: 165
    Layout.preferredWidth: 165
    Layout.preferredHeight: 165
    Layout.minimumWidth: 165
    Layout.maximumWidth: 165
    Layout.minimumHeight: 165
    Layout.maximumHeight: 165
    Layout.fillWidth: false
    Layout.fillHeight: false

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        ComboBox {
            id: typeCombo
            visible: root.gameTypesModel && root.gameTypesModel.length > 1
            model: root.gameTypesModel
            currentIndex: Math.max(0, model ? model.indexOf(root.selectedType) : 0)
            enabled: !root.isProcessing && root.gameTypesModel.length > 1
            Layout.fillWidth: true
            Layout.preferredHeight: 28

            contentItem: Text {
                text: typeCombo.displayText
                font: typeCombo.font
                color: typeCombo.palette.text
                leftPadding: 10
                rightPadding: typeCombo.indicator ? typeCombo.indicator.width + 10 : 20
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignLeft
                elide: Text.ElideLeft
            }

            onActivated: {
                root.typeSelected(currentText)
            }
        }

        Button {
            id: folderButton
            enabled: !root.isProcessing
            Layout.fillWidth: true
            Layout.fillHeight: true

            background: Rectangle {
                radius: 4
                color: {
                    if (root.isValid) {
                        return folderButton.down ? "#81C784" : (folderButton.hovered ? "#A5D6A7" : "#C8E6C9")
                    }
                    if (folderButton.down) return folderButton.palette.dark
                    if (folderButton.hovered) return folderButton.palette.midlight
                    return folderButton.palette.button
                }
                border.color: root.isValid ? "#4CAF50" : folderButton.palette.mid
                border.width: root.isValid ? 2 : 1
            }

            contentItem: Text {
                text: {
                    if (root.gamePath === "") {
                        return root.isCustomGame ? qsTr("Press to Select gameinfo.txt") : qsTr("Press to Select Game Folder")
                    }
                    return root.gamePath
                }
                font.pixelSize: 11
                font.bold: root.gamePath !== ""
                color: root.isValid ? "#1B5E20" : (root.gamePath === "" ? folderButton.palette.placeholderText : folderButton.palette.text)
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WrapAnywhere
                elide: Text.ElideMiddle
                padding: 6
            }

            onClicked: {
                root.browseClicked()
            }
        }

        Button {
            id: validateButton
            text: qsTr("Validate Game File")
            enabled: !root.isProcessing && !root.isCustomGame
            Layout.fillWidth: true
            Layout.preferredHeight: 26

            contentItem: Text {
                text: validateButton.text
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: validateButton.palette.buttonText
                elide: Text.ElideRight
            }

            onClicked: {
                root.validateClicked()
            }
        }
    }
}

