import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HwMonitor

Item {
    id: page

    function focusSearch() {
        search.forceActiveFocus()
        search.selectAll()
    }

    // SensorType values: Temperature, FanSpeed, Voltage, Power, Utilization, Frequency (-1 = all)
    readonly property var typeFilters: [-1, 0, 1, 2, 3, 4, 5]

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            PageTitle { text: qsTr("Sensors") }
            Item { Layout.fillWidth: true }
            TextField {
                id: search
                Component.onCompleted: text = app.sensorFilter.filterText
                Layout.preferredWidth: 260
                placeholderText: qsTr("Search sensors")
                color: Theme.text
                placeholderTextColor: Theme.textMuted
                leftPadding: 12
                background: Rectangle {
                    radius: 6
                    color: Theme.surface
                    border.color: search.activeFocus ? Theme.accent : Theme.border
                }
                onTextChanged: app.sensorFilter.filterText = text
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 4
            columnSpacing: 10
            Repeater {
                model: [
                    { label: qsTr("Sensors"), value: app.sensorCount, icon: "∿", color: Theme.accent },
                    { label: qsTr("Available"), value: app.sensorsAvailable, icon: "✓", color: Theme.ok },
                    { label: qsTr("Unavailable"), value: app.sensorCount - app.sensorsAvailable, icon: "!", color: Theme.warn },
                    { label: qsTr("Devices"), value: app.devices.length, icon: "▦", color: Theme.memoryColor }
                ]
                delegate: StatTile {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 72
                    label: modelData.label
                    value: String(modelData.value)
                    icon: modelData.icon
                    accent: modelData.color
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
        PillTabs {
            titles: [qsTr("All"), qsTr("Temperature"), qsTr("Fan"), qsTr("Voltage"), qsTr("Power"),
                     qsTr("Utilization"), qsTr("Frequency")]
            current: Math.max(0, page.typeFilters.indexOf(app.sensorFilter.typeFilter))
            accent: Theme.accent
            onSelected: index => app.sensorFilter.typeFilter = page.typeFilters[index]
        }
            Item { Layout.fillWidth: true }
            PillTabs {
                titles: [qsTr("★ Favorites")]
                current: app.sensorFilter.favoritesOnly ? 0 : -1
                accent: Theme.warn
                onSelected: app.sensorFilter.favoritesOnly = !app.sensorFilter.favoritesOnly
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 1
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: false
                    Layout.preferredHeight: 38
                    Layout.maximumHeight: 38
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    spacing: 8
                    Item { Layout.preferredWidth: 22 }
                    Repeater {
                        model: [qsTr("Sensor"), qsTr("Value"), qsTr("Min"), qsTr("Max"), qsTr("History"), qsTr("Status")]
                        delegate: Text {
                            required property int index
                            required property string modelData
                            // The history column is a chart and cannot be sorted.
                            readonly property string key: ["name", "value", "min", "max", "", "status"][index]
                            readonly property bool sorted: key !== "" && app.sensorFilter.sortKey === key
                            text: modelData + (sorted ? (app.sensorFilter.sortDescending ? " ▼" : " ▲") : "")
                            color: sorted ? Theme.text : Theme.textMuted
                            font.pixelSize: 12
                            TapHandler { enabled: parent.key !== ""; onTapped: app.sensorFilter.sortBy(parent.key) }
                            HoverHandler { cursorShape: parent.key !== "" ? Qt.PointingHandCursor : Qt.ArrowCursor }
                            Layout.fillWidth: index === 0
                            Layout.preferredWidth: index === 0 ? 100 : (index >= 4 ? 100 : 110)
                            horizontalAlignment: index === 0 ? Text.AlignLeft : index === 4 ? Text.AlignHCenter : Text.AlignRight
                        }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

                ListView {
                    id: list
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: app.sensorFilter
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    Label {
                        anchors.centerIn: parent
                        visible: list.count === 0
                        text: search.text !== "" || app.sensorFilter.typeFilter >= 0 || app.sensorFilter.favoritesOnly ? qsTr("No sensors match the filter")
                                                                       : qsTr("No sensors detected")
                        color: Theme.textMuted
                    }

                    section.property: "deviceName"
                    section.criteria: ViewSection.FullString
                    section.delegate: Rectangle {
                        required property string section
                        width: list.width
                        height: 40
                        color: Theme.surfaceAlt
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            text: parent.section
                            color: Theme.text
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                    }

                    delegate: Rectangle {
                        id: row
                        required property string name
                        required property string unit
                        required property bool available
                        required property bool hasStats
                        required property var value
                        required property var minValue
                        required property var maxValue
                        required property string sensorId
                        required property int type
                        required property string deviceName

                        width: list.width
                        height: 38
                        color: hover.hovered ? Theme.surfaceAlt : "transparent"
                        HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            onTapped: detail.show(row.sensorId, row.name, row.deviceName, row.unit,
                                                  row.available ? row.value : undefined)
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 8
                            Text {
                                readonly property bool fav: app.favoriteSensors.indexOf(row.sensorId) >= 0
                                text: fav ? "★" : "☆"
                                color: fav ? Theme.warn : Theme.textMuted
                                font.pixelSize: 15
                                Layout.preferredWidth: 22
                                horizontalAlignment: Text.AlignHCenter
                                TapHandler { onTapped: app.toggleFavorite(row.sensorId) }
                            }
                            Text {
                                text: row.name
                                color: Theme.text
                                font.pixelSize: 13
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                Layout.preferredWidth: 100
                            }
                            Text {
                                text: row.available ? Theme.format(row.value, row.unit) : Theme.na
                                color: row.available ? Theme.text : Theme.textMuted
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: 110
                            }
                            Text {
                                text: row.hasStats ? Theme.format(row.minValue, row.unit) : Theme.na
                                color: Theme.textMuted
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: 110
                            }
                            Text {
                                text: row.hasStats ? Theme.format(row.maxValue, row.unit) : Theme.na
                                color: Theme.textMuted
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: 110
                            }
                            TemperatureChart {
                                sensorId: row.sensorId
                                lineColor: Theme.accent
                                filled: true
                                implicitHeight: 24
                                Layout.preferredWidth: 100
                                Layout.preferredHeight: 24
                            }
                            Item {
                                Layout.preferredWidth: 100
                                Layout.preferredHeight: 22
                                Rectangle {
                                    anchors.right: parent.right
                                    height: 22
                                    width: statusText.implicitWidth + 22
                                    radius: 11
                                    color: Qt.rgba((row.available ? Theme.ok : Theme.warn).r,
                                                   (row.available ? Theme.ok : Theme.warn).g,
                                                   (row.available ? Theme.ok : Theme.warn).b, 0.14)
                                    Text {
                                        id: statusText
                                        anchors.centerIn: parent
                                        text: row.available ? qsTr("OK") : qsTr("Unavailable")
                                        color: row.available ? Theme.ok : Theme.warn
                                        font.pixelSize: 12
                                    }
                                }
                            }
                        }
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: Theme.border
                            opacity: 0.35
                        }
                    }
                }
            }
        }
    }

    SensorDetailDialog { id: detail }
}
