import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import "../components"

Item {
    id: page
    required property var palette

    ListModel { id: chatHistory }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header
        Rectangle {
            Layout.fillWidth: true
            height: 56
            color: "#111"
            border.color: page.palette.border

            Text {
                anchors.centerIn: parent
                text: qsTr("Chat with Agents")
                font { family: "Inter"; pixelSize: 16; weight: Font.Medium }
                color: page.palette.textPrim
            }
        }

        // Message list
        ListView {
            id: msgList
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: chatHistory
            spacing: 0
            clip: true
            onCountChanged: positionViewAtEnd()

            topMargin: 16
            leftMargin: 16
            rightMargin: 16

            delegate: ChatBubble {
                width: msgList.width - 32
                role:    model.role
                content: model.content
                time:    model.time
            }
        }

        // Input row
        Rectangle {
            Layout.fillWidth: true
            height: 72
            color: "#111"
            border.color: page.palette.border

            RowLayout {
                anchors { fill: parent; margins: 12 }
                spacing: 12

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#1a1a1a"
                    border.color: msgInput.activeFocus ? page.palette.accent : page.palette.border
                    radius: 8

                    TextField {
                        id: msgInput
                        anchors { fill: parent; leftMargin: 14; rightMargin: 14 }
                        verticalAlignment: TextInput.AlignVCenter
                        font { family: "Inter"; pixelSize: 14 }
                        color: page.palette.textPrim
                        placeholderText: qsTr("Message an agent…")
                        background: null
                        clip: true

                        Keys.onReturnPressed: sendBtn.clicked()
                    }
                }

                Rectangle {
                    id: sendBtn
                    width: 48; height: 48
                    color: page.palette.accent
                    radius: 8

                    signal clicked()
                    onClicked: {
                        const text = msgInput.text.trim()
                        if (!text) return
                        chatHistory.append({
                            role: "user",
                            content: text,
                            time: Qt.formatTime(new Date(), "hh:mm")
                        })
                        msgInput.text = ""
                        // TODO: route to selected agent via Runtime
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "↑"
                        font.pixelSize: 20
                        color: "white"
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: sendBtn.clicked()
                    }
                }
            }
        }
    }
}
