import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HwMonitor

// Titled card with a Y axis, a 60 second chart and a time axis.
Rectangle {
    id: card
    property string title
    property string sensorId
    property string secondSensorId
    property color accent: Theme.accent
    property real fixedMax: 0       // > 0 fixes the Y axis at 0..fixedMax
    property string unit: "%"
    property var legend: []
    property alias chart: chart
    property string valueText            // current value shown at the top right
    property string footerText           // e.g. "Peak: 11.9 MB/s", shown between the time labels
    property real titleSize: 14

    implicitHeight: 230
    radius: Theme.radius
    color: Theme.surface
    border.color: Theme.border

    function axisLabel(v) {
        if (unit === "%") return Math.round(v) + "%"
        if (unit === "MB/s") return Theme.formatRate(v)
        if (unit === "MHz") return v >= 1000 ? (v / 1000).toFixed(1) + " GHz" : Math.round(v) + " MHz"
        if (unit === "°C") return Theme.tempText(v, 0)
        if (unit === "GB") return v.toFixed(v < 10 ? 1 : 0) + " " + Theme.unitGB
        return Theme.formatRateScale(v, unit)
    }
    readonly property real axisTop: fixedMax > 0 ? fixedMax : chart.scaleMax
    readonly property real axisBottom: fixedMax > 0 ? 0 : chart.scaleMin

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: card.title
                color: Theme.text
                font.pixelSize: card.titleSize
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Text {
                visible: card.valueText !== ""
                text: card.valueText
                color: Theme.text
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }
            Row {
                spacing: 12
                visible: card.legend.length > 0
                Repeater {
                    model: card.legend
                    delegate: Row {
                        required property int index
                        required property string modelData
                        spacing: 5
                        Rectangle {
                            width: 10; height: 10; radius: 2
                            anchors.verticalCenter: parent.verticalCenter
                            color: index === 0 ? card.accent : Qt.lighter(card.accent, 1.6)
                        }
                        Text { text: modelData; color: Theme.textMuted; font.pixelSize: 12 }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            // Y axis labels: top .. bottom
            Item {
                Layout.preferredWidth: 58
                Layout.fillHeight: true
                Repeater {
                    model: 5
                    delegate: Text {
                        required property int index
                        anchors.right: parent.right
                        y: Math.round((parent.height - height) * index / 4)
                        text: card.axisLabel(card.axisTop - (card.axisTop - card.axisBottom) * index / 4)
                        color: Theme.textMuted
                        font.pixelSize: 11
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Theme.background
                radius: 4
                border.color: Theme.border

                TemperatureChart {
                    id: chart
                    anchors.fill: parent
                    anchors.margins: 1
                    sensorId: card.sensorId
                    secondSensorId: card.secondSensorId
                    lineColor: card.accent
                    secondColor: Qt.lighter(card.accent, 1.6)
                    gridColor: Qt.rgba(card.accent.r, card.accent.g, card.accent.b, 0.16)
                    grid: true
                    filled: true
                    fixedSpan: 60
                    fixedMin: 0
                    fixedMax: card.fixedMax
                    minSpan: card.unit === "MB/s" ? 0.5 : (card.unit === "°C" || card.unit === "dBm") ? 10 : card.unit === "MHz" ? 200 : card.unit === "W" ? 5 : 5
                    floorZero: card.unit !== "°C" && card.unit !== "dBm"
                    zeroBased: card.unit === "%" || card.unit === "MB/s" || card.unit === "GB" || card.unit === "MHz" || card.unit === "W"
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 66
            Text { text: qsTr("60 seconds"); color: Theme.textMuted; font.pixelSize: 11 }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                text: card.footerText
                color: Theme.textMuted
                font.pixelSize: 11
            }
            Text { text: "0"; color: Theme.textMuted; font.pixelSize: 11 }
        }
    }
}
