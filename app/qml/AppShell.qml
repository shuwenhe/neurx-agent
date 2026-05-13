import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "components"
import "pages"

Item {
    id: shell

    property string selectedFilePath: ""
    property string selectedFileName: ""
    property string selectedFileContent: ""

    // Palette
    readonly property color bg:        "#0d0d0d"
    readonly property color surface:   "#1a1a1a"
    readonly property color border:    "#2a2a2a"
    readonly property color accent:    "#6c63ff"
    readonly property color accentDim: "#4a44b8"
    readonly property color textPrim:  "#f0f0f0"
    readonly property color textSec:   "#9a9a9a"
    readonly property color success:   "#22c55e"
    readonly property color warning:   "#f59e0b"
    readonly property color danger:    "#ef4444"

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ── Side navigation ───────────────────────────────────────
        SideNav {
            id: sideNav
            Layout.fillHeight: true
            Layout.preferredWidth: 304
            currentIndex: pageStack.currentIndex
            onPageRequested: (idx) => pageStack.currentIndex = idx
            onFileRequested: (filePath, fileName) => {
                shell.selectedFilePath = filePath
                shell.selectedFileName = fileName
                shell.selectedFileContent = Runtime.readLocalFile(filePath)
            }
        }

        // ── Main content area ─────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: shell.bg

            RowLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 560
                    Layout.minimumWidth: 380
                    color: "#0f1115"
                    border.color: shell.border

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            color: "#161a20"
                            border.color: shell.border

                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideMiddle
                                text: shell.selectedFilePath.length > 0
                                      ? shell.selectedFilePath
                                      : qsTr("EDITOR")
                                color: "#b7c0cc"
                                font.pixelSize: 12
                            }
                        }

                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true

                            TextArea {
                                readOnly: true
                                wrapMode: TextArea.NoWrap
                                selectByMouse: true
                                textFormat: TextEdit.PlainText
                                text: shell.selectedFilePath.length > 0
                                      ? shell.selectedFileContent
                                      : qsTr("点击 Explorer 中的文件，在这里查看内容。")
                                color: "#d6deea"
                                background: Rectangle { color: "#0f1115" }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: shell.bg

                    StackLayout {
                        id: pageStack
                        anchors.fill: parent
                        currentIndex: 2

                        DashboardPage { palette: shell }
                        AgentsPage    { palette: shell }
                        ChatPage      { palette: shell }
                        ToolsPage     { palette: shell }
                        SettingsPage  { palette: shell }
                    }
                }
            }
        }
    }
}
