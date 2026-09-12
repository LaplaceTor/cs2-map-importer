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

    // The tool task node may be projected into the view model slightly after the
    // window opens (created when the tool's first log block arrives), and the state
    // badge must track a running task. refreshTimer re-fetches until the model
    // appears and the task reaches a terminal state.
    property var messagesModel: logViewModel ? logViewModel.getToolMessagesModel(toolTaskId) : null
    property string stateString: logViewModel ? logViewModel.getToolTaskState(toolTaskId) : ""
    property string logFilePath: logViewModel ? logViewModel.getToolTaskLogFilePath(toolTaskId) : ""

    Timer {
        id: refreshTimer
        interval: 250
        repeat: true
        running: root.visible && root.logViewModel !== null
            && (root.messagesModel === null
                || ["PENDING", "RUNNING"].indexOf((root.stateString || "").toUpperCase()) !== -1)
        onTriggered: {
            if (!root.logViewModel) {
                return
            }
            root.messagesModel = root.logViewModel.getToolMessagesModel(root.toolTaskId)
            root.stateString = root.logViewModel.getToolTaskState(root.toolTaskId)
            root.logFilePath = root.logViewModel.getToolTaskLogFilePath(root.toolTaskId)
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
                        var text = root.logViewModel.getToolFullLogText(root.toolTaskId)
                        clipboardHelper.text = text
                        clipboardHelper.selectAll()
                        clipboardHelper.copy()
                    }
                }
            }

            StyledCheckBox {
                text: qsTr("Auto-scroll")
                checked: root.autoScroll
                onCheckedChanged: {
                    root.autoScroll = checked
                    if (checked && messageListView.count > 0) {
                        Qt.callLater(messageListView.positionViewAtEnd)
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
                    text: root.stateString || "UNKNOWN"
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

            ListView {
                id: messageListView
                anchors.fill: parent
                anchors.margins: 6
                clip: true
                spacing: 3
                model: root.messagesModel
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {
                    active: messageListView.contentHeight > messageListView.height
                }

                onCountChanged: {
                    if (root.autoScroll) {
                        Qt.callLater(messageListView.positionViewAtEnd)
                    }
                }

                Component.onCompleted: {
                    if (root.autoScroll) {
                        Qt.callLater(messageListView.positionViewAtEnd)
                    }
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

    TextEdit {
        id: clipboardHelper
        visible: false
    }
}
