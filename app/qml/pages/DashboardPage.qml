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
            text: qsTr("Dashboard")
            font { family: "Sans Serif"; pixelSize: 28; weight: Font.Bold }
            color: page.palette.textPrim
        }

        // Stats row
        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            StatCard {
                label: qsTr("Agents"); value: Runtime.agentCount
                accent: page.palette.accent
            }
            StatCard {
                label: qsTr("Runtime")
                value: Runtime.running ? qsTr("Active") : qsTr("Stopped")
                accent: Runtime.running ? page.palette.success : page.palette.textSec
            }
        }

        // Log console
        Text {
            text: qsTr("Live Log")
            font { family: "Sans Serif"; pixelSize: 16; weight: Font.Medium }
            color: page.palette.textPrim
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#111"
            border.color: page.palette.border
            radius: 8
            clip: true

            ListView {
                id: logView
                anchors { fill: parent; margins: 12 }
                model: LogModel
                clip: true
                spacing: 2
                onCountChanged: positionViewAtEnd()

                delegate: RowLayout {
                    width: logView.width
                    spacing: 8

                    Text {
                        text: model.time
                        font { family: "monospace"; pixelSize: 11 }
                        color: "#666"
                    }
                    Text {
                        text: model.level.toUpperCase()
                        font { family: "monospace"; pixelSize: 11 }
                        color: model.level === "error"   ? "#ef4444"
                             : model.level === "warning" ? "#f59e0b"
                             : model.level === "info"    ? "#6c63ff"
                             : "#555"
                        Layout.preferredWidth: 40
                    }
                    Text {
                        text: "[" + model.tag + "]"
                        font { family: "monospace"; pixelSize: 11 }
                        color: "#888"
                    }
                    Text {
                        text: model.message
                        font { family: "monospace"; pixelSize: 11 }
                        color: "#ccc"
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    // Inline StatCard
    component StatCard: Rectangle {
        required property string label
        required property var    value
        required property color  accent
        Layout.fillWidth: true
        height: 96
        color: "#1a1a1a"
        border.color: "#2a2a2a"
        radius: 12

        Column {
            anchors.centerIn: parent
            spacing: 6
            Text {
                text: parent.parent.value.toString()
                font { family: "Sans Serif"; pixelSize: 32; weight: Font.Bold }
                color: parent.parent.accent
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: parent.parent.label
                font { family: "Sans Serif"; pixelSize: 13 }
                color: "#9a9a9a"
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }
}
