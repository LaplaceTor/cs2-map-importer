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
        function onUpdateAvailable(version, notes, url) {
            updateDialog.version = version;
            updateDialog.notes = notes;
            updateDialog.url = url;
            updateDialog.open();
        }
        function onNoUpdateAvailable() {
            messageDialog.title = "Update Check"
            messageDialog.text = "You are already using the latest version."
            messageDialog.open()
        }
    }

    // Dialogs
    Dialog {
        id: updateDialog
        title: "Update Available"
        modal: true
        anchors.centerIn: parent
        width: 400
        height: 300

        property string version: ""
        property string notes: ""
        property string url: ""

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 10

            Label {
                text: "A new version (v" + updateDialog.version + ") is available!"
                font.bold: true
                Layout.fillWidth: true
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                TextArea {
                    text: updateDialog.notes
                    readOnly: true
                    wrapMode: TextArea.Wrap
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignRight

                Button {
                    text: "Download Page"
                    onClicked: {
                        Qt.openUrlExternally(updateDialog.url)
                        updateDialog.close()
                    }
                }
                Button {
                    text: "Close"
                    onClicked: updateDialog.close()
                }
            }
        }
    }

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

    FileDialog {
        id: materialFilesDialog
        title: "Select Material Files"
        nameFilters: ["Material files (*.vmt)"]
        fileMode: FileDialog.OpenFiles
        onAccepted: {
            backend.AddMaterialList(selectedFiles)
        }
    }

    FolderDialog {
        id: materialFolderDialog
        title: "Select Material Folder"
        onAccepted: {
            backend.AddMaterialFolder(selectedFolder)
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
            spacing: 10

            TabBar {
                id: mainTabBar
                Layout.fillWidth: true
                TabButton {
                    text: "Map"
                }
                TabButton {
                    text: "Material"
                }
            }

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

            StackLayout {
                id: mainStackLayout
                currentIndex: mainTabBar.currentIndex
                Layout.fillWidth: true
                Layout.fillHeight: true

                // --- Map Tab ---
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 15

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
                    // Map Tab End here, closing layout tag later.
                    Item { Layout.fillHeight: true } // Spacer
                } // Map Tab End

                // --- Material Tab ---
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 15

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 15

                        Label {
                            text: "SELECT CS2 ADDON:"
                            Layout.alignment: Qt.AlignVCenter
                        }

                        ComboBox {
                            id: materialAddonCombo
                            Layout.fillWidth: true
                            model: backend.cs2Addons
                            onActivated: {
                                backend.materialAddon = currentText
                            }
                            Component.onCompleted: {
                                currentIndex = indexOfValue(backend.materialAddon)
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#222"
                        border.color: "#555"
                        border.width: 1

                        Label {
                            anchors.fill: parent
                            anchors.margins: 10
                            text: "How to use this material import tool:\nFirst, move all the materials into materials folder inside game folder, or you're sure it's from game source files;\nAfter that, you can use the buttons below to select vmt files or folders to add to the list, or type into the input box underside which is inside game source files;\nFinally, select the cs2 addon you want to add these materials for, and then press START to got them."
                            wrapMode: Text.WordWrap
                            color: "#999"
                            visible: backend.materialList.length === 0
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        ScrollView {
                            anchors.fill: parent
                            clip: true
                            visible: backend.materialList.length > 0

                            ListView {
                                anchors.fill: parent
                                model: backend.materialList
                                delegate: RowLayout {
                                    width: ListView.view.width
                                    height: 30
                                    spacing: 5
                                    Label {
                                        Layout.fillWidth: true
                                        Layout.leftMargin: 5
                                        text: modelData
                                        color: "white"
                                        elide: Text.ElideMiddle
                                    }
                                    Button {
                                        text: "X"
                                        Layout.preferredWidth: 30
                                        Layout.preferredHeight: 30
                                        onClicked: backend.RemoveMaterial(index)
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        TextField {
                            id: materialInput
                            Layout.fillWidth: true
                            placeholderText: "e.g. materials/kz_communityjump/pavement_01.vmt"
                            onAccepted: {
                                if (text !== "") {
                                    backend.AddMaterial(text)
                                    text = ""
                                }
                            }
                        }

                        Button {
                            text: "Files"
                            onClicked: materialFilesDialog.open()
                        }

                        Button {
                            text: "Folder"
                            onClicked: materialFolderDialog.open()
                        }
                    }
                } // Material Tab End
            }

            // Row 3: START and STOP Buttons
            RowLayout {
                Layout.fillWidth: true
                spacing: 15

                Button {
                    id: goButton
                    text: "START"
                    enabled: mainTabBar.currentIndex === 0 ? (backend.canGo && !backend.isGoing) : (backend.materialList.length > 0 && backend.materialAddon !== "" && !backend.isGoing)
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
                        if (mainTabBar.currentIndex === 0) {
                            backend.Start()
                        } else {
                            backend.StartMaterialImport()
                        }
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



            // Row 5: UPDATE
            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                spacing: 10

                Button {
                    text: "Check Update"
                    Layout.alignment: Qt.AlignVCenter
                    onClicked: backend.CheckForUpdate()
                }

                Label {
                    text: "Current Version: " + backend.currentVersion
                    Layout.alignment: Qt.AlignVCenter
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
