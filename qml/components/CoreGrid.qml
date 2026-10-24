import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HwMonitor

// One small live chart per logical processor, labelled with its current load.
Rectangle {
    id: card
    property string title
    property int columns: 4
    property int cellHeight: 46
    property color accent: Theme.cpuColor

    implicitHeight: body.implicitHeight + 28
    radius: Theme.radius
    color: Theme.surface
    border.color: Theme.border

    readonly property var values: app.details.coreUtil !== undefined ? app.details.coreUtil : []

    ColumnLayout {
        id: body
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        Text {
            visible: card.title !== ""
            text: card.title
            color: Theme.text
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }

        GridLayout {
            Layout.fillWidth: true
            columns: card.columns
            columnSpacing: 8
            rowSpacing: 8
            Repeater {
                model: card.values.length
                delegate: Rectangle {
                    id: cell
                    required property int index
                    readonly property var value: card.values[index]
                    Layout.fillWidth: true
                    Layout.preferredHeight: card.cellHeight
                    color: Theme.background
                    border.color: Qt.rgba(card.accent.r, card.accent.g, card.accent.b, 0.55)
                    radius: 3
                    clip: true

                    TemperatureChart {
                        anchors.fill: parent
                        anchors.margins: 1
                        sensorId: "system/cpu" + cell.index + "_util"
                        lineColor: card.accent
                        fixedMin: 0
                        fixedMax: 100
                        fixedSpan: 60
                        filled: true
                    }
                    Column {
                        x: 6; y: 3
                        spacing: 0
                        Text { text: "CPU " + cell.index; color: Theme.text; font.pixelSize: 11; font.weight: Font.Medium }
                        Text {
                            text: cell.value !== undefined && cell.value !== null ? Math.round(cell.value) + "%" : Theme.na
                            color: Theme.textMuted
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }
    }
}
