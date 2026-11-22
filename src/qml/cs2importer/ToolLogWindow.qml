import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

ApplicationWindow {
    id: root

    property var logViewModel: null
    property var toolTaskId: 0
    property string toolName: ""
    property bool autoScroll: true
    property bool userScrolledUp: false

    property var messagesModel: null
    property string stateString: "UNKNOWN"
    property string logFilePath: ""

    function syncToolTask() {
        if (!root.logViewModel || !root.toolTaskId) {
            root.messagesModel = null
            root.stateString = "UNKNOWN"
            root.logFilePath = ""
            return
        }
        root.messagesModel = root.logViewModel.toolMessagesModel(root.toolTaskId)
        root.stateString = root.logViewModel.toolTaskState(root.toolTaskId)
        root.logFilePath = root.logViewModel.toolTaskLogFilePath(root.toolTaskId)
    }

    Component.onCompleted: syncToolTask()
    onToolTaskIdChanged: syncToolTask()
    onLogViewModelChanged: syncToolTask()

    Connections {
        target: root.logViewModel
        function onToolTaskUpdated(updatedId) {
            if (updatedId === root.toolTaskId) {
                root.syncToolTask()
            }
        }
    }

    signal windowClosed(var id)

    width: 760
    height: 480
    minimumWidth: 500
    minimumHeight: 300
    title: qsTr("External Tool Log - %1").arg(toolName ? toolName : ("Task " + toolTaskId))
    color: palette.window
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint | Qt.WindowCloseButtonHint

    onClosing: {
        root.windowClosed(root.toolTaskId)
        Qt.callLater(root.destroy)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // Toolbar
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: qsTr("Open log file")
                enabled: root.logFilePath !== ""
                onClicked: {
                    if (root.logViewModel) {
                        root.logViewModel.openToolLogFile(root.toolTaskId)
                    }
                }
            }

            Button {
                text: qsTr("Copy all")
                onClicked: {
                    if (root.logViewModel) {
                        root.logViewModel.copyToolLogToClipboard(root.toolTaskId)
                    }
                }
            }

            StyledCheckBox {
                text: qsTr("Auto-scroll")
                checked: root.autoScroll
                onCheckedChanged: {
                    root.autoScroll = checked
                    if (checked) {
                        root.userScrolledUp = false
                        if (messageListView.count > 0) {
                            Qt.callLater(messageListView.positionViewAtEnd)
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
            }

            // State Badge
            Rectangle {
                implicitWidth: stateText.implicitWidth + 12
                implicitHeight: 22
                radius: 3
                Layout.alignment: Qt.AlignVCenter

                color: {
                    var s = (root.stateString || "").toUpperCase()
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
                    text: (root.stateString === "PENDING") ? qsTr("PENDING")
                        : (root.stateString === "RUNNING") ? qsTr("RUNNING")
                        : (root.stateString === "COMPLETED") ? qsTr("COMPLETED")
                        : (root.stateString === "FAILED") ? qsTr("FAILED")
                        : (root.stateString === "CANCELLED") ? qsTr("CANCELLED")
                        : (root.stateString === "SKIPPED") ? qsTr("SKIPPED")
                        : qsTr("UNKNOWN")
                    color: "#FFFFFF"
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            Label {
                text: qsTr("Lines: %1").arg(messageListView.count)
                font.pixelSize: 12
                color: palette.placeholderText
                Layout.alignment: Qt.AlignVCenter
            }
        }

        // Command summary banner
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: cmdText.implicitHeight + 10
            color: "#1E1E1E"
            border.color: "#333333"
            border.width: 1
            radius: 3

            RowLayout {
                anchors.fill: parent
                anchors.margins: 5
                spacing: 6

                Text {
                    text: qsTr("CMD:")
                    color: "#90CAF9"
                    font.bold: true
                    font.pixelSize: 11
                    font.family: "Consolas, 'Courier New', monospace"
                    Layout.alignment: Qt.AlignTop
                }

                TextEdit {
                    id: cmdText
                    Layout.fillWidth: true
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    text: root.toolName
                    font.family: "Consolas, 'Courier New', monospace"
                    font.pixelSize: 11
                    color: "#E0E0E0"
                }
            }
        }

        // Console Area
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#141414"
            border.color: "#2C2C2C"
            border.width: 1
            radius: 4
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 4

                Rectangle {
                    id: truncationBanner
                    Layout.fillWidth: true
                    implicitHeight: truncationText.implicitHeight + 8
                    visible: root.messagesModel && root.messagesModel.truncatedCount > 0
                    color: "#263238"
                    border.color: "#455A64"
                    border.width: 1
                    radius: 2

                    Text {
                        id: truncationText
                        anchors.fill: parent
                        anchors.margins: 4
                        wrapMode: Text.Wrap
                        text: qsTr("Earlier %1 log line(s) omitted from view. Open the log file for full history.")
                            .arg(root.messagesModel ? root.messagesModel.truncatedCount : 0)
                        color: "#B0BEC5"
                        font.pixelSize: 11
                    }
                }

                ListView {
                    id: messageListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 3
                    reuseItems: true
                    model: root.messagesModel
                    boundsBehavior: Flickable.StopAtBounds

                    WheelHandler {
                        id: toolWheelHandler
                        target: null
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                        onWheel: (event) => {
                            var delta = event.angleDelta.y !== 0 ? event.angleDelta.y : event.pixelDelta.y;
                            if (delta > 0) {
                                root.userScrolledUp = true;
                            } else if (delta < 0) {
                                if (messageListView.atYEnd) {
                                    root.userScrolledUp = false;
                                }
                            }
                            event.accepted = false; // Allow native list scrolling
                        }
                    }

                    ScrollBar.vertical: ScrollBar {
                        id: toolScrollBar
                        active: messageListView.contentHeight > messageListView.height
                        onPressedChanged: {
                            if (!pressed && messageListView.atYEnd) {
                                root.userScrolledUp = false;
                            }
                        }
                        onPositionChanged: {
                            if (pressed) {
                                if (messageListView.atYEnd) {
                                    root.userScrolledUp = false;
                                } else {
                                    root.userScrolledUp = true;
                                }
                            }
                        }
                    }

                    onCountChanged: {
                        if (root.autoScroll && !root.userScrolledUp) {
                            Qt.callLater(messageListView.positionViewAtEnd)
                        }
                    }

                    Component.onCompleted: {
                        if (root.autoScroll) {
                            Qt.callLater(messageListView.positionViewAtEnd)
                        }
                    }

                    delegate: RowLayout {
                        id: msgRow
                        width: messageListView.width - 12
                        spacing: 6

                        required property string timestampString
                        required property int level
                        required property string message

                        ListView.onReused: msgTextEdit.deselect()

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
                                if (msgRow.level === 3 || msgRow.level === 4) return "#C62828"
                                if (msgRow.level === 2) return "#EF6C00"
                                if (msgRow.level === 0) return "#424242"
                                return "#2E7D32"
                            }

                            Text {
                                id: lvlText
                                anchors.centerIn: parent
                                text: {
                                    if (msgRow.level === 0) return "DEBUG"
                                    if (msgRow.level === 2) return "WARN"
                                    if (msgRow.level === 3) return "ERROR"
                                    if (msgRow.level === 4) return "CRIT"
                                    return "INFO"
                                }
                                color: "#FFFFFF"
                                font.bold: true
                                font.pixelSize: 9
                            }
                        }

                        TextEdit {
                            id: msgTextEdit
                            Layout.fillWidth: true
                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.Wrap
                            text: msgRow.message || ""
                            font.family: "Consolas, 'Courier New', monospace"
                            font.pixelSize: 12
                            color: {
                                if (msgRow.level === 3 || msgRow.level === 4) return "#FF5252"
                                if (msgRow.level === 2) return "#FFD740"
                                if (msgRow.level === 0) return "#9E9E9E"
                                return "#ECEFF1"
                            }
                        }
                    }
                }
            }
        }
    }
}
