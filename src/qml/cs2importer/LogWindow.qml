import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: root

    property QtObject logViewModel: null

    width: 800
    height: 520
    minimumWidth: 500
    minimumHeight: 300
    title: qsTr("Logs — CS2 Importer")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // Toolbar
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: qsTr("Clear")
                onClicked: {
                    if (root.logViewModel) {
                        root.logViewModel.clear()
                    }
                }
            }

            Button {
                text: qsTr("Copy All")
                onClicked: {
                    if (root.logViewModel) {
                        root.logViewModel.copyToClipboard()
                    }
                }
            }

            CheckBox {
                text: qsTr("Auto-scroll")
                checked: root.logViewModel ? root.logViewModel.autoScroll : true
                onCheckedChanged: {
                    if (root.logViewModel) {
                        root.logViewModel.autoScroll = checked
                    }
                }
            }

            Item {
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Lines: %1").arg(root.logViewModel ? root.logViewModel.lineCount : 0)
                font.pixelSize: 12
                color: palette.placeholderText
            }
        }

        // Log Console Area
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#181818"
            border.color: "#333333"
            border.width: 1
            radius: 4

            ScrollView {
                id: logScrollView
                anchors.fill: parent
                anchors.margins: 6
                clip: true

                TextArea {
                    id: logOutput
                    width: logScrollView.availableWidth
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    textFormat: Text.RichText
                    font.family: "Consolas, 'Courier New', monospace"
                    font.pixelSize: 12
                    color: "#ECEFF1"
                    text: root.logViewModel ? root.logViewModel.formattedLogText : ""
                    background: null

                    onTextChanged: {
                        if (root.logViewModel && root.logViewModel.autoScroll && logScrollView.ScrollBar.vertical) {
                            logScrollView.ScrollBar.vertical.position = 1.0 - logScrollView.ScrollBar.vertical.size
                        }
                    }
                }
            }
        }
    }
}

