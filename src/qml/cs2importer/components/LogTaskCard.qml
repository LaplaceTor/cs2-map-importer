import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: rootCard

    property var owningModel: null
    property int cardIndex: (typeof index !== "undefined") ? index : 0
    property int cardDepth: (typeof model !== "undefined" && model && model.depth !== undefined) ? model.depth : 0
    property string taskName: (typeof model !== "undefined" && model && model.taskName !== undefined) ? model.taskName : ""
    property string stateString: (typeof model !== "undefined" && model && model.stateString !== undefined) ? model.stateString : ""
    property double progress: (typeof model !== "undefined" && model && model.progress !== undefined) ? model.progress : 0.0
    property bool expanded: (typeof model !== "undefined" && model && model.expanded !== undefined) ? model.expanded : true
    property int messageCount: (typeof model !== "undefined" && model && model.messageCount !== undefined) ? model.messageCount : 0
    property int subTasksCount: (typeof model !== "undefined" && model && model.subTasksCount !== undefined) ? model.subTasksCount : 0
    property bool hasSubTasks: (typeof model !== "undefined" && model && model.hasSubTasks !== undefined) ? model.hasSubTasks : false
    property var messagesModel: (typeof model !== "undefined" && model && model.messagesModel !== undefined) ? model.messagesModel : null
    property var subTasksModel: (typeof model !== "undefined" && model && model.subTasksModel !== undefined) ? model.subTasksModel : null
        id: cardContent
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        // Task Header
        Rectangle {
            Layout.fillWidth: true
            color: headerMouseArea.containsMouse ? "#2A2A2A" : "transparent"
            radius: 3

            RowLayout {
                spacing: 8

                // Expand / Collapse Chevron
                Text {
                    text: rootCard.expanded ? "▼" : "▶"
                    color: rootCard.cardDepth > 0 ? "#90A4AE" : "#B0BEC5"
                    font.pixelSize: 11
                    Layout.alignment: Qt.AlignVCenter
                }

                // Task Name
                Text {
                    text: rootCard.taskName
                    font.bold: rootCard.cardDepth === 0
                    font.pixelSize: rootCard.cardDepth === 0 ? 13 : 12
                    Layout.alignment: Qt.AlignVCenter
                }

                // State Badge
                Rectangle {
                    implicitHeight: 18
                    radius: 3
                    Layout.alignment: Qt.AlignVCenter

                    color: {
                        var s = (rootCard.stateString || "").toUpperCase()
                        if (s === "RUNNING") return "#1565C0"
                        if (s === "COMPLETED") return "#2E7D32"
                        if (s === "FAILED") return "#C62828"
                        if (s === "CANCELLED") return "#E65100"
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
                        rootCard.owningModel.toggleTaskExpanded(rootCard.cardIndex)
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
                delegate: Loader {
                    id: subTaskLoader
                    Layout.fillWidth: true
                    source: "LogTaskCard.qml"

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

                    Binding {
                        target: subTaskLoader.item
                        property: "owningModel"
                        value: rootCard.subTasksModel
                    }
                    Binding {
                        target: subTaskLoader.item
                        property: "index"
                        value: subTaskLoader.index
                    }
                    Binding {
                        target: subTaskLoader.item
                        property: "depth"
                        value: subTaskLoader.depth
                    }
                    Binding {
                        target: subTaskLoader.item
                        property: "taskName"
                        value: subTaskLoader.taskName
                    }
                    Binding {
                        target: subTaskLoader.item
                        property: "stateString"
                        value: subTaskLoader.stateString
                    }
                    Binding {
                        target: subTaskLoader.item
                        property: "progress"
                        value: subTaskLoader.progress
                    }
                    Binding {
                        target: subTaskLoader.item
                        property: "expanded"
                        value: subTaskLoader.expanded
                    }
                    Binding {
                        target: subTaskLoader.item
                        property: "messageCount"
                        value: subTaskLoader.messageCount
                    }
                    Binding {
                        target: subTaskLoader.item
                        property: "subTasksCount"
                        value: subTaskLoader.subTasksCount
                    }
                    Binding {
                        target: subTaskLoader.item
                        property: "hasSubTasks"
                        value: subTaskLoader.hasSubTasks
                    }
                    Binding {
                        target: subTaskLoader.item
                        property: "messagesModel"
                        value: subTaskLoader.messagesModel
                    }
                    Binding {
                        target: subTaskLoader.item
                        property: "subTasksModel"
                        value: subTaskLoader.subTasksModel
                    }
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

