import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HwMonitor

Item {
    id: page
    property string message
    // "" = all, "enabled" or "disabled"
    property string filter: ""
    readonly property var filters: ["", "enabled", "disabled"]

    // Re-read the autostart directories whenever the page is opened.
    onVisibleChanged: if (visible) app.refreshStartup()
    Component.onCompleted: app.refreshStartup()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            PageTitle { text: qsTr("Startup apps") }
            Item { Layout.fillWidth: true }
            Label {
                text: page.message
                color: Theme.warn
                elide: Text.ElideRight
                Layout.maximumWidth: 420
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 3
            columnSpacing: 10
            Repeater {
                model: [
                    { label: qsTr("Total"), value: app.startup.count, icon: "⏻", color: Theme.accent },
                    { label: qsTr("Enabled"), value: app.startup.enabledCount, icon: "✓", color: Theme.ok },
                    { label: qsTr("Disabled"), value: app.startup.count - app.startup.enabledCount, icon: "■", color: Theme.textMuted }
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
                titles: [qsTr("All"), qsTr("Enabled"), qsTr("Disabled")]
                current: Math.max(0, page.filters.indexOf(page.filter))
                accent: Theme.accent
                onSelected: index => page.filter = page.filters[index]
            }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                color: Theme.textMuted
                font.pixelSize: 12
                elide: Text.ElideRight
                text: qsTr("Programs started when you log in. Disabling a system-wide entry only adds an override in your own autostart folder.")
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
                    spacing: 12
                    Text { text: qsTr("Name"); color: Theme.textMuted; font.pixelSize: 12; Layout.preferredWidth: 240 }
                    Text { text: qsTr("Command"); color: Theme.textMuted; font.pixelSize: 12; Layout.fillWidth: true; Layout.preferredWidth: 100 }
                    Text { text: qsTr("Source"); color: Theme.textMuted; font.pixelSize: 12; Layout.preferredWidth: 70 }
                    Text {
                        text: qsTr("Status")
                        color: Theme.textMuted
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignRight
                        Layout.preferredWidth: 150
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

                ListView {
                    id: list
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: app.startup
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    Label {
                        anchors.centerIn: parent
                        visible: list.count === 0
                        color: Theme.textMuted
                        text: qsTr("No startup entries found")
                    }

                    delegate: Rectangle {
                        id: row
                        required property string id
                        required property string name
                        required property string comment
                        required property string exec
                        required property string source
                        required property bool enabled

                        readonly property bool shown: page.filter === "" || (enabled ? "enabled" : "disabled") === page.filter

                        visible: shown
                        width: list.width
                        height: shown ? 46 : 0
                        color: hover.hovered ? Theme.surfaceAlt : "transparent"

                        ToolTip.visible: hover.hovered && comment !== ""
                        ToolTip.delay: 600
                        ToolTip.text: comment
                        HoverHandler { id: hover }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 12
                            Text {
                                text: row.name
                                color: row.enabled ? Theme.text : Theme.textMuted
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                                Layout.preferredWidth: 240
                            }
                            Text {
                                text: row.exec
                                color: Theme.textMuted
                                font.pixelSize: 12
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                                Layout.preferredWidth: 100
                            }
                            Text {
                                text: row.source
                                color: Theme.textMuted
                                font.pixelSize: 12
                                Layout.preferredWidth: 70
                            }
                            RowLayout {
                                Layout.fillWidth: false
                                Layout.preferredWidth: 150
                                spacing: 10
                                Item { Layout.fillWidth: true }
                                Rectangle {
                                    height: 22
                                    width: statusText.implicitWidth + 24
                                    radius: 11
                                    color: Qt.rgba((row.enabled ? Theme.ok : Theme.textMuted).r,
                                                   (row.enabled ? Theme.ok : Theme.textMuted).g,
                                                   (row.enabled ? Theme.ok : Theme.textMuted).b, 0.14)
                                    Text {
                                        id: statusText
                                        anchors.centerIn: parent
                                        text: row.enabled ? qsTr("Enabled") : qsTr("Disabled")
                                        color: row.enabled ? Theme.ok : Theme.textMuted
                                        font.pixelSize: 12
                                    }
                                }
                                Toggle {
                                    checked: row.enabled
                                    onToggled: {
                                        page.message = app.setStartupEnabled(row.id, checked)
                                        // The model reloads on success; on failure restore the switch.
                                        checked = Qt.binding(() => row.enabled)
                                    }
                                }
                            }
                        }
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: Theme.border
                            opacity: 0.45
                        }
                    }
                }
            }
        }
    }
}
