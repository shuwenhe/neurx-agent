import QtQuick
import QtQuick.Layouts

Item {
    id: bubble
    required property string role     // "user" | "assistant" | "system"
    required property string content
    required property string time

    readonly property bool isUser: role === "user"

    implicitHeight: row.implicitHeight + 16

    RowLayout {
        id: row
        anchors { top: parent.top; bottom: parent.bottom;
                  left: parent.left; right: parent.right;
                  topMargin: 8; bottomMargin: 8 }
        layoutDirection: bubble.isUser ? Qt.RightToLeft : Qt.LeftToRight
        spacing: 12

        // Avatar
        Rectangle {
            width: 36; height: 36
            radius: 18
            color: bubble.isUser ? "#6c63ff" : "#1e3a5f"
            Text {
                anchors.centerIn: parent
                text: bubble.isUser ? "U" : "⊞"
                color: "white"
                font.pixelSize: 15
            }
        }

        // Bubble body
        Column {
            spacing: 4
            Layout.maximumWidth: bubble.width * 0.72

            Rectangle {
                width: msgText.implicitWidth + 24
                height: msgText.implicitHeight + 16
                color: bubble.isUser ? "#6c63ff" : "#1a1a1a"
                border.color: bubble.isUser ? "transparent" : "#2a2a2a"
                border.width: 1
                radius: 12

                Text {
                    id: msgText
                    anchors { fill: parent; margins: 12 }
                    text: bubble.content
                    wrapMode: Text.WordWrap
                    font { family: "Sans Serif"; pixelSize: 14 }
                    color: "#f0f0f0"
                }
            }

            Text {
                text: bubble.time
                font { family: "Sans Serif"; pixelSize: 10 }
                color: "#666"
                anchors.right: bubble.isUser ? parent.right : undefined
            }
        }
    }
}
