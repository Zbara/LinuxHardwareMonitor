import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HwMonitor

// Metric tile: round icon, label and big value on top, a sparkline or a bar across the bottom.
Rectangle {
    id: tile
    property string label
    property string value
    property string icon: "◈"
    property color accent: Theme.accent
    property string sparkId: ""      // live mini chart under the value
    property real bar: -1            // 0..1 progress bar under the value (-1: none)
    property real sparkFixedMax: 0

    implicitHeight: 86
    radius: Theme.radius
    color: Theme.surface
    border.color: Theme.border

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                Layout.alignment: Qt.AlignTop
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                radius: 15
                color: Qt.rgba(tile.accent.r, tile.accent.g, tile.accent.b, 0.16)
                border.color: Qt.rgba(tile.accent.r, tile.accent.g, tile.accent.b, 0.5)
                Text {
                    anchors.centerIn: parent
                    text: tile.icon
                    color: tile.accent
                    font.pixelSize: 15
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    text: tile.label
                    color: Theme.textMuted
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Text {
                    text: tile.value
                    color: Theme.text
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            visible: tile.sparkId !== "" || tile.bar >= 0

            TemperatureChart {
                anchors.fill: parent
                visible: tile.sparkId !== ""
                sensorId: tile.sparkId
                lineColor: tile.accent
                fixedMax: tile.sparkFixedMax
                floorZero: true
                minSpan: 5
                filled: true
            }
            Rectangle {
                visible: tile.bar >= 0
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 2
                width: parent.width
                height: 6
                radius: 3
                color: Theme.surfaceAlt
                Rectangle {
                    width: parent.width * Math.max(0, Math.min(1, tile.bar))
                    height: parent.height
                    radius: 3
                    color: tile.accent
                }
            }
        }
    }
}
