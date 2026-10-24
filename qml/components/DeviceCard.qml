import QtQuick
import QtQuick.Layouts
import HwMonitor

// Overview card for one hardware device: type icon, name, sensor availability, hottest
// temperature and its live chart.
Rectangle {
    id: card
    property var info

    // DeviceCategory: 0 CPU, 1 GPU, 2 storage, 3 system, 4 other
    function glyph() {
        const n = (info.deviceName || "").toLowerCase()
        if (n.indexOf("battery") >= 0) return "▮"
        if (n.indexOf("wi-fi") >= 0 || n.indexOf("wifi") >= 0) return "◉"
        if (n.indexOf("network") >= 0) return "⇄"
        if (n.indexOf("disks") >= 0) return "▭"
        if (n.indexOf("acpi") >= 0 || n.indexOf("laptop") >= 0) return "◈"
        return ["▣", "▦", "▭", "▤", "◈"][info.category] || "◈"
    }
    readonly property color accent: info.category === 0 ? Theme.cpuColor
                                  : info.category === 1 ? Theme.gpuColor
                                  : info.category === 2 ? Theme.diskColor : Theme.textMuted

    radius: Theme.radius
    color: Theme.surface
    border.color: Theme.border
    implicitHeight: 118

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Rectangle {
                Layout.alignment: Qt.AlignTop
                Layout.preferredWidth: 38
                Layout.preferredHeight: 38
                radius: 8
                color: Qt.rgba(card.accent.r, card.accent.g, card.accent.b, 0.14)
                border.color: Qt.rgba(card.accent.r, card.accent.g, card.accent.b, 0.4)
                Text { anchors.centerIn: parent; text: card.glyph(); color: card.accent; font.pixelSize: 18 }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: card.info.deviceName
                    color: Theme.text
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Text {
                    text: qsTr("%1 of %2 sensors available").arg(card.info.availableCount).arg(card.info.totalCount)
                    color: Theme.textMuted
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 50
            spacing: 8
            Text {
                text: card.info.available ? Theme.format(card.info.temperature, "°C") : Theme.na
                color: card.info.available ? Theme.tempColor(card.info.temperature) : Theme.textMuted
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }
            TemperatureChart {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: card.info.sensorId !== ""
                sensorId: card.info.sensorId
                lineColor: Theme.tempColor(card.info.temperature)
                filled: true
            }
            Item { Layout.fillWidth: card.info.sensorId === ""; visible: card.info.sensorId === "" }
        }
    }
}
