import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HwMonitor

Item {
    id: page

    property string selected: ""
    property string pendingAction: ""

    readonly property var stateText: ({
        running: qsTr("Running"), exited: qsTr("Exited"), stopped: qsTr("Stopped"),
        failed: qsTr("Failed"), starting: qsTr("Starting"), other: qsTr("Other")
    })
    readonly property var filters: ["", "running", "stopped", "failed"]

    function stateColor(state) {
        return state === "running" ? Theme.ok : state === "failed" ? Theme.hot
             : state === "starting" ? Theme.warn : state === "exited" ? Theme.accent : Theme.textMuted
    }

    function ask(action) {
        pendingAction = action
        if (action === "start") app.serviceAction(selected, action)
        else confirm.open()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            PageTitle { text: qsTr("Services") }
            Item { Layout.fillWidth: true }
            TextField {
                id: search
                Component.onCompleted: text = app.services.filterText
                Layout.preferredWidth: 260
                placeholderText: qsTr("Search services")
                color: Theme.text
                placeholderTextColor: Theme.textMuted
                leftPadding: 12
                background: Rectangle {
                    radius: 6
                    color: Theme.surface
                    border.color: search.activeFocus ? Theme.accent : Theme.border
                }
                onTextChanged: app.services.filterText = text
            }
            Button { text: qsTr("Start"); enabled: page.selected !== ""; onClicked: page.ask("start") }
            Button { text: qsTr("Stop"); enabled: page.selected !== ""; onClicked: page.ask("stop") }
            Button { text: qsTr("Restart"); enabled: page.selected !== ""; onClicked: page.ask("restart") }
        }

        // Summary tiles
        GridLayout {
            Layout.fillWidth: true
            columns: 4
            columnSpacing: 10
            Repeater {
                model: [
                    { label: qsTr("Running"), value: app.servicesRunning, icon: "▶", color: Theme.ok },
                    { label: qsTr("Failed"), value: app.servicesFailed, icon: "✕", color: Theme.hot },
                    { label: qsTr("Stopped"), value: app.servicesStopped, icon: "■", color: Theme.textMuted },
                    { label: qsTr("Total"), value: app.servicesTotal, icon: "⚒", color: Theme.accent }
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
                titles: [qsTr("All"), qsTr("Running"), qsTr("Stopped"), qsTr("Failed")]
                current: Math.max(0, page.filters.indexOf(app.services.stateFilter))
                accent: Theme.accent
                onSelected: index => app.services.stateFilter = page.filters[index]
            }
            Item { Layout.fillWidth: true }
            Label {
                color: Theme.textMuted
                font.pixelSize: 12
                text: qsTr("Changing system services asks for your password.")
            }
        }

        Rectangle {
            Layout.fillWidth: true
            visible: app.servicesMessage !== ""
            implicitHeight: msg.implicitHeight + 16
            radius: 6
            color: Theme.surface
            border.color: Theme.warn
            Label {
                id: msg
                anchors.fill: parent
                anchors.margins: 8
                text: app.servicesMessage
                color: Theme.warn
                wrapMode: Text.WordWrap
                font.pixelSize: 12
            }
        }

        // Table card
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
                    Repeater {
                        model: [
                            { key: "name", title: qsTr("Name"), width: 260 },
                            { key: "", title: qsTr("Description"), width: -1 },
                            { key: "status", title: qsTr("Status"), width: 130 }
                        ]
                        delegate: Text {
                            required property var modelData
                            readonly property bool sorted: modelData.key !== "" && app.services.sortKey === modelData.key
                            text: modelData.title + (sorted ? (app.services.sortDescending ? " ▼" : " ▲") : "")
                            color: sorted ? Theme.accent : Theme.textMuted
                            font.pixelSize: 12
                            verticalAlignment: Text.AlignVCenter
                            Layout.fillWidth: modelData.width < 0
                            Layout.preferredWidth: modelData.width < 0 ? 100 : modelData.width
                            Layout.fillHeight: true
                            MouseArea {
                                anchors.fill: parent
                                enabled: parent.modelData.key !== ""
                                cursorShape: Qt.PointingHandCursor
                                onClicked: app.services.sortBy(parent.modelData.key)
                            }
                        }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

                ListView {
                    id: list
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: app.services
                    boundsBehavior: Flickable.StopAtBounds
                    reuseItems: true
                    ScrollBar.vertical: ScrollBar {}

                    Label {
                        anchors.centerIn: parent
                        visible: list.count === 0
                        color: Theme.textMuted
                        text: app.servicesTotal === 0 ? qsTr("Reading services…") : qsTr("No services match")
                    }

                    delegate: Rectangle {
                        id: row
                        required property string name
                        required property string description
                        required property string state
                        required property string sub

                        readonly property bool isSelected: page.selected === name

                        width: list.width
                        height: 40
                        color: isSelected ? Theme.selection : hover.hovered ? Theme.surfaceAlt : "transparent"

                        HoverHandler { id: hover }
                        TapHandler { onTapped: page.selected = row.name }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 12
                            Text {
                                text: row.name
                                color: Theme.text
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                                Layout.preferredWidth: 260
                            }
                            Text {
                                text: row.description
                                color: Theme.textMuted
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                Layout.preferredWidth: 100
                            }
                            Item {
                                Layout.preferredWidth: 130
                                Layout.preferredHeight: 22
                                Rectangle {
                                    height: 22
                                    width: pillText.implicitWidth + 24
                                    radius: 11
                                    color: Qt.rgba(page.stateColor(row.state).r, page.stateColor(row.state).g,
                                                   page.stateColor(row.state).b, 0.14)
                                    Row {
                                        anchors.centerIn: parent
                                        spacing: 6
                                        Rectangle {
                                            width: 6; height: 6; radius: 3
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: page.stateColor(row.state)
                                        }
                                        Text {
                                            id: pillText
                                            text: page.stateText[row.state] !== undefined ? page.stateText[row.state] : row.state
                                            color: page.stateColor(row.state)
                                            font.pixelSize: 12
                                        }
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

    Dialog {
        id: confirm
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 380
        modal: true
        title: page.pendingAction === "stop" ? qsTr("Stop service?") : qsTr("Restart service?")
        standardButtons: Dialog.Yes | Dialog.Cancel
        background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }
        header: Label {
            text: confirm.title
            color: Theme.text
            font.pixelSize: 16
            font.weight: Font.DemiBold
            padding: 16
        }
        contentItem: Label {
            text: qsTr("“%1” will be affected immediately.").arg(page.selected)
            color: Theme.textMuted
            wrapMode: Text.WordWrap
        }
        onAccepted: app.serviceAction(page.selected, page.pendingAction)
    }
}
