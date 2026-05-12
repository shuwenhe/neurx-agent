import QtQuick
import QtQuick.Layouts

Rectangle {
    id: card
    required property string agentId
    required property string name
    required property int    status   // Agent::Status enum

    readonly property var statusLabels: ["Idle","Running","Paused","Error","Stopped"]
    readonly property var statusColors: ["#6b7280","#22c55e","#f59e0b","#ef4444","#6b7280"]

    color: "#1a1a1a"
    border.color: "#2a2a2a"
    border.width: 1
    radius: 12

    ColumnLayout {
        anchors { fill: parent; margins: 16 }
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: card.name
                font { family: "Inter"; pixelSize: 15; weight: Font.Medium }
                color: "#f0f0f0"
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            StatusDot { active: card.status === 1 /* Running */ }
        }

        Text {
            text: card.statusLabels[card.status] ?? "Unknown"
            font { family: "Inter"; pixelSize: 12 }
            color: card.statusColors[card.status] ?? "#9a9a9a"
        }

        Text {
            text: card.agentId
            font { family: "Inter"; pixelSize: 10 }
            color: "#555"
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }
}
