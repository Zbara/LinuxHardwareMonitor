import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HwMonitor

// Titled card of label / value rows. In `bars` mode every row also draws a progress bar
// (rows: { label, fraction 0..1, value }).
Rectangle {
    id: card
    property string title
    property var rows: []
    property bool bars: false
    property color accent: Theme.accent
    property int labelWidth: 170

    implicitHeight: body.implicitHeight + 28
    radius: Theme.radius
    color: Theme.surface
    border.color: Theme.border

    ColumnLayout {
        id: body
        anchors.fill: parent
        anchors.margins: 14
        spacing: 2

        Text {
            text: card.title
            color: Theme.text
            font.pixelSize: 14
            font.weight: Font.DemiBold
            Layout.bottomMargin: 8
        }

        Repeater {
            model: card.rows.length
            delegate: RowLayout {
                id: row
                required property int index
                readonly property var info: card.rows[index]
                Layout.fillWidth: true
                Layout.preferredHeight: 26
                spacing: 8

                Text {
                    text: row.info.label + (card.bars ? "" : ":")
                    color: Theme.textMuted
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    Layout.preferredWidth: card.labelWidth
                }
                Rectangle {
                    visible: card.bars
                    Layout.fillWidth: true
                    Layout.preferredHeight: 8
                    radius: 4
                    color: Theme.surfaceAlt
                    Rectangle {
                        width: parent.width * Math.max(0, Math.min(1, row.info.fraction >= 0 ? row.info.fraction : 0))
                        height: parent.height
                        radius: 4
                        color: card.accent
                    }
                }
                Text {
                    text: row.info.value
                    color: Theme.text
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    horizontalAlignment: card.bars ? Text.AlignRight : Text.AlignLeft
                    Layout.fillWidth: !card.bars
                    Layout.preferredWidth: card.bars ? 44 : -1
                }
            }
        }
    }
}
