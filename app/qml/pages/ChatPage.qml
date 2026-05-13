pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import "../components"

Item {
    id: page
    required property var palette

    property string selectedModelId: "gpt-5.5"
    property string selectedModelLabel: "GPT-5.5"
    property string selectedEffort: "Medium"
    property string selectedPermissions: qsTr("Default permissions")
    property bool ideContextEnabled: true
    property bool workLocally: true

    function openMenuNear(anchor, menu) {
        const pos = anchor.mapToItem(page, 0, 0)
        const desiredHeight = menu.popupHeight || menu.implicitHeight || 180
        menu.x = Math.max(8, Math.min(page.width - menu.width - 8, pos.x))
        menu.y = Math.max(8, pos.y - desiredHeight - 8)
        menu.open()
    }

    function hasModel(modelId) {
        for (let i = 0; i < availableModels.count; ++i) {
            if (availableModels.get(i).idValue === modelId)
                return true
        }
        return false
    }

    function pruneRuntimeModels() {
        for (let i = availableModels.count - 1; i >= 0; --i) {
            if (availableModels.get(i).provider === "Ollama")
                availableModels.remove(i)
        }
    }

    function syncRuntimeModels() {
        if (!Runtime || !Runtime.localModels)
            return

        pruneRuntimeModels()
        for (const model of Runtime.localModels) {
            if (!hasModel(model.idValue))
                availableModels.append(model)
        }
    }

    Component.onCompleted: {
        syncRuntimeModels()
        if (Runtime && Runtime.refreshLocalModels)
            Runtime.refreshLocalModels()
    }

    ListModel { id: chatHistory }

    ListModel {
        id: availableModels
        ListElement { idValue: "gpt-5.5"; label: "GPT-5.5"; provider: "OpenAI"; detail: "Agentic coding and planning" }
        ListElement { idValue: "gpt-5.4"; label: "GPT-5.4"; provider: "OpenAI"; detail: "Balanced software tasks" }
        ListElement { idValue: "gpt-5.4-mini"; label: "GPT-5.4 Mini"; provider: "OpenAI"; detail: "Fast lightweight work" }
        ListElement { idValue: "o3"; label: "o3"; provider: "OpenAI"; detail: "Deep reasoning" }
        ListElement { idValue: "gpt-4.1"; label: "GPT-4.1"; provider: "OpenAI"; detail: "Compatibility" }
        ListElement { idValue: "claude-sonnet"; label: "Claude Sonnet"; provider: "Anthropic"; detail: "Long form coding" }
        ListElement { idValue: "qwen-max"; label: "Qwen Max"; provider: "Qwen"; detail: "Multilingual tasks" }
    }

    ListModel {
        id: effortOptions
        ListElement { label: "Low"; detail: "Quick" }
        ListElement { label: "Medium"; detail: "Balanced" }
        ListElement { label: "High"; detail: "Thorough" }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: "#111"
            border.color: page.palette.border

            Text {
                anchors.centerIn: parent
                text: qsTr("Chat with Agents")
                font { pixelSize: 16; weight: Font.Medium }
                color: page.palette.textPrim
            }
        }

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
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Math.max(122, composerColumn.implicitHeight + 24)
            color: "#111"
            border.color: page.palette.border

            Rectangle {
                id: composer
                anchors { fill: parent; margins: 12 }
                color: "#171717"
                border.color: msgInput.activeFocus ? page.palette.accent : page.palette.border
                radius: 10

                ColumnLayout {
                    id: composerColumn
                    anchors { fill: parent; margins: 8 }
                    spacing: 6

                    TextArea {
                        id: msgInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(96, Math.max(44, implicitHeight))
                        Layout.minimumHeight: 44
                        font { pixelSize: 14 }
                        color: page.palette.textPrim
                        placeholderText: qsTr("Message an agent...")
                        background: null
                        clip: true
                        wrapMode: TextEdit.Wrap
                        selectByMouse: true
                        leftPadding: 6
                        rightPadding: 6
                        topPadding: 8
                        bottomPadding: 4

                        Keys.onReturnPressed: (event) => {
                            if (event.modifiers & Qt.ShiftModifier) {
                                event.accepted = false
                                return
                            }
                            sendBtn.clicked()
                            event.accepted = true
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        spacing: 8

                        ComposerButton {
                            text: "+"
                            compact: true
                            tooltip: qsTr("Attach context")
                        }

                        ComposerButton {
                            id: permissionsButton
                            text: "◉ " + page.selectedPermissions + " v"
                            tooltip: qsTr("Permission mode")
                            onClicked: page.openMenuNear(permissionsButton, permissionsMenu)
                        }

                        ComposerButton {
                            id: locationButton
                            text: page.workLocally ? "▱ " + qsTr("Work locally") + " v" : "☁ " + qsTr("Cloud") + " v"
                            tooltip: qsTr("Execution location")
                            onClicked: page.openMenuNear(locationButton, localMenu)
                        }

                        Item { Layout.fillWidth: true }

                        ComposerButton {
                            id: modelButton
                            text: page.selectedModelLabel + "  " + page.selectedEffort + " v"
                            tooltip: qsTr("Select model and reasoning")
                            onClicked: page.openMenuNear(modelButton, modelMenu)
                        }

                        ComposerButton {
                            text: "✧ " + qsTr("IDE context")
                            checked: page.ideContextEnabled
                            tooltip: qsTr("Toggle IDE context")
                            onClicked: page.ideContextEnabled = !page.ideContextEnabled
                        }

                        Rectangle {
                            id: sendBtn
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 34
                            color: msgInput.text.trim().length > 0 ? page.palette.textPrim : "#5f5f5f"
                            radius: 17

                            signal clicked()
                            onClicked: {
                                const text = msgInput.text.trim()
                                if (!text)
                                    return

                                chatHistory.append({
                                    role: "user",
                                    content: text + "\n\n" + qsTr("Model: ") + page.selectedModelLabel + " · " + page.selectedEffort,
                                    time: Qt.formatTime(new Date(), "hh:mm"),
                                    model: page.selectedModelId,
                                    effort: page.selectedEffort,
                                    permissions: page.selectedPermissions,
                                    ideContext: page.ideContextEnabled
                                })
                                msgInput.text = ""
                                // TODO: route selected model/effort/permissions to Runtime.
                            }

                            Text {
                                anchors.centerIn: parent
                                text: "↑"
                                font.pixelSize: 19
                                color: "#111"
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: sendBtn.clicked()
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            implicitWidth: localStatusText.implicitWidth + 18
                            implicitHeight: 24
                            radius: 12
                            color: Runtime.localModels.length > 0 ? "#13261c" : "#2a1d12"
                            border.color: Runtime.localModels.length > 0 ? "#28543a" : "#5f3b1d"
                            border.width: 1

                            Text {
                                id: localStatusText
                                anchors.centerIn: parent
                                text: Runtime.localModelStatus.length > 0 ? Runtime.localModelStatus : qsTr("Checking local models")
                                color: Runtime.localModels.length > 0 ? "#86efac" : "#fbbf24"
                                font.pixelSize: 11
                            }
                        }

                        ComposerButton {
                            text: qsTr("Refresh local models")
                            tooltip: Runtime.localModelSource.length > 0
                                ? qsTr("Source: ") + Runtime.localModelSource
                                : qsTr("Ollama executable not found")
                            onClicked: Runtime.refreshLocalModels()
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }
    }

    Menu {
        id: permissionsMenu
        property int popupHeight: 110
        width: 250
        background: MenuPanel {}
        Action { text: qsTr("Default permissions"); onTriggered: page.selectedPermissions = text }
        Action { text: qsTr("Read only"); onTriggered: page.selectedPermissions = text }
        Action { text: qsTr("Auto approve safe commands"); onTriggered: page.selectedPermissions = text }
    }

    Menu {
        id: localMenu
        property int popupHeight: 78
        width: 220
        background: MenuPanel {}
        Action { text: qsTr("Work locally"); onTriggered: page.workLocally = true }
        Action { text: qsTr("Use cloud runtime"); onTriggered: page.workLocally = false }
    }

    Popup {
        id: modelMenu
        property int popupHeight: 364
        width: 300
        height: popupHeight
        padding: 6
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: MenuPanel {}

        contentItem: ColumnLayout {
            spacing: 6

            ListView {
                id: modelList
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(230, contentHeight)
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: availableModels

                delegate: Rectangle {
                    id: modelItem
                    required property string idValue
                    required property string label
                    required property string provider
                    required property string detail

                    width: modelList.width
                    height: 46
                    radius: 6
                    color: modelMouse.containsMouse ? "#262626" : "transparent"

                    RowLayout {
                        anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                        spacing: 10

                        Text {
                            text: page.selectedModelId === modelItem.idValue ? "✓" : ""
                            color: page.palette.accent
                            font.pixelSize: 13
                            Layout.preferredWidth: 14
                        }

                        Column {
                            Layout.fillWidth: true
                            spacing: 1
                            Text {
                                text: modelItem.label
                                color: page.palette.textPrim
                                font { pixelSize: 13; weight: page.selectedModelId === modelItem.idValue ? Font.DemiBold : Font.Normal }
                            }
                            Text {
                                width: parent.width
                                text: modelItem.provider + " · " + modelItem.detail
                                color: page.palette.textSec
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }
                    }

                    MouseArea {
                        id: modelMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            page.selectedModelId = modelItem.idValue
                            page.selectedModelLabel = modelItem.label
                            modelMenu.close()
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: page.palette.border
            }

            ListView {
                id: effortList
                Layout.fillWidth: true
                Layout.preferredHeight: contentHeight
                interactive: false
                model: effortOptions

                delegate: Rectangle {
                    id: effortItem
                    required property string label
                    required property string detail

                    width: effortList.width
                    height: 36
                    radius: 6
                    color: effortMouse.containsMouse ? "#262626" : "transparent"

                    RowLayout {
                        anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                        spacing: 10

                        Text {
                            text: page.selectedEffort === effortItem.label ? "✓" : ""
                            color: page.palette.accent
                            font.pixelSize: 13
                            Layout.preferredWidth: 14
                        }
                        Text {
                            text: effortItem.label + " · " + effortItem.detail
                            color: page.palette.textPrim
                            font.pixelSize: 12
                        }
                    }

                    MouseArea {
                        id: effortMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            page.selectedEffort = effortItem.label
                            modelMenu.close()
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: Runtime

        function onLocalModelsChanged() {
            page.syncRuntimeModels()
        }
    }

    component MenuPanel: Rectangle {
        implicitWidth: 260
        implicitHeight: 36
        color: "#1a1a1a"
        border.color: page.palette.border
        radius: 8
    }

    component ComposerButton: Rectangle {
        id: control
        property alias text: label.text
        property string tooltip: ""
        property bool checked: false
        property bool compact: false
        signal clicked()

        Layout.preferredWidth: compact ? 30 : Math.min(220, Math.max(34, label.implicitWidth + 18))
        Layout.preferredHeight: 30
        color: checked ? "#24283a" : mouse.containsMouse ? "#242424" : "transparent"
        border.color: checked ? page.palette.accentDim : "transparent"
        border.width: checked ? 1 : 0
        radius: 6

        Text {
            id: label
            anchors.centerIn: parent
            width: parent.width - 12
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            color: control.checked ? page.palette.textPrim : page.palette.textSec
            font { pixelSize: 12 }
        }

        ToolTip.visible: mouse.containsMouse && control.tooltip.length > 0
        ToolTip.text: control.tooltip
        ToolTip.delay: 500

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: control.clicked()
        }
    }
}
