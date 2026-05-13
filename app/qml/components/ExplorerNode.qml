pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

Column {
    id: node
    property string name: ""
    property string path: ""
    property bool   isDir: false
    property int    depth: 0
    property bool expanded: false
    property string focusPath: ""
    property int focusRequest: 0
    property Flickable scrollTarget: null
    signal fileActivated(string filePath, string fileName)

    width: parent ? parent.width : 240

    readonly property bool isFocusedFile: node.focusPath.length > 0 && node.path === node.focusPath
    readonly property bool isFocusedAncestor: node.isDir
                                           && node.focusPath.length > 0
                                           && node.focusPath.indexOf(node.path.endsWith("/") ? node.path : node.path + "/") === 0

    function syncFocusState() {
        if (node.isFocusedAncestor)
            node.expanded = true

        if (node.isFocusedFile)
            node.scheduleScrollIntoView()
    }

    function scrollIntoView() {
        if (!node.isFocusedFile || !node.scrollTarget)
            return

        const topLeft = row.mapToItem(node.scrollTarget, 0, 0)
        const bottomLeft = row.mapToItem(node.scrollTarget, 0, row.height)

        if (topLeft.y < 0) {
            node.scrollTarget.contentY = Math.max(0, node.scrollTarget.contentY + topLeft.y - 12)
        } else if (bottomLeft.y > node.scrollTarget.height) {
            node.scrollTarget.contentY = Math.max(0,
                node.scrollTarget.contentY + (bottomLeft.y - node.scrollTarget.height) + 12)
        }
    }

    function scheduleScrollIntoView() {
        scrollRetryTimer.remainingAttempts = 8
        scrollRetryTimer.restart()
    }

    function openContextMenu(mouseX) {
        fileContextMenu.targetPath = node.path
        fileContextMenu.targetName = node.name
        fileContextMenu.targetIsDir = node.isDir
        fileContextMenu.x = Math.max(6, Math.min(mouseX, row.width - fileContextMenu.width - 6))
        fileContextMenu.y = row.height - 1
        fileContextMenu.open()
    }

    Component.onCompleted: syncFocusState()
    onFocusPathChanged: syncFocusState()
    onIsFocusedFileChanged: syncFocusState()
    onIsFocusedAncestorChanged: syncFocusState()
    onFocusRequestChanged: syncFocusState()
    onExpandedChanged: {
        if (node.isFocusedAncestor || node.isFocusedFile)
            Qt.callLater(node.syncFocusState)
    }

    Timer {
        id: scrollRetryTimer
        interval: 35
        repeat: true
        property int remainingAttempts: 0

        onTriggered: {
            node.scrollIntoView()
            remainingAttempts -= 1
            if (remainingAttempts <= 0)
                stop()
        }
    }

    function fileIcon(fileName) {
        const lower = fileName.toLowerCase()
        if (lower.endsWith(".qml"))   return "Q"
        if (lower.endsWith(".cpp") || lower.endsWith(".h") || lower.endsWith(".hpp")) return "C"
        if (lower.endsWith(".json"))  return "{}"
        if (lower.endsWith(".md"))    return "M"
        return "·"
    }

    function fileColor(fileName) {
        const lower = fileName.toLowerCase()
        if (lower.endsWith(".qml"))   return "#c586c0"
        if (lower.endsWith(".cpp") || lower.endsWith(".h") || lower.endsWith(".hpp")) return "#4fc1ff"
        if (lower.endsWith(".json"))  return "#dcdcaa"
        if (lower.endsWith(".md"))    return "#6a9955"
        return "#858585"
    }

    Rectangle {
        id: row
        width: node.width
        height: 24
          color: rowMouse.containsMouse ? "#2a2d2e"
              : node.isFocusedFile ? "#264f78"
              : node.isFocusedAncestor ? "#1f2430"
              : "transparent"

        Row {
            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
            leftPadding: 8 + node.depth * 12
            rightPadding: 8
            spacing: 4

            Text {
                width: 10
                text: node.isDir ? (node.expanded ? "⌄" : "›") : ""
                color: node.isFocusedFile ? "#ffffff" : "#cccccc"
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
            }

            Text {
                width: 16
                text: node.isDir ? (node.expanded ? "▾" : "▸") : node.fileIcon(node.name)
                color: node.isFocusedFile ? "#ffffff" : (node.isDir ? "#c5c5c5" : node.fileColor(node.name))
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
            }

            Text {
                width: Math.max(120, row.width - 48 - node.depth * 12)
                text: node.name
                color: node.isFocusedFile ? "#ffffff" : (node.depth === 0 ? "#ffffff" : "#cccccc")
                font {
                    pixelSize: node.depth === 0 ? 12 : 13
                    weight:    node.depth === 0 ? Font.DemiBold : Font.Normal
                }
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
        }

        MouseArea {
            id: rowMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            cursorShape: Qt.PointingHandCursor

            onPressed: function(mouse) {
                if (mouse.button !== Qt.RightButton)
                    return

                node.openContextMenu(mouse.x)
            }

            onClicked: function(mouse) {
                if (mouse.button !== Qt.LeftButton)
                    return

                if (node.isDir)
                    node.expanded = !node.expanded
                else
                    node.fileActivated(node.path, node.name)
            }
        }

        Menu {
            id: fileContextMenu
            property string targetPath: ""
            property string targetName: ""
            property bool targetIsDir: false
            width: 180

            MenuItem {
                text: qsTr("Copy Path")
                onTriggered: Runtime.copyToClipboard(fileContextMenu.targetPath)
            }

            MenuItem {
                text: qsTr("Reveal in Finder")
                onTriggered: Runtime.revealInFinder(fileContextMenu.targetPath)
            }
        }
    }

    Loader {
        active: node.isDir && node.expanded
        width: node.width
        source: "ExplorerChildren.qml"

        onLoaded: {
            if (!item)
                return

            item.width = Qt.binding(function() { return node.width })
            item.parentPath = node.path
            item.depth = node.depth + 1
            item.showHidden = false
            item.focusPath = Qt.binding(function() { return node.focusPath })
            item.focusRequest = Qt.binding(function() { return node.focusRequest })
            item.scrollTarget = Qt.binding(function() { return node.scrollTarget })
            item.fileActivated.connect(function(filePath, fileName) {
                node.fileActivated(filePath, fileName)
            })
            Qt.callLater(node.syncFocusState)
        }
    }
}
