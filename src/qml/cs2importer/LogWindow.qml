import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

ApplicationWindow {
    id: root

    property QtObject logViewModel: null
    property var toolWindows: ({})

    function openToolLogWindow(toolTaskId, commandLine) {
        if (!toolTaskId) return;
        var key = String(toolTaskId);
        if (toolWindows[key]) {
            try {
                toolWindows[key].show();
                toolWindows[key].raise();
                toolWindows[key].requestActivate();
                return;
            } catch (e) {
                delete toolWindows[key];
            }
        }

        var cleanCmd = (commandLine || ("Task " + toolTaskId)).trim();
        if (cleanCmd.startsWith("[EXEC] ")) {
            cleanCmd = cleanCmd.substring(7).trim();
        }

        var component = Qt.createComponent("ToolLogWindow.qml");
        var createWin = function() {
            var win = component.createObject(null, {
                "logViewModel": root.logViewModel,
                "toolTaskId": toolTaskId,
                "toolName": cleanCmd
            });
            if (win) {
                toolWindows[key] = win;
                win.windowClosed.connect(function(closedId) {
                    delete toolWindows[String(closedId)];
                });
                win.show();
                win.raise();
                win.requestActivate();
            }
        };

        if (component.status === Component.Ready) {
            createWin();
        } else if (component.status === Component.Loading) {
            component.statusChanged.connect(function() {
                if (component.status === Component.Ready) {
                    createWin();
                }
            });
        } else if (component.status === Component.Error) {
            console.error("Failed to create ToolLogWindow:", component.errorString());
        }
    }

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
                text: qsTr("Open log folder")
                onClicked: {
                    if (root.logViewModel) {
                        root.logViewModel.openLogFolder()
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
                objectName: "taskListView"
                anchors.fill: parent
                anchors.margins: 6
                clip: true
                spacing: 8
                bottomMargin: Math.max(0, taskListView.height - 48)
                model: root.logViewModel
                boundsBehavior: Flickable.StopAtBounds

                NumberAnimation {
                    id: scrollAnim
                    target: taskListView
                    property: "contentY"
                    duration: 250
                    easing.type: Easing.OutCubic
                }

                function smoothScrollTo(targetY) {
                    scrollAnim.stop();
                    var minY = taskListView.originY;
                    var maxScrollY = taskListView.originY + Math.max(0, taskListView.contentHeight - taskListView.height + taskListView.bottomMargin);
                    var clampedY = Math.max(minY, Math.min(targetY, maxScrollY));
                    scrollAnim.to = clampedY;
                    scrollAnim.start();
                }

                function positionRootCardAtTop(cardItem) {
                    if (!cardItem) return;
                    taskListView.forceLayout();
                    var visualY = cardItem.mapToItem(taskListView, 0, 0).y;
                    var targetY = taskListView.contentY + visualY;
                    smoothScrollTo(targetY);
                }

                ScrollBar.vertical: ScrollBar {
                    id: vScrollBar
                    active: true
                }

                delegate: LogTaskCard {
                    required property var model
                    required property int index
                    cardIndex: index
                    taskId: (model && model.taskId !== undefined) ? model.taskId : 0
                    cardDepth: (model && model.depth !== undefined) ? model.depth : 0
                    taskName: (model && model.taskName !== undefined) ? model.taskName : ""
                    stateString: (model && model.stateString !== undefined) ? model.stateString : ""
                    progress: (model && model.progress !== undefined) ? model.progress : 0.0
                    expanded: (model && model.expanded !== undefined) ? model.expanded : true
                    messageCount: (model && model.messageCount !== undefined) ? model.messageCount : 0
                    hasSubTasks: (model && model.hasSubTasks !== undefined) ? model.hasSubTasks : false
                    subTasksCount: (model && model.subTasksCount !== undefined) ? model.subTasksCount : 0
                    messagesModel: (model && model.messagesModel !== undefined) ? model.messagesModel : null
                    subTasksModel: (model && model.subTasksModel !== undefined) ? model.subTasksModel : null
                    owningModel: root.logViewModel
                    autoScroll: root.logViewModel ? root.logViewModel.autoScroll : true
                    taskView: taskListView
                    logWindow: root
                }
            }
        }
    }
}
