import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import cs2importer 1.0

ApplicationWindow {
    id: window
    width: 900
    height: 700
    visible: true
    title: "CS2 Importer"

    // Property to bind the backend object
    property var backend: backendObject
    property string selectedMapFileName: "vmf/bsp name"
    property bool logVisible: true

    Connections {
        target: backend
        function onLogMessage(msg) {
            logOutput.append(msg)
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
                Rectangle {
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: 180
                    border.color: "black"
                    border.width: 3
                    color: "transparent"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 10

                        Label {
                            text: "SOURCE 1 GAME"
                            Layout.alignment: Qt.AlignHCenter
                            font.pixelSize: 14
                        }

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
                Rectangle {
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: 180
                    border.color: "black"
                    border.width: 3
                    color: "transparent"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 10

                        Label {
                            text: "Counter Strike 2"
                            Layout.alignment: Qt.AlignHCenter
                            font.pixelSize: 14
                        }

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

                Rectangle {
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: 50
                    border.color: "black"
                    border.width: 3
                    color: "transparent"

                    Button {
                        anchors.fill: parent
                        anchors.margins: 3
                        text: selectedMapFileName
                        background: Rectangle { color: "transparent" }
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
                }

                Label {
                    text: "➡"
                    font.pixelSize: 40
                    Layout.alignment: Qt.AlignVCenter
                }

                Rectangle {
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: 50
                    border.color: "black"
                    border.width: 3
                    color: "transparent"

                    TextField {
                        id: addonEdit
                        anchors.fill: parent
                        anchors.margins: 3
                        placeholderText: "s2 addon name"
                        text: backend.addon_name
                        onTextChanged: backend.addon_name = text
                        background: Rectangle { color: "transparent" }
                        horizontalAlignment: TextInput.AlignHCenter
                        verticalAlignment: TextInput.AlignVCenter
                    }
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

                background: Rectangle {
                    border.color: "black"
                    border.width: 3
                    color: parent.pressed ? "#e0e0e0" : "transparent"
                }

                onClicked: backend.go()
            }

            // Row 4: OPTIONS
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                border.color: "black"
                border.width: 3
                color: "transparent"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 5

                    Label {
                        text: "OPTIONS"
                        Layout.alignment: Qt.AlignHCenter
                        font.pixelSize: 14
                        font.bold: true
                    }

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

            Button {
                text: logVisible ? "HIDE" : "SHOW"
                Layout.alignment: Qt.AlignLeft

                background: Rectangle {
                    border.color: "black"
                    border.width: 2
                    color: parent.pressed ? "#e0e0e0" : "transparent"
                }
                contentItem: Text {
                    text: parent.text
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: logVisible = !logVisible
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "black"
                border.color: "black"
                border.width: 3
                visible: logVisible

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 5

                    TextArea {
                        id: logOutput
                        readOnly: true
                        color: "white"
                        font.family: "monospace"
                        wrapMode: TextArea.Wrap
                        background: Rectangle { color: "transparent" }
                    }
                }
            }

            Item {
                Layout.fillHeight: true
                visible: !logVisible
            }
        }
    }
}
