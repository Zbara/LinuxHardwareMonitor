import QtQuick
import QtQuick.Controls
import HwMonitor

// Themed on/off switch (the stock Basic switch has poor contrast in dark mode).
Switch {
    id: control
    implicitWidth: 48
    implicitHeight: 26

    indicator: Rectangle {
        implicitWidth: 44
        implicitHeight: 24
        x: control.leftPadding
        y: (control.height - height) / 2
        radius: height / 2
        color: control.checked ? Theme.accent : Theme.surfaceAlt
        border.color: control.checked ? Theme.accent : Theme.border

        Rectangle {
            x: control.checked ? parent.width - width - 3 : 3
            anchors.verticalCenter: parent.verticalCenter
            width: 18
            height: 18
            radius: 9
            color: control.checked ? "white" : Theme.textMuted
            Behavior on x { NumberAnimation { duration: 110 } }
        }
    }
}
