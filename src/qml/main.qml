import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    width: 1024
    height: 768
    minimumWidth: 1024
    maximumWidth: 1024
    minimumHeight: 768
    maximumHeight: 768
    visible: true
    title: "CS2 IMPORTER"

    // Property to bind the backend object
    property var backend: backendObject
    property string selectedMapFileName: "Select VMF/BSP"
    property var logLines: []

    Connections {
        target: backend
        function onLogMessage(msg) {
            // Very simple HTML escape to ensure < and > don't mess up RichText
            var safeMsg = msg.replace(/&/g, "&amp;")
                             .replace(/</g, "&lt;")
                             .replace(/>/g, "&gt;");

            // Highlight
            if (safeMsg.indexOf("ERROR") !== -1) {
                safeMsg = "<font color='red'>" + safeMsg + "</font>";
            }else if(safeMsg.indexOf("Skip") !== -1) {
                safeMsg = "<font color='yellow'>" + safeMsg + "</font>";
            }else if(safeMsg.indexOf("Unable") !== -1) {
                safeMsg = "<font color='yellow'>" + safeMsg + "</font>";
            }else if(safeMsg.indexOf("WARN") !== -1) {
                safeMsg = "<font color='yellow'>" + safeMsg + "</font>";
            }
            logLines.push(safeMsg)
            if (logLines.length > 100) {
                logLines.shift()
            }
            logOutput.text = logLines.join("<br>")

            if (logScrollView.ScrollBar.vertical) {
                logScrollView.ScrollBar.vertical.position = 1.0 - logScrollView.ScrollBar.vertical.size
            }
        }
        function onAlertMessage(title, msg) {
            messageDialog.title = title
            messageDialog.text = msg
            messageDialog.open()
        }
    }

    // Dialogs
    FolderDialog {
        id: cs2FolderDialog
        title: "Select CS2 folder"
        onAccepted: backend.SelectCs2FolderDialog(selectedFolder)
    }

    FolderDialog {
        id: s1FolderDialog
        title: "Select Source 1 Game Folder"
        onAccepted: backend.SelectS1FolderDialog(selectedFolder)
    }

    FileDialog {
        id: mapFileDialog
        title: "Select VMF or BSP"
        nameFilters: ["Map files (*.vmf *.bsp)"]
        onAccepted: {
            let path = selectedFile.toString()
            let fileName = path.substring(path.lastIndexOf('/') + 1)
            selectedMapFileName = fileName
            if (path.endsWith(".vmf")) {
                backend.SelectVmfDialog(selectedFile)
            } else if (path.endsWith(".bsp")) {
                backend.SelectBspDialog(selectedFile)
            }
        }
    }

    MessageDialog {
        id: messageDialog
        title: "Message"
        buttons: MessageDialog.Ok
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 15

        // Left Column
        ColumnLayout {
            Layout.fillHeight: true
            Layout.preferredWidth: 400
            Layout.maximumWidth: 400
            spacing: 20

            // Row 1: Folders
            RowLayout {
                Layout.fillWidth: true
                spacing: 15

                // Source 1 Game Box
                GroupBox {
                    title: "Source 1 Game"
                    Layout.preferredWidth: 165
                    Layout.preferredHeight: 180

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10

                        ComboBox {
                            id: s1GameCombo
                            model: ["CSGO", "CSS", "HL2", "L4D", "L4D2", "Portal", "Portal2", "TF2", "GMod"]
                            Layout.fillWidth: true
                            Component.onCompleted: {
                                let type = backend.s1GameType.toLowerCase();
                                if (type === "css") currentIndex = 1;
                                else if (type === "hl2") currentIndex = 2;
                                else if (type === "l4d") currentIndex = 3;
                                else if (type === "l4d2") currentIndex = 4;
                                else if (type === "portal") currentIndex = 5;
                                else if (type === "portal2") currentIndex = 6;
                                else if (type === "tf2") currentIndex = 7;
                                else if (type === "gmod") currentIndex = 8;
                                else currentIndex = 0;
                            }
                            onActivated: backend.SetS1GameType(currentText.toLowerCase())
                        }

                        Button {
                            id: s1FolderButton
                            text: backend.s1gameBasefolder === "" ? "Press to Select Game Folder" : backend.s1gameBasefolder
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            contentItem: Text {
                                text: parent.text
                                wrapMode: Text.WordWrap
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: s1FolderDialog.open()
                        }

                        Button {
                            text: "Validate Game File"
                            Layout.fillWidth: true
                            onClicked: backend.ValidateS1()
                        }
                    }
                }

                Label {
                    text: "➡"
                    font.pixelSize: 40
                    Layout.alignment: Qt.AlignVCenter
                }

                // Counter Strike 2 Box
                GroupBox {
                    title: "Counter Strike 2"
                    Layout.preferredWidth: 165
                    Layout.preferredHeight: 180

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10

                        Button {
                            id: cs2FolderButton
                            text: backend.cs2Basefolder === "" ? "Press to Select Game Folder" : backend.cs2Basefolder
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            contentItem: Text {
                                text: parent.text
                                wrapMode: Text.WordWrap
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: cs2FolderDialog.open()
                        }

                        Button {
                            text: "Validate Game File"
                            Layout.fillWidth: true
                            onClicked: backend.ValidateCs2()
                        }
                    }
                }
            }

            // Row 2: Maps and Addon
            RowLayout {
                Layout.fillWidth: true
                spacing: 15

                Button {
                    Layout.preferredWidth: 165
                    Layout.preferredHeight: 40
                    text: selectedMapFileName
                    contentItem: Text {
                        text: parent.text
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideMiddle
                    }
                    onClicked: {
                        mapFileDialog.currentFolder = backend.vmfDefaultPathUrl
                        mapFileDialog.open()
                    }
                }

                Label {
                    text: "➡"
                    font.pixelSize: 40
                    Layout.alignment: Qt.AlignVCenter
                }

                TextField {
                    id: addonEdit
                    Layout.preferredWidth: 165
                    Layout.preferredHeight: 40
                    placeholderText: "Addon Name in CS2"
                    text: backend.addonName
                    onTextChanged: backend.addonName = text
                    horizontalAlignment: TextInput.AlignHCenter
                    verticalAlignment: TextInput.AlignVCenter
                }
            }

            // Row 3: START and STOP Buttons
            RowLayout {
                Layout.fillWidth: true
                spacing: 15

                Button {
                    id: goButton
                    text: "START"
                    enabled: backend.canGo && !backend.isGoing
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40

                    contentItem: Text {
                        text: parent.text
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.bold: true
                    }

                    onClicked: {
                        logLines = []
                        logOutput.clear()
                        backend.Start()
                    }
                }

                Button {
                    id: stopButton
                    text: "STOP"
                    enabled: backend.isGoing
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40

                    contentItem: Text {
                        text: parent.text
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.bold: true
                    }

                    onClicked: {
                        backend.Stop()
                    }
                }
            }

            // Row 4: OPTIONS
            GroupBox {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    Label {
                        text: "OPTIONS"
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }

                    anchors.fill: parent
                    spacing: 5

                    CheckBox {
                        id: useBspCheckbox
                        text: "Clean Unnecessary Faces"
                        checked: backend.usebsp
                        onCheckedChanged: backend.usebsp = checked
                        ToolTip.text: "This runs the map through a special vbsp process to generate clean map geometry from brushes"
                        ToolTip.visible: hovered
                    }
                    CheckBox {
                        id: nomergeInstancesCheckbox
                        text: "Keep Instances' Faces"
                        checked: backend.usebspNomergeinstances
                        enabled: useBspCheckbox.checked
                        onCheckedChanged: backend.usebspNomergeinstances = checked
                        ToolTip.text: "if you wish to both generate clean geo and also preserve func_instances got merge in"
                        ToolTip.visible: hovered
                    }
                    CheckBox {
                        id: skipDepsCheckbox
                        text: "Skip References Import"
                        checked: backend.skipdeps
                        onCheckedChanged: backend.skipdeps = checked
                        ToolTip.text: "Optional: skips importing all dependencies/content and only generates the vmap file(s)"
                        ToolTip.visible: hovered
                    }
                    Item { Layout.fillHeight: true } // Spacer
                }
            }
        }

        // Right Column (Log Area)
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 5

            Label {
                text: "LOG"
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "black"
                border.color: "black"
                border.width: 3

                ScrollView {
                    id: logScrollView
                    anchors.fill: parent
                    anchors.margins: 5

                    TextArea {
                        id: logOutput
                        readOnly: true
                        color: "white"
                        font.family: "monospace"
                        wrapMode: TextArea.Wrap
                        textFormat: Text.RichText
                        background: Rectangle { color: "black" }
                    }
                }
            }
        }
    }
}
