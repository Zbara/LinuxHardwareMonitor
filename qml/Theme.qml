pragma Singleton
import QtQuick

QtObject {
    property bool dark: true

    readonly property color background: dark ? "#14171c" : "#f2f4f7"
    readonly property color surface: dark ? "#1d2127" : "#ffffff"
    readonly property color surfaceAlt: dark ? "#262b33" : "#e8ebf0"
    readonly property color border: dark ? "#2f353e" : "#d5dae1"
    readonly property color text: dark ? "#e8eaed" : "#1b1f24"
    readonly property color textMuted: dark ? "#8b94a1" : "#5f6a78"
    readonly property color accent: "#4c9aff"
    readonly property color warn: "#f0a030"
    readonly property color hot: "#e5534b"
    readonly property color ok: "#3fb27f"

    // Task-manager style resource colours (same hues in both themes).
    readonly property color cpuColor: "#17a2d8"
    readonly property color memoryColor: "#a855c7"
    readonly property color diskColor: "#4daa3e"
    readonly property color networkColor: "#d98a2b"
    readonly property color gpuColor: "#3b82f6"
    readonly property color batteryColor: "#8bc34a"

    readonly property color sidebar: dark ? "#181b20" : "#e9ecf1"
    readonly property color selection: dark ? "#2a3340" : "#d4e4fb"

    // Strings reused all over the UI (re-evaluated when the language changes).
    readonly property string na: qsTr("N/A")
    readonly property string unitB: qsTr("B")
    readonly property string unitKB: qsTr("KB")
    readonly property string unitMB: qsTr("MB")
    readonly property string unitGB: qsTr("GB")
    readonly property string unitTB: qsTr("TB")
    readonly property string unitMBs: qsTr("MB/s")
    readonly property string unitKBs: qsTr("KB/s")

    readonly property int radius: 10
    readonly property int spacing: 12

    // Data and thresholds are always Celsius; only what is shown changes.
    readonly property bool fahrenheit: app.useFahrenheit
    function toFahrenheit(c) { return c * 9 / 5 + 32 }
    function tempText(celsius, digits) {
        if (celsius === undefined || celsius === null || isNaN(celsius))
            return na
        return (fahrenheit ? toFahrenheit(celsius) : celsius).toFixed(digits === undefined ? 0 : digits) + (fahrenheit ? " °F" : " °C")
    }

    function format(value, unit) {
        if (value === undefined || value === null || isNaN(value))
            return na
        if (unit === "°C")
            return tempText(value, 1)
        const digits = unit === "RPM" || unit === "MHz" || unit === "%" ? 0 : unit === "MB/s" ? 2 : 1
        return value.toFixed(digits) + " " + unit
    }

    function tempColor(value) {
        if (value === undefined || value === null || isNaN(value))
            return textMuted
        return value >= 85 ? hot : value >= 70 ? warn : accent
    }

    function formatBytes(bytes) {
        const units = [unitB, unitKB, unitMB, unitGB, unitTB]
        let v = bytes, i = 0
        while (v >= 1024 && i < units.length - 1) { v /= 1024; i++ }
        return v.toFixed(i === 0 || v >= 100 ? 0 : 1) + " " + units[i]
    }

    // Heat-map shading for table cells: v in 0..1 -> warm tint over the row.
    function heat(v) {
        if (v === undefined || v === null || isNaN(v) || v <= 0)
            return "transparent"
        const t = Math.min(1, v)
        // yellow -> orange as the value grows
        return Qt.rgba(1.0, 0.80 - 0.34 * t, 0.12, 0.07 + 0.36 * t)
    }

    function formatUptime(seconds) {
        if (seconds === undefined || seconds === null || seconds < 0)
            return na
        const d = Math.floor(seconds / 86400)
        const h = Math.floor(seconds % 86400 / 3600)
        const m = Math.floor(seconds % 3600 / 60)
        const s = Math.floor(seconds % 60)
        const p = n => (n < 10 ? "0" : "") + n
        return d + ":" + p(h) + ":" + p(m) + ":" + p(s)
    }

    function formatRate(mbps) {
        if (mbps === undefined || mbps === null || isNaN(mbps))
            return na
        if (mbps >= 1) return mbps.toFixed(mbps >= 100 ? 0 : 1) + " " + unitMBs
        return (mbps * 1000).toFixed(mbps >= 0.1 ? 0 : 1) + " " + unitKBs
    }

    function gb(v, digits) {
        return v === undefined || v === null || isNaN(v) ? na : v.toFixed(digits === undefined ? 1 : digits) + " " + unitGB
    }

    // Axis label for a chart maximum.
    function formatRateScale(max, unit) {
        if (unit === "dBm") return isNaN(max) || max === 0 ? "" : max.toFixed(0) + " dBm"
        if (!(max > 0)) return ""
        if (unit === "MB/s") return formatRate(max)
        return max.toFixed(max < 10 ? 1 : 0) + " " + unit
    }

    function formatSize(bytes) {
        if (bytes === undefined || bytes === null || bytes < 0) return na
        return bytes >= 1e12 ? (bytes / 1e12).toFixed(2) + " " + unitTB : (bytes / 1e9).toFixed(0) + " " + unitGB
    }

    // GB value shown in the most readable unit (MB below 1 GB).
    function gbAuto(v) {
        if (v === undefined || v === null || isNaN(v)) return na
        return v >= 1 ? v.toFixed(v >= 100 ? 0 : 1) + " " + unitGB : (v * 1024).toFixed(0) + " " + unitMB
    }
}
