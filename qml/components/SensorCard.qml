import QtQuick
import HwMonitor
import QtQuick.Layouts

Rectangle {
    id: card
    property string title
    property string unit
    property bool sensorAvailable
    property var value
    property string sensorId
    property string subtitle
    property int availableCount
    property int totalCount
    property bool isTemperature: unit === "°C"

    radius: Theme.radius
    color: Theme.surface
    border.color: Theme.border
    implicitHeight: 150

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: card.title
                color: Theme.textMuted
                font.pixelSize: 13
                Layout.fillWidth: true
            }
            Rectangle {
                width: 8; height: 8; radius: 4
                color: card.sensorAvailable ? Theme.ok : Theme.textMuted
            }
            Text {
                text: card.totalCount > 0 ? card.availableCount + "/" + card.totalCount : "—"
                color: Theme.textMuted
                font.pixelSize: 11
            }
        }
        Text {
            text: card.sensorAvailable ? Theme.format(card.value, card.unit) : Theme.na
            color: card.sensorAvailable && card.isTemperature ? Theme.tempColor(card.value)
                                                              : card.sensorAvailable ? Theme.text : Theme.textMuted
            font.pixelSize: 28
            font.weight: Font.DemiBold
        }
        Text {
            text: card.sensorAvailable ? card.subtitle : qsTr("No sensor available")
            color: Theme.textMuted
            font.pixelSize: 11
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        TemperatureChart {
            Layout.fillWidth: true
            Layout.fillHeight: true
            sensorId: card.sensorId
            minSpan: card.unit === "MB/s" ? 0.5 : card.unit === "%" ? 10 : 5
            floorZero: card.unit === "MB/s" || card.unit === "%"
            lineColor: card.isTemperature ? Theme.tempColor(card.value) : Theme.accent
        }
    }
}
