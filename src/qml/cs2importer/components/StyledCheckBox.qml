import QtQuick
import QtQuick.Controls

CheckBox {
    id: control

    indicator: Rectangle {
        implicitWidth: 16
        implicitHeight: 16
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: 3
        color: "#FFFFFF"
        border.color: control.down ? "#333333" : (control.hovered ? "#555555" : "#767676")
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: "✓"
            font.pixelSize: 13
            font.bold: true
            color: "#000000"
            visible: control.checked
        }
    }

    contentItem: Text {
        leftPadding: control.indicator ? (control.indicator.width + control.spacing) : 0
        text: control.text
        font: control.font
        opacity: enabled ? 1.0 : 0.4
        color: control.palette.text
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        visible: control.text.length > 0
    }
}
