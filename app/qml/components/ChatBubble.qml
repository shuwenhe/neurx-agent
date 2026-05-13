import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

Item {
    id: bubble
    required property string role     // "user" | "assistant" | "system"
    required property string content
    required property string time

    readonly property bool isUser: role === "user"
    readonly property var parsedToolSteps: parseToolSteps(content)
    readonly property string finalAnswerText: extractFinalAnswer(content)
    readonly property bool showToolTrace: !isUser && parsedToolSteps.length > 0
    readonly property string copyText: bubble.showToolTrace && bubble.finalAnswerText.length > 0
                                       ? bubble.finalAnswerText
                                       : bubble.content

    implicitHeight: row.implicitHeight + 16

    function parseToolSteps(rawContent) {
        const steps = []
        const lines = String(rawContent || "").split("\n")

        for (let i = 0; i < lines.length; ++i) {
            const line = lines[i].trim()
            if (line.indexOf("[tool]") === 0) {
                let name = line.substring("[tool]".length).trim()
                let args = ""
                const argsPos = name.indexOf(" args=")
                if (argsPos >= 0) {
                    args = name.substring(argsPos + 6).trim()
                    name = name.substring(0, argsPos).trim()
                }
                steps.push({
                    kind: "tool",
                    title: name.length > 0 ? name : "tool",
                    detail: args
                })
            } else if (line.indexOf("[tool_result]") === 0) {
                steps.push({
                    kind: "result",
                    title: qsTr("Result"),
                    detail: line.substring("[tool_result]".length).trim()
                })
            }
        }

        return steps
    }

    function extractFinalAnswer(rawContent) {
        const lines = String(rawContent || "").split("\n")
        const kept = []

        for (let i = 0; i < lines.length; ++i) {
            const line = lines[i]
            const trimmed = line.trim()
            if (trimmed.indexOf("[tool]") === 0 || trimmed.indexOf("[tool_result]") === 0)
                continue
            kept.push(line)
        }

        return kept.join("\n").trim()
    }

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

            Rectangle {
                id: messageBody
                width: bubble.width * 0.72
                height: contentColumn.implicitHeight + 16
                color: bubble.isUser ? "#6c63ff" : "#1a1a1a"
                border.color: bubble.isUser ? "transparent" : "#2a2a2a"
                border.width: 1
                radius: 12

                Rectangle {
                    id: copyButton
                    width: 26
                    height: 24
                    radius: 6
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.topMargin: 6
                    anchors.rightMargin: 6
                    z: 2
                    visible: copyMouse.containsMouse || bodyHover.containsMouse
                    opacity: copyText.length > 0 ? 1.0 : 0.4
                    color: copyMouse.containsMouse ? "#303030" : "#242424"
                    border.color: bubble.isUser ? "#8b84ff" : "#3a3a3a"

                    Text {
                        anchors.centerIn: parent
                        text: "⧉"
                        color: "#e5e7eb"
                        font.pixelSize: 13
                    }

                    ToolTip.visible: copyMouse.containsMouse
                    ToolTip.text: qsTr("Copy")
                    ToolTip.delay: 450

                    MouseArea {
                        id: copyMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        enabled: bubble.copyText.length > 0
                        onClicked: Runtime.copyToClipboard(bubble.copyText)
                    }
                }

                MouseArea {
                    id: bodyHover
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }

                Column {
                    id: contentColumn
                    anchors { fill: parent; margins: 12 }
                    width: parent.width - 24
                    spacing: 8

                    Text {
                        visible: !bubble.showToolTrace
                        width: parent.width
                        text: bubble.content
                        wrapMode: Text.WordWrap
                        font { pixelSize: 14 }
                        color: "#f0f0f0"
                    }

                    Repeater {
                        model: bubble.showToolTrace ? bubble.parsedToolSteps : []

                        delegate: Rectangle {
                            required property var modelData

                            width: contentColumn.width
                            radius: 8
                            color: modelData.kind === "tool" ? "#122433" : "#1d2b1f"
                            border.color: modelData.kind === "tool" ? "#2d5f82" : "#3b6b40"
                            border.width: 1
                            implicitHeight: stepColumn.implicitHeight + 14

                            Column {
                                id: stepColumn
                                anchors { fill: parent; margins: 7 }
                                spacing: 4

                                Text {
                                    width: parent.width
                                    text: modelData.kind === "tool"
                                        ? qsTr("Tool: ") + modelData.title
                                        : modelData.title
                                    color: "#e8f0ff"
                                    font { pixelSize: 12; weight: Font.DemiBold }
                                    wrapMode: Text.WordWrap
                                }

                                Text {
                                    visible: modelData.detail.length > 0
                                    width: parent.width
                                    text: modelData.detail
                                    color: "#c7d2da"
                                    font.pixelSize: 11
                                    wrapMode: Text.WrapAnywhere
                                }
                            }
                        }
                    }

                    Text {
                        visible: bubble.showToolTrace && bubble.finalAnswerText.length > 0
                        width: parent.width
                        text: bubble.finalAnswerText
                        wrapMode: Text.WordWrap
                        font { pixelSize: 14 }
                        color: "#f0f0f0"
                    }
                }
            }

            Text {
                text: bubble.time
                font { pixelSize: 10 }
                color: "#666"
                anchors.right: bubble.isUser ? parent.right : undefined
            }
        }
    }
}
