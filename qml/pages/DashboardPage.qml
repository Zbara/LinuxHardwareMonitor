import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HwMonitor

ScrollView {
    id: page
    contentWidth: availableWidth
    clip: true

    // app.highlights order: 0 CPU °C, 1 GPU °C, 2 storage °C, 3 fan, 4 CPU %, 5 CPU MHz, 6 RAM %,
    // 7 GPU %, 8 GPU memory, 9 net down, 10 net up, 11 disk read, 12 disk write.
    readonly property var icons: ["▣", "▦", "▭", "✻", "▣", "◔", "▥", "▦", "▥", "↓", "↑", "↓", "↑"]
    readonly property var colors: [Theme.hot, Theme.hot, Theme.hot, Theme.accent, Theme.cpuColor, Theme.cpuColor,
                                   Theme.memoryColor, Theme.gpuColor, Theme.memoryColor, Theme.networkColor,
                                   Theme.networkColor, Theme.diskColor, Theme.diskColor]
    readonly property var firstRow: [0, 1, 2, 3, 4, 5]
    readonly property var secondRow: [7, 8, 9, 10, 11, 12, 6]

    function gpuMemoryTotal() {
        const gpus = app.details.gpus
        return gpus !== undefined && gpus.length > 0 && gpus[0].memTotal.available ? gpus[0].memTotal.value : 0
    }

    // One overview tile for highlights[i].
    component OverviewTile: StatTile {
        id: ot
        required property int slot
        readonly property var info: slot < app.highlights.length ? app.highlights[slot] : ({ available: false, value: NaN, unit: "", title: "", sensorId: "" })
        Layout.fillWidth: true
        Layout.preferredWidth: 150
        label: info.title
        value: !info.available ? Theme.na
               : slot === 8 && page.gpuMemoryTotal() > 0 ? Theme.gb(info.value, 1) + " / " + Theme.gb(page.gpuMemoryTotal(), 0)
               : Theme.format(info.value, info.unit)
        icon: page.icons[slot]
        accent: info.unit === "°C" && info.available ? Theme.tempColor(info.value) : page.colors[slot]
        // Capacity-style tiles get a bar, rate/temperature tiles a live chart.
        sparkId: slot === 6 || (slot === 8 && page.gpuMemoryTotal() > 0) ? "" : info.sensorId
        sparkFixedMax: info.unit === "%" ? 100 : 0
        bar: slot === 6 && info.available ? info.value / 100
             : slot === 8 && info.available && page.gpuMemoryTotal() > 0 ? info.value / page.gpuMemoryTotal() : -1
    }

    ColumnLayout {
        width: page.availableWidth
        spacing: 14

        PageTitle {
            text: qsTr("Overview")
            Layout.leftMargin: 20
            Layout.topMargin: 20
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            columns: Math.max(2, Math.min(page.firstRow.length, Math.floor((page.availableWidth - 40) / 190)))
            columnSpacing: 10
            rowSpacing: 10
            Repeater {
                model: page.firstRow
                delegate: OverviewTile {
                    required property int modelData
                    slot: modelData
                    implicitHeight: 124
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            columns: Math.max(2, Math.min(page.secondRow.length, Math.floor((page.availableWidth - 40) / 170)))
            columnSpacing: 10
            rowSpacing: 10
            Repeater {
                model: page.secondRow
                delegate: OverviewTile {
                    required property int modelData
                    slot: modelData
                    implicitHeight: 112
                }
            }
        }

        RowLayout {
            Layout.leftMargin: 20
            Layout.topMargin: 6
            visible: app.devices.length > 0
            Label {
                text: qsTr("Devices")
                color: Theme.text
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }
        }

        Label {
            Layout.leftMargin: 20
            visible: app.devices.length === 0
            text: qsTr("No sensors detected. Check that hwmon drivers are loaded and readable without root.")
            color: Theme.textMuted
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.bottomMargin: 20
            columns: Math.max(1, Math.floor((page.availableWidth - 40 + 10) / 340))
            columnSpacing: 10
            rowSpacing: 10

            Repeater {
                model: app.devices
                delegate: DeviceCard {
                    required property var modelData
                    Layout.fillWidth: true
                    info: modelData
                }
            }
        }
    }
}
