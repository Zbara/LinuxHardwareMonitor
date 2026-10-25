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

    property string selectedKey: ""
    property string selectedName: ""
    property int selectedCount: 0
    property string message
    property bool force: false
    property bool selectedHasChildren: false
    property var details: ({})
    property var affinity: []
    property string selectedCmdline: ""
    property int selectedPid: 0

    function select(row) {
        selectedKey = row.key
        selectedName = row.groupName
        selectedCount = row.hasChildren ? row.count : 1
        selectedHasChildren = row.hasChildren
        selectedCmdline = row.cmdline
        selectedPid = row.pid
        message = ""
    }

    readonly property real nameWidth: 0 // name column stretches
    readonly property var cols: ({ owner: 120, cpu: 92, memory: 104, disk: 104, threads: 84 })

    readonly property real cpuTotal: app.highlights.length > 4 && app.highlights[4].available ? app.highlights[4].value : NaN
    readonly property real memTotal: app.highlights.length > 6 && app.highlights[6].available ? app.highlights[6].value : NaN
    readonly property real diskTotal: {
        if (app.highlights.length < 13) return NaN
        const r = app.highlights[11], w = app.highlights[12]
        return r.available && w.available ? r.value + w.value : NaN
    }

    function askEnd(forceKill) {
        force = forceKill
        confirm.open()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        anchors.bottomMargin: 12
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            PageTitle { text: qsTr("Processes") }
            Item { Layout.fillWidth: true }
            TextField {
                id: search
                Component.onCompleted: text = app.processes.filterText // the filter outlives the page
                Layout.preferredWidth: 300
                placeholderText: qsTr("Type a name, PID or user to search")
                color: Theme.text
                placeholderTextColor: Theme.textMuted
                leftPadding: 12
                background: Rectangle {
                    radius: 6
                    color: Theme.surface
                    border.color: search.activeFocus ? Theme.accent : Theme.border
                }
                onTextChanged: app.processes.filterText = text
            }
            Button {
                text: qsTr("Force kill")
                implicitWidth: Math.max(96, implicitContentWidth + 28)
                implicitHeight: 36
                enabled: page.selectedKey !== ""
                onClicked: page.askEnd(true)
            }
            Button {
                id: endButton
                text: qsTr("End task")
                implicitWidth: Math.max(96, endLabel.implicitWidth + 28)
                implicitHeight: 36
                enabled: page.selectedKey !== ""
                onClicked: page.askEnd(false)
                contentItem: Text {
                    id: endLabel
                    text: endButton.text
                    color: endButton.enabled ? "white" : Theme.textMuted
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: endButton.enabled ? (endButton.down ? "#b23b36" : Theme.hot) : Theme.surfaceAlt
                }
            }
        }

        // Summary tiles with live sparklines
        GridLayout {
            Layout.fillWidth: true
            columns: 5
            columnSpacing: 10
            readonly property var hl: app.highlights
            Repeater {
                model: [
                    { label: qsTr("Processes"), value: app.processCount > 0 ? String(app.processCount) : "…", icon: "▤", color: Theme.accent, spark: "", max: 0 },
                    { label: qsTr("Threads"), value: app.processCount > 0 ? String(app.threadCount) : "…", icon: "≋", color: Theme.accent, spark: "", max: 0 },
                    { label: qsTr("CPU"), value: isNaN(page.cpuTotal) ? Theme.na : Math.round(page.cpuTotal) + "%", icon: "▣", color: Theme.cpuColor,
                      spark: app.highlights.length > 4 ? app.highlights[4].sensorId : "", max: 100 },
                    { label: qsTr("Memory"), value: isNaN(page.memTotal) ? Theme.na : Math.round(page.memTotal) + "%", icon: "▥", color: Theme.memoryColor,
                      spark: app.highlights.length > 6 ? app.highlights[6].sensorId : "", max: 100 },
                    { label: qsTr("Disk"), value: isNaN(page.diskTotal) ? Theme.na : Theme.formatRate(page.diskTotal), icon: "▭", color: Theme.diskColor,
                      spark: app.highlights.length > 12 ? app.highlights[12].sensorId : "", max: 0 }
                ]
                delegate: StatTile {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredWidth: 150
                    implicitHeight: 86
                    label: modelData.label
                    value: modelData.value
                    icon: modelData.icon
                    accent: modelData.color
                    sparkId: modelData.spark
                    sparkFixedMax: modelData.max
                }
            }
        }

        // Table card: column header (with totals) and the process list.
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

        // Header with column totals, like Task Manager's utilisation row.
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.preferredHeight: 62
            Layout.maximumHeight: 62
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 0

                HeaderCell {
                    Layout.fillWidth: true
                    sortKey: "name"
                    title: qsTr("Name")
                    align: Text.AlignLeft
                }
                HeaderCell {
                    Layout.preferredWidth: page.cols.owner
                    sortKey: "owner"
                    title: qsTr("User")
                    align: Text.AlignLeft
                }
                HeaderCell {
                    Layout.preferredWidth: page.cols.cpu
                    sortKey: "cpu"
                    title: qsTr("CPU")
                    total: isNaN(page.cpuTotal) ? Theme.na : Math.round(page.cpuTotal) + "%"
                    heatValue: page.cpuTotal / 60
                }
                HeaderCell {
                    Layout.preferredWidth: page.cols.memory
                    sortKey: "memory"
                    title: qsTr("Memory")
                    total: isNaN(page.memTotal) ? Theme.na : Math.round(page.memTotal) + "%"
                    heatValue: page.memTotal / 100
                }
                HeaderCell {
                    Layout.preferredWidth: page.cols.disk
                    sortKey: "disk"
                    title: qsTr("Disk")
                    total: isNaN(page.diskTotal) ? Theme.na : Theme.formatRate(page.diskTotal)
                    heatValue: page.diskTotal / 40
                }
                HeaderCell {
                    Layout.preferredWidth: page.cols.threads
                    sortKey: "threads"
                    title: qsTr("Threads")
                    total: app.processCount > 0 ? app.threadCount : "—"
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: app.processes
            boundsBehavior: Flickable.StopAtBounds
            reuseItems: true
            ScrollBar.vertical: ScrollBar {}

            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                color: Theme.textMuted
                text: app.processCount === 0 ? qsTr("Reading processes…") : qsTr("No processes match your search")
            }

            delegate: Rectangle {
                id: row
                required property int index
                required property string key
                required property bool isGroup
                required property bool hasChildren
                required property bool expanded
                required property int pid
                required property string name
                required property int count
                required property string owner
                required property string state
                required property double cpu
                required property var memory
                required property double disk
                required property int threads
                required property string cmdline
                required property string groupName

                readonly property bool selected: page.selectedKey === key

                width: list.width
                height: isGroup ? 36 : 32
                color: selected ? Theme.selection
                               : hover.hovered ? Theme.surfaceAlt
                               : isGroup ? "transparent" : Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.55)
                border.color: "transparent"

                ToolTip.visible: hover.hovered && cmdline !== ""
                ToolTip.delay: 700
                ToolTip.text: cmdline

                HoverHandler { id: hover }
                TapHandler {
                    onTapped: page.select(row)
                    onDoubleTapped: if (row.hasChildren) app.processes.toggleExpanded(row.groupName)
                }
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: {
                        page.select(row)
                        contextMenu.popup()
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 0

                    // Name column: indent + chevron + name (+ count / PID)
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 6
                        Item { Layout.preferredWidth: row.isGroup ? 0 : 26 }
                        Item {
                            Layout.preferredWidth: 18
                            Layout.fillHeight: true
                            Text {
                                anchors.centerIn: parent
                                visible: row.isGroup && row.hasChildren
                                text: row.expanded ? "⌄" : "›"
                                color: Theme.textMuted
                                font.pixelSize: 16
                            }
                            MouseArea {
                                anchors.fill: parent
                                enabled: row.isGroup && row.hasChildren
                                onClicked: app.processes.toggleExpanded(row.groupName)
                            }
                        }
                        Text {
                            text: row.name
                            color: row.state === "Z" ? Theme.textMuted : Theme.text
                            font.italic: row.state === "Z"
                            font.pixelSize: 13
                            font.weight: row.isGroup ? Font.Medium : Font.Normal
                            elide: Text.ElideRight
                            Layout.maximumWidth: row.width * 0.5
                        }
                        Text {
                            visible: row.isGroup && row.hasChildren
                            text: "(" + row.count + ")"
                            color: Theme.textMuted
                            font.pixelSize: 12
                        }
                        Text {
                            visible: !row.isGroup || !row.hasChildren
                            text: qsTr("PID %1").arg(row.pid)
                            color: Theme.textMuted
                            font.pixelSize: 11
                        }
                        Item { Layout.fillWidth: true }
                    }
                    Text {
                        Layout.preferredWidth: page.cols.owner
                        text: row.owner
                        color: Theme.textMuted
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    HeatCell {
                        Layout.preferredWidth: page.cols.cpu
                        Layout.fillHeight: true
                        text: row.cpu >= 0.05 ? row.cpu.toFixed(1) + "%" : "0%"
                        value: row.cpu / 30
                    }
                    HeatCell {
                        Layout.preferredWidth: page.cols.memory
                        Layout.fillHeight: true
                        text: Theme.formatBytes(row.memory)
                        value: row.memory / (2 * 1024 * 1024 * 1024)
                    }
                    HeatCell {
                        Layout.preferredWidth: page.cols.disk
                        Layout.fillHeight: true
                        text: row.disk < 0 ? "—" : Theme.formatRate(row.disk / 1e6)
                        value: row.disk < 0 ? 0 : row.disk / 1e6 / 20
                    }
                    Text {
                        Layout.preferredWidth: page.cols.threads
                        text: row.threads
                        color: Theme.textMuted
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignRight
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.border
                    opacity: 0.5
                }
            }
        }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                color: Theme.textMuted
                font.pixelSize: 12
                text: qsTr("%1 processes · %2 threads").arg(app.processCount).arg(app.threadCount)
            }
            Item { Layout.fillWidth: true }
            Label {
                text: page.message
                color: Theme.warn
                elide: Text.ElideRight
                Layout.maximumWidth: 500
            }
        }
    }

    // ---- inline components -------------------------------------------------

    component HeaderCell: Item {
        id: cell
        property string sortKey
        property string title
        property string total
        property real heatValue: 0
        property int align: Text.AlignRight
        readonly property bool sorted: app.processes.sortKey === sortKey

        Layout.fillHeight: true
        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 6
            anchors.bottomMargin: 6
            color: cell.total !== "" ? Theme.heat(cell.heatValue) : "transparent"
            radius: 4
        }
        Column {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: cell.align === Text.AlignLeft ? parent.left : undefined
            anchors.right: cell.align === Text.AlignRight ? parent.right : undefined
            anchors.leftMargin: 6
            anchors.rightMargin: 8
            spacing: 2
            Text {
                visible: cell.total !== ""
                anchors.right: parent.right
                text: cell.total
                color: Theme.text
                font.pixelSize: 17
                font.weight: Font.DemiBold
            }
            Text {
                anchors.right: cell.align === Text.AlignRight ? parent.right : undefined
                text: cell.title + (cell.sorted ? (app.processes.sortDescending ? " ▼" : " ▲") : "")
                color: cell.sorted ? Theme.accent : Theme.textMuted
                font.pixelSize: 12
            }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: app.processes.sortBy(cell.sortKey)
        }
    }

    component HeatCell: Item {
        property string text
        property real value: 0
        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 1
            anchors.bottomMargin: 1
            color: Theme.heat(parent.value)
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 8
            text: parent.text
            color: Theme.text
            font.pixelSize: 12
        }
    }

    Menu {
        id: contextMenu
        MenuItem {
            text: page.selectedHasChildren ? qsTr("End all %1 tasks").arg(page.selectedCount) : qsTr("End task")
            onTriggered: page.askEnd(false)
        }
        MenuItem {
            text: qsTr("Force kill")
            onTriggered: page.askEnd(true)
        }
        MenuItem {
            text: qsTr("Suspend")
            onTriggered: page.message = app.suspendProcesses(app.processes.pidsFor(page.selectedKey), true)
        }
        MenuItem {
            text: qsTr("Resume")
            onTriggered: page.message = app.suspendProcesses(app.processes.pidsFor(page.selectedKey), false)
        }
        MenuSeparator {}
        Menu {
            title: qsTr("Set priority")
            enabled: page.selectedPid > 0
            Repeater {
                model: [
                    { text: qsTr("High"), nice: -10 }, { text: qsTr("Above normal"), nice: -5 },
                    { text: qsTr("Normal"), nice: 0 }, { text: qsTr("Below normal"), nice: 10 },
                    { text: qsTr("Low"), nice: 19 }
                ]
                delegate: MenuItem {
                    required property var modelData
                    text: modelData.text
                    onTriggered: page.message = app.setProcessPriority(page.selectedPid, modelData.nice)
                }
            }
        }
        MenuItem {
            enabled: page.selectedPid > 0
            text: qsTr("Set affinity…")
            onTriggered: { page.affinity = app.processAffinity(page.selectedPid); affinityDialog.open() }
        }
        MenuItem {
            enabled: page.selectedPid > 0
            text: qsTr("Properties")
            onTriggered: { page.details = app.processDetails(page.selectedPid); propsDialog.open() }
        }
        MenuSeparator {}
        MenuItem {
            visible: page.selectedHasChildren
            height: visible ? implicitHeight : 0
            text: qsTr("Expand / collapse")
            onTriggered: app.processes.toggleExpanded(page.selectedName)
        }
        MenuItem {
            visible: page.selectedPid > 0
            height: visible ? implicitHeight : 0
            text: qsTr("Copy PID")
            onTriggered: app.copyToClipboard(String(page.selectedPid))
        }
        MenuItem {
            visible: page.selectedCmdline !== ""
            height: visible ? implicitHeight : 0
            text: qsTr("Copy command line")
            onTriggered: app.copyToClipboard(page.selectedCmdline)
        }
        MenuItem {
            text: qsTr("Copy name")
            onTriggered: app.copyToClipboard(page.selectedName)
        }
    }

    function stateName(code) {
        return ({ R: qsTr("Running"), S: qsTr("Sleeping"), D: qsTr("Waiting for I/O"), Z: qsTr("Zombie"),
                  T: qsTr("Stopped"), t: qsTr("Stopped"), I: qsTr("Idle") })[code] || code
    }
    function duration(seconds) {
        if (seconds === undefined) return Theme.na
        const d = Math.floor(seconds / 86400), h = Math.floor(seconds % 86400 / 3600), m = Math.floor(seconds % 3600 / 60)
        return (d > 0 ? d + ":" : "") + (h < 10 && d > 0 ? "0" : "") + h + ":" + (m < 10 ? "0" : "") + m + ":" + (Math.floor(seconds % 60) < 10 ? "0" : "") + Math.floor(seconds % 60)
    }

    // Which logical processors the selected process may use.
    Dialog {
        id: affinityDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(560, page.width - 60)
        modal: true
        title: qsTr("Processor affinity")
        standardButtons: Dialog.Ok | Dialog.Cancel
        background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }
        header: Label {
            text: affinityDialog.title + "  ·  " + page.selectedName
            color: Theme.text
            font.pixelSize: 16
            font.weight: Font.DemiBold
            padding: 16
            elide: Text.ElideRight
        }
        readonly property int cpuCount: app.details.logicalCpus > 0 ? app.details.logicalCpus : 0
        property var chosen: ({})
        onAboutToShow: {
            const m = {}
            for (const c of page.affinity) m[c] = true
            chosen = m
        }
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                text: qsTr("Select the processors this process may run on.")
                color: Theme.textMuted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            RowLayout {
                spacing: 8
                Button {
                    text: qsTr("Select all")
                    onClicked: {
                        const m = {}
                        for (let i = 0; i < affinityDialog.cpuCount; i++) m[i] = true
                        affinityDialog.chosen = m
                    }
                }
                Button { text: qsTr("Clear"); onClicked: affinityDialog.chosen = ({}) }
            }
            GridLayout {
                columns: 8
                columnSpacing: 4
                rowSpacing: 2
                Repeater {
                    model: affinityDialog.cpuCount
                    delegate: CheckBox {
                        required property int index
                        text: "CPU " + index
                        checked: affinityDialog.chosen[index] === true
                        onToggled: {
                            const m = Object.assign({}, affinityDialog.chosen)
                            if (checked) m[index] = true; else delete m[index]
                            affinityDialog.chosen = m
                        }
                        contentItem: Text {
                            leftPadding: parent.indicator.width + 6
                            text: parent.text
                            color: Theme.text
                            font.pixelSize: 11
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }
            Label {
                visible: Object.keys(affinityDialog.chosen).length === 0
                text: qsTr("At least one processor must be selected")
                color: Theme.warn
                font.pixelSize: 12
            }
        }
        onAccepted: {
            const cpus = Object.keys(chosen).map(k => parseInt(k))
            page.message = app.setProcessAffinity(page.selectedPid, cpus)
        }
    }

    // Properties of the selected process, read once when the dialog opens.
    Dialog {
        id: propsDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(640, page.width - 60)
        modal: true
        standardButtons: Dialog.Close
        background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }
        header: Label {
            text: page.details.name !== undefined ? page.details.name + "  ·  PID " + page.details.pid : qsTr("The process no longer exists")
            color: Theme.text
            font.pixelSize: 16
            font.weight: Font.DemiBold
            padding: 16
            elide: Text.ElideRight
        }
        contentItem: GridLayout {
            columns: 2
            columnSpacing: 18
            rowSpacing: 7
            readonly property var rows: page.details.name === undefined ? [] : [
                [qsTr("Parent process"), page.details.parentName !== undefined ? page.details.parentName + " (" + page.details.ppid + ")" : String(page.details.ppid)],
                [qsTr("User"), page.details.user],
                [qsTr("State"), page.stateName(page.details.state)],
                [qsTr("Priority (nice)"), String(page.details.nice)],
                [qsTr("Threads"), String(page.details.threads)],
                [qsTr("Memory"), Theme.formatBytes(page.details.rssBytes)],
                [qsTr("Virtual memory"), Theme.formatBytes(page.details.vsizeBytes)],
                [qsTr("CPU time"), page.duration(page.details.cpuSeconds)],
                [qsTr("Running for"), page.duration(page.details.runningSeconds)],
                [qsTr("Open files"), page.details.openFiles !== undefined ? String(page.details.openFiles) : Theme.na],
                [qsTr("Executable"), page.details.exe],
                [qsTr("Working directory"), page.details.cwd],
                [qsTr("Command line"), page.details.cmdline]
            ]
            Repeater {
                model: parent.rows.length * 2
                delegate: Text {
                    required property int index
                    readonly property var cell: parent.rows[Math.floor(index / 2)]
                    text: index % 2 === 0 ? cell[0] + ":" : (cell[1] !== undefined && cell[1] !== "" ? cell[1] : Theme.na)
                    color: index % 2 === 0 ? Theme.textMuted : Theme.text
                    font.pixelSize: 12
                    wrapMode: Text.WrapAnywhere
                    Layout.fillWidth: index % 2 === 1
                    Layout.preferredWidth: index % 2 === 0 ? 150 : -1
                    Layout.alignment: Qt.AlignTop
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
        title: page.force ? qsTr("Force kill?") : qsTr("End task?")
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
            text: (page.selectedCount > 1
                   ? qsTr("This will end all %1 processes of “%2”.").arg(page.selectedCount).arg(page.selectedName)
                   : qsTr("This will end “%1”.").arg(page.selectedName))
                  + " " + qsTr("Unsaved data in it may be lost.")
                  + (page.force ? " " + qsTr("The processes are killed immediately (SIGKILL).") : "")
            color: Theme.textMuted
            wrapMode: Text.WordWrap
        }
        onAccepted: {
            const error = app.endProcesses(app.processes.pidsFor(page.selectedKey), page.force)
            page.message = error
            if (error === "")
                page.selectedKey = ""
        }
    }
}
