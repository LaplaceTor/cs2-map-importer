import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: rootCard

    required property var owningModel
    required property int index
    required property int depth
    required property string taskName
    required property string stateString
    required property double progress
    required property bool expanded
    required property int messageCount
    required property int subTasksCount
    required property bool hasSubTasks
    required property var messagesModel
    required property var subTasksModel

    width: parent ? parent.width : 0
    color: depth > 0 ? "#1C1C1C" : "#212121"
    border.color: expanded ? (depth > 0 ? "#383838" : "#424242") : "#282828"
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
                anchors.leftMargin: 6 + rootCard.depth * 12
                anchors.rightMargin: 6
                spacing: 8

                // Expand / Collapse Chevron
                Text {
                    text: rootCard.expanded ? "▼" : "▶"
                    color: rootCard.depth > 0 ? "#90A4AE" : "#B0BEC5"
                    font.pixelSize: 11
                    Layout.alignment: Qt.AlignVCenter
                }

                // Task Name
                Text {
                    text: rootCard.taskName
                    color: "#ECEFF1"
                    font.bold: rootCard.depth === 0
                    font.pixelSize: rootCard.depth === 0 ? 13 : 12
                    Layout.alignment: Qt.AlignVCenter
                }

                // State Badge
                Rectangle {
                    implicitWidth: stateText.implicitWidth + 10
                    implicitHeight: 18
                    radius: 3
                    Layout.alignment: Qt.AlignVCenter

                    color: {
                        var s = (rootCard.stateString || "").toUpperCase()
                        if (s === "RUNNING") return "#1565C0"
                        if (s === "COMPLETED") return "#2E7D32"
                        if (s === "FAILED") return "#C62828"
                        if (s === "CANCELLED") return "#E65100"
                        if (s === "SKIPPED") return "#546E7A"
                        return "#424242"
                    }

                    Text {
                        id: stateText
                        anchors.centerIn: parent
                        text: rootCard.stateString
                        color: "#FFFFFF"
                        font.pixelSize: 9
                        font.bold: true
                    }
                }

                // Progress tag if running with progress
                Text {
                    visible: (rootCard.stateString || "").toUpperCase() === "RUNNING" && rootCard.progress > 0
                    text: Math.round(rootCard.progress * 100) + "%"
                    color: "#90CAF9"
                    font.pixelSize: 11
                    Layout.alignment: Qt.AlignVCenter
                }

                Item {
                    Layout.fillWidth: true
                }

                // Subtasks tag
                Text {
                    visible: rootCard.hasSubTasks
                    text: qsTr("%1 sub-task(s)").arg(rootCard.subTasksCount)
                    color: "#81D4FA"
                    font.pixelSize: 11
                    Layout.alignment: Qt.AlignVCenter
                }

                // Message Count Tag
                Text {
                    visible: rootCard.messageCount > 0
                    text: qsTr("%1 log(s)").arg(rootCard.messageCount)
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
                    if (rootCard.owningModel && typeof rootCard.owningModel.toggleTaskExpanded === "function") {
                        rootCard.owningModel.toggleTaskExpanded(rootCard.index)
                    }
                }
            }
        }

        // Child Sub-Tasks Area (Visible when expanded)
        ColumnLayout {
            Layout.fillWidth: true
            visible: rootCard.expanded && rootCard.hasSubTasks
            spacing: 6

            Repeater {
                model: rootCard.subTasksModel
                delegate: LogTaskCard {
                    Layout.fillWidth: true
                    owningModel: rootCard.subTasksModel
                }
            }
        }

        // Message Output Area (Visible when expanded and has messages)
        Rectangle {
            id: messageArea
            Layout.fillWidth: true
            visible: rootCard.expanded && (rootCard.messageCount > 0)
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
                model: rootCard.messagesModel
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {
                    active: messageListView.contentHeight > messageListView.height
                }

                delegate: RowLayout {
                    width: messageListView.width - 12
                    spacing: 6

                    required property string timestampString
                    required property string levelString
                    required property string message

                    Text {
                        text: "[" + (timestampString || "00:00:00") + "]"
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
                            var lvl = (levelString || "").toUpperCase()
                            if (lvl === "ERROR" || lvl === "CRITICAL") return "#C62828"
                            if (lvl === "WARNING") return "#EF6C00"
                            if (lvl === "DEBUG") return "#424242"
                            return "#2E7D32"
                        }

                        Text {
                            id: lvlText
                            anchors.centerIn: parent
                            text: {
                                var str = (levelString || "INFO").toUpperCase()
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
                        text: message || ""
                        font.family: "Consolas, 'Courier New', monospace"
                        font.pixelSize: 12
                        color: {
                            var lvl = (levelString || "").toUpperCase()
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

