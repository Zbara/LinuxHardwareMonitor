import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HwMonitor

Item {
    id: page

    // Index into `items` (CPU, Memory, each disk, each non-virtual NIC, each GPU).
    property int selected: 0
    // Which tab of the selected resource is shown.
    property int viewIndex: 0
    // Screenshot helper: jump to the CPU "Logical processors" tab.
    property bool coreView: false
    onCoreViewChanged: if (coreView) { selected = 0; viewIndex = 1 }
    onSelectedChanged: viewIndex = 0

    // ---- data helpers ------------------------------------------------------
    // highlights[]: 0 CPU °C, 4 CPU %, 5 CPU MHz, 6 RAM %
    function h(i) {
        const list = app.highlights
        return i < list.length ? list[i] : ({ available: false, value: NaN, sensorId: "" })
    }
    function ok(ref) { return ref !== undefined && ref !== null && ref.available }
    function pct(ref) { return ok(ref) ? Math.round(ref.value) + "%" : Theme.na }
    function num(ref, digits, unit) {
        if (!ok(ref)) return Theme.na
        return unit === "°C" ? Theme.tempText(ref.value, digits) : ref.value.toFixed(digits) + (unit ? " " + unit : "")
    }
    function rate(ref) { return ok(ref) ? Theme.formatRate(ref.value) : Theme.na }
    function ghz(mhz) { return mhz === undefined || mhz === null || isNaN(mhz) ? Theme.na : (mhz / 1000).toFixed(2) + " GHz" }
    function or(v, fallback) { return v === undefined || v === null || v === "" ? fallback : v }
    function bytes(v) { return v === undefined || v === null ? Theme.na : Theme.formatBytes(v) }
    function count(v) { return v === undefined || v === null ? Theme.na : Math.round(v).toLocaleString(Qt.locale(), "f", 0) }
    function kbText(kb) {
        return kb === undefined || kb === null ? "—" : (kb >= 1024 ? (kb / 1024).toFixed(kb % 1024 === 0 ? 0 : 1) + " " + Theme.unitMB : kb + " " + Theme.unitKB)
    }
    function signalQuality(dbm) {
        return dbm >= -50 ? qsTr("Excellent") : dbm >= -60 ? qsTr("Good") : dbm >= -70 ? qsTr("Fair") : qsTr("Weak")
    }
    function frac(a, b) { return a !== undefined && a !== null && b !== undefined && b > 0 ? Math.max(0, Math.min(1, a / b)) : -1 }
    // "Peak: 11.98 MB/s" from a sensor reference's session maximum; empty when there is none yet.
    function peakRate(ref) { return ref !== undefined && ref !== null && ref.max !== undefined && ref.max !== null ? qsTr("Peak: %1").arg(Theme.formatRate(ref.max)) : "" }
    function row(label, value) { return { label: label, value: String(or(value, Theme.na)) } }
    function stateText(state) {
        // Tunnels (VPN) report "unknown" because they have no carrier to sense; they are simply active.
        return state === "up" ? qsTr("Connected") : state === "down" ? qsTr("Disconnected") : state === "unknown" ? qsTr("Active") : or(state, Theme.na)
    }
    function linkSpeed(mbps) { return mbps > 0 ? (mbps >= 1000 ? (mbps / 1000).toFixed(1) + " Gbps" : mbps + " Mbps") : Theme.na }

    function tile(label, value, icon, extra) {
        return Object.assign({ label: label, value: String(value), icon: icon, sparkId: "", bar: -1, sparkFixedMax: 0 }, extra || {})
    }
    function chartDef(title, id1, unit, extra) {
        return Object.assign({ title: title, id1: id1, id2: "", fixedMax: 0, unit: unit, legend: [], valueText: "", footerText: "" }, extra || {})
    }
    function view(title, sections) { return { title: title, sections: sections } }
    function charts(list, cols, extra) { return Object.assign({ kind: "charts", charts: list, cols: cols, sideTiles: false, height: 0 }, extra || {}) }
    // Only charts the hardware can actually feed (an Intel iGPU has no load or VRAM sensors); null if none.
    function liveCharts(list, cols, extra) {
        const live = list.filter(c => c.id1 !== "")
        return live.length > 0 ? charts(live, Math.min(cols, live.length), extra) : null
    }

    // ---- resources ---------------------------------------------------------
    function cpuItem(d) {
        const util = h(4), freq = h(5), temp = d.cpuTemp || {}
        const usageChart = chartDef(qsTr("CPU Usage"), util.sensorId, "%", { fixedMax: 100 })
        const freqChart = chartDef(qsTr("Frequency"), "system/cpu_freq_avg", "MHz",
                                   { id2: "system/cpu_freq_max", legend: [qsTr("Average"), qsTr("Fastest core")],
                                     valueText: qsTr("Current") + " " + ghz(freq.value) })
        const tempChart = chartDef(qsTr("Temperature"), temp.id || "", "°C",
                                   { fixedMax: 100, valueText: qsTr("Current") + " " + num(temp, 0, "°C") })
        const facts = [
            row(qsTr("Base speed"), ghz(d.baseMhz)),
            row(qsTr("Max speed"), ghz(d.maxMhz)),
            row(qsTr("Sockets"), d.sockets > 0 ? d.sockets : undefined),
            row(qsTr("Cores"), d.physicalCores > 0 ? d.physicalCores : undefined),
            row(qsTr("Logical processors"), d.logicalCpus > 0 ? d.logicalCpus : undefined),
            row(qsTr("Virtualization"), d.virtualization ? qsTr("Supported") : qsTr("Not detected")),
            row(qsTr("L1 cache"), kbText(d.l1Kb)),
            row(qsTr("L2 cache"), kbText(d.l2Kb)),
            row(qsTr("L3 cache"), kbText(d.l3Kb)),
            row(qsTr("Power limit"), d.powerLimitW !== undefined ? d.powerLimitW.toFixed(0) + " W" : undefined),
            row(qsTr("Up time"), Theme.formatUptime(d.uptimeSeconds)),
            row(qsTr("Load avg (1/5/15)"),
                d.load1 !== undefined && d.load5 !== undefined && d.load15 !== undefined
                ? d.load1.toFixed(2) + " / " + d.load5.toFixed(2) + " / " + d.load15.toFixed(2) : undefined)
        ]
        return {
            kind: "cpu", title: qsTr("CPU"), color: Theme.cpuColor, icon: "▣",
            tile: pct(util) + "  " + ghz(freq.value),
            heading: or(d.cpuModel, qsTr("Processor")),
            tileId1: util.sensorId, tileFixed: 100,
            views: [
                view(qsTr("Overall utilization"), [
                    charts([usageChart], 1, { sideTiles: true, height: 300 }),
                    { kind: "cpuRow2", freqChart: freqChart, tempChart: tempChart },
                    { kind: "strip", cols: 4, facts: facts }
                ]),
                view(qsTr("Logical processors"), [{ kind: "cores", columns: 8, cellHeight: 70 }]),
                view(qsTr("Temperature"), [charts([chartDef(qsTr("Temperature"), temp.id || "", "°C", { fixedMax: 100, valueText: num(temp, 0, "°C") })], 1, { height: 320 })]),
                view(qsTr("Frequencies"), [charts([freqChart], 1, { height: 320 })]),
                view(qsTr("Details"), [{ kind: "strip", cols: 2, facts: facts }])
            ],
            tiles: [
                tile(qsTr("Utilization"), pct(util), "▣", { sparkId: util.sensorId, sparkFixedMax: 100 }),
                tile(qsTr("Speed"), ghz(freq.value), "◔", { sparkId: freq.sensorId }),
                tile(qsTr("Temperature"), num(temp, 0, "°C"), "♨", { sparkId: temp.id || "", accent: Theme.hot }),
                tile(qsTr("Processes"), app.processCount > 0 ? app.processCount : "…", "▤"),
                tile(qsTr("Threads"), app.processCount > 0 ? app.threadCount : "…", "≋"),
                tile(qsTr("Handles"), d.fileHandles !== undefined ? Math.round(d.fileHandles) : Theme.na, "⌗"),
                tile(qsTr("Sockets"), d.sockets > 0 ? d.sockets : Theme.na, "▢"),
                tile(qsTr("Cores"), d.physicalCores > 0 ? d.physicalCores : Theme.na, "◫"),
                tile(qsTr("Logical processors"), d.logicalCpus > 0 ? d.logicalCpus : Theme.na, "▦")
            ]
        }
    }

    function memoryItem(d) {
        const usage = h(6), total = d.memTotalGb
        const used = d.memUsedGb, cached = d.memCachedGb, buffers = d.memBuffersGb
        const free = total !== undefined && used !== undefined && cached !== undefined && buffers !== undefined
                     ? Math.max(0, total - used - cached - buffers) : undefined
        const seg = (label, value, color) => ({ label: label,
            value: value === undefined ? Theme.na : Theme.gb(value, 1) + (total > 0 ? " (" + Math.round(value / total * 100) + "%)" : ""),
            fraction: value !== undefined && total > 0 ? value / total : 0, color: color })
        const composition = {
            kind: "bar", title: qsTr("Memory composition"), totalText: qsTr("Total") + ": " + Theme.gb(total, 1),
            segments: [seg(qsTr("In use"), used, Theme.memoryColor), seg(qsTr("Cached"), cached, Qt.lighter(Theme.memoryColor, 1.7)),
                       seg(qsTr("Free"), free, "#4a5d8a"), seg(qsTr("Buffers"), buffers, Theme.diskColor)]
        }
        const facts = [
            row(qsTr("Total"), Theme.gb(total, 1)),
            row(qsTr("Usage"), pct(usage)),
            row(qsTr("Committed"),
                d.memCommittedGb !== undefined && d.memCommitLimitGb !== undefined
                ? d.memCommittedGb.toFixed(1) + " / " + Theme.gb(d.memCommitLimitGb, 1) : undefined),
            row(qsTr("Slab (reclaimable)"), Theme.gbAuto(d.memSlabReclGb)),
            row(qsTr("Slab (unreclaim.)"), Theme.gbAuto(d.memSlabUnreclGb)),
            row(qsTr("Shared (tmpfs)"), Theme.gbAuto(d.memSharedGb)),
            row(qsTr("Pending write-back"), Theme.gbAuto(d.memDirtyGb)),
            row(qsTr("Swap in use"),
                d.swapTotalGb !== undefined && d.swapTotalGb > 0
                ? Theme.gb(d.swapUsedGb, 1) + " / " + Theme.gb(d.swapTotalGb, 1) : qsTr("No swap"))
        ]
        return {
            kind: "memory", title: qsTr("Memory"), color: Theme.memoryColor, icon: "▥",
            tile: used !== undefined && total !== undefined
                  ? Theme.gb(used) + " / " + Theme.gb(total, 1) + " (" + pct(usage) + ")" : pct(usage),
            heading: total !== undefined ? Theme.gb(total, 1) : "",
            tileId1: usage.sensorId, tileFixed: 100,
            views: [
                view(qsTr("Memory usage"), [
                    charts([chartDef(qsTr("Memory usage"), usage.sensorId, "%", { fixedMax: 100 })], 1, { height: 270 }),
                    { kind: "tiles", cols: 4 },
                    composition,
                    { kind: "memoryModules" },
                    { kind: "strip", cols: 2, facts: facts }
                ]),
                view(qsTr("Composition"), [composition]),
                view(qsTr("Details"), [{ kind: "memoryModules" }, { kind: "strip", cols: 2, facts: facts }])
            ],
            tiles: [
                tile(qsTr("In use"), Theme.gb(used, 1), "▥", { bar: frac(used, total) }),
                tile(qsTr("Available"), Theme.gb(d.memAvailableGb, 1), "◧", { bar: frac(d.memAvailableGb, total) }),
                tile(qsTr("Total"), Theme.gb(total, 1), "▩", { bar: total !== undefined ? 1 : -1 }),
                tile(qsTr("Cached"), Theme.gb(cached, 1), "▤", { bar: frac(cached, total) }),
                tile(qsTr("Committed"), Theme.gb(d.memCommittedGb, 1), "◈"),
                tile(qsTr("Buffers"), Theme.gbAuto(buffers), "≋"),
                tile(qsTr("Swap"),
                     d.swapTotalGb !== undefined && d.swapTotalGb > 0
                     ? Theme.gbAuto(d.swapUsedGb) + " / " + Theme.gb(d.swapTotalGb, 1) : qsTr("No swap"), "⇄",
                     { bar: frac(d.swapUsedGb, d.swapTotalGb) }),
                tile(qsTr("Shared (tmpfs)"), Theme.gbAuto(d.memSharedGb), "◫")
            ]
        }
    }

    function powerOnText(h) {
        if (h === undefined || h === null) return Theme.na
        return h >= 48 ? qsTr("%1 h (%2 days)").arg(Math.round(h).toLocaleString(Qt.locale(), "f", 0)).arg(Math.floor(h / 24))
                       : qsTr("%1 h").arg(Math.round(h))
    }
    function healthStatusText(status) {
        return status === "good" ? qsTr("Good") : status === "warning" ? qsTr("Warning") : status === "failing" ? qsTr("Failing") : Theme.na
    }
    // The Health tab: what the drive itself reports (SMART through udisks2, no root needed). Values the
    // drive does not report stay N/A.
    function healthView(h) {
        const ok = h.status === "good"
        const wear = h.percentUsed !== undefined && h.percentUsed !== null
        const warnings = h.criticalWarnings && h.criticalWarnings.length > 0 ? h.criticalWarnings.join(", ") : qsTr("None")
        const sections = []
        if (wear) {
            const used = Math.min(100, h.percentUsed)
            sections.push({
                kind: "bar", title: qsTr("Wear"), totalText: qsTr("Estimated by the drive"),
                segments: [
                    { label: qsTr("Used"), value: h.percentUsed + "%", fraction: used / 100, color: h.percentUsed >= 90 ? Theme.hot : h.percentUsed >= 70 ? Theme.warn : Theme.diskColor },
                    { label: qsTr("Remaining"), value: Math.max(0, 100 - h.percentUsed) + "%", fraction: 0, color: "#4a5d8a" }
                ]
            })
        }
        const known = v => v !== undefined && v !== null
        const statusRows = [row(qsTr("Status"), healthStatusText(h.status))]
        if (h.nvme) {
            statusRows.push(row(qsTr("Spare capacity"), known(h.availableSpare) ? h.availableSpare + "%" : undefined),
                            row(qsTr("Media errors"), known(h.mediaErrors) ? count(h.mediaErrors) : undefined),
                            row(qsTr("Error log entries"), known(h.errorLogEntries) ? count(h.errorLogEntries) : undefined),
                            row(qsTr("Critical warnings"), warnings))
        } else {
            statusRows.push(row(qsTr("Bad sectors"), known(h.badSectors) ? count(h.badSectors) : undefined))
        }
        statusRows.push(row(qsTr("Self-test"), h.selfTest))
        const status = { title: qsTr("Health"), rows: statusRows }
        const usage = { title: qsTr("Usage over the drive's life"), rows: [
            row(qsTr("Power-on time"), powerOnText(h.powerOnHours)),
            row(qsTr("Data written"), bytes(h.dataWrittenBytes)),
            row(qsTr("Data read"), bytes(h.dataReadBytes)),
            row(qsTr("Power cycles"), h.powerCycles !== undefined && h.powerCycles !== null ? count(h.powerCycles) : undefined),
            row(qsTr("Unsafe shutdowns"), h.unsafeShutdowns !== undefined && h.unsafeShutdowns !== null ? count(h.unsafeShutdowns) : undefined) ] }
        sections.push({ kind: "cards", cards: [status, usage] })
        return view(qsTr("Health"), sections)
    }

    function diskItem(disk, i) {
        const type = disk.rotational ? qsTr("HDD") : (disk.name.indexOf("nvme") === 0 ? qsTr("SSD (NVMe)") : qsTr("SSD"))
        const hasTemp = disk.temp.id !== ""
        const used = disk.usedBytes, size = disk.sizeBytes
        const free = used !== undefined && size > 0 ? Math.max(0, size - used) : undefined
        const usageBar = {
            kind: "bar", title: qsTr("Disk usage"), totalText: qsTr("Total") + ": " + Theme.formatSize(size),
            segments: [
                { label: qsTr("Used"), value: used !== undefined ? Theme.formatSize(used) + " (" + Math.round(used / size * 100) + "%)" : Theme.na,
                  fraction: used !== undefined && size > 0 ? used / size : 0, color: Theme.diskColor },
                { label: qsTr("Free"), value: free !== undefined ? Theme.formatSize(free) + " (" + Math.round(free / size * 100) + "%)" : Theme.na,
                  fraction: 0, color: "#4a5d8a" }
            ]
        }
        const deviceInfo = { title: qsTr("Device information"), rows: [
            row(qsTr("Model"), disk.model), row(qsTr("Serial number"), disk.serial), row(qsTr("Firmware"), disk.firmware),
            row(qsTr("Interface"), disk.interface), row(qsTr("Device"), "/dev/" + disk.name),
            row(qsTr("Logical sector size"), disk.blockSize > 0 ? disk.blockSize + " B" : undefined), row(qsTr("Type"), type) ] }
        const stats = { title: qsTr("Statistics since boot"), rows: [
            row(qsTr("Data read"), bytes(disk.readTotalBytes)), row(qsTr("Data written"), bytes(disk.writeTotalBytes)),
            row(qsTr("Capacity"), Theme.formatSize(size)) ] }
        const activity = [
            chartDef(qsTr("Active time"), disk.busy.id, "%", { fixedMax: 100, valueText: pct(disk.busy) }),
            chartDef(qsTr("Read speed"), disk.read.id, "MB/s", { valueText: rate(disk.read), footerText: peakRate(disk.read) }),
            chartDef(qsTr("Write speed"), disk.write.id, "MB/s", { valueText: rate(disk.write), footerText: peakRate(disk.write) })
        ]
        const tempChart = chartDef(qsTr("Temperature"), disk.temp.id, "°C", { fixedMax: 100, valueText: num(disk.temp, 0, "°C") })
        const views = [view(qsTr("Activity"), [charts(activity, 3), { kind: "tiles", cols: 6 }, usageBar, { kind: "cards", cards: [deviceInfo, stats] }])]
        if (hasTemp)
            views.push(view(qsTr("Temperature"), [charts([tempChart], 1, { height: 320 })]))
        if (disk.health && disk.health.available)
            views.push(healthView(disk.health))
        views.push(view(qsTr("Details"), [{ kind: "cards", cards: [deviceInfo, stats] }]))
        const tiles = [
            tile(qsTr("Active time"), pct(disk.busy), "◉", { sparkId: disk.busy.id, sparkFixedMax: 100 }),
            tile(qsTr("Read speed"), rate(disk.read), "↓", { sparkId: disk.read.id, accent: Theme.ok }),
            tile(qsTr("Write speed"), rate(disk.write), "↑", { sparkId: disk.write.id, accent: Theme.warn })
        ]
        if (hasTemp)
            tiles.push(tile(qsTr("Temperature"), num(disk.temp, 0, "°C"), "♨", { sparkId: disk.temp.id, accent: Theme.hot }))
        tiles.push(tile(qsTr("Capacity"), Theme.formatSize(size), "▭"), tile(qsTr("Type"), type, "▤"))
        return {
            kind: "disk", title: qsTr("Disk %1 (%2)").arg(i).arg(disk.name), color: Theme.diskColor, icon: "▭",
            tile: type + "  " + pct(disk.busy),
            heading: or(disk.model, disk.name) + (size > 0 ? "  •  " + Theme.formatSize(size) : ""),
            tileId1: disk.read.id, tileId2: disk.write.id, tileFixed: 0,
            views: views, tiles: tiles
        }
    }

    // Docker/podman/libvirt bridges and veth pairs come in dozens and say nothing about the
    // machine; VPN and tunnel interfaces (CloudflareWARP, tun0, wg0, tailscale0) do, so they stay.
    function isContainerInterface(name) {
        return /^(veth|br-|docker|virbr|vnet|lxc|lxd|cni|flannel|cali|kube-|podman|tap[0-9a-f]{6})/.test(name)
    }
    function kindText(kind) {
        return kind === "Wi-Fi" ? qsTr("Wi-Fi") : kind === "Ethernet" ? qsTr("Ethernet") : kind === "Virtual" ? qsTr("Virtual") : or(kind, Theme.na)
    }

    function netItem(net) {
        const wifi = net.wifi !== undefined
        const virtual = net.kind === "Virtual"
        const w = wifi ? net.wifi : ({})
        const hasSignal = net.signal.id !== ""
        const speed = wifi && w.bitrateMbps > 0 ? w.bitrateMbps : net.speedMbps
        const connected = net.state === "up"
        const throughput = chartDef(qsTr("Throughput"), net.down.id, "MB/s",
                                    { id2: net.up.id, legend: [qsTr("Download"), qsTr("Upload")],
                                      footerText: [peakRate(net.down) ? "↓ " + peakRate(net.down) : "", peakRate(net.up) ? "↑ " + peakRate(net.up) : ""].filter(x => x).join("    ") })
        const rx = net.rxBytes, tx = net.txBytes
        const tiles = [
            tile(qsTr("Download"), rate(net.down), "↓", { sparkId: net.down.id, accent: Theme.networkColor }),
            tile(qsTr("Upload"), rate(net.up), "↑", { sparkId: net.up.id, accent: Theme.warn })
        ]
        // Highest rate seen since the app started
        tiles.push(tile(qsTr("Peak download"), net.down.max !== undefined && net.down.max !== null ? Theme.formatRate(net.down.max) : Theme.na, "⇡"),
                   tile(qsTr("Peak upload"), net.up.max !== undefined && net.up.max !== null ? Theme.formatRate(net.up.max) : Theme.na, "⇡", { accent: Theme.warn }))
        if (wifi) {
            tiles.push(tile(qsTr("Total downloaded"), bytes(rx), "☁"), tile(qsTr("Total sent"), bytes(tx), "☁"))
            tiles.push(tile(qsTr("Signal level"), ok(net.signal) ? net.signal.value.toFixed(0) + " dBm" : Theme.na, "◉",
                            { sparkId: net.signal.id, accent: Theme.ok }))
            tiles.push(tile(qsTr("Link speed"), linkSpeed(speed), "⇄"),
                       tile(qsTr("State"), stateText(net.state), "◈", { accent: connected ? Theme.ok : Theme.textMuted }),
                       tile(qsTr("Band"), or(w.band, Theme.na), "◫"))
        } else {
            tiles.push(tile(qsTr("Link speed"), linkSpeed(speed), "⇄"),
                       tile(qsTr("State"), stateText(net.state), "◈", { accent: connected ? Theme.ok : Theme.textMuted }),
                       tile(qsTr("Connection type"), kindText(net.kind), "▤"),
                       tile(qsTr("Total received"), bytes(rx), "↓"), tile(qsTr("Total sent"), bytes(tx), "↑"),
                       tile(qsTr("Packets in"), count(net.rxPackets), "↓"), tile(qsTr("Packets out"), count(net.txPackets), "↑"),
                       tile(qsTr("Errors"), count(net.errors), "✕", { accent: net.errors > 0 ? Theme.hot : Theme.textMuted }))
        }
        const adapter = { title: wifi ? qsTr("Adapter info") : qsTr("Interface info"), rows: [
            row(qsTr("Name"), net.name), row(qsTr("Vendor"), net.vendor), row(qsTr("Model"), net.model),
            row(qsTr("MAC address"), net.mac), row(qsTr("Driver"), net.driver), row(qsTr("Bus address"), net.busAddress) ] }
        const params = wifi
            ? { title: qsTr("Network info"), rows: [
                  row(qsTr("SSID"), w.connected ? w.ssid : undefined), row(qsTr("Mode"), w.mode),
                  row(qsTr("Frequency"), w.frequencyMhz > 0 ? (w.frequencyMhz / 1000).toFixed(2) + " GHz" : undefined),
                  row(qsTr("Channel"), w.channel > 0 ? w.channel : undefined), row(qsTr("Security"), w.security),
                  row("BSSID", w.bssid), row(qsTr("IPv4"), net.ipv4), row(qsTr("IPv6"), net.ipv6) ] }
            : { title: qsTr("Network parameters"), rows: [
                  row(qsTr("Connection type"), kindText(net.kind)), row(qsTr("Link speed"), linkSpeed(speed)),
                  row(qsTr("Duplex"), net.duplex === "full" ? qsTr("Full duplex") : net.duplex === "half" ? qsTr("Half duplex") : undefined), row(qsTr("MTU"), net.mtu), row(qsTr("State"), stateText(net.state)),
                  row(qsTr("IPv4"), net.ipv4), row(qsTr("IPv6"), net.ipv6) ] }
        const views = [view(qsTr("Throughput"), [charts([throughput], 1, { height: 270 }), { kind: "tiles", cols: wifi ? 4 : 5 },
                            { kind: "cards", cards: wifi ? [params, adapter] : [adapter, params] }])]
        if (hasSignal)
            views.push(view(qsTr("Signal strength"), [charts([chartDef(qsTr("Signal strength"), net.signal.id, "dBm",
                                                                       { valueText: ok(net.signal) ? net.signal.value.toFixed(0) + " dBm" : "" })], 1, { height: 320 })]))
        views.push(view(qsTr("Details"), [{ kind: "cards", cards: wifi ? [params, adapter] : [adapter, params] }]))
        return {
            kind: "net", title: wifi ? qsTr("Wi-Fi") : virtual ? net.name : qsTr("Ethernet"), color: Theme.networkColor, icon: wifi ? "◉" : "⇄",
            tile: "↓ " + rate(net.down) + "   ↑ " + rate(net.up),
            heading: (wifi && w.connected ? w.ssid + "  •  " : "") + (virtual ? qsTr("Virtual interface") : or(net.model, net.name)),
            tileId1: net.down.id, tileId2: net.up.id, tileFixed: 0,
            views: views, tiles: tiles
        }
    }

    // "NVIDIA GPU", "AMD GPU", "Intel GPU" when there are several cards, so they can be told apart.
    function gpuTitle(gpu, i, count) {
        if (count <= 1) return qsTr("GPU")
        const n = (gpu.name || "").toLowerCase()
        const vendor = n.indexOf("nvidia") >= 0 ? "NVIDIA" : (n.indexOf("amd") >= 0 || n.indexOf("radeon") >= 0 || n.indexOf("ati") >= 0) ? "AMD"
                       : n.indexOf("intel") >= 0 ? "Intel" : ""
        return vendor !== "" ? qsTr("%1 GPU").arg(vendor) : qsTr("GPU %1").arg(i)
    }
    function fanText(ref) { return ok(ref) ? ref.value.toFixed(0) + " " + (ref.unit || "%") : Theme.na }
    // What the list tile shows: load and temperature, or whatever the driver does report (Intel: clock).
    function gpuSummary(gpu) {
        const parts = []
        if (ok(gpu.util)) parts.push(pct(gpu.util))
        if (ok(gpu.temp)) parts.push(ok(gpu.util) ? "(" + num(gpu.temp, 0, "°C") + ")" : num(gpu.temp, 0, "°C"))
        if (parts.length === 0 && ok(gpu.clock)) parts.push(num(gpu.clock, 0, "MHz"))
        return parts.length > 0 ? parts.join("  ") : Theme.na
    }

    function gpuItem(gpu, i, count) {
        const memMax = ok(gpu.memTotal) ? gpu.memTotal.value : 0
        const info = gpu.info || {}
        const util = chartDef(qsTr("Utilization"), gpu.util.id, "%", { fixedMax: 100, valueText: pct(gpu.util) })
        const mem = chartDef(qsTr("Dedicated GPU memory"), gpu.mem.id, "GB",
                             { fixedMax: memMax, valueText: ok(gpu.mem) ? Theme.gb(gpu.mem.value, 1) + (memMax > 0 ? " / " + Theme.gb(memMax, 0) : "") : "" })
        const temp = chartDef(qsTr("Temperature"), gpu.temp.id, "°C", { fixedMax: 100, valueText: num(gpu.temp, 0, "°C") })
        const clock = chartDef(qsTr("Clock"), gpu.clock.id, "MHz", { valueText: num(gpu.clock, 0, "MHz") })
        const power = chartDef(qsTr("Power"), gpu.power.id, "W", { valueText: num(gpu.power, 1, "W") })
        const compBar = (label, ref) => ({ label: label, fraction: ok(ref) ? ref.value / 100 : -1, value: pct(ref) })
        const infoCard = { title: qsTr("Graphics card information"), rows: [
            row(qsTr("Name"), gpu.name), row(qsTr("Video memory"), memMax > 0 ? Theme.gb(memMax, 0) : undefined),
            row(qsTr("Driver version"), info.driver), row(qsTr("CUDA version"), info.cuda),
            row(qsTr("PCI ID"), info.pci_id), row(qsTr("Subsystem ID"), info.pci_subsystem),
            row(qsTr("Bus address"), info.pci_bus), row(qsTr("Power limit"), ok(gpu.powerLimit) ? num(gpu.powerLimit, 0, "W") : undefined) ] }
        const components = { title: qsTr("Component load"), bars: true, rows: [
            compBar(qsTr("GPU core"), gpu.util), compBar(qsTr("Memory controller"), gpu.utilMem),
            compBar(qsTr("Video encode"), gpu.enc), compBar(qsTr("Video decode"), gpu.dec) ] }
        return {
            kind: "gpu", title: gpuTitle(gpu, i, count), color: Theme.gpuColor, icon: "▦",
            tile: gpuSummary(gpu),
            heading: gpu.name,
            tileId1: gpu.util.id !== "" ? gpu.util.id : (gpu.clock.id !== "" ? gpu.clock.id : gpu.temp.id),
            tileFixed: gpu.util.id !== "" ? 100 : 0,
            views: [
                view(qsTr("Overview"), [
                    liveCharts([util, mem], 2),
                    liveCharts([temp, clock, power], 3, { height: 190 }),
                    { kind: "tiles", cols: 4 },
                    { kind: "cards", cards: [infoCard, components] }
                ].filter(x => x !== null)),
                view(qsTr("Memory"), [liveCharts([mem], 1, { height: 320 })].filter(x => x !== null)),
                view(qsTr("Temperature"), [liveCharts([temp], 1, { height: 320 })].filter(x => x !== null)),
                view(qsTr("Power"), [liveCharts([power, clock], 2, { height: 300 })].filter(x => x !== null)),
                view(qsTr("Details"), [{ kind: "cards", cards: [infoCard, components] }])
            ].filter(v => v.sections.length > 0),
            tiles: [
                tile(qsTr("Utilization"), pct(gpu.util), "◉", { sparkId: gpu.util.id, sparkFixedMax: 100 }),
                tile(qsTr("Memory"), ok(gpu.mem) ? Theme.gb(gpu.mem.value, 1) + (memMax > 0 ? " / " + Theme.gb(memMax, 0) : "") : Theme.na, "▥",
                     { bar: frac(ok(gpu.mem) ? gpu.mem.value : undefined, memMax), accent: Theme.memoryColor }),
                tile(qsTr("Temperature"), num(gpu.temp, 0, "°C"), "♨", { sparkId: gpu.temp.id, accent: Theme.hot }),
                tile(qsTr("Power"), num(gpu.power, 1, "W"), "↯", { sparkId: gpu.power.id, accent: Theme.warn }),
                tile(qsTr("Current clock"), num(gpu.clock, 0, "MHz"), "◔", { sparkId: gpu.clock.id }),
                tile(qsTr("Max clock"), num(gpu.clockMax, 0, "MHz"), "◔"),
                tile(qsTr("Memory controller"), pct(gpu.utilMem), "▥", { sparkId: gpu.utilMem.id, sparkFixedMax: 100, accent: Theme.memoryColor }),
                tile(qsTr("Fan"), fanText(gpu.fan), "✻")
            ]
        }
    }

    function batteryStatus(code) {
        return ({ Charging: qsTr("Charging"), Discharging: qsTr("Discharging"), Full: qsTr("Full"),
                  "Not charging": qsTr("Not charging") })[code] || or(code, Theme.na)
    }
    function hoursText(hours) {
        if (!(hours > 0) || !isFinite(hours) || hours > 99) return Theme.na
        const h = Math.floor(hours), m = Math.round((hours - h) * 60)
        return h + ":" + (m < 10 ? "0" : "") + m
    }

    function batteryItem(b, i, count) {
        const info = b.info || {}
        const charge = b.charge, power = b.power
        const status = info.status
        const energy = ok(b.energy) ? b.energy.value : undefined
        const full = ok(b.energy_full) ? b.energy_full.value : undefined
        const watts = ok(power) ? power.value : 0
        // Time left (discharging) or until full (charging), from the current power draw.
        let hours = NaN, timeLabel = qsTr("Time remaining")
        if (energy !== undefined && watts > 0.5) {
            if (status === "Discharging") hours = energy / watts
            else if (status === "Charging" && full !== undefined) { hours = (full - energy) / watts; timeLabel = qsTr("Time to full") }
        }
        const chargeChart = chartDef(qsTr("Charge level"), charge.id, "%", { fixedMax: 100, valueText: pct(charge) })
        const powerChart = chartDef(qsTr("Power"), power.id, "W", { valueText: num(power, 1, "W") })
        const infoCard = { title: qsTr("Battery information"), rows: [
            row(qsTr("Manufacturer"), info.manufacturer), row(qsTr("Model"), info.model), row(qsTr("Technology"), info.technology),
            row(qsTr("Serial number"), info.serial), row(qsTr("Cycles"), info.cycles),
            row(qsTr("Design capacity"), ok(b.energy_design) ? num(b.energy_design, 1, "Wh") : undefined),
            row(qsTr("Full charge capacity"), full !== undefined ? full.toFixed(1) + " Wh" : undefined),
            row(qsTr("Health"), ok(b.health) ? pct(b.health) : undefined) ] }
        const powerCard = { title: qsTr("Power"), rows: [
            row(qsTr("Status"), batteryStatus(status)),
            row(qsTr("AC adapter"), info.ac === "online" ? qsTr("Online") : info.ac === "offline" ? qsTr("Offline") : undefined),
            row(qsTr("Voltage"), num(b.voltage, 2, "V")), row(qsTr("Power"), num(power, 1, "W")),
            row(timeLabel, hoursText(hours)) ] }
        return {
            kind: "battery", title: count > 1 ? qsTr("Battery %1").arg(i) : qsTr("Battery"), color: Theme.batteryColor, icon: "▮",
            tile: pct(charge) + "  " + batteryStatus(status),
            heading: [info.manufacturer, info.model].filter(x => x).join(" ") + (info.technology ? "  •  " + info.technology : ""),
            tileId1: charge.id, tileFixed: 100,
            views: [
                view(qsTr("Overview"), [
                    charts([chargeChart, powerChart], 2, { height: 260 }),
                    { kind: "tiles", cols: 4 },
                    { kind: "cards", cards: [infoCard, powerCard] }
                ]),
                view(qsTr("Details"), [{ kind: "cards", cards: [infoCard, powerCard] }])
            ],
            tiles: [
                tile(qsTr("Charge"), pct(charge), "▮", { sparkId: charge.id, sparkFixedMax: 100 }),
                tile(qsTr("Status"), batteryStatus(status), "◈", { accent: status === "Charging" ? Theme.ok : status === "Discharging" ? Theme.warn : Theme.batteryColor }),
                tile(qsTr("Power"), num(power, 1, "W"), "↯", { sparkId: power.id, accent: Theme.warn }),
                tile(qsTr("Voltage"), num(b.voltage, 2, "V"), "⌁", { sparkId: b.voltage.id, accent: Theme.accent }),
                tile(qsTr("Health"), ok(b.health) ? pct(b.health) : Theme.na, "✚", { bar: ok(b.health) ? b.health.value / 100 : -1 }),
                tile(qsTr("Cycles"), or(info.cycles, Theme.na), "⟳"),
                tile(qsTr("Energy"), energy !== undefined && full !== undefined ? energy.toFixed(1) + " / " + full.toFixed(1) + " Wh" : Theme.na, "▥",
                     { bar: frac(energy, full) }),
                tile(timeLabel, hoursText(hours), "◔")
            ]
        }
    }

    function resources() {
        const d = app.details
        const list = [cpuItem(d), memoryItem(d)]
        const disks = d.disks || []
        for (let i = 0; i < disks.length; i++) list.push(diskItem(disks[i], i))
        for (const n of (d.nets || []))
            if (!isContainerInterface(n.name)) list.push(netItem(n))
        const gpus = d.gpus || []
        for (let i = 0; i < gpus.length; i++) list.push(gpuItem(gpus[i], i, gpus.length))
        const batteries = d.batteries || []
        for (let i = 0; i < batteries.length; i++) list.push(batteryItem(batteries[i], i, batteries.length))
        return list
    }

    readonly property var items: resources()
    readonly property var current: items[Math.min(selected, items.length - 1)]
    readonly property var currentView: current.views[Math.min(viewIndex, current.views.length - 1)]

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ---- resource list ---------------------------------------------------
        ScrollView {
            Layout.fillHeight: true
            Layout.preferredWidth: 262
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: 262 - 16
                x: 12
                spacing: 8

                PageTitle {
                    text: qsTr("Performance")
                    font.pixelSize: 20
                    Layout.fillWidth: true
                    Layout.topMargin: 14
                    Layout.bottomMargin: 4
                }

                Repeater {
                    model: page.items.length
                    delegate: Rectangle {
                        id: tileItem
                        required property int index
                        readonly property var res: page.items[index]
                        readonly property bool active: page.selected === index

                        Layout.fillWidth: true
                        implicitHeight: 62
                        radius: 8
                        color: active ? Qt.rgba(res.color.r, res.color.g, res.color.b, 0.12)
                                      : hover.hovered ? Theme.surfaceAlt : "transparent"
                        border.width: active ? 2 : 1
                        border.color: active ? res.color : "transparent"

                        HoverHandler { id: hover }
                        TapHandler { onTapped: page.selected = tileItem.index }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 7
                            spacing: 10
                            Rectangle {
                                Layout.preferredWidth: 84
                                Layout.fillHeight: true
                                color: Theme.background
                                border.color: tileItem.res.color
                                TemperatureChart {
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    sensorId: tileItem.res.tileId1
                                    secondSensorId: tileItem.res.tileId2 !== undefined ? tileItem.res.tileId2 : ""
                                    lineColor: tileItem.res.color
                                    secondColor: Qt.lighter(tileItem.res.color, 1.6)
                                    fixedMin: 0
                                    fixedMax: tileItem.res.tileFixed
                                    minSpan: 0.5
                                    floorZero: true
                                    zeroBased: true
                                    filled: true
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Text {
                                    text: tileItem.res.title
                                    color: Theme.text
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: tileItem.res.tile
                                    color: Theme.textMuted
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }
                Item { Layout.preferredHeight: 14 }
            }
        }

        Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: Theme.border }

        // ---- details ---------------------------------------------------------
        ScrollView {
            id: detail
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            readonly property bool wide: availableWidth >= 940

            ColumnLayout {
                width: detail.availableWidth
                spacing: 14

                // Header: icon, title, subtitle on the left, the tabs of this resource on the right.
                GridLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.topMargin: 18
                    columns: detail.wide ? 2 : 1
                    columnSpacing: 14
                    rowSpacing: 12

                    Row {
                        Layout.fillWidth: true
                        spacing: 14
                        Rectangle {
                            width: 46; height: 46; radius: 12
                            color: Qt.rgba(page.current.color.r, page.current.color.g, page.current.color.b, 0.14)
                            border.color: page.current.color
                            Text {
                                anchors.centerIn: parent
                                text: page.current.icon
                                color: page.current.color
                                font.pixelSize: 24
                            }
                        }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            Text {
                                text: page.current.title
                                color: Theme.text
                                font.pixelSize: 24
                                font.weight: Font.DemiBold
                            }
                            Text {
                                text: page.current.heading
                                color: Theme.textMuted
                                font.pixelSize: 13
                            }
                        }
                    }

                    PillTabs {
                        Layout.alignment: detail.wide ? Qt.AlignRight | Qt.AlignVCenter : Qt.AlignLeft
                        titles: page.current.views.map(v => v.title)
                        current: Math.min(page.viewIndex, page.current.views.length - 1)
                        accent: page.current.color
                        onSelected: index => page.viewIndex = index
                    }
                }

                // The sections of the selected tab, top to bottom.
                Repeater {
                    model: page.currentView.sections.length
                    delegate: Item {
                        id: sec
                        required property int index
                        readonly property var section: page.currentView.sections[index]
                        readonly property string kind: section.kind
                        Layout.fillWidth: true
                        Layout.leftMargin: 24
                        Layout.rightMargin: 24
                        Layout.bottomMargin: index === page.currentView.sections.length - 1 ? 24 : 0
                        implicitHeight: kind === "charts" ? chartsBlock.implicitHeight
                                      : kind === "tiles" ? tilesBlock.implicitHeight
                                      : kind === "bar" ? barBlock.implicitHeight
                                      : kind === "cards" ? cardsBlock.implicitHeight
                                      : kind === "cpuRow2" ? cpuBlock.implicitHeight
                                      : kind === "memoryModules" ? modulesBlock.implicitHeight
                                      : kind === "cores" ? coresBlock.implicitHeight
                                      : stripBlock.implicitHeight

                        // charts (optionally with the metric tiles beside them)
                        GridLayout {
                            id: chartsBlock
                            visible: sec.kind === "charts"
                            width: parent.width
                            columns: detail.wide && sec.section.sideTiles ? 2 : 1
                            columnSpacing: 14
                            rowSpacing: 14

                            GridLayout {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignTop
                                columns: detail.wide ? Math.max(1, sec.section.cols || 1) : 1
                                columnSpacing: 14
                                rowSpacing: 14
                                Repeater {
                                    model: sec.kind === "charts" ? sec.section.charts.length : 0
                                    delegate: ChartCard {
                                        required property int index
                                        readonly property var def: sec.section.charts[index]
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: sec.section.height > 0 ? sec.section.height
                                                                : (sec.section.charts.length === 1 && !sec.section.sideTiles ? 280 : 240)
                                        title: def.title
                                        footerText: def.footerText
                                        sensorId: def.id1
                                        secondSensorId: def.id2
                                        accent: page.current.color
                                        fixedMax: def.fixedMax
                                        unit: def.unit
                                        legend: def.legend
                                        valueText: def.valueText
                                    }
                                }
                            }

                            GridLayout {
                                visible: sec.section.sideTiles === true
                                Layout.alignment: Qt.AlignTop
                                Layout.fillWidth: !detail.wide
                                Layout.preferredWidth: detail.wide ? 3 * 158 + 2 * 10 : -1
                                columns: 3
                                columnSpacing: 10
                                rowSpacing: 10
                                Repeater {
                                    model: sec.section.sideTiles === true ? page.current.tiles.length : 0
                                    delegate: StatTile {
                                        required property int index
                                        readonly property var info: page.current.tiles[index]
                                        Layout.fillWidth: true
                                        Layout.preferredWidth: 150
                                        label: info.label
                                        value: info.value
                                        icon: info.icon
                                        accent: info.accent !== undefined ? info.accent : page.current.color
                                        sparkId: info.sparkId
                                        sparkFixedMax: info.sparkFixedMax
                                        bar: info.bar
                                    }
                                }
                            }
                        }

                        // metric tiles on their own row(s)
                        GridLayout {
                            id: tilesBlock
                            visible: sec.kind === "tiles"
                            width: parent.width
                            columns: Math.max(2, Math.min(sec.section.cols || 4, Math.floor(parent.width / 170)))
                            columnSpacing: 10
                            rowSpacing: 10
                            Repeater {
                                model: sec.kind === "tiles" ? page.current.tiles.length : 0
                                delegate: StatTile {
                                    required property int index
                                    readonly property var info: page.current.tiles[index]
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 150
                                    label: info.label
                                    value: info.value
                                    icon: info.icon
                                    accent: info.accent !== undefined ? info.accent : page.current.color
                                    sparkId: info.sparkId
                                    sparkFixedMax: info.sparkFixedMax
                                    bar: info.bar
                                }
                            }
                        }

                        // stacked bar (memory composition, disk usage)
                        BarCard {
                            id: barBlock
                            visible: sec.kind === "bar"
                            width: parent.width
                            title: sec.kind === "bar" ? sec.section.title : ""
                            totalText: sec.kind === "bar" ? sec.section.totalText : ""
                            segments: sec.kind === "bar" ? sec.section.segments : []
                        }

                        // info cards
                        GridLayout {
                            id: cardsBlock
                            visible: sec.kind === "cards"
                            width: parent.width
                            columns: detail.wide ? Math.max(1, sec.kind === "cards" ? sec.section.cards.length : 1) : 1
                            columnSpacing: 14
                            rowSpacing: 14
                            Repeater {
                                model: sec.kind === "cards" ? sec.section.cards.length : 0
                                delegate: InfoCard {
                                    required property int index
                                    readonly property var def: sec.section.cards[index]
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 100
                                    Layout.alignment: Qt.AlignTop
                                    title: def.title
                                    rows: def.rows
                                    bars: def.bars === true
                                    accent: page.current.color
                                    labelWidth: def.bars === true ? 130 : 150
                                }
                            }
                        }

                        // CPU: per-core card on the left, frequency + temperature on the right
                        GridLayout {
                            id: cpuBlock
                            visible: sec.kind === "cpuRow2"
                            width: parent.width
                            columns: detail.wide ? 2 : 1
                            columnSpacing: 14
                            rowSpacing: 14
                            CoreGrid {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 560
                                Layout.alignment: Qt.AlignTop
                                title: qsTr("Usage per core")
                                columns: detail.wide ? 4 : 4
                                cellHeight: 44
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 400
                                Layout.alignment: Qt.AlignTop
                                spacing: 14
                                ChartCard {
                                    visible: sec.kind === "cpuRow2"
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 200
                                    title: sec.kind === "cpuRow2" ? sec.section.freqChart.title : ""
                                    sensorId: sec.kind === "cpuRow2" ? sec.section.freqChart.id1 : ""
                                    secondSensorId: sec.kind === "cpuRow2" ? sec.section.freqChart.id2 : ""
                                    accent: Theme.memoryColor
                                    unit: "MHz"
                                    valueText: sec.kind === "cpuRow2" ? sec.section.freqChart.valueText : ""
                                }
                                ChartCard {
                                    visible: sec.kind === "cpuRow2"
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 200
                                    title: sec.kind === "cpuRow2" ? sec.section.tempChart.title : ""
                                    sensorId: sec.kind === "cpuRow2" ? sec.section.tempChart.id1 : ""
                                    accent: Theme.warn
                                    unit: "°C"
                                    fixedMax: 100
                                    valueText: sec.kind === "cpuRow2" ? sec.section.tempChart.valueText : ""
                                }
                            }
                        }

                        // RAM modules from the firmware (needs administrator rights, so only on request)
                        Rectangle {
                            id: modulesBlock
                            visible: sec.kind === "memoryModules"
                            width: parent.width
                            implicitHeight: modulesColumn.implicitHeight + 28
                            radius: Theme.radius
                            color: Theme.surface
                            border.color: Theme.border
                            readonly property var mm: app.memoryModules
                            readonly property bool ready: mm.state === "ready"

                            ColumnLayout {
                                id: modulesColumn
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 10

                                Text { text: qsTr("Memory modules"); color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold }

                                // not loaded yet / loading / failed
                                ColumnLayout {
                                    visible: !modulesBlock.ready
                                    Layout.fillWidth: true
                                    spacing: 8
                                    Text {
                                        visible: modulesBlock.mm.state !== "error"
                                        Layout.fillWidth: true
                                        wrapMode: Text.WordWrap
                                        color: Theme.textMuted
                                        font.pixelSize: 12
                                        text: modulesBlock.mm.state === "loading"
                                              ? qsTr("Waiting for administrator authentication…")
                                              : qsTr("Slots, speed, form factor and type come from the firmware (SMBIOS), which only an administrator can read. The app asks for your password once and runs dmidecode; nothing is stored.")
                                    }
                                    Text {
                                        visible: modulesBlock.mm.state === "error"
                                        Layout.fillWidth: true
                                        wrapMode: Text.WordWrap
                                        color: Theme.warn
                                        font.pixelSize: 12
                                        text: modulesBlock.mm.error || ""
                                    }
                                    Button {
                                        visible: modulesBlock.mm.state !== "loading"
                                        text: modulesBlock.mm.state === "error" ? qsTr("Try again") : qsTr("Load module details")
                                        onClicked: app.loadMemoryModules()
                                    }
                                }

                                // loaded
                                GridLayout {
                                    visible: modulesBlock.ready
                                    Layout.fillWidth: true
                                    columns: detail.wide ? 2 : 1
                                    columnSpacing: 28
                                    rowSpacing: 14

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.preferredWidth: 100
                                        Layout.alignment: Qt.AlignTop
                                        spacing: 0
                                        Repeater {
                                            model: modulesBlock.ready ? [
                                                [qsTr("Used slots"), qsTr("%1 of %2").arg(modulesBlock.mm.slotsUsed).arg(modulesBlock.mm.slotsTotal)],
                                                [qsTr("Speed"), modulesBlock.mm.speedMts > 0 ? modulesBlock.mm.speedMts + " MT/s" : Theme.na],
                                                [qsTr("Form factor"), modulesBlock.mm.formFactor || Theme.na],
                                                [qsTr("Type"), modulesBlock.mm.type || Theme.na],
                                                [qsTr("Type detail"), modulesBlock.mm.typeDetail || Theme.na],
                                                [qsTr("Error correction"), modulesBlock.mm.errorCorrection || Theme.na]
                                            ] : []
                                            delegate: Item {
                                                required property var modelData
                                                Layout.fillWidth: true
                                                implicitHeight: 34
                                                Column {
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    Text { text: modelData[0]; color: Theme.textMuted; font.pixelSize: 11 }
                                                    Text { text: modelData[1]; color: Theme.text; font.pixelSize: 13 }
                                                }
                                            }
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.preferredWidth: 100
                                        Layout.alignment: Qt.AlignTop
                                        spacing: 6
                                        Text { text: qsTr("Installed modules"); color: Theme.textMuted; font.pixelSize: 11 }
                                        Repeater {
                                            model: modulesBlock.ready ? modulesBlock.mm.modules : []
                                            delegate: Rectangle {
                                                required property var modelData
                                                Layout.fillWidth: true
                                                implicitHeight: modCol.implicitHeight + 16
                                                radius: 6
                                                color: Theme.background
                                                Column {
                                                    id: modCol
                                                    anchors.fill: parent
                                                    anchors.margins: 8
                                                    spacing: 2
                                                    Text {
                                                        width: parent.width
                                                        elide: Text.ElideRight
                                                        color: Theme.text
                                                        font.pixelSize: 13
                                                        font.weight: Font.Medium
                                                        text: Theme.gb(modelData.sizeMb / 1024, 0) + "  ·  " + modelData.type + "  " + modelData.speedMts + " MT/s"
                                                    }
                                                    Text {
                                                        width: parent.width
                                                        elide: Text.ElideRight
                                                        color: Theme.textMuted
                                                        font.pixelSize: 11
                                                        text: [modelData.manufacturer, modelData.partNumber, modelData.locator].filter(x => x).join("  ·  ")
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // large per-core grid (the "Logical processors" tab)
                        CoreGrid {
                            id: coresBlock
                            visible: sec.kind === "cores"
                            width: parent.width
                            columns: sec.kind === "cores" ? sec.section.columns : 4
                            cellHeight: sec.kind === "cores" ? sec.section.cellHeight : 44
                        }

                        // facts in a compact multi-column card
                        Rectangle {
                            id: stripBlock
                            visible: sec.kind === "strip"
                            width: parent.width
                            implicitHeight: stripGrid.implicitHeight + 28
                            radius: Theme.radius
                            color: Theme.surface
                            border.color: Theme.border
                            GridLayout {
                                id: stripGrid
                                anchors.fill: parent
                                anchors.margins: 14
                                columns: detail.wide ? (sec.section.cols || 2) : Math.min(2, sec.section.cols || 2)
                                columnSpacing: 28
                                rowSpacing: 0
                                Repeater {
                                    model: sec.kind === "strip" ? sec.section.facts.length : 0
                                    delegate: Item {
                                        id: fact
                                        required property int index
                                        readonly property var info: sec.section.facts[index]
                                        Layout.fillWidth: true
                                        implicitHeight: 27
                                        Text {
                                            id: factLabel
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: Math.min(150, parent.width * 0.55)
                                            text: fact.info.label + ":"
                                            color: Theme.textMuted
                                            font.pixelSize: 12
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.left: factLabel.right
                                            anchors.right: parent.right
                                            text: fact.info.value
                                            color: Theme.text
                                            font.pixelSize: 12
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
