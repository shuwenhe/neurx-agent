import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

Item {
    id: page
    required property var palette

    ColumnLayout {
        anchors { fill: parent; margins: 32 }
        spacing: 24

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("Agents")
                font { family: "Inter"; pixelSize: 28; weight: Font.Bold }
                color: page.palette.textPrim
                Layout.fillWidth: true
            }
            Text {
                text: Runtime.agentCount + qsTr(" registered")
                font { family: "Inter"; pixelSize: 13 }
                color: page.palette.textSec
            }
        }

        // Agent grid
        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: AgentModel
            cellWidth: 260
            cellHeight: 120
            clip: true

            delegate: AgentCard {
                width: grid.cellWidth - 16
                height: grid.cellHeight - 16
                agentId: model.agentId
                name:    model.name
                status:  model.status
            }
        }

        // Empty state
        Text {
            visible: Runtime.agentCount === 0
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("No agents registered yet.\nCreate agents via the plugins system or API.")
            horizontalAlignment: Text.AlignHCenter
            font { family: "Inter"; pixelSize: 14 }
            color: page.palette.textSec
            lineHeight: 1.6
        }
    }
}
