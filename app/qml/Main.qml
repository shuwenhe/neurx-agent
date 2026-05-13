import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window

Window {
    id: root
    visible: true
    visibility: Window.FullScreen
    width: 1280
    height: 800
    minimumWidth: 800
    minimumHeight: 600
    title: qsTr("NeurX AgentOS")
    color: "#0d0d0d"

    Shortcut {
        sequence: StandardKey.Close
        onActivated: root.close()
    }

    AppShell {
        anchors.fill: parent
    }
}
