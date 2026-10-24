import QtQuick
import QtQuick.Controls
import HwMonitor

// Sidebar entry: glyph + label, accent bar on the left when selected.
Item {
    id: item
    property string text
    property string glyph
    property bool selected: false
    signal clicked

    implicitHeight: 40

    Rectangle {
        anchors.fill: parent
        radius: 6
        color: item.selected ? Theme.selection : hover.hovered ? Theme.surfaceAlt : "transparent"
    }
    Rectangle {
        visible: item.selected
        x: 0
        anchors.verticalCenter: parent.verticalCenter
        width: 3
        height: 18
        radius: 2
        color: Theme.accent
    }
    Row {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 14
        spacing: 12
        Text {
            width: 18
            horizontalAlignment: Text.AlignHCenter
            text: item.glyph
            color: item.selected ? Theme.accent : Theme.textMuted
            font.pixelSize: 16
        }
        Text {
            text: item.text
            color: item.selected ? Theme.text : Theme.textMuted
            font.pixelSize: 14
        }
    }
    HoverHandler { id: hover }
    TapHandler { onTapped: item.clicked() }
}
