import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

Item {
    id: page
    required property var palette

    ColumnLayout {
        anchors { fill: parent; margins: 32 }
        spacing: 24

        Text {
            text: qsTr("Tools")
            font { pixelSize: 28; weight: Font.Bold }
            color: page.palette.textPrim
        }

        Text {
            text: qsTr("Tools registered in the ToolRegistry are listed here.\nAgents use these tools to interact with the world.")
            font { pixelSize: 14 }
            color: page.palette.textSec
            lineHeight: 1.6
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"
            // Tool list will be bound to a ToolListModel when implemented
            Text {
                anchors.centerIn: parent
                text: qsTr("Tool registry view — coming soon")
                font { pixelSize: 14 }
                color: page.palette.textSec
            }
        }
    }
}
