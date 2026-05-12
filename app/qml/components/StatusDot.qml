import QtQuick

Rectangle {
    id: dot
    required property bool active
    width: 8; height: 8
    radius: 4
    color: active ? "#22c55e" : "#6b7280"

    SequentialAnimation on opacity {
        running: dot.active
        loops: Animation.Infinite
        NumberAnimation { to: 0.4; duration: 900 }
        NumberAnimation { to: 1.0; duration: 900 }
    }
}
