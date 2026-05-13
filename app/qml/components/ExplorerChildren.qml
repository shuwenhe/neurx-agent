pragma ComponentBehavior: Bound

import QtQuick
import Qt.labs.folderlistmodel

Column {
    id: children

    property string parentPath: ""
    property int depth: 1
    property bool showHidden: false
    property string focusPath: ""
    property int focusRequest: 0
    property Flickable scrollTarget: null
    signal fileActivated(string filePath, string fileName)

    Connections {
        target: Runtime

        function onFileChanged(filePath) {
            if (!filePath || children.parentPath.length === 0)
                return

            const normalizedParent = children.parentPath.endsWith("/")
                ? children.parentPath
                : children.parentPath + "/"
            const normalizedChanged = String(filePath)

            if (normalizedChanged === children.parentPath
                    || normalizedChanged.indexOf(normalizedParent) === 0) {
                folderModel.refresh()
            }
        }
    }

    FolderListModel {
        id: folderModel
        folder: children.parentPath.length > 0 ? "file://" + children.parentPath : ""
        showDirs: true
        showFiles: true
        showDotAndDotDot: false
        showHidden: children.showHidden
        sortField: FolderListModel.Name
        caseSensitive: false
    }

    Repeater {
        model: folderModel

        delegate: ExplorerNode {
            required property string fileName
            required property string filePath
            required property bool fileIsDir

            width: children.width
            name: fileName
            path: filePath
            isDir: fileIsDir
            depth: children.depth
            focusPath: children.focusPath
            focusRequest: children.focusRequest
            scrollTarget: children.scrollTarget
            onFileActivated: (selectedPath, selectedName) =>
                children.fileActivated(selectedPath, selectedName)
        }
    }
}
