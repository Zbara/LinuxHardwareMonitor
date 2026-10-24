import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HwMonitor

// Stacked horizontal bar with a legend (memory composition, disk usage).
// segments: [{ label, value (text), fraction (0..1 of the bar), color }]
Rectangle {
    id: card
    property string title
    property string totalText
    property var segments: []
    property color accent: Theme.accent

    implicitHeight: body.implicitHeight + 28
    radius: Theme.radius
    color: Theme.surface
    border.color: Theme.border

    ColumnLayout {
        id: body
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: card.title
                color: Theme.text
                font.pixelSize: 14
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
            Text { text: card.totalText; color: Theme.textMuted; font.pixelSize: 12 }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            radius: 4
            color: Theme.surfaceAlt
            clip: true
            Row {
                anchors.fill: parent
                Repeater {
                    model: card.segments.length
                    delegate: Rectangle {
                        required property int index
                        width: Math.max(0, parent.width * Math.min(1, card.segments[index].fraction))
                        height: parent.height
                        color: card.segments[index].color
                    }
                }
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 36
            Repeater {
                model: card.segments.length
                delegate: Row {
                    required property int index
                    readonly property var seg: card.segments[index]
                    spacing: 8
                    Rectangle {
                        width: 12; height: 12; radius: 6
                        anchors.verticalCenter: parent.verticalCenter
                        color: seg.color
                    }
                    Column {
                        spacing: 1
                        Text { text: seg.label; color: Theme.textMuted; font.pixelSize: 12 }
                        Text { text: seg.value; color: Theme.text; font.pixelSize: 13; font.weight: Font.Medium }
                    }
                }
            }
        }
    }
}
