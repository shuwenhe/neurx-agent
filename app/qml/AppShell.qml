import QtQuick
import QtQuick.Layouts
import "components"
import "pages"

Item {
    id: shell

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
        }

        // ── Main content area ─────────────────────────────────────
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
