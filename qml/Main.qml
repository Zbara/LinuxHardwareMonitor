import QtQuick
import HwMonitor
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1480
    height: 860
    minimumWidth: 960
    minimumHeight: 520
    visible: true
    title: qsTr("Linux Hardware Monitor")
    color: Theme.background

    property alias pageIndex: pages.currentIndex
    // Screenshot helper: open the simulation scenario panel.
    property bool scenarioPanel: false
    onScenarioPanelChanged: if (scenarioPanel) scenarioPopup.open()

    // Restore where the user left off (saved when the window is closed).
    Component.onCompleted: {
        const w = app.uiSetting("width", 0), h = app.uiSetting("height", 0)
        if (w >= minimumWidth && h >= minimumHeight) { width = w; height = h }
        const page = app.uiSetting("page", 0)
        if (page >= 0 && page < pages.count) pages.currentIndex = page
        resourceIndex = app.uiSetting("resource", 0)
    }
    onClosing: close => {
        if (visibility === Window.Windowed) {
            app.setUiSetting("width", width)
            app.setUiSetting("height", height)
        }
        app.setUiSetting("page", pages.currentIndex)
        app.setUiSetting("resource", resourceIndex)
        // With the tray enabled the window only hides; the app keeps running (Quit is in the tray menu).
        if (app.trayEnabled && app.trayAvailable) {
            close.accepted = false
            window.hide()
        }
    }
    // Remembered here because the Performance page itself only exists while it is shown.
    property int resourceIndex: 0
    property bool coreView: false

    // Hardware data that is not from this machine is always announced.
    header: Rectangle {
        visible: app.simulationKind !== ""
        height: visible ? 36 : 0
        color: Theme.warn
        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.right: scenarioButton.visible ? scenarioButton.left : parent.right
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            color: "#1b1f24"
            font.pixelSize: 13
            font.weight: Font.DemiBold
            text: app.simulationKind === "mock"
                  ? qsTr("Simulation mode — %1").arg(app.simulationName) + "  ·  "
                    + qsTr("Hardware readings are simulated; processes, services and startup apps are real.")
                  : qsTr("Test data — %1").arg(app.simulationName) + "  ·  "
                    + qsTr("Hardware readings come from prepared test files, not from this machine.")
        }
        // Make things go wrong on demand: overheating, a card falling off the bus, a dead fan...
        Rectangle {
            id: scenarioButton
            visible: app.simulationActions.length > 0
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            width: scenarioLabel.implicitWidth + 24
            height: 26
            radius: 13
            color: "#1b1f24"
            Text { id: scenarioLabel; anchors.centerIn: parent; text: qsTr("Scenarios ▾"); color: "white"; font.pixelSize: 12 }
            TapHandler { onTapped: scenarioPopup.opened ? scenarioPopup.close() : scenarioPopup.open() }
        }
    }

    Popup {
        id: scenarioPopup
        parent: Overlay.overlay
        x: window.width - width - 10
        y: 40
        width: 360
        padding: 12
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }
        contentItem: ColumnLayout {
            spacing: 6
            Text { text: qsTr("Trigger a scenario"); color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold }
            Text {
                text: qsTr("It lasts 20–30 seconds, then everything returns to normal.")
                color: Theme.textMuted
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            readonly property var labels: ({
                overheat_cpu: qsTr("Overheat %1"), cpu_sensor_dropout: qsTr("Lose the temperature sensor of %1"),
                overheat_gpu: qsTr("Overheat %1"), gpu_loss: qsTr("Disconnect %1"),
                fan_stop: qsTr("Stop fan: %1"), fan_boost: qsTr("Spin up fan: %1"),
                disk_dropout: qsTr("Lose the temperature sensor of disk %1")
            })
            Repeater {
                model: app.simulationActions
                delegate: Button {
                    required property int index
                    required property var modelData
                    Layout.fillWidth: true
                    text: parent.labels[modelData.kind].arg(modelData.target)
                    onClicked: app.runSimulationAction(index)
                }
            }
            Button {
                Layout.fillWidth: true
                text: qsTr("Cancel all triggered scenarios")
                onClicked: app.clearSimulationScenarios()
            }
        }
    }

    palette.window: Theme.background
    palette.windowText: Theme.text
    palette.base: Theme.surface
    palette.text: Theme.text
    palette.button: Theme.surfaceAlt
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: "#ffffff"
    palette.mid: Theme.border
    palette.dark: Theme.textMuted
    palette.light: Theme.dark ? "#d0d6de" : "#ffffff"

    Binding {
        target: Theme
        property: "dark"
        value: app.darkTheme
    }

    Binding {
        target: app
        property: "servicesActive"
        value: pages.currentIndex === 2
    }

    // Process data is only collected while a page that shows it is visible.
    Binding {
        target: app
        property: "processesActive"
        value: pages.currentIndex <= 1
    }

    readonly property var navItems: [
        { title: qsTr("Performance"), glyph: "◔" },
        { title: qsTr("Processes"), glyph: "☰" },
        { title: qsTr("Services"), glyph: "⚒" },
        { title: qsTr("Startup apps"), glyph: "⏻" },
        { title: qsTr("Sensors"), glyph: "∿" },
        { title: qsTr("Overview"), glyph: "▦" },
        { title: qsTr("Settings"), glyph: "⚙" }
    ]

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 216
            color: Theme.sidebar

            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: Theme.border
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: 14
                anchors.bottomMargin: 10
                anchors.leftMargin: 8
                anchors.rightMargin: 9
                spacing: 2

                Label {
                    text: qsTr("Hardware Monitor")
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    Layout.rightMargin: 10
                    color: Theme.text
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    Layout.leftMargin: 10
                    Layout.bottomMargin: 14
                }

                Repeater {
                    model: window.navItems
                    delegate: NavItem {
                        required property int index
                        required property var modelData
                        visible: index < 6
                        Layout.fillWidth: true
                        text: modelData.title
                        glyph: modelData.glyph
                        selected: pages.currentIndex === index
                        onClicked: pages.currentIndex = index
                    }
                }

                Item { Layout.fillHeight: true }

                NavItem {
                    Layout.fillWidth: true
                    text: window.navItems[6].title
                    glyph: window.navItems[6].glyph
                    selected: pages.currentIndex === 6
                    onClicked: pages.currentIndex = 6
                }
            }
        }

        StackLayout {
            id: pages
            Layout.fillWidth: true
            Layout.fillHeight: true
            // Only the visible page exists. Hidden pages used to keep every binding (and, on the
            // Sensors page, hundreds of live charts) running once a second for nothing.
            Loader {
                id: performanceLoader
                active: StackLayout.isCurrentItem
                sourceComponent: PerformancePage {}
                onLoaded: {
                    item.selected = window.resourceIndex
                    if (window.coreView) item.coreView = true
                }
            }
            Connections {
                target: performanceLoader.item
                ignoreUnknownSignals: true
                function onSelectedChanged() { if (performanceLoader.item) window.resourceIndex = performanceLoader.item.selected }
            }
            Loader { active: StackLayout.isCurrentItem; sourceComponent: ProcessesPage {} }
            Loader { active: StackLayout.isCurrentItem; sourceComponent: ServicesPage {} }
            Loader { active: StackLayout.isCurrentItem; sourceComponent: StartupPage {} }
            Loader { active: StackLayout.isCurrentItem; sourceComponent: SensorsPage {} }
            Loader { active: StackLayout.isCurrentItem; sourceComponent: DashboardPage {} }
            Loader { active: StackLayout.isCurrentItem; sourceComponent: SettingsPage {} }
        }
    }
}
