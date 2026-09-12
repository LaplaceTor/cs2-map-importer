import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: rootCard

    property var owningModel: null
    property var taskId: (typeof model !== "undefined" && model && model.taskId !== undefined) ? model.taskId : 0
    property int cardIndex: (typeof index !== "undefined") ? index : 0
    property int cardDepth: (typeof model !== "undefined" && model && model.depth !== undefined) ? model.depth : 0
    property string taskName: (typeof model !== "undefined" && model && model.taskName !== undefined) ? model.taskName : ""
    property string stateString: (typeof model !== "undefined" && model && model.stateString !== undefined) ? model.stateString : ""
    property double progress: (typeof model !== "undefined" && model && model.progress !== undefined) ? model.progress : 0.0
    property bool expanded: (typeof model !== "undefined" && model && model.expanded !== undefined) ? model.expanded : true
    property int messageCount: (typeof model !== "undefined" && model && model.messageCount !== undefined) ? model.messageCount : 0
    property int subTasksCount: (typeof model !== "undefined" && model && model.subTasksCount !== undefined) ? model.subTasksCount : 0
    property bool hasSubTasks: (typeof model !== "undefined" && model && model.hasSubTasks !== undefined ? model.hasSubTasks : false)
    property var messagesModel: (typeof model !== "undefined" && model && model.messagesModel !== undefined) ? model.messagesModel : null
    property var subTasksModel: (typeof model !== "undefined" && model && model.subTasksModel !== undefined) ? model.subTasksModel : null
    property bool autoScroll: true
    property var taskView: null
    property var logWindow: null
    readonly property var effectiveView: taskView || rootCard.ListView.view || null

    x: cardDepth * 16
    width: (effectiveView ? effectiveView.width - 12 : (parent ? parent.width : 0)) - (cardDepth * 16)
    clip: true
    color: cardDepth === 0 ? "#212121" : (cardDepth === 1 ? "#1A1A1A" : "#141414")
    border.color: expanded ? (cardDepth === 0 ? "#4A4A4A" : (cardDepth === 1 ? "#3D3D3D" : "#333333")) : (cardDepth === 0 ? "#2E2E2E" : "#252525")
    border.width: 1

    implicitHeight: {
        if (!expanded) {
            return 40;
        }
        var view = effectiveView;
        if (cardDepth === 0) {
            var viewH = view ? view.height : 400;
            return Math.max(220, viewH - 12);
        }
        if (messageCount === 0) {
            return 40;
        }
        return 40 + 6 + Math.min(260, Math.max(30, messageListView.contentHeight + 12));
    }
    height: implicitHeight

    function scrollToBottom() {
        if (autoScroll && messageListView && messageListView.count > 0) {
            messageListView.positionViewAtEnd()
        }
    }

    Component.onCompleted: {
        var view = effectiveView;
        if (cardDepth === 0 && expanded && view) {
            Qt.callLater(function() {
                if (view) {
                    if (typeof view.positionRootCardAtTop === "function") {
                        view.positionRootCardAtTop(rootCard);
                    } else {
                        view.forceLayout();
                        view.positionViewAtIndex(cardIndex, ListView.Beginning);
                    }
                }
            });
        }
    }

    onExpandedChanged: {
        var view = effectiveView;
        if (expanded) {
            if (cardDepth === 0 && view) {
                Qt.callLater(function() {
                    if (view) {
                        if (typeof view.positionRootCardAtTop === "function") {
                            view.positionRootCardAtTop(rootCard);
                        } else {
                            view.forceLayout();
                            view.positionViewAtIndex(cardIndex, ListView.Beginning);
                        }
                    }
                });
            }
            if (autoScroll) {
                Qt.callLater(scrollToBottom);
            }
        } else {
            if (view) {
                Qt.callLater(function() {
                    if (view) {
                        view.returnToBounds();
                    }
                });
            }
        }
    }

    onAutoScrollChanged: {
        if (autoScroll && expanded) {
            Qt.callLater(scrollToBottom)
        }
    }

    // Task Header
    Rectangle {
        id: headerArea
        anchors.top: parent.top
        anchors.topMargin: 6
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.right: parent.right
        anchors.rightMargin: 6
        height: 28
        color: headerMouseArea.containsMouse ? "#2A2A2A" : "transparent"
        radius: 3

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 8

                // Expand / Collapse Chevron
                Text {
                    text: rootCard.expanded ? "▼" : "▶"
                    color: rootCard.cardDepth > 0 ? "#90A4AE" : "#B0BEC5"
                    font.pixelSize: 11
                    Layout.alignment: Qt.AlignVCenter
                }

                // Task Name (with text elide and tooltip for full command)
                Text {
                    id: taskNameText
                    text: rootCard.taskName
                    color: "#ECEFF1"
                    font.bold: rootCard.cardDepth === 0
                    font.pixelSize: rootCard.cardDepth === 0 ? 13 : 11
                    font.family: rootCard.cardDepth > 0 ? "Consolas, 'Courier New', monospace" : "Segoe UI, sans-serif"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 40
                    elide: Text.ElideRight
                    Layout.alignment: Qt.AlignVCenter

                    ToolTip.text: rootCard.taskName
                    ToolTip.visible: taskNameHoverArea.containsMouse && rootCard.taskName.length > 30
                    ToolTip.delay: 300

                    MouseArea {
                        id: taskNameHoverArea
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.NoButton
                    }
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
                        text: (rootCard.stateString === "PENDING") ? qsTr("PENDING")
                        : (rootCard.stateString === "RUNNING") ? qsTr("RUNNING")
                        : (rootCard.stateString === "COMPLETED") ? qsTr("COMPLETED")
                        : (rootCard.stateString === "FAILED") ? qsTr("FAILED")
                        : (rootCard.stateString === "CANCELLED") ? qsTr("CANCELLED")
                        : (rootCard.stateString === "SKIPPED") ? qsTr("SKIPPED")
                        : rootCard.stateString
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
                    if (rootCard.owningModel) {
                        var willExpand = !rootCard.expanded;
                        var tId = rootCard.taskId;

                        if (typeof rootCard.owningModel.toggleTaskExpandedById === "function") {
                            rootCard.owningModel.toggleTaskExpandedById(tId);
                        } else if (typeof rootCard.owningModel.toggleTaskExpanded === "function") {
                            rootCard.owningModel.toggleTaskExpanded(rootCard.cardIndex);
                        }

                        var view = rootCard.effectiveView;
                        if (view) {
                            if (willExpand && rootCard.cardDepth === 0) {
                                Qt.callLater(function() {
                                    if (view) {
                                        if (typeof view.positionRootCardAtTop === "function") {
                                            view.positionRootCardAtTop(rootCard);
                                        } else {
                                            view.forceLayout();
                                            view.positionViewAtIndex(rootCard.cardIndex, ListView.Beginning);
                                        }
                                    }
                                });
                            } else if (!willExpand) {
                                Qt.callLater(function() {
                                    if (view) {
                                        view.returnToBounds();
                                    }
                                });
                            }
                        }
                    }
                }
            }
        }

    // Message Output Area (Visible when expanded and has messages or is root card)
    Rectangle {
        id: messageArea
        anchors.top: headerArea.bottom
        anchors.topMargin: 6
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 6
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.right: parent.right
        anchors.rightMargin: 6
        visible: rootCard.expanded && (rootCard.cardDepth === 0 || rootCard.messageCount > 0)
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

                onCountChanged: {
                    if (rootCard.expanded && rootCard.autoScroll) {
                        Qt.callLater(messageListView.positionViewAtEnd)
                    }
                }

                Component.onCompleted: {
                    if (rootCard.expanded && rootCard.autoScroll) {
                        Qt.callLater(messageListView.positionViewAtEnd)
                    }
                }

                delegate: RowLayout {
                    id: msgRow
                    width: messageListView.width - 12
                    spacing: 6

                    required property string timestampString
                    required property string levelString
                    required property string message
                    required property var toolTaskId

                    Text {
                        text: "[" + (msgRow.timestampString || "00:00:00") + "]"
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
                            var lvl = (msgRow.levelString || "").toUpperCase()
                            if (lvl === "ERROR" || lvl === "CRITICAL") return "#C62828"
                            if (lvl === "WARNING") return "#EF6C00"
                            if (lvl === "DEBUG") return "#424242"
                            return "#2E7D32"
                        }

                        Text {
                            id: lvlText
                            anchors.centerIn: parent
                            text: {
                                var str = (msgRow.levelString || "INFO").toUpperCase()
                                if (str === "WARNING") return "WARN"
                                if (str === "CRITICAL") return "CRIT"
                                return str
                            }
                            color: "#FFFFFF"
                            font.bold: true
                            font.pixelSize: 9
                        }
                    }

                    // Detail Log Button for Tool Executions
                    Rectangle {
                        id: toolDetailBtn
                        visible: Number(msgRow.toolTaskId) > 0
                        implicitWidth: toolBtnRow.implicitWidth + 10
                        implicitHeight: 18
                        radius: 2
                        color: toolBtnMouseArea.containsMouse ? "#1976D2" : "#0D47A1"
                        border.color: "#64B5F6"
                        border.width: 1
                        Layout.alignment: Qt.AlignTop

                        RowLayout {
                            id: toolBtnRow
                            anchors.centerIn: parent
                            spacing: 4

                            Text {
                                text: "📄"
                                font.pixelSize: 10
                            }

                            Text {
                                text: qsTr("Detailed Log")
                                color: "#FFFFFF"
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        MouseArea {
                            id: toolBtnMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (rootCard.logWindow && typeof rootCard.logWindow.openToolLogWindow === "function") {
                                    rootCard.logWindow.openToolLogWindow(msgRow.toolTaskId, msgRow.message)
                                }
                            }
                        }
                    }

                    TextEdit {
                        Layout.fillWidth: true
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.Wrap
                        text: msgRow.message || ""
                        font.family: "Consolas, 'Courier New', monospace"
                        font.pixelSize: 12
                        color: {
                            var lvl = (msgRow.levelString || "").toUpperCase()
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
