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
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            enabled: !root.isProcessing && root.gameTypesModel.length > 1
            model: root.gameTypesModel
            currentIndex: Math.max(0, model ? model.indexOf(root.selectedType) : 0)

            onActivated: {
                root.typeSelected(currentText)
            }
        }

        Button {
            id: folderButton
            Layout.fillWidth: true
            Layout.fillHeight: true
            enabled: !root.isProcessing

            contentItem: ColumnLayout {
                spacing: 2
                anchors.centerIn: parent

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: {
                        if (root.gamePath === "") {
                            return root.isCustomGame ? qsTr("Press to Select gameinfo.txt") : qsTr("Press to Select Game Folder")
                        }
                        return root.gamePath
                    }
                    elide: Text.ElideMiddle
                    Layout.maximumWidth: folderButton.width - 20
                    color: root.isValid ? (folderButton.palette.text) : (root.gamePath === "" ? folderButton.palette.placeholderText : "#E57373")
                    font.pixelSize: 12
                    font.bold: root.gamePath !== ""
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    visible: root.isValid && root.gameTitle !== ""
                    text: root.gameTitle
                    elide: Text.ElideRight
                    Layout.maximumWidth: folderButton.width - 20
                    font.pixelSize: 11
                    color: "#81C784"
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            onClicked: {
                root.browseClicked()
            }
        }

        Button {
            id: validateButton
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            enabled: !root.isProcessing && !root.isCustomGame
            text: qsTr("Validate Game Files (Steam)")

            onClicked: {
                root.validateClicked()
            }
        }
    }
}

