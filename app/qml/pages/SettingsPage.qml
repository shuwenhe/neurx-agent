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
            text: qsTr("Settings")
            font { family: "Inter"; pixelSize: 28; weight: Font.Bold }
            color: page.palette.textPrim
        }

        // LLM Backend section
        SectionLabel { text: qsTr("LLM Backend") }

        SettingRow {
            label: qsTr("API Endpoint")
            placeholder: "https://api.openai.com/v1/chat/completions"
        }
        SettingRow {
            label: qsTr("API Key")
            placeholder: "sk-…"
            echoMode: TextInput.Password
        }
        SettingRow {
            label: qsTr("Model")
            placeholder: "gpt-4o"
        }

        // Runtime section
        SectionLabel { text: qsTr("Runtime") }

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("Auto-start runtime")
                font { family: "Inter"; pixelSize: 14 }
                color: page.palette.textPrim
                Layout.fillWidth: true
            }
            Switch {
                checked: true
            }
        }

        Item { Layout.fillHeight: true }
    }

    // ---------- inline components ----------
    component SectionLabel: Text {
        font { family: "Inter"; pixelSize: 13; weight: Font.Medium }
        color: page.palette.accent
        topPadding: 12
    }

    component SettingRow: RowLayout {
        required property string label
        required property string placeholder
        property int echoMode: TextInput.Normal

        Layout.fillWidth: true
        spacing: 16

        Text {
            text: parent.label
            font { family: "Inter"; pixelSize: 14 }
            color: page.palette.textPrim
            Layout.preferredWidth: 140
        }

        Rectangle {
            Layout.fillWidth: true
            height: 40
            color: "#1a1a1a"
            border.color: page.palette.border
            radius: 6

            TextInput {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                verticalAlignment: TextInput.AlignVCenter
                font { family: "Inter"; pixelSize: 14 }
                color: page.palette.textPrim
                placeholderText: parent.parent.placeholder
                echoMode: parent.parent.echoMode
                clip: true
            }
        }
    }
}
