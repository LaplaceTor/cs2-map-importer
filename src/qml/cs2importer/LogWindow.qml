import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

ApplicationWindow {
    id: root

    property QtObject logViewModel: null

    width: 850
    height: 560
    minimumWidth: 520
    minimumHeight: 320
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

                delegate: Rectangle {
                    id: taskCard
                    width: taskListView.width - (vScrollBar.visible ? vScrollBar.width + 4 : 0)
                    color: "#212121"
                    border.color: model.expanded ? "#424242" : "#2E2E2E"
                    border.width: 1
                    radius: 4

                    implicitHeight: cardContent.implicitHeight + 12

                    ColumnLayout {
                        id: cardContent
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 6

                        // Task Header
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 28
                            color: headerMouseArea.containsMouse ? "#2A2A2A" : "transparent"
                            radius: 3

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                anchors.rightMargin: 6
                                spacing: 8

                                // Expand / Collapse Chevron
                                Text {
                                    text: model.expanded ? "▼" : "▶"
                                    color: "#B0BEC5"
                                    font.pixelSize: 11
                                    Layout.alignment: Qt.AlignVCenter
                                }

                                // Task Name
                                Text {
                                    text: model.taskName
                                    color: "#ECEFF1"
                                    font.bold: true
                                    font.pixelSize: 13
                                    Layout.alignment: Qt.AlignVCenter
                                }

                                // State Badge
                                Rectangle {
                                    implicitWidth: stateText.implicitWidth + 10
                                    implicitHeight: 20
                                    radius: 3
                                    Layout.alignment: Qt.AlignVCenter

                                    color: {
                                        var s = (model.stateString || "").toUpperCase()
                                        if (s === "RUNNING") return "#1565C0"
                                        if (s === "COMPLETED") return "#2E7D32"
                                        if (s === "FAILED") return "#C62828"
                                        if (s === "CANCELLED") return "#E65100"
                                        return "#424242"
                                    }

                                    Text {
                                        id: stateText
                                        anchors.centerIn: parent
                                        text: model.stateString
                                        color: "#FFFFFF"
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                }

                                // Progress tag if running with progress
                                Text {
                                    visible: (model.stateString || "").toUpperCase() === "RUNNING" && model.progress > 0
                                    text: Math.round(model.progress * 100) + "%"
                                    color: "#90CAF9"
                                    font.pixelSize: 11
                                    Layout.alignment: Qt.AlignVCenter
                                }

                                Item {
                                    Layout.fillWidth: true
                                }

                                // Message Count Tag
                                Text {
                                    text: qsTr("%1 message(s)").arg(model.messageCount)
                                    color: "#78909C"
                                    font.pixelSize: 11
                                    Layout.alignment: Qt.AlignVCenter
                                }
                            }

                            MouseArea {
                                id: headerMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (root.logViewModel) {
                                        root.logViewModel.toggleTaskExpanded(index)
                                    }
                                }
                            }
                        }

                        // Message Output Area (visible when expanded)
                        Rectangle {
                            id: messageArea
                            Layout.fillWidth: true
                            visible: model.expanded
                            implicitHeight: visible ? Math.max(20, logOutput.implicitHeight + 8) : 0
                            color: "#141414"
                            radius: 3
                            border.color: "#2C2C2C"
                            border.width: 1

                            TextArea {
                                id: logOutput
                                anchors.fill: parent
                                anchors.margins: 4
                                readOnly: true
                                selectByMouse: true
                                wrapMode: TextEdit.Wrap
                                textFormat: Text.RichText
                                font.family: "Consolas, 'Courier New', monospace"
                                font.pixelSize: 12
                                color: "#ECEFF1"
                                text: model.formattedMessages || ""
                                background: null
                            }
                        }
                    }
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
