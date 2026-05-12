import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window

Window {
    id: root
    visible: true
    width: 1280
    height: 800
    minimumWidth: 800
    minimumHeight: 600
    title: qsTr("NeurX AgentOS")
    color: "#0d0d0d"

    // Load custom fonts
    FontLoader { source: "qrc:/neurx/resources/fonts/Inter-Regular.ttf" }
    FontLoader { source: "qrc:/neurx/resources/fonts/Inter-Bold.ttf"    }

    AppShell {
        anchors.fill: parent
    }
}
