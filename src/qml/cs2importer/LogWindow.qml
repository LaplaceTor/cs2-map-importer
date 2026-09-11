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
                    owningModel: root.logViewModel
                    autoScroll: root.logViewModel ? root.logViewModel.autoScroll : true
                    taskView: taskListView
                    logWindow: root
                }

                onCountChanged: {
                    if (root.logViewModel && root.logViewModel.autoScroll) {
                        Qt.callLater(taskListView.positionViewAtEnd)
                    }
                }
            }

            Connections {
                target: root.logViewModel
                function onAutoScrollChanged() {
                    if (root.logViewModel && root.logViewModel.autoScroll) {
                        Qt.callLater(taskListView.positionViewAtEnd)
                    }
                }
            }
        }
    }
}
