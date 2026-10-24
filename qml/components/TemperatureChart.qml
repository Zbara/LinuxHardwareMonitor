import QtQuick
import HwMonitor

// Line chart over app.history(); repaints on every poll without recreating
// anything. NaN samples break the line. Up to two series share one Y axis.
Canvas {
    id: chart
    property string sensorId: ""
    property color lineColor: Theme.accent
    property string secondSensorId: ""
    property color secondColor: Theme.accent

    // When max > min the Y axis is fixed instead of auto-scaled.
    property real fixedMin: 0
    property real fixedMax: 0
    // Smallest Y range for auto-scaling; keeps tiny noise from looking dramatic.
    property real minSpan: 5
    property bool floorZero: false
    // Rates and percentages: the Y axis always starts at 0.
    property bool zeroBased: false
    property bool filled: false
    // > 0: X axis shows exactly this many seconds; otherwise it fills as data arrives.
    property real fixedSpan: 0
    property bool grid: false
    property color gridColor: Theme.border

    // Output: the Y range used by the last paint (for axis labels).
    property real scaleMin: 0
    property real scaleMax: 0

    implicitHeight: 48
    renderTarget: Canvas.FramebufferObject

    Connections {
        target: app
        function onSummariesChanged() { chart.requestPaint() }
    }
    onSensorIdChanged: requestPaint()
    onSecondSensorIdChanged: requestPaint()
    onLineColorChanged: requestPaint()
    onSecondColorChanged: requestPaint()
    onFilledChanged: requestPaint()
    onGridColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()

        if (grid) {
            ctx.strokeStyle = gridColor
            ctx.lineWidth = 1
            ctx.beginPath()
            for (let i = 1; i < 10; i++) {
                const gx = Math.round(width * i / 10) + 0.5
                ctx.moveTo(gx, 0); ctx.lineTo(gx, height)
            }
            for (let j = 1; j < 4; j++) {
                const gy = Math.round(height * j / 4) + 0.5
                ctx.moveTo(0, gy); ctx.lineTo(width, gy)
            }
            ctx.stroke()
        }

        const series = []
        if (sensorId !== "") series.push({ pts: app.history(sensorId), color: lineColor })
        if (secondSensorId !== "") series.push({ pts: app.history(secondSensorId), color: secondColor })
        let oldest = 0, lo = Infinity, hi = -Infinity
        for (const s of series) {
            if (s.pts.length === 0) continue
            oldest = Math.min(oldest, s.pts[0].x)
            for (const p of s.pts) {
                if (isNaN(p.y)) continue
                lo = Math.min(lo, p.y)
                hi = Math.max(hi, p.y)
            }
        }
        if (!isFinite(lo))
            return

        if (fixedMax > fixedMin) {
            lo = fixedMin
            hi = fixedMax
        } else {
            if (zeroBased) {
                lo = 0
                if (hi < minSpan) hi = minSpan
            } else {
                if (hi - lo < minSpan) {
                    const mid = (hi + lo) / 2
                    lo = mid - minSpan / 2
                    hi = mid + minSpan / 2
                }
                if (floorZero && lo < 0) {
                    hi -= lo
                    lo = 0
                }
            }
            // A little headroom so peaks do not touch the frame.
            hi += (hi - lo) * 0.05
        }
        scaleMin = lo
        scaleMax = hi

        const span = fixedSpan > 0 ? fixedSpan
                                   : Math.min(app.historySeconds, Math.max(10, -oldest))
        const pad = grid ? 1 : 3
        const x = t => width + (t / span) * width
        const y = v => height - pad - (v - lo) / (hi - lo) * (height - 2 * pad)

        for (const s of series) {
            // One path per contiguous run so gaps stay gaps.
            let run = []
            const runs = []
            for (const p of s.pts) {
                if (isNaN(p.y)) { if (run.length) runs.push(run); run = []; continue }
                run.push(p)
            }
            if (run.length) runs.push(run)

            if (filled) {
                // Fade from the line down to transparent, like the reference design.
                const grad = ctx.createLinearGradient(0, 0, 0, height)
                grad.addColorStop(0, Qt.rgba(s.color.r, s.color.g, s.color.b, 0.40))
                grad.addColorStop(1, Qt.rgba(s.color.r, s.color.g, s.color.b, 0.03))
                ctx.fillStyle = grad
                for (const r of runs) {
                    ctx.beginPath()
                    ctx.moveTo(x(r[0].x), height)
                    for (const p of r) ctx.lineTo(x(p.x), y(p.y))
                    ctx.lineTo(x(r[r.length - 1].x), height)
                    ctx.closePath()
                    ctx.fill()
                }
            }
            ctx.strokeStyle = s.color
            ctx.lineWidth = grid ? 1.6 : 1.5
            ctx.lineJoin = "round"
            for (const r of runs) {
                ctx.beginPath()
                ctx.moveTo(x(r[0].x), y(r[0].y))
                for (let i = 1; i < r.length; i++) ctx.lineTo(x(r[i].x), y(r[i].y))
                ctx.stroke()
            }
        }
    }
}
