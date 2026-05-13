pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

Rectangle {
    id: editor

    signal webUrlChanged(string url)
    signal webNewTabRequested(string url)

    property string fileName: ""
    property string filePath: ""
    property string content: ""
    property color backgroundColor: "#0f1115"
    property color gutterColor: "#0c0f14"
    property color borderColor: "#2a2a2a"
    property color textColor: "#d6deea"
    property color lineNumberColor: "#5d6775"

    color: backgroundColor

    readonly property int lineHeight: 20
    readonly property int fontSize: 13
    readonly property int lineCount: Math.max(1, content.length === 0 ? 1 : content.split("\n").length)
    readonly property int gutterWidth: Math.max(48, 24 + lineCount.toString().length * 8)
    readonly property bool isWebTarget: filePath.toLowerCase().indexOf("http://") === 0
                                     || filePath.toLowerCase().indexOf("https://") === 0

    property var webViewObject: null
    property string webViewError: ""
    property bool webCanGoBack: false
    property bool webCanGoForward: false
    property bool webLoading: false
    property string webCurrentUrl: ""
    property bool webAddressEditing: false

    function ensureWebView() {
        if (!isWebTarget) {
            if (webViewObject) {
                webViewObject.destroy()
                webViewObject = null
            }
            webViewError = ""
            webCanGoBack = false
            webCanGoForward = false
            webLoading = false
            webCurrentUrl = ""
            return
        }

        if (!webViewObject) {
            try {
                webViewObject = Qt.createQmlObject(
                    "import QtQuick; import QtWebEngine; import QtWebChannel; WebEngineView { anchors.fill: parent; anchors.topMargin: 34; settings.forceDarkMode: true; onUrlChanged: editor.onDynamicUrlChanged(url.toString()); onNewWindowRequested: function(request) { editor.onDynamicNewWindowRequested(request.requestedUrl.toString()) } }",
                    webContainer,
                    "dynamicWebView")
                webViewError = ""
            } catch (err) {
                webViewError = qsTr("Web rendering is unavailable. Please install Qt WebEngine and Qt WebChannel for this Qt kit.")
                webViewObject = null
                return
            }
        }

        if (webViewObject)
            webViewObject.url = filePath
    }

    function onDynamicUrlChanged(urlText) {
        if (!urlText || urlText.length === 0)
            return
        webCurrentUrl = urlText
        webUrlChanged(urlText)
    }

    function onDynamicNewWindowRequested(urlText) {
        const target = normalizeWebUrl(urlText || "")
        if (!target)
            return
        webNewTabRequested(target)
    }

    function syncWebState() {
        if (!webViewObject)
            return
        webCanGoBack = !!webViewObject.canGoBack
        webCanGoForward = !!webViewObject.canGoForward
        webLoading = !!webViewObject.loading
        webCurrentUrl = webViewObject.url ? webViewObject.url.toString() : ""
        if (webAddressField && !webAddressField.activeFocus)
            webAddressField.text = webCurrentUrl.length > 0 ? webCurrentUrl : filePath
    }

    function webGoBack() {
        if (webViewObject && webCanGoBack)
            webViewObject.goBack()
    }

    function webGoForward() {
        if (webViewObject && webCanGoForward)
            webViewObject.goForward()
    }

    function webReload() {
        if (webViewObject)
            webViewObject.reload()
    }

    function normalizeWebUrl(value) {
        const text = (value || "").trim()
        if (!text)
            return ""
        if (text.toLowerCase().indexOf("http://") === 0 || text.toLowerCase().indexOf("https://") === 0)
            return text
        return "https://" + text
    }

    function navigateWebUrl(value) {
        const target = normalizeWebUrl(value)
        if (!target || !webViewObject)
            return
        webViewObject.url = target
        webAddressField.text = target
    }

    onIsWebTargetChanged: ensureWebView()
    onFilePathChanged: ensureWebView()
    onContentChanged: {
        if (codeText && codeText.text !== content)
            codeText.text = content
        if (highlightLayer)
            highlightLayer.text = highlightedHtml()
    }
    Component.onCompleted: ensureWebView()

    Timer {
        interval: 150
        repeat: true
        running: editor.isWebTarget && editor.webViewObject !== null
        onTriggered: editor.syncWebState()
    }

    function escapeHtml(value) {
        return value.replace(/&/g, "&amp;")
                    .replace(/</g, "&lt;")
                    .replace(/>/g, "&gt;")
                    .replace(/"/g, "&quot;")
                    .replace(/'/g, "&#39;")
    }

    function styled(value, color) {
        return "<span style=\"color:" + color + "\">" + value + "</span>"
    }

    function isIdentStart(ch) {
        return /[A-Za-z_]/.test(ch)
    }

    function isIdentPart(ch) {
        return /[A-Za-z0-9_]/.test(ch)
    }

    function highlightedHtml() {
        if (content.length === 0)
            return styled(qsTr("Open a file from Explorer to view it here."), "#697386")

        const keywordColor = "#c586c0"
        const typeColor = "#4ec9b0"
        const stringColor = "#ce9178"
        const numberColor = "#b5cea8"
        const commentColor = "#6a9955"
        const opColor = "#d4d4d4"

        const keywords = {
            "alignas": true, "alignof": true, "and": true, "auto": true, "bool": true,
            "break": true, "case": true, "catch": true, "class": true, "const": true,
            "constexpr": true, "continue": true, "default": true, "delete": true,
            "do": true, "else": true, "enum": true, "explicit": true, "export": true,
            "false": true, "for": true, "function": true, "if": true, "import": true,
            "in": true, "inline": true, "let": true, "namespace": true, "new": true,
            "nullptr": true, "private": true, "property": true, "protected": true,
            "public": true, "readonly": true, "required": true, "return": true,
            "signal": true, "slots": true, "static": true, "struct": true, "switch": true,
            "this": true, "true": true, "try": true, "typedef": true, "typename": true,
            "using": true, "var": true, "virtual": true, "void": true, "while": true
        }
        const types = {
            "Action": true, "Column": true, "ColumnLayout": true, "Component": true,
            "Flickable": true, "Item": true, "Layout": true, "Loader": true,
            "MouseArea": true, "QByteArray": true, "QFile": true, "QFileInfo": true,
            "QList": true, "QObject": true, "QProcess": true, "QString": true,
            "QStringList": true, "QVariantList": true, "QVariantMap": true,
            "Rectangle": true, "Repeater": true, "Row": true, "RowLayout": true,
            "Text": true, "TextArea": true, "TextEdit": true
        }

        const lines = content.split("\n")
        let inBlockComment = false
        const rendered = []

        for (const line of lines) {
            let i = 0
            let out = ""

            while (i < line.length) {
                const ch = line[i]
                const next = i + 1 < line.length ? line[i + 1] : ""

                if (inBlockComment) {
                    const end = line.indexOf("*/", i)
                    if (end === -1) {
                        out += styled(escapeHtml(line.slice(i)), commentColor)
                        i = line.length
                    } else {
                        out += styled(escapeHtml(line.slice(i, end + 2)), commentColor)
                        i = end + 2
                        inBlockComment = false
                    }
                    continue
                }

                if (ch === "/" && next === "/") {
                    out += styled(escapeHtml(line.slice(i)), commentColor)
                    break
                }

                if (ch === "/" && next === "*") {
                    const end = line.indexOf("*/", i + 2)
                    if (end === -1) {
                        out += styled(escapeHtml(line.slice(i)), commentColor)
                        inBlockComment = true
                        break
                    }
                    out += styled(escapeHtml(line.slice(i, end + 2)), commentColor)
                    i = end + 2
                    continue
                }

                if (ch === "\"" || ch === "'") {
                    const quote = ch
                    let j = i + 1
                    while (j < line.length) {
                        if (line[j] === "\\" && j + 1 < line.length) {
                            j += 2
                            continue
                        }
                        if (line[j] === quote) {
                            ++j
                            break
                        }
                        ++j
                    }
                    out += styled(escapeHtml(line.slice(i, j)), stringColor)
                    i = j
                    continue
                }

                if (/[0-9]/.test(ch)) {
                    let j = i + 1
                    while (j < line.length && /[A-Fa-f0-9._xXuUlL]/.test(line[j]))
                        ++j
                    out += styled(escapeHtml(line.slice(i, j)), numberColor)
                    i = j
                    continue
                }

                if (isIdentStart(ch)) {
                    let j = i + 1
                    while (j < line.length && isIdentPart(line[j]))
                        ++j
                    const word = line.slice(i, j)
                    out += keywords[word] ? styled(word, keywordColor)
                         : types[word] ? styled(word, typeColor)
                         : escapeHtml(word)
                    i = j
                    continue
                }

                if ("{}[]();:,.+-*/=%!<>|&?".indexOf(ch) !== -1)
                    out += styled(escapeHtml(ch), opColor)
                else if (ch === " ")
                    out += "&nbsp;"
                else if (ch === "\t")
                    out += "&nbsp;&nbsp;&nbsp;&nbsp;"
                else
                    out += escapeHtml(ch)
                ++i
            }

            rendered.push(out.length > 0 ? out : "&nbsp;")
        }

           return "<span style=\"color:" + editor.textColor + "\">"
               + rendered.join("<br>")
               + "</span>"
    }

    function lineNumbers() {
        const nums = []
        for (let i = 1; i <= lineCount; ++i)
            nums.push(i.toString())
        return nums.join("\n")
    }

    Flickable {
        id: scroll
        anchors.fill: parent
        visible: !editor.isWebTarget
        clip: true
        contentWidth: Math.max(width, codeText.contentWidth + editor.gutterWidth + 32)
        contentHeight: Math.max(height, codeText.contentHeight + 24)
        boundsBehavior: Flickable.StopAtBounds

        Rectangle {
            x: 0
            y: scroll.contentY
            width: editor.gutterWidth
            height: scroll.height
            color: editor.gutterColor
            z: 2
        }

        Text {
            id: numbers
            x: 0
            y: 12
            width: editor.gutterWidth - 10
            text: editor.lineNumbers()
            color: editor.lineNumberColor
            horizontalAlignment: Text.AlignRight
            font { family: "Menlo"; pixelSize: editor.fontSize }
            lineHeight: editor.lineHeight
            lineHeightMode: Text.FixedHeight
            z: 3
        }

        Rectangle {
            x: editor.gutterWidth
            y: 0
            width: 1
            height: Math.max(scroll.height, scroll.contentHeight)
            color: editor.borderColor
            z: 2
        }

        // Highlighted display layer (below)
        TextEdit {
            id: highlightLayer
            x: editor.gutterWidth + 14
            y: 12
            text: editor.highlightedHtml()
            textFormat: TextEdit.RichText
            readOnly: true
            selectByMouse: false
            wrapMode: TextEdit.NoWrap
            cursorVisible: false
            font { family: "Menlo"; pixelSize: editor.fontSize }
            renderType: Text.NativeRendering
            z: 1
        }

        // Editable layer (above, transparent)
        TextEdit {
            id: codeText
            x: editor.gutterWidth + 14
            y: 12
            text: ""
            textFormat: TextEdit.PlainText
            readOnly: false
            selectByMouse: true
            persistentSelection: true
            wrapMode: TextEdit.NoWrap
            cursorVisible: true
            color: "transparent"
            selectionColor: "#4a5c7a"
            font { family: "Menlo"; pixelSize: editor.fontSize }
            renderType: Text.NativeRendering
            z: 2
            
            onTextChanged: {
                editor.content = text
            }
        }

        MouseArea {
            anchors.fill: codeText
            acceptedButtons: Qt.RightButton
            hoverEnabled: true
            cursorShape: Qt.IBeamCursor

            onPressed: function(mouse) {
                editorContextMenu.x = mouse.x + codeText.x
                editorContextMenu.y = mouse.y + codeText.y
                editorContextMenu.open()
            }
        }

        // Sync highlight layer when content changes
        Connections {
            target: codeText
            function onTextChanged() {
                highlightLayer.text = editor.highlightedHtml()
            }
        }

        Menu {
            id: editorContextMenu

            MenuItem {
                text: qsTr("Cut")
                enabled: codeText.selectedText.length > 0
                onTriggered: codeText.cut()
            }

            MenuItem {
                text: qsTr("Copy")
                enabled: codeText.selectedText.length > 0
                onTriggered: codeText.copy()
            }

            MenuItem {
                text: qsTr("Paste")
                onTriggered: codeText.paste()
            }

            MenuSeparator {}

            MenuItem {
                text: qsTr("Copy Path")
                enabled: editor.filePath.length > 0
                onTriggered: Runtime.copyToClipboard(editor.filePath)
            }

            MenuItem {
                text: qsTr("Select All")
                onTriggered: codeText.selectAll()
            }

            MenuItem {
                text: qsTr("Reveal in Finder")
                enabled: editor.filePath.length > 0
                onTriggered: Runtime.revealInFinder(editor.filePath)
            }
        }

        Shortcut {
            sequences: [ StandardKey.Copy ]
            enabled: codeText.activeFocus
            onActivated: codeText.copy()
        }

        Shortcut {
            sequences: [ StandardKey.SelectAll ]
            enabled: codeText.activeFocus
            onActivated: codeText.selectAll()
        }
    }

    Item {
        id: webContainer
        anchors.fill: parent
        visible: editor.isWebTarget

        Rectangle {
            id: webToolbar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 34
            color: "#10151c"
            border.color: editor.borderColor

            Row {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 8

                Rectangle {
                    width: 24
                    height: 24
                    radius: 4
                    anchors.verticalCenter: parent.verticalCenter
                    color: editor.webCanGoBack ? "#1f2937" : "#151a22"
                    border.color: editor.borderColor
                    Text { anchors.centerIn: parent; text: "<"; color: editor.webCanGoBack ? "#d1d5db" : "#6b7280"; font.pixelSize: 12 }
                    MouseArea { anchors.fill: parent; enabled: editor.webCanGoBack; onClicked: editor.webGoBack() }
                }

                Rectangle {
                    width: 24
                    height: 24
                    radius: 4
                    anchors.verticalCenter: parent.verticalCenter
                    color: editor.webCanGoForward ? "#1f2937" : "#151a22"
                    border.color: editor.borderColor
                    Text { anchors.centerIn: parent; text: ">"; color: editor.webCanGoForward ? "#d1d5db" : "#6b7280"; font.pixelSize: 12 }
                    MouseArea { anchors.fill: parent; enabled: editor.webCanGoForward; onClicked: editor.webGoForward() }
                }

                Rectangle {
                    width: 26
                    height: 24
                    radius: 4
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#1f2937"
                    border.color: editor.borderColor
                    Text { anchors.centerIn: parent; text: editor.webLoading ? "..." : "R"; color: "#d1d5db"; font.pixelSize: 12 }
                    MouseArea { anchors.fill: parent; onClicked: editor.webReload() }
                }

                Rectangle {
                    width: 26
                    height: 24
                    radius: 4
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#1f2937"
                    border.color: editor.borderColor
                    Text { anchors.centerIn: parent; text: "↗"; color: "#d1d5db"; font.pixelSize: 12 }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: Qt.openUrlExternally(editor.webCurrentUrl.length > 0 ? editor.webCurrentUrl : editor.filePath)
                    }
                }

                TextField {
                    id: webAddressField
                    width: webToolbar.width - 220
                    height: 24
                    anchors.verticalCenter: parent.verticalCenter
                    text: editor.webCurrentUrl.length > 0 ? editor.webCurrentUrl : editor.filePath
                    color: "#d1d5db"
                    placeholderText: qsTr("Enter URL")
                    placeholderTextColor: "#6f7b8a"
                    selectedTextColor: "#ffffff"
                    selectionColor: "#334155"
                    font.pixelSize: 11
                    leftPadding: 8
                    rightPadding: 8
                    background: Rectangle {
                        radius: 4
                        color: "#0f131a"
                        border.color: webAddressField.activeFocus ? "#4a5c7a" : editor.borderColor
                        border.width: 1
                    }

                    onActiveFocusChanged: {
                        editor.webAddressEditing = activeFocus
                        if (!activeFocus)
                            text = editor.webCurrentUrl.length > 0 ? editor.webCurrentUrl : editor.filePath
                    }

                    onAccepted: editor.navigateWebUrl(text)
                }

                Rectangle {
                    width: 28
                    height: 24
                    radius: 4
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#1f2937"
                    border.color: editor.borderColor
                    Text { anchors.centerIn: parent; text: "→"; color: "#d1d5db"; font.pixelSize: 12 }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: editor.navigateWebUrl(webAddressField.text)
                    }
                }
            }
        }

        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: webToolbar.bottom
            anchors.bottom: parent.bottom
            clip: true
        }

        Text {
            anchors.centerIn: parent
            visible: editor.isWebTarget && editor.webViewError.length > 0
            text: editor.webViewError
            color: "#f59e0b"
            font.pixelSize: 13
        }
    }
}
