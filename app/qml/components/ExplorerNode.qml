pragma ComponentBehavior: Bound

import QtQuick

Column {
    id: node
    property string name: ""
    property string path: ""
    property bool   isDir: false
    property int    depth: 0
    property bool expanded: false

    width: parent ? parent.width : 240

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
        color: rowMouse.containsMouse ? "#2a2d2e" : "transparent"

        Row {
            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
            leftPadding: 8 + node.depth * 12
            rightPadding: 8
            spacing: 4

            Text {
                width: 10
                text: node.isDir ? (node.expanded ? "⌄" : "›") : ""
                color: "#cccccc"
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
            }

            Text {
                width: 16
                text: node.isDir ? (node.expanded ? "▾" : "▸") : node.fileIcon(node.name)
                color: node.isDir ? "#c5c5c5" : node.fileColor(node.name)
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
            }

            Text {
                width: Math.max(120, row.width - 48 - node.depth * 12)
                text: node.name
                color: node.depth === 0 ? "#ffffff" : "#cccccc"
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
            cursorShape: node.isDir ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: {
                if (node.isDir)
                    node.expanded = !node.expanded
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
        }
    }
}
