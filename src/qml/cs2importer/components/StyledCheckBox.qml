import QtQuick
import QtQuick.Controls

CheckBox {
    id: control

    padding: 2
    topPadding: 1
    bottomPadding: 1
    leftPadding: 0
    rightPadding: 0

    implicitWidth: Math.max(implicitIndicatorWidth + implicitContentWidth + leftPadding + rightPadding, implicitBackgroundWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitIndicatorHeight, implicitContentHeight) + topPadding + bottomPadding

    indicator: Rectangle {
        implicitWidth: 16
        implicitHeight: 16
        x: control.leftPadding
        y: control.topPadding + (control.contentItem.lineCount > 1 ? 2 : Math.round((control.availableHeight - height) / 2))
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
        leftPadding: control.indicator ? (control.indicator.width + 6) : 0
        width: control.availableWidth
        text: control.text
        font: control.font
        opacity: control.enabled ? 1.0 : 0.4
        color: control.palette.text
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
        visible: control.text.length > 0
    }
}
