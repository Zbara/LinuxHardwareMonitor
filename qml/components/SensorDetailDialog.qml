import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HwMonitor

// Large chart of one sensor over the whole history window with its statistics and a CSV export.
Dialog {
    id: dialog

    property string sensorId
    property string sensorName
    property string deviceName
    property string unit
    property var current            // latest value, undefined when the sensor is unavailable
    property var stats: ({})
    property string exportResult

    function show(id, name, device, sensorUnit, value) {
        sensorId = id
        sensorName = name
        deviceName = device
        unit = sensorUnit
        current = value
        exportResult = ""
        refresh()
        open()
    }
    function refresh() { stats = app.sensorStats(sensorId) }
    function minutes(seconds) {
        return seconds < 90 ? qsTr("%1 s").arg(Math.round(seconds)) : qsTr("%1 min").arg(Math.round(seconds / 60))
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(820, parent.width - 60)
    height: Math.min(560, parent.height - 60)
    modal: true
    standardButtons: Dialog.Close
    background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }
    header: ColumnLayout {
        spacing: 2
        Label {
            text: dialog.sensorName
            color: Theme.text
            font.pixelSize: 18
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            Layout.fillWidth: true
            Layout.topMargin: 16
            Layout.leftMargin: 18
            Layout.rightMargin: 18
        }
        Label {
            text: dialog.deviceName
            color: Theme.textMuted
            font.pixelSize: 12
            Layout.leftMargin: 18
        }
    }

    Connections {
        target: app
        enabled: dialog.visible
        function onSummariesChanged() { dialog.refresh() }
    }

    contentItem: ColumnLayout {
        spacing: 12

        GridLayout {
            Layout.fillWidth: true
            columns: 5
            columnSpacing: 10
            Repeater {
                model: [
                    { label: qsTr("Current"), value: dialog.current !== undefined && dialog.current !== null
                                                    ? Theme.format(dialog.current, dialog.unit) : Theme.na },
                    { label: qsTr("Min"), value: dialog.stats.samples ? Theme.format(dialog.stats.min, dialog.unit) : Theme.na },
                    { label: qsTr("Max"), value: dialog.stats.samples ? Theme.format(dialog.stats.max, dialog.unit) : Theme.na },
                    { label: qsTr("Average"), value: dialog.stats.samples ? Theme.format(dialog.stats.average, dialog.unit) : Theme.na },
                    { label: qsTr("Period"), value: dialog.stats.samples ? dialog.minutes(dialog.stats.spanSeconds) : Theme.na }
                ]
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 56
                    radius: 8
                    color: Theme.surfaceAlt
                    Column {
                        anchors.centerIn: parent
                        spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.label; color: Theme.textMuted; font.pixelSize: 11 }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.value; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8
            Item {
                Layout.preferredWidth: 70
                Layout.fillHeight: true
                Repeater {
                    model: 5
                    delegate: Text {
                        required property int index
                        anchors.right: parent.right
                        y: Math.round((parent.height - height) * index / 4)
                        text: Theme.format(chart.scaleMax - (chart.scaleMax - chart.scaleMin) * index / 4, dialog.unit)
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
                    sensorId: dialog.sensorId
                    lineColor: Theme.accent
                    gridColor: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
                    grid: true
                    filled: true
                    latestRunOnly: false
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                text: qsTr("Export CSV")
                onClicked: {
                    const path = app.exportSensor(dialog.sensorId)
                    dialog.exportResult = path !== "" ? qsTr("Saved to ") + path : qsTr("Export failed")
                }
            }
            Label {
                Layout.fillWidth: true
                text: dialog.exportResult
                color: Theme.textMuted
                font.pixelSize: 12
                elide: Text.ElideMiddle
            }
        }
    }
}
