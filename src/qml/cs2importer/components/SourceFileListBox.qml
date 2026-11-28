import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GroupBox {
    id: root

    property string titleText: qsTr("FILES")
    property var files: []
    property var allowedExtensions: []
    property bool allowFolders: false
    property QtObject pathExpander: null
    property bool enabledState: true

    signal addRequested()
    signal addFolderRequested()
    signal clearRequested()
    signal fileRemoved(int index)
    signal emptySelectionEncountered()

    function addFiles(newFileList) {
        let current = root.files.slice()
        let seen = {}
        for (let i = 0; i < current.length; ++i) {
            seen[current[i].toLowerCase()] = true
        }
        let changed = false
        for (let i = 0; i < newFileList.length; ++i) {
            let p = newFileList[i]
            if (p) {
                let lower = p.toLowerCase()
                if (!seen[lower]) {
                    seen[lower] = true
                    current.push(p)
                    changed = true
                }
            }
        }
        if (changed) {
            root.files = current
        }
    }

    function addFolder(folderPath) {
        if (!folderPath || !root.pathExpander) {
            return 0
        }
        let expanded = root.pathExpander.expandSourcePaths([folderPath], root.allowedExtensions)
        if (expanded && expanded.length > 0) {
            root.addFiles(expanded)
            return expanded.length
        }
        root.emptySelectionEncountered()
        return 0
    }

    function removeFileAt(index) {
        if (index >= 0 && index < root.files.length) {
            let current = root.files.slice()
            current.splice(index, 1)
            root.files = current
            root.fileRemoved(index)
        }
    }

    function clearAll() {
        root.files = []
        root.clearRequested()
    }

    title: titleText
    label: Label {
        x: root.leftPadding
        width: root.availableWidth
        text: root.titleText + (root.files.length > 0 ? " (" + root.files.length + ")" : "")
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        color: root.palette.windowText
        elide: Text.ElideRight
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        // Top toolbar: Add, optional Folder, & Clear buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Button {
                id: addBtn
                text: qsTr("Add")
                Layout.fillWidth: true
                Layout.preferredHeight: 24
                enabled: root.enabledState

                contentItem: Text {
                    text: addBtn.text
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: addBtn.palette.buttonText
                }

                onClicked: root.addRequested()
            }

            Button {
                id: addFolderBtn
                visible: root.allowFolders
                text: qsTr("Folder")
                Layout.fillWidth: true
                Layout.preferredHeight: 24
                enabled: root.enabledState

                contentItem: Text {
                    text: addFolderBtn.text
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: addFolderBtn.palette.buttonText
                }

                onClicked: root.addFolderRequested()
            }

            Button {
                id: clearBtn
                text: qsTr("Clear")
                Layout.preferredWidth: 46
                Layout.preferredHeight: 24
                enabled: root.enabledState && root.files.length > 0

                contentItem: Text {
                    text: clearBtn.text
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: clearBtn.palette.buttonText
                }

                onClicked: root.clearAll()
            }
        }

        // DropArea & ListView container
        Rectangle {
            id: listContainer
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: dropArea.containsDrag ? (root.palette.highlight ? Qt.alpha(root.palette.highlight, 0.25) : "#400078D7") : Qt.alpha(root.palette.base, 0.35)
            border.color: dropArea.containsDrag ? root.palette.highlight : Qt.alpha(root.palette.mid, 0.6)
            border.width: dropArea.containsDrag ? 2 : 1
            radius: 3
            clip: true

            DropArea {
                id: dropArea
                anchors.fill: parent
                enabled: root.enabledState

                onDropped: function(drop) {
                    if (!drop.hasUrls) return
                    let rawPaths = []
                    for (let i = 0; i < drop.urls.length; ++i) {
                        let u = drop.urls[i].toString()
                        let p = ""
                        if (u.indexOf("file:///") === 0) {
                            p = u.substring(8)
                        } else if (u.indexOf("file://") === 0) {
                            p = u.substring(7)
                        } else {
                            p = u
                        }
                        p = decodeURIComponent(p).replace(/\//g, "\\")
                        if (p) {
                            rawPaths.push(p)
                        }
                    }

                    if (root.allowFolders && root.pathExpander) {
                        let expanded = root.pathExpander.expandSourcePaths(rawPaths, root.allowedExtensions)
                        if (expanded && expanded.length > 0) {
                            root.addFiles(expanded)
                        } else if (rawPaths.length > 0) {
                            root.emptySelectionEncountered()
                        }
                        drop.acceptProposedAction()
                        return
                    }

                    let droppedPaths = []
                    for (let i = 0; i < rawPaths.length; ++i) {
                        let p = rawPaths[i]
                        if (root.allowedExtensions && root.allowedExtensions.length > 0) {
                            let match = false
                            let lower = p.toLowerCase()
                            for (let j = 0; j < root.allowedExtensions.length; ++j) {
                                if (lower.endsWith(root.allowedExtensions[j].toLowerCase())) {
                                    match = true
                                    break
                                }
                            }
                            if (!match) continue
                        }
                        droppedPaths.push(p)
                    }
                    if (droppedPaths.length > 0) {
                        root.addFiles(droppedPaths)
                    }
                    drop.acceptProposedAction()
                }
            }

            Label {
                anchors.centerIn: parent
                visible: root.files.length === 0
                text: root.allowFolders
                    ? qsTr("Drop files or folders here\nor click Add / Folder")
                    : qsTr("Drop files here\nor click Add")
                font.pixelSize: 11
                color: root.palette.placeholderText
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            ScrollView {
                anchors.fill: parent
                visible: root.files.length > 0
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                ListView {
                    id: fileListView
                    model: root.files
                    spacing: 2
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: Rectangle {
                        id: itemDelegate
                        width: fileListView.width
                        height: 24
                        color: itemMouseArea.containsMouse ? root.palette.alternateBase : "transparent"
                        radius: 2

                        MouseArea {
                            id: itemMouseArea
                            anchors.fill: parent
                            hoverEnabled: true

                            ToolTip.visible: containsMouse
                            ToolTip.text: modelData
                            ToolTip.delay: 500
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 4
                            anchors.rightMargin: 4
                            spacing: 4

                            Text {
                                Layout.fillWidth: true
                                text: {
                                    let s = modelData
                                    let idx = Math.max(s.lastIndexOf('/'), s.lastIndexOf('\\'))
                                    return idx >= 0 ? s.substring(idx + 1) : s
                                }
                                font.pixelSize: 11
                                color: root.palette.text
                                elide: Text.ElideMiddle
                                verticalAlignment: Text.AlignVCenter
                            }

                            ToolButton {
                                text: "✕"
                                Layout.preferredWidth: 18
                                Layout.preferredHeight: 18
                                font.pixelSize: 9
                                enabled: root.enabledState
                                onClicked: root.removeFileAt(index)
                            }
                        }
                    }
                }
            }
        }
    }
}
