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

    Component.onCompleted: {
        backend.AutoCheckForUpdate()
    }

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
        id: mdlFileDialog
        title: "Select MDL File"
        nameFilters: ["Model files (*.mdl)"]
        onAccepted: {
            backend.SelectMdlDialog(selectedFile)
        }
    }

    FileDialog {
        id: pcfFileDialog
        title: "Select PCF File"
        nameFilters: ["Particle files (*.pcf)"]
        onAccepted: {
            backend.SelectPcfDialog(selectedFile)
        }
    }

    FileDialog {
        id: imageFileDialog
        title: "Select Image File"
        nameFilters: ["Image files (*.tga *.png *.jpg *.jpeg *.tif *.tiff *.vtf)"]
        onAccepted: {
            backend.SelectImageDialog(selectedFile)
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

            TabBar {
                id: tabBar
                Layout.fillWidth: true
                enabled: !backend.isGoing
                currentIndex: backend.activeTab
                onCurrentIndexChanged: {
                    backend.activeTab = currentIndex
                }

                TabButton {
                    text: "Map"
                }
                TabButton {
                    text: "Material"
                }
                TabButton {
                    text: "Model"
                }
                TabButton {
                    text: "Particle"
                }
            }

            // Row 1: Folders
            RowLayout {
                Layout.fillWidth: true
                spacing: 15

                // Source 1 Game Box
                GroupBox {
                    visible: backend.activeTab !== 1
                    title: "Source 1 Game"
                    Layout.preferredWidth: 165
                    Layout.preferredHeight: 180

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10

                        ComboBox {
                            id: s1GameCombo
                            enabled: !backend.isGoing
                            model: ["CSGO", "CSS", "HL2", "L4D", "L4D2", "Portal", "Portal2", "TF2", "GMod", "BlackMesa"]
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
                                else if (type === "blackmesa") currentIndex = 9;
                                else currentIndex = 0;
                            }
                            onActivated: backend.SetS1GameType(currentText.toLowerCase())
                        }

                        Button {
                            id: s1FolderButton
                            enabled: !backend.isGoing
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
                            enabled: !backend.isGoing
                            text: "Validate Game File"
                            Layout.fillWidth: true
                            onClicked: backend.ValidateS1()
                        }
                    }
                }

                // Picture File Preview Box
                GroupBox {
                    visible: backend.activeTab === 1
                    title: "Picture File Preview"
                    Layout.preferredWidth: 165
                    Layout.preferredHeight: 180

                    ColumnLayout {
                        anchors.fill: parent
                        Image {
                            id: previewImage
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            fillMode: Image.PreserveAspectFit
                            source: backend.materialPreviewUrl !== "" ? backend.materialPreviewUrl : ""
                            cache: false
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
                            enabled: !backend.isGoing
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
                            enabled: !backend.isGoing
                            text: "Validate Game File"
                            Layout.fillWidth: true
                            onClicked: backend.ValidateCs2()
                        }
                    }
                }
            }


            StackLayout {
                id: stackLayout
                Layout.fillWidth: true
                Layout.fillHeight: true
                enabled: !backend.isGoing
                currentIndex: tabBar.currentIndex

                // Item 0: Map Tab Layout
                ColumnLayout {
                    spacing: 20
                    Layout.fillWidth: true
                    Layout.fillHeight: true

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

                    // Row 4: OPTIONS (Map)
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
                                text: "Keep func_instance as its own part"
                                checked: backend.usebspNomergeinstances
                                enabled: useBspCheckbox.checked
                                onCheckedChanged: backend.usebspNomergeinstances = checked
                                ToolTip.text: "if you wish to both generate clean geo and also preserve func_instances got merge in"
                                ToolTip.visible: hovered
                            }
                            CheckBox {
                                id: keepFuncDetailAsBrushCheckbox
                                text: "Keep func_detail as brush"
                                checked: backend.keepFuncDetailAsBrush
                                onCheckedChanged: backend.keepFuncDetailAsBrush = checked
                                ToolTip.text: "Optional: preserves func_detail entities by converting them to func_brush instead of letting them merge with world geometry"
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

                // Item 1: Material Tab Layout
                ColumnLayout {
                    spacing: 20
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    // Row 2: Material selection and Addon dropdown
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 15

                        Button {
                            id: selectImageButton
                            Layout.preferredWidth: 165
                            Layout.preferredHeight: 40
                            text: backend.materialFile === "" ? "SELECT IMAGE" : backend.materialFile.substring(backend.materialFile.lastIndexOf('/') + 1)
                            contentItem: Text {
                                text: parent.text
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideMiddle
                            }
                            onClicked: {
                                imageFileDialog.open()
                            }
                        }

                        Label {
                            text: "➡"
                            font.pixelSize: 40
                            Layout.alignment: Qt.AlignVCenter
                        }

                        ComboBox {
                            id: materialAddonCombo
                            Layout.preferredWidth: 165
                            Layout.preferredHeight: 40
                            model: backend.cs2AddonsList
                            currentIndex: Math.max(0, model.indexOf(backend.selectedMdlAddon))
                            onActivated: backend.selectedMdlAddon = currentText
                        }
                    }

                    // Row 4: OPTIONS (Material)
                    GroupBox {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        title: "OPTIONS"

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 10

                            TabBar {
                                id: materialOptionsTabBar
                                Layout.fillWidth: true
                                currentIndex: 0

                                TabButton { text: "General" }
                                TabButton { text: "NormalMap" }
                                TabButton { text: "HeightMap" }
                                TabButton { text: "MetalnessMap" }
                                TabButton { text: "RoughnessMap" }
                            }

                            StackLayout {
                                id: materialOptionsStack
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                currentIndex: materialOptionsTabBar.currentIndex

                                // General Tab
                                ColumnLayout {
                                    Label {
                                        text: "COMING SOON"
                                        font.bold: true
                                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                    }
                                }
                                // NormalMap Tab
                                ColumnLayout {
                                    Label {
                                        text: "COMING SOON"
                                        font.bold: true
                                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                    }
                                }
                                // HeightMap Tab
                                ColumnLayout {
                                    Label {
                                        text: "COMING SOON"
                                        font.bold: true
                                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                    }
                                }
                                // MetalnessMap Tab
                                ColumnLayout {
                                    Label {
                                        text: "COMING SOON"
                                        font.bold: true
                                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                    }
                                }
                                // RoughnessMap Tab
                                ColumnLayout {
                                    Label {
                                        text: "COMING SOON"
                                        font.bold: true
                                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                    }
                                }
                            }
                        }
                    }
                }

                // Item 2: Model Tab Layout
                ColumnLayout {
                    spacing: 20
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    // Row 2: Model selection and Addon dropdown (Model)
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 15

                        Button {
                            Layout.preferredWidth: 165
                            Layout.preferredHeight: 40
                            text: backend.mdlFile === "" ? "SELECT MDL" : backend.mdlFile.substring(backend.mdlFile.lastIndexOf('/') + 1)
                            contentItem: Text {
                                text: parent.text
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideMiddle
                            }
                            onClicked: {
                                mdlFileDialog.open()
                            }
                        }

                        Label {
                            text: "➡"
                            font.pixelSize: 40
                            Layout.alignment: Qt.AlignVCenter
                        }

                        ComboBox {
                            id: addonCombo
                            Layout.preferredWidth: 165
                            Layout.preferredHeight: 40
                            model: backend.cs2AddonsList
                            currentIndex: Math.max(0, model.indexOf(backend.selectedMdlAddon))
                            onActivated: backend.selectedMdlAddon = currentText
                        }
                    }

                    // Row 4: OPTIONS (Model)
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
                                text: "Skip animation import"
                                checked: backend.modelSkipAnimation
                                onCheckedChanged: backend.modelSkipAnimation = checked
                                ToolTip.text: "Skip writing dmx files (anims) (-skipcommondmxwrite)"
                                ToolTip.visible: hovered
                            }

                            CheckBox {
                                text: "Change bindpose from Yup to Zup"
                                checked: backend.modelChangeBindpose
                                onCheckedChanged: backend.modelChangeBindpose = checked
                                ToolTip.text: "Change bindpose from Yup to Zup (-YupToZup)"
                                ToolTip.visible: hovered
                            }

                            CheckBox {
                                text: "Override \"lean\" sequence"
                                checked: backend.modelOverrideLean
                                onCheckedChanged: backend.modelOverrideLean = checked
                                ToolTip.text: "Override \"lean\" sequence (-overridelean)"
                                ToolTip.visible: hovered
                            }

                            CheckBox {
                                text: "Import mdl hull bounds from the studiohdr"
                                checked: backend.modelHeaderHullBounds
                                onCheckedChanged: backend.modelHeaderHullBounds = checked
                                ToolTip.text: "Import mdl hull bounds from the studiohdr (-header_hull_bounds)"
                                ToolTip.visible: hovered
                            }

                            CheckBox {
                                text: "Import all lods"
                                checked: backend.modelImportLods
                                onCheckedChanged: backend.modelImportLods = checked
                                ToolTip.text: "Import all lods (-lods)"
                                ToolTip.visible: hovered
                            }

                            CheckBox {
                                text: "Write weapons sequences & weighlists into a prefab"
                                checked: backend.modelWriteWeaponPrefab
                                onCheckedChanged: backend.modelWriteWeaponPrefab = checked
                                ToolTip.text: "Write sequences & weighlists into a prefab (-write_weapon_anim_prefab)"
                                ToolTip.visible: hovered
                            }

                            Item { Layout.fillHeight: true } // Spacer
                        }
                    }
                }

                // Item 2: Particle Tab Layout
                ColumnLayout {
                    spacing: 20
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    // Row 2: Particle selection and Addon dropdown (Particle)
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 15

                        Button {
                            Layout.preferredWidth: 165
                            Layout.preferredHeight: 40
                            text: backend.pcfFile === "" ? "SELECT PCF" : backend.pcfFile.substring(backend.pcfFile.lastIndexOf('/') + 1)
                            contentItem: Text {
                                text: parent.text
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideMiddle
                            }
                            onClicked: {
                                pcfFileDialog.open()
                            }
                        }

                        Label {
                            text: "➡"
                            font.pixelSize: 40
                            Layout.alignment: Qt.AlignVCenter
                        }

                        ComboBox {
                            id: particleAddonCombo
                            Layout.preferredWidth: 165
                            Layout.preferredHeight: 40
                            model: backend.cs2AddonsList
                            currentIndex: Math.max(0, model.indexOf(backend.selectedMdlAddon))
                            onActivated: backend.selectedMdlAddon = currentText
                        }
                    }

                    // Row 4: OPTIONS (Particle)
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
                                text: "Respect $DEPTHBLEND in particle materials"
                                checked: backend.particleAllowDepthBlend
                                onCheckedChanged: backend.particleAllowDepthBlend = checked
                                ToolTip.text: "Respect $DEPTHBLEND in particle materials (-particle_allow_depth_blend)"
                                ToolTip.visible: hovered
                            }

                            CheckBox {
                                text: "Disable diffuse lighting on imported particle systems"
                                checked: backend.particleDisableDiffuse
                                onCheckedChanged: backend.particleDisableDiffuse = checked
                                ToolTip.text: "Disable diffuse lighting on imported particle systems (-particle_disable_diffuse)"
                                ToolTip.visible: hovered
                            }

                            Item { Layout.fillHeight: true } // Spacer
                        }
                    }
                }
            }

            // Row 3: START and STOP Buttons / CREATE Button
            RowLayout {
                Layout.fillWidth: true
                spacing: 15

                // CREATE Button (visible only when Material tab is active)
                Button {
                    visible: backend.activeTab === 1
                    text: "CREATE"
                    enabled: false
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40

                    contentItem: Text {
                        text: parent.text
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.bold: true
                    }
                }

                Button {
                    visible: backend.activeTab !== 1
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
                    visible: backend.activeTab !== 1
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
                spacing: 10

                Label {
                    text: "Theme: "
                    Layout.alignment: Qt.AlignVCenter
                }

                Button {
                    id: themeButton
                    text: backend.theme === "system" ? "System" : (backend.theme === "light" ? "Light" : "Dark")
                    Layout.alignment: Qt.AlignVCenter
                    onClicked: {
                        if (backend.theme === "system") {
                            backend.theme = "light"
                        } else if (backend.theme === "light") {
                            backend.theme = "dark"
                        } else {
                            backend.theme = "system"
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    text: "Check Update"
                    enabled: !backend.isGoing
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
