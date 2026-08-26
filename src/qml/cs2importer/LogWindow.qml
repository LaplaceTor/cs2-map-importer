import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

ApplicationWindow {
    id: root

    property QtObject logViewModel: null

    width: 880
    height: 580
    minimumWidth: 540
    minimumHeight: 340
    title: qsTr("CS2 IMPORTER - Logs")
    color: palette.window
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint | Qt.WindowCloseButtonHint
    transientParent: null

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

            Button {
                text: qsTr("Expand All")
                onClicked: {
                    if (root.logViewModel) {
                        root.logViewModel.expandAll()
                    }
                }
            }

            Button {
                text: qsTr("Collapse All")
                onClicked: {
                    if (root.logViewModel) {
                        root.logViewModel.collapseAll()
                    }
                }
            }

            StyledCheckBox {
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
                text: qsTr("Tasks: %1 | Messages: %2")
                    .arg(root.logViewModel ? root.logViewModel.taskCount : 0)
                    .arg(root.logViewModel ? root.logViewModel.totalMessageCount : 0)
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
            clip: true

            ListView {
                id: taskListView
                anchors.fill: parent
                anchors.margins: 6
                clip: true
                spacing: 8
                model: root.logViewModel
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {
                    id: vScrollBar
                    active: true
                }

                delegate: LogTaskCard {
                    width: taskListView.width - (vScrollBar.visible ? vScrollBar.width + 4 : 0)
                    owningModel: root.logViewModel
                }

                onCountChanged: {
                    if (root.logViewModel && root.logViewModel.autoScroll) {
                        Qt.callLater(taskListView.positionViewAtEnd)
                    }
                }
            }
        }
    }
}
