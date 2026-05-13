pragma ComponentBehavior: Bound

import QtQuick
import Qt.labs.folderlistmodel

Column {
    id: children

    property string parentPath: ""
    property int depth: 1
    property bool showHidden: false
    property string focusPath: ""
    property Item scrollTarget: null
    signal fileActivated(string filePath, string fileName)

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
            scrollTarget: children.scrollTarget
            onFileActivated: (selectedPath, selectedName) =>
                children.fileActivated(selectedPath, selectedName)
        }
    }
}
