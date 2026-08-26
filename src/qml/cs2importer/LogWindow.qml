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

                delegate: Loader {
                    id: rootTaskLoader
                    width: taskListView.width - (vScrollBar.visible ? vScrollBar.width + 4 : 0)
                    sourceComponent: taskCardComponent
                    property var taskModelData: model
                }

                onCountChanged: {
                    if (root.logViewModel && root.logViewModel.autoScroll) {
                        Qt.callLater(taskListView.positionViewAtEnd)
                    }
                }
            }
        }
    }

    // Recursive Task Card Component
    Component {
        id: taskCardComponent

        Rectangle {
            id: taskCard
            width: parent ? parent.width : 0
            color: (model.depth || 0) > 0 ? "#1C1C1C" : "#212121"
            border.color: model.expanded ? ((model.depth || 0) > 0 ? "#383838" : "#424242") : "#282828"
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
                        anchors.leftMargin: 6 + (model.depth || 0) * 12
                        anchors.rightMargin: 6
                        spacing: 8

                        // Expand / Collapse Chevron
                        Text {
                            text: model.expanded ? "▼" : "▶"
                            color: (model.depth || 0) > 0 ? "#90A4AE" : "#B0BEC5"
                            font.pixelSize: 11
                            Layout.alignment: Qt.AlignVCenter
                        }

                        // Task Name
                        Text {
                            text: model.taskName
                            color: "#ECEFF1"
                            font.bold: (model.depth || 0) === 0
                            font.pixelSize: (model.depth || 0) === 0 ? 13 : 12
                            Layout.alignment: Qt.AlignVCenter
                        }

                        // State Badge
                        Rectangle {
                            implicitWidth: stateText.implicitWidth + 10
                            implicitHeight: 18
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
                                font.pixelSize: 9
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

                        // Subtasks tag
                        Text {
                            visible: model.hasSubTasks
                            text: qsTr("%1 sub-task(s)").arg(model.subTasksCount)
                            color: "#81D4FA"
                            font.pixelSize: 11
                            Layout.alignment: Qt.AlignVCenter
                        }

                        // Message Count Tag
                        Text {
                            visible: model.messageCount > 0
                            text: qsTr("%1 log(s)").arg(model.messageCount)
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
                            // Toggle expanded state on this task's model
                            if (model.expanded !== undefined) {
                                model.expanded = !model.expanded
                            }
                        }
                    }
                }

                // Child Sub-Tasks Area (Visible when expanded)
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: model.expanded && model.hasSubTasks
                    spacing: 6

                    Repeater {
                        model: model.subTasksModel
                        delegate: Loader {
                            Layout.fillWidth: true
                            sourceComponent: taskCardComponent
                        }
                    }
                }

                // Message Output Area (Visible when expanded and has messages)
                Rectangle {
                    id: messageArea
                    Layout.fillWidth: true
                    visible: model.expanded && (model.messageCount > 0)
                    implicitHeight: visible ? Math.min(280, Math.max(30, messageListView.contentHeight + 12)) : 0
                    color: "#141414"
                    radius: 3
                    border.color: "#2C2C2C"
                    border.width: 1

                    ListView {
                        id: messageListView
                        anchors.fill: parent
                        anchors.margins: 6
                        clip: true
                        spacing: 3
                        model: model.messagesModel
                        boundsBehavior: Flickable.StopAtBounds

                        ScrollBar.vertical: ScrollBar {
                            active: messageListView.contentHeight > messageListView.height
                        }

                        delegate: RowLayout {
                            width: messageListView.width - 12
                            spacing: 6

                            Text {
                                text: "[" + (model.timestampString || "00:00:00") + "]"
                                color: "#757575"
                                font.family: "Consolas, 'Courier New', monospace"
                                font.pixelSize: 11
                                Layout.alignment: Qt.AlignTop
                            }

                            Rectangle {
                                implicitWidth: lvlText.implicitWidth + 6
                                implicitHeight: 16
                                radius: 2
                                Layout.alignment: Qt.AlignTop

                                color: {
                                    var lvl = (model.levelString || "").toUpperCase()
                                    if (lvl === "ERROR" || lvl === "CRITICAL") return "#C62828"
                                    if (lvl === "WARNING") return "#EF6C00"
                                    if (lvl === "DEBUG") return "#424242"
                                    return "#2E7D32"
                                }

                                Text {
                                    id: lvlText
                                    anchors.centerIn: parent
                                    text: {
                                        var str = (model.levelString || "INFO").toUpperCase()
                                        if (str === "WARNING") return "WARN"
                                        if (str === "CRITICAL") return "CRIT"
                                        return str
                                    }
                                    color: "#FFFFFF"
                                    font.bold: true
                                    font.pixelSize: 9
                                }
                            }

                            TextEdit {
                                Layout.fillWidth: true
                                readOnly: true
                                selectByMouse: true
                                wrapMode: TextEdit.Wrap
                                text: model.message || ""
                                font.family: "Consolas, 'Courier New', monospace"
                                font.pixelSize: 12
                                color: {
                                    var lvl = (model.levelString || "").toUpperCase()
                                    if (lvl === "ERROR" || lvl === "CRITICAL") return "#FF5252"
                                    if (lvl === "WARNING") return "#FFD740"
                                    if (lvl === "DEBUG") return "#9E9E9E"
                                    return "#ECEFF1"
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
