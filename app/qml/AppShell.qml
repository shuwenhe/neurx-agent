import QtQuick
import QtQuick.Layouts
import "components"
import "pages"

Item {
    id: shell

    property string selectedFilePath: ""
    property string selectedFileName: ""
    property string selectedFileContent: ""
    property real explorerPaneWidth: 304
    property real editorPaneWidth: 560

    readonly property real splitterWidth: 6
    readonly property real minExplorerPaneWidth: 220
    readonly property real minEditorPaneWidth: 320
    readonly property real minAgentPaneWidth: 420

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

    function clampPaneWidths() {
        const total = shell.width
        const fixed = splitterWidth * 2
        const maxExplorer = Math.max(minExplorerPaneWidth,
            total - fixed - minEditorPaneWidth - minAgentPaneWidth)

        explorerPaneWidth = Math.max(minExplorerPaneWidth,
            Math.min(explorerPaneWidth, maxExplorer))

        const maxEditor = Math.max(minEditorPaneWidth,
            total - fixed - explorerPaneWidth - minAgentPaneWidth)
        editorPaneWidth = Math.max(minEditorPaneWidth,
            Math.min(editorPaneWidth, maxEditor))
    }

    function fileNameFromPath(filePath) {
        const parts = filePath.split("/")
        return parts.length > 0 ? parts[parts.length - 1] : filePath
    }

    function openEditorFile(filePath, fileName, remember) {
        if (!filePath || filePath.length === 0)
            return

        shell.selectedFilePath = filePath
        shell.selectedFileName = fileName && fileName.length > 0
            ? fileName
            : shell.fileNameFromPath(filePath)
        shell.selectedFileContent = Runtime.readLocalFile(filePath)

        if (remember)
            Runtime.rememberOpenedEditorFile(filePath)
    }

    onWidthChanged: clampPaneWidths()
    Component.onCompleted: {
        clampPaneWidths()
        const lastFile = Runtime.lastOpenedEditorFile()
        if (lastFile.length > 0)
            openEditorFile(lastFile, fileNameFromPath(lastFile), false)
    }

    Row {
        anchors.fill: parent
        spacing: 0

        SideNav {
            id: sideNav
            height: parent.height
            width: shell.explorerPaneWidth
            currentIndex: pageStack.currentIndex
            onPageRequested: (idx) => pageStack.currentIndex = idx
            onFileRequested: (filePath, fileName) => {
                shell.openEditorFile(filePath, fileName, true)
            }
        }

        Rectangle {
            id: splitterExplorerEditor
            width: shell.splitterWidth
            height: parent.height
            color: "transparent"

            property real startX: 0
            property real startWidth: 0
            property bool dragging: false

            Rectangle {
                anchors.centerIn: parent
                width: splitterExplorerEditor.dragging ? 3 : 2
                height: parent.height
                radius: 1
                color: splitterExplorerEditor.dragging
                       ? shell.accent
                       : (splitMouse1.containsMouse ? "#6e7681" : shell.border)
                opacity: splitterExplorerEditor.dragging ? 1.0 : (splitMouse1.containsMouse ? 0.95 : 0.7)

                Behavior on color {
                    ColorAnimation { duration: 120 }
                }
                Behavior on opacity {
                    NumberAnimation { duration: 120 }
                }
            }

            MouseArea {
                id: splitMouse1
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.SplitHCursor

                onPressed: {
                    splitterExplorerEditor.dragging = true
                    splitterExplorerEditor.startX = mouse.x
                    splitterExplorerEditor.startWidth = shell.explorerPaneWidth
                }

                onReleased: splitterExplorerEditor.dragging = false
                onCanceled: splitterExplorerEditor.dragging = false

                onPositionChanged: {
                    if (!pressed)
                        return

                    const delta = mouse.x - splitterExplorerEditor.startX
                    const maxExplorer = Math.max(shell.minExplorerPaneWidth,
                        shell.width - shell.splitterWidth * 2 - shell.minEditorPaneWidth - shell.minAgentPaneWidth)

                    shell.explorerPaneWidth = Math.max(shell.minExplorerPaneWidth,
                        Math.min(maxExplorer, splitterExplorerEditor.startWidth + delta))
                    shell.clampPaneWidths()
                }
            }
        }

        Rectangle {
            id: editorPane
            width: shell.editorPaneWidth
            height: parent.height
            color: "#0f1115"
            border.color: shell.border

            Column {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    width: parent.width
                    height: 36
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

                CodeEditor {
                    width: parent.width
                    height: parent.height - 36
                    fileName: shell.selectedFileName
                    filePath: shell.selectedFilePath
                    content: shell.selectedFilePath.length > 0 ? shell.selectedFileContent : ""
                    backgroundColor: "#0f1115"
                    gutterColor: "#0c0f14"
                    borderColor: shell.border
                }
            }
        }

        Rectangle {
            id: splitterEditorAgent
            width: shell.splitterWidth
            height: parent.height
            color: "transparent"

            property real startX: 0
            property real startWidth: 0
            property bool dragging: false

            Rectangle {
                anchors.centerIn: parent
                width: splitterEditorAgent.dragging ? 3 : 2
                height: parent.height
                radius: 1
                color: splitterEditorAgent.dragging
                       ? shell.accent
                       : (splitMouse2.containsMouse ? "#6e7681" : shell.border)
                opacity: splitterEditorAgent.dragging ? 1.0 : (splitMouse2.containsMouse ? 0.95 : 0.7)

                Behavior on color {
                    ColorAnimation { duration: 120 }
                }
                Behavior on opacity {
                    NumberAnimation { duration: 120 }
                }
            }

            MouseArea {
                id: splitMouse2
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.SplitHCursor

                onPressed: {
                    splitterEditorAgent.dragging = true
                    splitterEditorAgent.startX = mouse.x
                    splitterEditorAgent.startWidth = shell.editorPaneWidth
                }

                onReleased: splitterEditorAgent.dragging = false
                onCanceled: splitterEditorAgent.dragging = false

                onPositionChanged: {
                    if (!pressed)
                        return

                    const delta = mouse.x - splitterEditorAgent.startX
                    const maxEditor = Math.max(shell.minEditorPaneWidth,
                        shell.width - shell.splitterWidth * 2 - shell.explorerPaneWidth - shell.minAgentPaneWidth)

                    shell.editorPaneWidth = Math.max(shell.minEditorPaneWidth,
                        Math.min(maxEditor, splitterEditorAgent.startWidth + delta))
                    shell.clampPaneWidths()
                }
            }
        }

        Rectangle {
            id: agentPane
            width: parent.width - sideNav.width - editorPane.width - shell.splitterWidth * 2
            height: parent.height
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
