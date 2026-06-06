import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import cs2importer 1.0 // Ensure our C++ module is imported if needed, but we might pass context property

ApplicationWindow {
    id: window
    width: 600
    height: 700
    visible: true
    title: "CS2 Importer"
    Material.theme: Material.Dark

    // Property to bind the backend object
    property var backend: backendObject

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
        id: vmfFileDialog
        title: "Select VMF"
        nameFilters: ["VMF files (*.vmf)"]
        onAccepted: backend.select_vmf_dialog(selectedFile)
    }

    FileDialog {
        id: bspFileDialog
        title: "Select BSP"
        nameFilters: ["BSP files (*.bsp)"]
        onAccepted: backend.select_bsp_dialog(selectedFile)
    }

    MessageDialog {
        id: messageDialog
        title: "Message"
        buttons: MessageDialog.Ok
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 10
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 15

            // CS2 selection row
            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: "Select CS2 folder"
                    ToolTip.text: "Use \"Counter-Strike Global Offensive\" folder or any folder inside it."
                    ToolTip.visible: hovered
                    onClicked: cs2FolderDialog.open()
                }
                Label {
                    text: backend.cs2_basefolder === "" ? "Not selected" : "Selected"
                    color: backend.cs2_basefolder === "" ? "red" : "lime"
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        color: "transparent"
                        border.color: parent.color
                        border.width: 1
                    }
                }
                Button {
                    text: "Validate CS2"
                    onClicked: backend.validate_cs2()
                }
            }

            // S1 selection row
            RowLayout {
                Layout.fillWidth: true
                ComboBox {
                    id: s1GameCombo
                    model: ["CSGO", "CSS"]
                    currentIndex: backend.s1_game_type === "css" ? 1 : 0
                    onActivated: backend.set_s1_game_type(currentText.toLowerCase())
                }
                Button {
                    text: "Select Source 1 game folder"
                    ToolTip.text: "Use \"csgo legacy\" folder or any folder inside it."
                    ToolTip.visible: hovered
                    onClicked: s1FolderDialog.open()
                }
                Label {
                    text: backend.s1game_basefolder === "" ? "Not selected" : "Selected"
                    color: backend.s1game_basefolder === "" ? "red" : "lime"
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        color: "transparent"
                        border.color: parent.color
                        border.width: 1
                    }
                }
                Button {
                    text: "Validate Source 1 Game"
                    onClicked: backend.validate_s1()
                }
            }

            // Map selection row
            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: "Select VMF"
                    ToolTip.text: "Does not need to be in a \"maps\" folder, one will be created then deleted afterwards if necessary."
                    ToolTip.visible: hovered
                    onClicked: {
                        vmfFileDialog.currentFolder = backend.vmf_default_path_url
                        vmfFileDialog.open()
                    }
                }
                Button {
                    text: "Select BSP"
                    onClicked: {
                        bspFileDialog.currentFolder = backend.vmf_default_path_url
                        bspFileDialog.open()
                    }
                }
                Label {
                    text: (backend.bsp_file === "" && backend.content_folder === "") ? "Not selected" : "Selected"
                    color: (backend.bsp_file === "" && backend.content_folder === "") ? "red" : "lime"
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        color: "transparent"
                        border.color: parent.color
                        border.width: 1
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Material.dividerColor
            }

            // Addon edit
            TextField {
                id: addonEdit
                Layout.fillWidth: true
                placeholderText: "Enter addon name:"
                text: backend.addon_name
                onTextChanged: backend.addon_name = text
            }

            // Checkboxes and GO button
            RowLayout {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    CheckBox {
                        id: useBspCheckbox
                        text: "clean unecessary faces in source 2 way"
                        checked: backend.usebsp
                        onCheckedChanged: backend.usebsp = checked
                        ToolTip.text: "This runs the map through a special vbsp process to generate clean map geometry from brushes..."
                        ToolTip.visible: hovered
                    }
                    CheckBox {
                        id: nomergeInstancesCheckbox
                        text: "keep instances"
                        checked: backend.usebsp_nomergeinstances
                        enabled: useBspCheckbox.checked
                        onCheckedChanged: backend.usebsp_nomergeinstances = checked
                        ToolTip.text: "Use this instead of -usebsp if you wish to both generate clean geo and also preserve func_instances..."
                        ToolTip.visible: hovered
                    }
                    CheckBox {
                        id: skipDepsCheckbox
                        text: "Skip references import"
                        checked: backend.skipdeps
                        onCheckedChanged: backend.skipdeps = checked
                        ToolTip.text: "Optional: skips importing all dependencies/content and only generates the vmap file(s)..."
                        ToolTip.visible: hovered
                    }
                }

                Button {
                    id: goButton
                    text: "GO!"
                    enabled: backend.can_go
                    Layout.alignment: Qt.AlignVCenter
                    Material.background: Material.Green
                    Material.foreground: "white"
                    font.bold: true
                    onClicked: backend.go()
                }
            }

            // Log Output
            TextArea {
                id: logOutput
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 300
                readOnly: true
                background: Rectangle {
                    color: "black"
                    border.color: Material.dividerColor
                }
                color: "white"
                font.family: "monospace"
                wrapMode: TextArea.Wrap
            }
        }
    }
}
