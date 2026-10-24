import QtQuick
import QtQuick.Controls
import HwMonitor

// Row of rounded tab buttons.
Row {
    id: tabs
    property var titles: []
    property int current: 0
    property color accent: Theme.accent
    signal selected(int index)
    spacing: 8

    Repeater {
        model: tabs.titles.length
        delegate: Rectangle {
            required property int index
            readonly property bool active: tabs.current === index
            width: label.implicitWidth + 28
            height: 32
            radius: 16
            color: active ? Qt.rgba(tabs.accent.r, tabs.accent.g, tabs.accent.b, 0.14) : "transparent"
            border.color: active ? tabs.accent : Theme.border
            Text {
                id: label
                anchors.centerIn: parent
                text: tabs.titles[parent.index]
                color: parent.active ? Theme.text : Theme.textMuted
                font.pixelSize: 12
            }
            TapHandler { onTapped: tabs.selected(parent.index) }
        }
    }
}
