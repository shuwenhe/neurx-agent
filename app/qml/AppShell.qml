import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import "components"
import "pages"

Item {
    id: shell

    property string selectedFilePath: ""
    property string selectedFileName: ""
    property string selectedFileContent: ""
    property string explorerFocusPath: ""
    property int explorerFocusRequest: 0
    property int activeEditorIndex: -1
    property real explorerPaneWidth: 304
    property real editorPaneWidth: 560
    property string pendingAddressTarget: ""
    property bool pendingAddressRemember: true
    property string addressStatusText: ""
    property bool addressStatusIsError: false
    property bool addressBusy: false
    property var recentAddressTargets: []
    property var pinnedAddressTargets: []
    property int addressSuggestionIndex: -1
    property int tabContextIndex: -1

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

    ListModel {
        id: openFilesModel
    }

    ListModel {
        id: addressSuggestionsModel
    }

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

    function normalizeAddressTarget(target) {
        const value = (target || "").trim()
        if (!value)
            return ""

        if (value.indexOf("://") > 0)
            return value

        if (value.indexOf("/") === 0)
            return value

        if (value.indexOf(".") >= 0 && value.indexOf(" ") < 0)
            return "https://" + value

        return value
    }

    function isUrlTarget(target) {
        const value = (target || "").trim().toLowerCase()
        return value.indexOf("http://") === 0 || value.indexOf("https://") === 0
    }

    function isLocalPathTarget(target) {
        const value = (target || "").trim()
        return value.length > 0 && value.indexOf("/") === 0
    }

    function canPersistPath(target) {
        return isLocalPathTarget(target)
    }

    function isReadError(content) {
        return String(content || "").indexOf("Unable to open") === 0
    }

    function setAddressStatus(text, isError) {
        addressStatusText = text
        addressStatusIsError = !!isError
        if (text && text.length > 0)
            clearAddressStatusTimer.restart()
    }

    function pushRecentAddressTarget(target) {
        const normalized = normalizeAddressTarget(target)
        if (!normalized)
            return

        const next = []
        if (pinnedAddressTargets.indexOf(normalized) < 0)
            next.push(normalized)
        for (let i = 0; i < recentAddressTargets.length; ++i) {
            const item = recentAddressTargets[i]
            if (item !== normalized && pinnedAddressTargets.indexOf(item) < 0)
                next.push(item)
            if (next.length >= 20)
                break
        }
        recentAddressTargets = next
        Runtime.setAddressHistory(recentAddressTargets)
        refreshAddressSuggestions()
    }

    function isPinnedAddressTarget(target) {
        return pinnedAddressTargets.indexOf(target) >= 0
    }

    function togglePinnedAddressTarget(target) {
        const normalized = normalizeAddressTarget(target)
        if (!normalized)
            return

        const nextPinned = []
        const existingIndex = pinnedAddressTargets.indexOf(normalized)
        if (existingIndex >= 0) {
            for (let i = 0; i < pinnedAddressTargets.length; ++i) {
                if (i !== existingIndex)
                    nextPinned.push(pinnedAddressTargets[i])
            }
        } else {
            nextPinned.push(normalized)
            for (let i = 0; i < pinnedAddressTargets.length; ++i) {
                const item = pinnedAddressTargets[i]
                if (item !== normalized)
                    nextPinned.push(item)
                if (nextPinned.length >= 20)
                    break
            }
        }

        pinnedAddressTargets = nextPinned
        Runtime.setPinnedAddressHistory(pinnedAddressTargets)

        const filteredRecent = []
        for (let i = 0; i < recentAddressTargets.length; ++i) {
            const item = recentAddressTargets[i]
            if (pinnedAddressTargets.indexOf(item) < 0)
                filteredRecent.push(item)
        }
        recentAddressTargets = filteredRecent
        Runtime.setAddressHistory(recentAddressTargets)
        refreshAddressSuggestions()
    }

    function clearRecentAddressTargets() {
        recentAddressTargets = []
        addressSuggestionsModel.clear()
        addressSuggestionIndex = -1
        suggestionsPopup.close()
        Runtime.clearAddressHistory()
        setAddressStatus(qsTr("Address history cleared"), false)
    }

    function refreshAddressSuggestions() {
        addressSuggestionsModel.clear()
        const query = (addressField ? addressField.text : "").trim().toLowerCase()

        for (let i = 0; i < pinnedAddressTargets.length; ++i) {
            const candidate = pinnedAddressTargets[i]
            if (!query || candidate.toLowerCase().indexOf(query) >= 0)
                addressSuggestionsModel.append({ value: candidate, pinned: true })
            if (addressSuggestionsModel.count >= 8)
                break
        }

        for (let i = 0; i < recentAddressTargets.length; ++i) {
            const candidate = recentAddressTargets[i]
            if (!query || candidate.toLowerCase().indexOf(query) >= 0)
                addressSuggestionsModel.append({ value: candidate, pinned: false })
            if (addressSuggestionsModel.count >= 8)
                break
        }

        if (addressSuggestionsModel.count > 0) {
            addressSuggestionIndex = Math.min(addressSuggestionIndex, addressSuggestionsModel.count - 1)
            if (addressSuggestionIndex < 0)
                addressSuggestionIndex = 0
            suggestionsPopup.open()
        } else {
            addressSuggestionIndex = -1
            suggestionsPopup.close()
        }
    }

    function selectCurrentSuggestion() {
        if (!suggestionsPopup.opened || addressSuggestionIndex < 0 || addressSuggestionIndex >= addressSuggestionsModel.count)
            return false

        const value = addressSuggestionsModel.get(addressSuggestionIndex).value
        addressField.text = value
        suggestionsPopup.close()
        shell.openEditorTarget(value, true)
        return true
    }

    function queueOpenEditorTarget(target, remember) {
        const normalized = normalizeAddressTarget(target)
        if (!normalized) {
            setAddressStatus(qsTr("Enter a file path or URL"), true)
            return
        }

        pendingAddressTarget = normalized
        pendingAddressRemember = remember
        addressBusy = true
        setAddressStatus(qsTr("Opening..."), false)
        openAddressTimer.restart()
    }

    function performOpenEditorTarget() {
        const normalized = pendingAddressTarget
        addressBusy = false
        if (!normalized)
            return

        if (shell.isUrlTarget(normalized)) {
            shell.openEditorFile(normalized, normalized, false, "")
            pushRecentAddressTarget(normalized)
            setAddressStatus(qsTr("Opened URL"), false)
            return
        }

        if (!shell.isLocalPathTarget(normalized)) {
            setAddressStatus(qsTr("Only absolute file paths and http/https URLs are supported"), true)
            return
        }

        const localContent = Runtime.readLocalFile(normalized)
        if (shell.isReadError(localContent)) {
            setAddressStatus(localContent, true)
            return
        }

        shell.openEditorFile(normalized, shell.fileNameFromPath(normalized), pendingAddressRemember, localContent)
        pushRecentAddressTarget(normalized)
        setAddressStatus(qsTr("Opened file"), false)
    }

    function openEditorTarget(target, remember) {
        queueOpenEditorTarget(target, remember)
    }

    function findOpenFileIndex(filePath) {
        for (let i = 0; i < openFilesModel.count; ++i) {
            if (openFilesModel.get(i).filePath === filePath)
                return i
        }
        return -1
    }

    function syncActiveEditor() {
        if (activeEditorIndex < 0 || activeEditorIndex >= openFilesModel.count) {
            selectedFilePath = ""
            selectedFileName = ""
            selectedFileContent = ""
            explorerFocusPath = ""
            if (addressField)
                addressField.text = ""
            return
        }

        const entry = openFilesModel.get(activeEditorIndex)
        selectedFilePath = entry.filePath
        selectedFileName = entry.fileName
        selectedFileContent = entry.content
        if (shell.isLocalPathTarget(selectedFilePath)) {
            explorerFocusPath = selectedFilePath
            explorerFocusRequest += 1
        } else {
            explorerFocusPath = ""
        }
        if (addressField)
            addressField.text = selectedFilePath
    }

    function openEditorFile(filePath, fileName, remember, contentOverride) {
        if (!filePath || filePath.length === 0)
            return

        const resolvedName = fileName && fileName.length > 0
            ? fileName
            : shell.fileNameFromPath(filePath)

        let index = shell.findOpenFileIndex(filePath)
        if (index < 0) {
            openFilesModel.append({
                filePath: filePath,
                fileName: resolvedName,
                content: contentOverride !== undefined
                    ? contentOverride
                    : Runtime.readLocalFile(filePath)
            })
            index = openFilesModel.count - 1
        } else {
            if (contentOverride !== undefined)
                openFilesModel.setProperty(index, "content", contentOverride)

            // Reopening an existing file makes it the newest tab on the far right.
            if (index !== openFilesModel.count - 1) {
                openFilesModel.move(index, openFilesModel.count - 1, 1)
                index = openFilesModel.count - 1
            }
        }

        activeEditorIndex = index
        shell.syncActiveEditor()
        shell.ensureNewestTabVisible()

        if (remember && shell.canPersistPath(filePath))
            Runtime.rememberOpenedEditorFile(filePath)
    }

    function handleEditorWebUrlChanged(url) {
        const normalized = (url || "").trim()
        if (!shell.isUrlTarget(normalized))
            return
        if (activeEditorIndex < 0 || activeEditorIndex >= openFilesModel.count)
            return

        const currentPath = openFilesModel.get(activeEditorIndex).filePath
        if (!shell.isUrlTarget(currentPath))
            return

        if (currentPath !== normalized) {
            openFilesModel.setProperty(activeEditorIndex, "filePath", normalized)
            openFilesModel.setProperty(activeEditorIndex, "fileName", normalized)
            selectedFilePath = normalized
            selectedFileName = normalized
            explorerFocusPath = ""
            pushRecentAddressTarget(normalized)
        }

        if (addressField && !addressField.activeFocus)
            addressField.text = normalized
    }

    function openWebUrlInNewEditorTab(url) {
        const normalized = shell.normalizeAddressTarget(url)
        if (!shell.isUrlTarget(normalized))
            return

        shell.openEditorFile(normalized, normalized, false, "")
        shell.pushRecentAddressTarget(normalized)
        shell.setAddressStatus(qsTr("Opened in new tab"), false)
    }

    function selectEditorFile(index) {
        if (index < 0 || index >= openFilesModel.count)
            return

        activeEditorIndex = index
        syncActiveEditor()
        if (shell.canPersistPath(selectedFilePath))
            Runtime.rememberOpenedEditorFile(selectedFilePath)
    }

    function closeEditorFile(index) {
        if (index < 0 || index >= openFilesModel.count)
            return

        const closingPath = openFilesModel.get(index).filePath
        const wasActive = index === activeEditorIndex

        openFilesModel.remove(index)

        if (openFilesModel.count === 0) {
            activeEditorIndex = -1
            syncActiveEditor()
            return
        }

        if (wasActive) {
            activeEditorIndex = Math.min(index, openFilesModel.count - 1)
        } else if (index < activeEditorIndex) {
            activeEditorIndex -= 1
        }

        syncActiveEditor()

        if (selectedFilePath !== closingPath && shell.canPersistPath(selectedFilePath))
            Runtime.rememberOpenedEditorFile(selectedFilePath)
    }

    function closeCurrentEditor() {
        if (activeEditorIndex >= 0)
            closeEditorFile(activeEditorIndex)
    }

    function updateActiveEditorContent(content) {
        if (activeEditorIndex < 0 || activeEditorIndex >= openFilesModel.count)
            return

        openFilesModel.setProperty(activeEditorIndex, "content", content)
        selectedFileContent = content
    }

    function closeOtherEditorFiles(index) {
        if (index < 0 || index >= openFilesModel.count)
            return

        const activePath = openFilesModel.get(index).filePath
        const activeName = openFilesModel.get(index).fileName
        const activeContent = openFilesModel.get(index).content
        openFilesModel.clear()
        openFilesModel.append({
            filePath: activePath,
            fileName: activeName,
            content: activeContent
        })
        activeEditorIndex = 0
        syncActiveEditor()

        if (shell.canPersistPath(selectedFilePath))
            Runtime.rememberOpenedEditorFile(selectedFilePath)
    }

    function closeEditorFilesToRight(index) {
        if (index < 0 || index >= openFilesModel.count)
            return

        for (let i = openFilesModel.count - 1; i > index; --i)
            openFilesModel.remove(i)

        activeEditorIndex = Math.min(activeEditorIndex, openFilesModel.count - 1)
        syncActiveEditor()

        if (shell.canPersistPath(selectedFilePath))
            Runtime.rememberOpenedEditorFile(selectedFilePath)
    }

    function closeAllEditorFiles() {
        openFilesModel.clear()
        activeEditorIndex = -1
        syncActiveEditor()
    }

    function openEditorTabContextMenu(index, tabItem, mouse) {
        if (index < 0 || index >= openFilesModel.count)
            return

        tabContextIndex = index
        const point = tabItem.mapToItem(editorTabMenu.parent, mouse.x, mouse.y)
        editorTabMenu.x = point.x
        editorTabMenu.y = point.y
        editorTabMenu.open()
    }

    function ensureNewestTabVisible() {
        Qt.callLater(function() {
            if (!tabsFlick)
                return
            tabsFlick.contentX = Math.max(0, tabsFlick.contentWidth - tabsFlick.width)
        })
    }

    onWidthChanged: clampPaneWidths()
    Component.onCompleted: {
        clampPaneWidths()
        const loadedHistory = Runtime.addressHistory()
        recentAddressTargets = loadedHistory ? loadedHistory : []
        const loadedPinned = Runtime.pinnedAddressHistory()
        pinnedAddressTargets = loadedPinned ? loadedPinned : []

        const filteredRecent = []
        for (let i = 0; i < recentAddressTargets.length; ++i) {
            if (pinnedAddressTargets.indexOf(recentAddressTargets[i]) < 0)
                filteredRecent.push(recentAddressTargets[i])
        }
        recentAddressTargets = filteredRecent

        const lastFile = Runtime.lastOpenedEditorFile()
        if (lastFile.length > 0)
            openEditorFile(lastFile, fileNameFromPath(lastFile), false)
    }

    Timer {
        id: openAddressTimer
        interval: 0
        repeat: false
        onTriggered: shell.performOpenEditorTarget()
    }

    Timer {
        id: clearAddressStatusTimer
        interval: 2000
        repeat: false
        onTriggered: shell.setAddressStatus("", false)
    }

    Connections {
        target: Runtime

        function onFileChanged(filePath) {
            const index = shell.findOpenFileIndex(filePath)
            if (index < 0)
                return

            const content = Runtime.readLocalFile(filePath)
            if (shell.isReadError(content))
                return

            openFilesModel.setProperty(index, "content", content)
            if (index === shell.activeEditorIndex)
                shell.syncActiveEditor()
        }
    }

    Row {
        anchors.fill: parent
        spacing: 0

        SideNav {
            id: sideNav
            height: parent.height
            width: shell.explorerPaneWidth
            currentIndex: pageStack.currentIndex
            focusPath: shell.explorerFocusPath
            focusRequest: shell.explorerFocusRequest
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

                onPressed: function(mouse) {
                    splitterExplorerEditor.dragging = true
                    splitterExplorerEditor.startX = mouse.x
                    splitterExplorerEditor.startWidth = shell.explorerPaneWidth
                }

                onReleased: splitterExplorerEditor.dragging = false
                onCanceled: splitterExplorerEditor.dragging = false

                onPositionChanged: function(mouse) {
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
                    height: 38
                    color: "#12161d"
                    border.color: shell.border

                    Menu {
                        id: editorTabMenu
                        modal: true

                        MenuItem {
                            text: qsTr("Close")
                            enabled: shell.tabContextIndex >= 0
                            onTriggered: shell.closeEditorFile(shell.tabContextIndex)
                        }

                        MenuItem {
                            text: qsTr("Close Others")
                            enabled: shell.tabContextIndex >= 0 && openFilesModel.count > 1
                            onTriggered: shell.closeOtherEditorFiles(shell.tabContextIndex)
                        }

                        MenuItem {
                            text: qsTr("Close to the Right")
                            enabled: shell.tabContextIndex >= 0
                                && shell.tabContextIndex < openFilesModel.count - 1
                            onTriggered: shell.closeEditorFilesToRight(shell.tabContextIndex)
                        }

                        MenuSeparator {}

                        MenuItem {
                            text: qsTr("Close All")
                            enabled: openFilesModel.count > 0
                            onTriggered: shell.closeAllEditorFiles()
                        }
                    }

                    Flickable {
                        id: tabsFlick
                        anchors.fill: parent
                        clip: true
                        contentWidth: tabsRow.implicitWidth + 16
                        contentHeight: tabsRow.implicitHeight
                        boundsBehavior: Flickable.StopAtBounds

                        Row {
                            id: tabsRow
                            x: 8
                            y: 0
                            spacing: 1

                            Repeater {
                                model: openFilesModel

                                delegate: Rectangle {
                                    required property int index
                                    required property string fileName
                                    required property string filePath

                                    width: Math.max(132, Math.min(240, tabLabel.implicitWidth + 42))
                                    height: 38
                                    color: index === shell.activeEditorIndex ? "#0f1115" : "#171c24"
                                    border.color: index === shell.activeEditorIndex ? shell.accent : shell.border

                                    Text {
                                        id: tabLabel
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        verticalAlignment: Text.AlignVCenter
                                        text: fileName
                                        color: index === shell.activeEditorIndex ? shell.textPrim : shell.textSec
                                        elide: Text.ElideMiddle
                                        font.pixelSize: 12
                                    }

                                    Rectangle {
                                        width: 18
                                        height: 18
                                        radius: 9
                                        anchors.right: parent.right
                                        anchors.rightMargin: 8
                                        anchors.verticalCenter: parent.verticalCenter
                                        z: 1
                                        visible: tabMouse.containsMouse || closeHover.containsMouse
                                        color: closeHover.containsMouse ? "#313742" : "transparent"

                                        Text {
                                            anchors.centerIn: parent
                                            text: "×"
                                            color: index === shell.activeEditorIndex ? shell.textPrim : shell.textSec
                                            font.pixelSize: 12
                                        }

                                        MouseArea {
                                            id: closeHover
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: shell.closeEditorFile(index)
                                        }
                                    }

                                    MouseArea {
                                        id: tabMouse
                                        anchors.fill: parent
                                        anchors.rightMargin: 28
                                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: function(mouse) {
                                            if (mouse.button === Qt.RightButton) {
                                                shell.openEditorTabContextMenu(index, parent, mouse)
                                            } else {
                                                shell.selectEditorFile(index)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 36
                    color: "#161a20"
                    border.color: shell.border

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        anchors.topMargin: 5
                        anchors.bottomMargin: 5
                        spacing: 8

                        TextField {
                            id: addressField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Enter file path or URL")
                            color: "#b7c0cc"
                            placeholderTextColor: "#6f7b8a"
                            selectedTextColor: shell.textPrim
                            selectionColor: shell.accentDim
                            font.pixelSize: 12
                            leftPadding: 8
                            rightPadding: 8
                            background: Rectangle {
                                radius: 5
                                color: "#10151c"
                                border.width: 1
                                border.color: addressField.activeFocus ? shell.accent : shell.border
                            }
                            onTextEdited: shell.refreshAddressSuggestions()
                            onActiveFocusChanged: {
                                if (activeFocus)
                                    shell.refreshAddressSuggestions()
                                else
                                    suggestionsPopup.close()
                            }
                            onAccepted: {
                                if (!shell.selectCurrentSuggestion())
                                    shell.openEditorTarget(text, true)
                            }
                            Keys.onEscapePressed: {
                                text = shell.selectedFilePath
                                suggestionsPopup.close()
                            }
                            Keys.onPressed: function(event) {
                                if (!suggestionsPopup.opened)
                                    return

                                if (event.key === Qt.Key_Down) {
                                    shell.addressSuggestionIndex = Math.min(
                                        shell.addressSuggestionIndex + 1,
                                        addressSuggestionsModel.count - 1)
                                    event.accepted = true
                                } else if (event.key === Qt.Key_Up) {
                                    shell.addressSuggestionIndex = Math.max(
                                        shell.addressSuggestionIndex - 1,
                                        0)
                                    event.accepted = true
                                } else if (event.key === Qt.Key_Tab) {
                                    if (shell.addressSuggestionIndex >= 0
                                            && shell.addressSuggestionIndex < addressSuggestionsModel.count) {
                                        addressField.text = addressSuggestionsModel.get(shell.addressSuggestionIndex).value
                                        suggestionsPopup.close()
                                        event.accepted = true
                                    }
                                }
                            }
                        }

                        Text {
                            Layout.preferredWidth: 120
                            text: shell.addressStatusText
                            color: shell.addressStatusIsError ? shell.danger : shell.textSec
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }

                        Rectangle {
                            Layout.preferredWidth: 50
                            Layout.preferredHeight: 24
                            radius: 5
                            color: shell.addressBusy ? "#2d3340" : "#222a34"
                            border.width: 1
                            border.color: shell.addressBusy ? shell.accent : shell.border

                            Text {
                                anchors.centerIn: parent
                                text: shell.addressBusy ? "..." : qsTr("Go")
                                color: shell.textPrim
                                font.pixelSize: 11
                            }

                            MouseArea {
                                anchors.fill: parent
                                enabled: !shell.addressBusy
                                cursorShape: Qt.PointingHandCursor
                                onClicked: shell.openEditorTarget(addressField.text, true)
                            }
                        }
                    }

                    Popup {
                        id: suggestionsPopup
                        x: 10
                        y: parent.height
                        width: parent.width - 70
                        height: Math.min(260, suggestionsList.contentHeight + clearHistoryRow.height + 12)
                        padding: 4
                        modal: false
                        focus: false
                        closePolicy: Popup.NoAutoClose
                        visible: opened

                        background: Rectangle {
                            color: "#11161d"
                            border.color: shell.border
                            radius: 6
                        }

                        contentItem: Column {
                            spacing: 4

                            ListView {
                                id: suggestionsList
                                width: parent.width
                                height: Math.min(220, contentHeight)
                                clip: true
                                model: addressSuggestionsModel
                                boundsBehavior: Flickable.StopAtBounds

                                delegate: Rectangle {
                                    required property int index
                                    required property string value
                                    required property bool pinned

                                    width: suggestionsList.width
                                    height: 28
                                    radius: 4
                                    color: index === shell.addressSuggestionIndex ? "#253042" : "transparent"

                                    Rectangle {
                                        width: 18
                                        height: 18
                                        radius: 9
                                        anchors.left: parent.left
                                        anchors.leftMargin: 6
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: pinMouse.containsMouse ? "#2b3340" : "transparent"

                                        Text {
                                            anchors.centerIn: parent
                                            text: pinned ? "★" : "☆"
                                            color: pinned ? "#facc15" : shell.textSec
                                            font.pixelSize: 11
                                        }

                                        MouseArea {
                                            id: pinMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            onClicked: shell.togglePinnedAddressTarget(value)
                                        }
                                    }

                                    Text {
                                        anchors.fill: parent
                                        anchors.leftMargin: 30
                                        anchors.rightMargin: 8
                                        verticalAlignment: Text.AlignVCenter
                                        text: value
                                        color: shell.textPrim
                                        elide: Text.ElideMiddle
                                        font.pixelSize: 11
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onEntered: shell.addressSuggestionIndex = index
                                        onClicked: {
                                            addressField.text = value
                                            suggestionsPopup.close()
                                            shell.openEditorTarget(value, true)
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                id: clearHistoryRow
                                width: parent.width
                                height: 28
                                radius: 4
                                color: clearMouse.containsMouse ? "#2a1a1a" : "transparent"
                                visible: shell.recentAddressTargets.length > 0 || shell.pinnedAddressTargets.length > 0

                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    verticalAlignment: Text.AlignVCenter
                                    text: qsTr("Clear recent history")
                                    color: "#fca5a5"
                                    font.pixelSize: 11
                                }

                                MouseArea {
                                    id: clearMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: shell.clearRecentAddressTargets()
                                }
                            }
                        }
                    }
                }

                CodeEditor {
                    id: editorView
                    width: parent.width
                    height: parent.height - 74
                    fileName: shell.selectedFileName
                    filePath: shell.selectedFilePath
                    content: shell.selectedFilePath.length > 0 ? shell.selectedFileContent : ""
                    backgroundColor: "#0f1115"
                    gutterColor: "#0c0f14"
                    borderColor: shell.border
                    onWebUrlChanged: (url) => shell.handleEditorWebUrlChanged(url)
                    onWebNewTabRequested: (url) => shell.openWebUrlInNewEditorTab(url)
                    onContentEdited: (text) => shell.updateActiveEditorContent(text)
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

                onPressed: function(mouse) {
                    splitterEditorAgent.dragging = true
                    splitterEditorAgent.startX = mouse.x
                    splitterEditorAgent.startWidth = shell.editorPaneWidth
                }

                onReleased: splitterEditorAgent.dragging = false
                onCanceled: splitterEditorAgent.dragging = false

                onPositionChanged: function(mouse) {
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
                ChatPage {
                    palette: shell
                    currentFilePath: shell.selectedFilePath
                    currentFileContent: shell.selectedFileContent
                }
                ToolsPage     { palette: shell }
                SettingsPage  { palette: shell }
            }
        }
    }
}
