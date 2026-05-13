pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

Rectangle {
    id: editor

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

        TextEdit {
            id: codeText
            x: editor.gutterWidth + 14
            y: 12
            text: editor.highlightedHtml()
            textFormat: TextEdit.RichText
            readOnly: true
            selectByMouse: true
            persistentSelection: true
            wrapMode: TextEdit.NoWrap
            cursorVisible: false
            font { family: "Menlo"; pixelSize: editor.fontSize }
            renderType: Text.NativeRendering
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

        Menu {
            id: editorContextMenu

            MenuItem {
                text: qsTr("Copy")
                enabled: codeText.selectedText.length > 0
                onTriggered: codeText.copy()
            }

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
            sequence: StandardKey.Copy
            enabled: codeText.activeFocus
            onActivated: codeText.copy()
        }

        Shortcut {
            sequence: StandardKey.SelectAll
            enabled: codeText.activeFocus
            onActivated: codeText.selectAll()
        }
    }
}
