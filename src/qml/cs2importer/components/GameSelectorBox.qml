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
    property bool isCustomGame: false
    property bool isProcessing: false

    signal typeSelected(string typeName)
    signal browseClicked()
    signal validateClicked()

    title: titleText
    Layout.fillWidth: true
    Layout.preferredHeight: 190

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        ComboBox {
            id: typeCombo
            model: root.gameTypesModel
            currentIndex: Math.max(0, model ? model.indexOf(root.selectedType) : 0)
            enabled: !root.isProcessing && root.gameTypesModel.length > 1
            Layout.fillWidth: true
            Layout.preferredHeight: 32

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

            contentItem: ColumnLayout {
                spacing: 2
                anchors.centerIn: parent
                width: Math.max(0, folderButton.width - 20)

                Text {
                    text: {
                        if (root.gamePath === "") {
                            return root.isCustomGame ? qsTr("Press to Select gameinfo.txt") : qsTr("Press to Select Game Folder")
                        }
                        return root.gamePath
                    }
                    font.pixelSize: 12
                    font.bold: root.gamePath !== ""
                    color: root.isValid ? (folderButton.palette.text) : (root.gamePath === "" ? folderButton.palette.placeholderText : "#E57373")
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideLeft
                    Layout.fillWidth: true
                }

                Text {
                    visible: root.isValid && root.gameTitle !== ""
                    text: root.gameTitle
                    font.pixelSize: 11
                    color: "#81C784"
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideLeft
                    Layout.fillWidth: true
                }
            }

            onClicked: {
                root.browseClicked()
            }
        }

        Button {
            id: validateButton
            text: qsTr("Validate Game Files (Steam)")
            enabled: !root.isProcessing && !root.isCustomGame
            Layout.fillWidth: true
            Layout.preferredHeight: 30

            contentItem: Text {
                text: validateButton.text
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: validateButton.palette.buttonText
                elide: Text.ElideLeft
            }

            onClicked: {
                root.validateClicked()
            }
        }
    }
}

