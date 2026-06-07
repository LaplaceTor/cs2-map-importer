import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import cs2importer 1.0

ApplicationWindow {
    id: window
    width: 900
    height: 700
    minimumWidth: 900
    maximumWidth: 900
    minimumHeight: 700
    maximumHeight: 700
    visible: true
    title: "CS2 Importer"

    // Property to bind the backend object
    property var backend: backendObject
    property string selectedMapFileName: "vmf/bsp name"

    Connections {
        target: backend
        function onLogMessage(msg) {
            // Very simple HTML escape to ensure < and > don't mess up RichText
            var safeMsg = msg.replace(/&/g, "&amp;")
                             .replace(/</g, "&lt;")
                             .replace(/>/g, "&gt;");

            // Highlight errors in red
            if (safeMsg.indexOf("ERROR") !== -1) {
                safeMsg = "<font color='red'>" + safeMsg + "</font>";
            }
            logOutput.append(safeMsg)
        }
        function onAlertMessage(title, msg) {
            messageDialog.title = title
            messageDialog.text = msg
            messageDialog.open()
        }
        function onCs2BasefolderChanged() {
            if (backend.cs2_basefolder !== "") {
                cs2FolderButton.text = backend.cs2_basefolder
            } else {
                cs2FolderButton.text = "Press to select game folder"
            }
        }
        function onS1gameBasefolderChanged() {
            if (backend.s1game_basefolder !== "") {
                s1FolderButton.text = backend.s1game_basefolder
            } else {
                s1FolderButton.text = "Press to select game folder"
            }
        }
        function onS1GameTypeChanged() {
            if (backend.s1game_basefolder === "") {
                s1FolderButton.text = "Press to select game folder"
            } else {
                s1FolderButton.text = backend.s1game_basefolder
            }
        }
    }

    // Dialogs
    FolderDialog {
        id: cs2FolderDialog
        title: "Select CS2 folder"
        onAccepted: backend.select_cs2_folder_dialog(selectedFolder)
    }

    FolderDialog {
        id: s1FolderDialog
        title: "Select Source 1 game folder"
        onAccepted: backend.select_s1_folder_dialog(selectedFolder)
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
                backend.select_vmf_dialog(selectedFile)
            } else if (path.endsWith(".bsp")) {
                backend.select_bsp_dialog(selectedFile)
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
                    title: "SOURCE 1 GAME"
                    Layout.preferredWidth: 160
                    Layout.preferredHeight: 180

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10

                        ComboBox {
                            id: s1GameCombo
                            model: ["CSGO", "CSS"]
                            currentIndex: backend.s1_game_type === "css" ? 1 : 0
                            Layout.fillWidth: true
                            onActivated: backend.set_s1_game_type(currentText.toLowerCase())
                        }

                        Button {
                            id: s1FolderButton
                            text: backend.s1game_basefolder === "" ? "Press to select game folder" : backend.s1game_basefolder
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            contentItem: Text {
                                text: parent.text
                                wrapMode: Text.WrapAnywhere
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            onClicked: s1FolderDialog.open()
                        }

                        Button {
                            text: "validate"
                            Layout.fillWidth: true
                            onClicked: backend.validate_s1()
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
                    Layout.preferredWidth: 160
                    Layout.preferredHeight: 180

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10

                        Button {
                            id: cs2FolderButton
                            text: backend.cs2_basefolder === "" ? "Press to select game folder" : backend.cs2_basefolder
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            contentItem: Text {
                                text: parent.text
                                wrapMode: Text.WrapAnywhere
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            onClicked: cs2FolderDialog.open()
                        }

                        Button {
                            text: "validate"
                            Layout.fillWidth: true
                            onClicked: backend.validate_cs2()
                        }
                    }
                }
            }

            // Row 2: Maps and Addon
            RowLayout {
                Layout.fillWidth: true
                spacing: 15

                Button {
                    Layout.preferredWidth: 160
                    Layout.preferredHeight: 40
                    text: selectedMapFileName
                    contentItem: Text {
                        text: parent.text
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideMiddle
                    }
                    onClicked: {
                        mapFileDialog.currentFolder = backend.vmf_default_path_url
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
                    Layout.preferredWidth: 160
                    Layout.preferredHeight: 40
                    placeholderText: "s2 addon name"
                    text: backend.addon_name
                    onTextChanged: backend.addon_name = text
                    horizontalAlignment: TextInput.AlignHCenter
                    verticalAlignment: TextInput.AlignVCenter
                }
            }

            // Row 3: START Button
            Button {
                id: goButton
                text: "START"
                enabled: backend.can_go
                Layout.fillWidth: true
                Layout.preferredHeight: 40

                contentItem: Text {
                    text: parent.text
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.bold: true
                }

                onClicked: {
                    logOutput.clear()
                    backend.go()
                }
            }

            // Row 4: OPTIONS
            GroupBox {
                title: "OPTIONS"
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 5

                    CheckBox {
                        id: useBspCheckbox
                        text: "Clean unnecessary faces"
                        checked: backend.usebsp
                        onCheckedChanged: backend.usebsp = checked
                        ToolTip.text: "This runs the map through a special vbsp process to generate clean map geometry from brushes..."
                        ToolTip.visible: hovered
                    }
                    CheckBox {
                        id: nomergeInstancesCheckbox
                        text: "Keep instances"
                        checked: backend.usebsp_nomergeinstances
                        enabled: useBspCheckbox.checked
                        onCheckedChanged: backend.usebsp_nomergeinstances = checked
                        ToolTip.text: "Use this instead of -usebsp if you wish to both generate clean geo and also preserve func_instances..."
                        ToolTip.visible: hovered
                    }
                    CheckBox {
                        id: skipDepsCheckbox
                        text: "skip references import"
                        checked: backend.skipdeps
                        onCheckedChanged: backend.skipdeps = checked
                        ToolTip.text: "Optional: skips importing all dependencies/content and only generates the vmap file(s)..."
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
