import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HwMonitor

ScrollView {
    id: page
    contentWidth: availableWidth
    clip: true

    // One titled card of label / control rows.
    component Section: Rectangle {
        id: section
        property string title
        default property alias content: rows.data
        Layout.fillWidth: true
        Layout.maximumWidth: 760
        implicitHeight: rows.implicitHeight + titleText.implicitHeight + 44
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border

        Text {
            id: titleText
            x: 18; y: 16
            text: section.title
            color: Theme.text
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }
        ColumnLayout {
            id: rows
            anchors.top: titleText.bottom
            anchors.topMargin: 8
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            spacing: 0
        }
    }

    // A row: label (+ optional hint) on the left, the control on the right.
    component SettingRow: Item {
        id: setting
        property string label
        property string hint
        default property alias control: slot.data
        Layout.fillWidth: true
        implicitHeight: 54

        Column {
            anchors.left: parent.left
            anchors.right: slot.left
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1
            Text { text: setting.label; color: Theme.text; font.pixelSize: 13 }
            Text {
                visible: setting.hint !== ""
                text: setting.hint
                color: Theme.textMuted
                font.pixelSize: 11
                width: parent.width
                elide: Text.ElideMiddle
            }
        }
        Item {
            id: slot
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 220
            height: 40
        }
    }

    ColumnLayout {
        width: page.availableWidth
        spacing: 14

        PageTitle {
            text: qsTr("Settings")
            Layout.leftMargin: 20
            Layout.topMargin: 20
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.bottomMargin: 20
            spacing: 14

            Section {
                title: qsTr("General")

                SettingRow {
                    label: qsTr("Language")
                    ComboBox {
                        anchors.right: parent.right
                        width: 220
                        textRole: "name"
                        valueRole: "code"
                        model: app.languages
                        Component.onCompleted: currentIndex = Math.max(0, indexOfValue(app.language))
                        onActivated: app.language = currentValue
                    }
                }
                SettingRow {
                    label: qsTr("Keep running in the system tray")
                    hint: app.trayAvailable ? "" : qsTr("System tray is not available in this desktop session")
                    Toggle {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        enabled: app.trayAvailable
                        checked: app.trayEnabled
                        onToggled: app.trayEnabled = checked
                    }
                }
                SettingRow {
                    label: qsTr("Tray icon")
                    ComboBox {
                        anchors.right: parent.right
                        width: 220
                        enabled: app.trayAvailable
                        readonly property var values: ["app", "temperature"]
                        model: [qsTr("Program icon"), qsTr("CPU temperature")]
                        currentIndex: Math.max(0, values.indexOf(app.trayStyle))
                        onActivated: app.trayStyle = values[currentIndex]
                    }
                }
                SettingRow {
                    label: qsTr("Start with the system")
                    Toggle {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        checked: app.autostart
                        onToggled: app.autostart = checked
                    }
                }
            }

            Section {
                title: qsTr("Appearance")

                SettingRow {
                    label: qsTr("Temperature unit")
                    ComboBox {
                        anchors.right: parent.right
                        width: 220
                        model: [qsTr("Celsius (°C)"), qsTr("Fahrenheit (°F)")]
                        currentIndex: app.useFahrenheit ? 1 : 0
                        onActivated: app.useFahrenheit = currentIndex === 1
                    }
                }
                SettingRow {
                    label: qsTr("Dark theme")
                    Toggle {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        checked: app.darkTheme
                        onToggled: app.darkTheme = checked
                    }
                }
            }

            Section {
                title: qsTr("Alerts")

                SettingRow {
                    label: qsTr("Notify when a sensor overheats")
                    hint: qsTr("A desktop notification, once per overheating event")
                    Toggle {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        checked: app.overheatNotify
                        onToggled: app.overheatNotify = checked
                    }
                }
                SettingRow {
                    label: qsTr("Temperature threshold")
                    ComboBox {
                        anchors.right: parent.right
                        width: 220
                        enabled: app.overheatNotify
                        readonly property var values: [70, 80, 85, 90, 95, 100]
                        model: values.map(v => Theme.tempText(v, 0))
                        currentIndex: Math.max(0, values.indexOf(app.overheatThreshold))
                        onActivated: app.overheatThreshold = values[currentIndex]
                    }
                }
            }

            Section {
                title: qsTr("Data")

                SettingRow {
                    label: qsTr("Update interval")
                    ComboBox {
                        anchors.right: parent.right
                        width: 220
                        readonly property var values: [500, 1000, 2000, 5000]
                        model: values.map(v => qsTr("%1 ms").arg(v))
                        currentIndex: Math.max(0, values.indexOf(app.intervalMs))
                        onActivated: app.intervalMs = values[currentIndex]
                    }
                }
                SettingRow {
                    label: qsTr("History length")
                    ComboBox {
                        anchors.right: parent.right
                        width: 220
                        readonly property var values: [60, 300, 900, 1800, 3600]
                        model: [qsTr("1 minute"), qsTr("5 minutes"), qsTr("15 minutes"), qsTr("30 minutes"), qsTr("1 hour")]
                        currentIndex: Math.max(0, values.indexOf(app.historySeconds))
                        onActivated: app.historySeconds = values[currentIndex]
                    }
                }
                SettingRow {
                    label: qsTr("Export history")
                    hint: exportResult.text
                    Button {
                        anchors.right: parent.right
                        text: qsTr("Export CSV")
                        onClicked: {
                            const path = app.exportHistory()
                            exportResult.text = path !== "" ? qsTr("Saved to ") + path : qsTr("Export failed")
                        }
                    }
                    Label { id: exportResult; visible: false }
                }
            }

            Section {
                title: qsTr("About")

                SettingRow {
                    label: qsTr("Linux Hardware Monitor %1").arg(app.appVersion)
                    hint: qsTr("Free software, GPL-3.0-or-later")
                    Item { width: 1; height: 1 }
                }
                SettingRow {
                    label: qsTr("System information")
                    hint: reportResult.text !== "" ? reportResult.text : qsTr("Hardware models and versions for a bug report; no serial numbers or addresses")
                    Button {
                        anchors.right: parent.right
                        text: qsTr("Copy")
                        onClicked: {
                            app.copyToClipboard(app.systemReport())
                            reportResult.text = qsTr("Copied to the clipboard")
                        }
                    }
                    Label { id: reportResult; visible: false }
                }
            }
        }
    }
}
