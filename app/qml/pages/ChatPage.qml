pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import "../components"

Item {
    id: page
    required property var palette

    property string selectedModelId: Runtime.chatModel.length > 0 ? Runtime.chatModel : "qwen2:0.5b"
    property string selectedModelLabel: Runtime.chatModel.length > 0 ? Runtime.chatModel : "qwen2:0.5b"
    property string selectedEffort: "Medium"
    property string selectedPermissions: qsTr("Default permissions")
    property bool ideContextEnabled: true
    property bool workLocally: true
    property bool isWaiting: false
    property int  pendingMsgIdx: -1
    property string pendingContent: ""
    property string lastUserPrompt: ""
    property bool approvalDialogOpen: false
    property string approvalTool: ""
    property string approvalReason: ""
    property string approvalArgsText: ""
    property string approvalPath: ""
    property string approvalPatch: ""
    property string approvalSummary: ""
    property string approvalCommand: ""
    property string approvalCommandReason: ""
    property string currentFilePath: ""
    property string currentFileContent: ""
    property bool patchShowContext: true
    property bool patchCollapseHunks: false
    property string patchSelectedFile: qsTr("(all files)")
    property var patchCollapsedByKey: ({})
    property string patchSearchText: ""
    property int patchCurrentHunkIndex: 0
    property var patchSearchResults: []
    property var patchSelectedHunkKeys: ({})
    property string currentToolName: ""
    property int currentToolIteration: 0
    property int maxToolIterations: 8
    property bool currentToolRunning: false
    property var lastOperationSummary: ({})
    property bool showOperationSummary: false
    property var mentionedFiles: []       // [{name, path, rel}]
    property bool mentionPickerOpen: false
    property string mentionQuery: ""
    property int mentionPickerIndex: 0
    property bool slashPickerOpen: false
    property string slashQuery: ""
    property int slashPickerIndex: 0
    property int waitingHintIndex: 0
    property int waitingDotPhase: 0
    property bool streamCursorOn: true
    property int streamElapsedSec: 0
    readonly property var waitingHints: [
        qsTr("Considering"),
        qsTr("Analyzing"),
        qsTr("Planning")
    ]

    // Slash command definitions
    readonly property var slashCommands: [
        { name: "fix",      icon: "🔧", desc: qsTr("Fix bugs and errors in the current file"),
          prompt: "Fix all bugs and errors in the current file. Explain what you changed and why." },
        { name: "explain",  icon: "💡", desc: qsTr("Explain what the current file does"),
          prompt: "Explain what this file does: its purpose, key functions, and how it fits in the project." },
        { name: "test",     icon: "🧪", desc: qsTr("Generate unit tests for the current file"),
          prompt: "Generate comprehensive unit tests for this file. Cover edge cases and typical usage." },
        { name: "doc",      icon: "📝", desc: qsTr("Add documentation comments to the current file"),
          prompt: "Add clear documentation comments to all public functions and classes in this file." },
        { name: "refactor", icon: "♻️",  desc: qsTr("Refactor and improve code quality"),
          prompt: "Refactor this file to improve readability, maintainability, and code quality. Follow best practices." },
        { name: "review",   icon: "🔍", desc: qsTr("Code review: find issues and improvements"),
          prompt: "Review this file for bugs, security issues, performance problems, and style improvements." },
        { name: "optimize", icon: "⚡", desc: qsTr("Optimize for performance"),
          prompt: "Analyze and optimize this file for performance. Identify bottlenecks and suggest improvements." },
        { name: "types",    icon: "🏷️",  desc: qsTr("Add or improve type annotations"),
          prompt: "Add or improve type annotations in this file for better type safety and IDE support." }
    ]

    function filteredSlashCommands() {
        const q = page.slashQuery.toLowerCase()
        const filtered = []
        for (let i = 0; i < page.slashCommands.length; ++i) {
            const command = page.slashCommands[i]
            if (String(command.name || "").toLowerCase().indexOf(q) === 0)
                filtered.push(command)
        }
        return filtered
    }

    function applySlashCommand(cmd) {
        const hasFile = page.currentFilePath && page.currentFilePath.length > 0
        const fileSuffix = hasFile
            ? qsTr(" Operate on: ") + page.currentFilePath
            : ""
        msgInput.text = ""
        page.slashPickerOpen = false
        page.slashQuery = ""
        page.ideContextEnabled = true
        page.beginAssistantTurn(cmd.prompt + fileSuffix, true)
    }

    function insertMentionChip(fileObj) {
        // Insert @filename token into the text input, replacing the partial @query
        const txt = msgInput.text
        const pos = msgInput.cursorPosition
        // Find the start of the @query
        let atPos = pos - 1
        while (atPos > 0 && txt[atPos - 1] !== ' ' && txt[atPos - 1] !== '\n')
            atPos--
        if (atPos < 0 || txt[atPos] !== '@')
            atPos = pos
        const before = txt.substring(0, atPos)
        const after  = txt.substring(pos)
        const chip   = "@" + fileObj.name + " "
        msgInput.text = before + chip + after
        msgInput.cursorPosition = before.length + chip.length

        // Track as mentioned
        let already = false
        for (let i = 0; i < page.mentionedFiles.length; ++i) {
            if (page.mentionedFiles[i].path === fileObj.path) {
                already = true
                break
            }
        }
        if (!already) {
            const arr = page.mentionedFiles.slice()
            arr.push(fileObj)
            page.mentionedFiles = arr
        }
        page.mentionPickerOpen = false
        page.mentionQuery = ""
        msgInput.forceActiveFocus()
    }

    function resolveMentionContext() {
        // Read content of all mentioned files and return as context string
        let ctx = ""
        for (let i = 0; i < page.mentionedFiles.length; ++i) {
            const f = page.mentionedFiles[i]
            const content = Runtime.readLocalFile(f.path)
            ctx += "\n\n--- @" + f.rel + " ---\n" + content
        }
        return ctx
    }

    function beginAssistantTurn(userText, addUserBubble) {
        let text = (userText || "").trim()
        if (!text || page.isWaiting)
            return

        // Inject @mention file contents into the message
        if (page.mentionedFiles.length > 0) {
            text = text + "\n\n[Attached files for context:" + resolveMentionContext() + "\n]"
            page.mentionedFiles = []
        }

        if (addUserBubble) {
            chatHistory.append({
                role: "user",
                content: userText.trim(),  // show original text without injected content
                time: Qt.formatTime(new Date(), "hh:mm")
            })
        }

        chatHistory.append({
            role: "assistant",
            content: page.currentWaitingHint(),
            time: ""
        })
        page.pendingMsgIdx = chatHistory.count - 1
        page.pendingContent = ""
        page.streamCursorOn = true
        page.streamElapsedSec = 0
        page.waitingHintIndex = 0
        page.waitingDotPhase = 0
        page.isWaiting = true
        page.lastUserPrompt = text

        Runtime.sendChatMessage(
            page.selectedModelId,
            text,
            page.currentFilePath,
            page.currentFileContent,
            page.selectedEffort,
            page.selectedPermissions,
            page.ideContextEnabled,
            page.workLocally)
    }

    function currentWaitingHint() {
        const dots = page.waitingDotPhase === 0 ? "." : (page.waitingDotPhase === 1 ? ".." : "...")
        if (page.currentToolRunning && page.currentToolName.length > 0)
            return qsTr("Running") + ": " + page.currentToolName.replace("codex_", "") + dots
        if (page.pendingContent.length > 0)
            return qsTr("Generating") + dots
        return page.waitingHints[page.waitingHintIndex] + dots
    }

    function refreshWaitingHint() {
        if (!page.isWaiting || page.pendingMsgIdx < 0 || page.pendingContent.length > 0)
            return
        chatHistory.setProperty(page.pendingMsgIdx, "content", page.currentWaitingHint())
    }

    function renderStreamingContent() {
        if (page.pendingContent.length === 0)
            return page.currentWaitingHint()
        return page.pendingContent + (page.streamCursorOn ? "|" : "")
    }

    function analyzeCurrentEditorFile() {
        if (page.isWaiting)
            return

        if (!page.currentFilePath || page.currentFilePath.length === 0) {
            page.appendAssistantNotice(qsTr("No file is currently open in the editor."))
            return
        }

        page.ideContextEnabled = true
        const prompt = qsTr("Analyze the file currently open in the editor: ")
            + page.currentFilePath
            + qsTr("\n\nFocus on structure, key responsibilities, potential bugs, maintainability issues, and concrete improvement suggestions. Do not modify files unless I explicitly ask.")
        page.beginAssistantTurn(prompt, true)
    }

    function parseApprovalMarker(content) {
        const text = String(content || "")
        const lines = text.split("\n")
        if (lines.length === 0)
            return null

        const first = lines[0].trim()
        if (first.indexOf("[approval_required_json]") === 0) {
            const jsonText = first.substring("[approval_required_json]".length).trim()
            try {
                const payload = JSON.parse(jsonText)
                const args = payload.args || {}
                return {
                    tool: payload.tool || "",
                    reason: payload.reason || "",
                    argsText: JSON.stringify(args, null, 2),
                    path: args.path || "",
                    patch: args.patch || "",
                    summary: args.summary || "",
                    command: args.command || "",
                    commandReason: args.reason || "",
                    content: lines.slice(1).join("\n").trim()
                }
            } catch (error) {
                return {
                    tool: "",
                    reason: qsTr("Approval required, but details could not be parsed."),
                    argsText: "",
                    path: "",
                    patch: "",
                    summary: "",
                    command: "",
                    commandReason: "",
                    content: lines.slice(1).join("\n").trim()
                }
            }
        }

        if (first.indexOf("[approval_required]") !== 0)
            return null

        const marker = /^\[approval_required\]\s+tool=(\S+)\s+reason=(.*)$/
        const match = first.match(marker)
        if (!match)
            return null

        return {
            tool: match[1],
            reason: match[2],
            argsText: "",
            path: "",
            patch: "",
            summary: "",
            command: "",
            commandReason: "",
            content: lines.slice(1).join("\n").trim()
        }
    }

    function resetApproval() {
        page.approvalDialogOpen = false
        page.approvalTool = ""
        page.approvalReason = ""
        page.approvalArgsText = ""
        page.approvalPath = ""
        page.approvalPatch = ""
        page.approvalSummary = ""
        page.approvalCommand = ""
        page.approvalCommandReason = ""
        page.patchSearchText = ""
        page.patchCurrentHunkIndex = 0
        page.patchSelectedHunkKeys = ({})
    }

    function appendAssistantNotice(text) {
        chatHistory.append({
            role: "assistant",
            content: text,
            time: Qt.formatTime(new Date(), "hh:mm")
        })
        page.scrollToLatest()
    }

    function approveCurrentPatch() {
        if (page.approvalTool !== "codex_apply_patch" || page.approvalPatch.length === 0) {
            page.appendAssistantNotice(qsTr("No patch is available to apply."))
            page.resetApproval()
            return
        }

        const patchToApply = page.buildFilteredPatch()
        if (patchToApply.length === 0) {
            page.appendAssistantNotice(qsTr("No hunks selected to apply."))
            page.resetApproval()
            return
        }

        const stats = page.getHunkSelectionStats()
        if (stats.selected < stats.total) {
            page.appendAssistantNotice(qsTr("Applying ") + stats.selected + qsTr(" of ") + stats.total + qsTr(" hunks..."))
        }

        const result = Runtime.applyApprovedPatch(page.approvalPath, patchToApply)
        if (result && result.ok) {
            page.appendAssistantNotice(qsTr("Patch applied: ") + (result.path || page.approvalPath))
        } else {
            const message = result && result.error ? result.error : qsTr("Patch failed.")
            page.appendAssistantNotice(qsTr("Patch failed: ") + message)
        }
        page.resetApproval()
    }

    function formatCommandResult(command, result) {
        let text = qsTr("Command finished") + ": `" + command + "`\n"
        if (result && result.exitCode !== undefined)
            text += qsTr("Exit code") + ": " + result.exitCode + "\n"
        if (result && result.stdout && result.stdout.length > 0)
            text += "\nstdout:\n```text\n" + result.stdout.trim() + "\n```\n"
        if (result && result.stderr && result.stderr.length > 0)
            text += "\nstderr:\n```text\n" + result.stderr.trim() + "\n```\n"
        if (result && result.error && result.error.length > 0)
            text += "\n" + qsTr("Error") + ": " + result.error
        return text.trim()
    }

    function patchStats(patchText) {
        const patch = String(patchText || "")
        const lines = patch.split("\n")
        let adds = 0
        let dels = 0
        const files = []

        for (let i = 0; i < lines.length; ++i) {
            const line = lines[i]
            if (line.indexOf("diff --git ") === 0) {
                const parts = line.split(" ")
                if (parts.length >= 4) {
                    let path = parts[3]
                    if (path.indexOf("b/") === 0)
                        path = path.substring(2)
                    if (files.indexOf(path) === -1)
                        files.push(path)
                }
            } else if (line.indexOf("+") === 0 && line.indexOf("+++") !== 0) {
                ++adds
            } else if (line.indexOf("-") === 0 && line.indexOf("---") !== 0) {
                ++dels
            }
        }

        return {
            additions: adds,
            deletions: dels,
            fileCount: files.length,
            filesText: files.join(", ")
        }
    }

    function patchPreviewByPrefix(patchText, prefix, limit) {
        const patch = String(patchText || "")
        const lines = patch.split("\n")
        const out = []
        const maxLines = limit > 0 ? limit : 80

        for (let i = 0; i < lines.length; ++i) {
            const line = lines[i]
            if (prefix === "+") {
                if (line.indexOf("+") === 0 && line.indexOf("+++") !== 0)
                    out.push(line)
            } else if (prefix === "-") {
                if (line.indexOf("-") === 0 && line.indexOf("---") !== 0)
                    out.push(line)
            }

            if (out.length >= maxLines)
                break
        }

        return out.length > 0 ? out.join("\n") : qsTr("(none)")
    }

    function patchFileOptions(patchText) {
        const patch = String(patchText || "")
        const lines = patch.split("\n")
        const files = [qsTr("(all files)")]

        for (let i = 0; i < lines.length; ++i) {
            const line = lines[i]
            if (line.indexOf("diff --git ") !== 0)
                continue

            const parts = line.split(" ")
            if (parts.length < 4)
                continue

            let path = parts[3]
            if (path.indexOf("b/") === 0)
                path = path.substring(2)
            if (files.indexOf(path) === -1)
                files.push(path)
        }
        return files
    }

    function cyclePatchFileFilter() {
        const options = page.patchFileOptions(page.approvalPatch)
        if (options.length === 0)
            return
        let idx = options.indexOf(page.patchSelectedFile)
        if (idx < 0)
            idx = 0
        idx = (idx + 1) % options.length
        page.patchSelectedFile = options[idx]
    }

    function patchHunksSearchFiltered(hunks) {
        if (!page.patchSearchText || page.patchSearchText.length === 0)
            return hunks

        const query = page.patchSearchText.toLowerCase()
        const filtered = []
        for (let i = 0; i < hunks.length; ++i) {
            const hunk = hunks[i]
            const searchable = (hunk.file || "") + " " + (hunk.header || "") + " " + (hunk.before || "") + " " + (hunk.after || "")
            if (searchable.toLowerCase().indexOf(query) >= 0)
                filtered.push(hunk)
        }
        return filtered
    }

    function cyclePatchHunkSelection(direction) {
        const allHunks = page.patchHunks(page.approvalPatch,
                                          5,
                                          36,
                                          page.patchShowContext,
                                          page.patchCollapseHunks,
                                          10,
                                          page.patchSelectedFile)
        const filtered = page.patchHunksSearchFiltered(allHunks)
        if (filtered.length === 0)
            return

        const nextIdx = (page.patchCurrentHunkIndex + direction) % filtered.length
        page.patchCurrentHunkIndex = Math.max(0, nextIdx)
    }

    function getSelectedHunk() {
        const allHunks = page.patchHunks(page.approvalPatch,
                                          5,
                                          36,
                                          page.patchShowContext,
                                          page.patchCollapseHunks,
                                          10,
                                          page.patchSelectedFile)
        const filtered = page.patchHunksSearchFiltered(allHunks)
        if (page.patchCurrentHunkIndex >= 0 && page.patchCurrentHunkIndex < filtered.length)
            return filtered[page.patchCurrentHunkIndex]
        return null
    }

    function toggleCurrentHunkCollapsed() {
        const hunk = page.getSelectedHunk()
        if (hunk)
            page.toggleHunkCollapsed(hunk.key)
    }

    function copyCurrentHunk() {
        const hunk = page.getSelectedHunk()
        if (hunk) {
            const text = "--- " + (hunk.file || "file") + "\n"
                       + hunk.header + "\n"
                       + hunk.before + "\n---\n"
                       + hunk.after
            Qt.callLater(function() { Qt.clipboard.text = text })
            page.appendAssistantNotice("Hunk copied to clipboard")
        }
    }

    function isHunkSelected(hunkKey) {
        // By default, hunks are selected (included) if not explicitly deselected
        return page.patchSelectedHunkKeys[hunkKey] !== false
    }

    function toggleHunkSelected(hunkKey) {
        const next = ({})
        for (const key in page.patchSelectedHunkKeys)
            next[key] = page.patchSelectedHunkKeys[key]
        next[hunkKey] = !next[hunkKey]
        page.patchSelectedHunkKeys = next
    }

    function toggleCurrentHunkSelected() {
        const hunk = page.getSelectedHunk()
        if (hunk)
            page.toggleHunkSelected(hunk.key)
    }

    function selectAllHunks() {
        page.patchSelectedHunkKeys = ({})
    }

    function deselectAllHunks() {
        const allHunks = page.patchHunks(page.approvalPatch,
                                          5,
                                          36,
                                          page.patchShowContext,
                                          page.patchCollapseHunks,
                                          10,
                                          page.patchSelectedFile)
        const next = ({})
        for (let i = 0; i < allHunks.length; ++i)
            next[allHunks[i].key] = false
        page.patchSelectedHunkKeys = next
    }

    function getHunkSelectionStats() {
        const allHunks = page.patchHunks(page.approvalPatch,
                                          5,
                                          36,
                                          page.patchShowContext,
                                          page.patchCollapseHunks,
                                          10,
                                          page.patchSelectedFile)
        let selected = 0
        for (let i = 0; i < allHunks.length; ++i) {
            if (page.isHunkSelected(allHunks[i].key))
                ++selected
        }
        return {
            selected: selected,
            total: allHunks.length
        }
    }

    function buildFilteredPatch() {
        const patch = String(page.approvalPatch || "")
        const lines = patch.split("\n")
        let selectedCount = 0
        for (const key in page.patchSelectedHunkKeys) {
            if (page.patchSelectedHunkKeys[key])
                ++selectedCount
        }

        // If no hunks explicitly selected, include all
        if (selectedCount === 0) {
            for (const key in page.patchSelectedHunkKeys) {
                if (page.patchSelectedHunkKeys[key] !== false)
                    ++selectedCount
            }
        }

        if (selectedCount === 0)
            return page.approvalPatch

        // Rebuild patch with only selected hunks
        const allHunks = page.patchHunks(page.approvalPatch,
                                          999,
                                          36,
                                          true,
                                          false,
                                          10,
                                          "")
        const includedFiles = []
        const includedHunks = []

        for (let i = 0; i < allHunks.length; ++i) {
            const hunk = allHunks[i]
            if (page.isHunkSelected(hunk.key)) {
                if (includedFiles.indexOf(hunk.file) === -1)
                    includedFiles.push(hunk.file)
                includedHunks.push(hunk)
            }
        }

        if (includedHunks.length === 0)
            return ""

        let result = ""
        let currentFile = ""
        let fileCount = 0

        for (let fileIndex = 0; fileIndex < includedFiles.length; ++fileIndex) {
            const file = includedFiles[fileIndex]
            fileCount++
            result += "diff --git a/" + file + " b/" + file + "\n"
            result += "index 0000000..0000000 100644\n"
            result += "--- a/" + file + "\n"
            result += "+++ b/" + file + "\n"

            // Add hunks for this file
            let firstHunk = true
            for (let i = 0; i < includedHunks.length; ++i) {
                if (includedHunks[i].file === file) {
                    if (!firstHunk)
                        result += "\n"
                    result += includedHunks[i].header + "\n"
                    result += includedHunks[i].before + "\n"
                    result += includedHunks[i].after + "\n"
                    firstHunk = false
                }
            }

            if (fileCount < includedFiles.length)
                result += "\n"
        }

        return result
    }

    function isHunkCollapsed(hunkKey) {
        return page.patchCollapsedByKey[hunkKey] === true
    }

    function toggleHunkCollapsed(hunkKey) {
        const next = ({})
        for (const key in page.patchCollapsedByKey)
            next[key] = page.patchCollapsedByKey[key]
        next[hunkKey] = !next[hunkKey]
        page.patchCollapsedByKey = next
    }

    function patchHunks(patchText, maxHunks, maxLinesPerSide, includeContext, collapse, collapsedLines, selectedFile) {
        const patch = String(patchText || "")
        const lines = patch.split("\n")
        const hunks = []
        const hunkLimit = maxHunks > 0 ? maxHunks : 4
        const fullLineLimit = maxLinesPerSide > 0 ? maxLinesPerSide : 40
        const compactLineLimit = collapsedLines > 0 ? collapsedLines : 10
        const lineLimit = collapse ? Math.min(fullLineLimit, compactLineLimit) : fullLineLimit
        const filterFile = selectedFile && selectedFile !== qsTr("(all files)") ? selectedFile : ""
        let currentFile = ""
        let current = null
        let hunkCounter = 0

        function pushCurrent() {
            if (!current)
                return
            const beforeLines = includeContext ? current.beforeAll : current.beforeChanged
            const afterLines = includeContext ? current.afterAll : current.afterChanged
            const beforeText = beforeLines.length > 0 ? beforeLines.join("\n") : qsTr("(none)")
            const afterText = afterLines.length > 0 ? afterLines.join("\n") : qsTr("(none)")

            if (filterFile.length > 0 && current.file !== filterFile) {
                current = null
                return
            }

            ++hunkCounter
            hunks.push({
                key: (current.file || "") + "|" + current.header + "|" + hunkCounter,
                file: current.file,
                header: current.header,
                additions: current.additions,
                deletions: current.deletions,
                before: beforeText,
                after: afterText
            })
            current = null
        }

        for (let i = 0; i < lines.length && hunks.length < hunkLimit; ++i) {
            const line = lines[i]
            if (line.indexOf("diff --git ") === 0) {
                pushCurrent()
                const parts = line.split(" ")
                if (parts.length >= 4) {
                    currentFile = parts[3]
                    if (currentFile.indexOf("b/") === 0)
                        currentFile = currentFile.substring(2)
                }
                continue
            }

            if (line.indexOf("@@") === 0) {
                pushCurrent()
                current = {
                    file: currentFile,
                    header: line,
                    beforeAll: [],
                    afterAll: [],
                    beforeChanged: [],
                    afterChanged: [],
                    additions: 0,
                    deletions: 0
                }
                continue
            }

            if (!current)
                continue

            if (line.indexOf("+") === 0 && line.indexOf("+++") !== 0) {
                const added = line.substring(1)
                ++current.additions
                if (current.afterAll.length < lineLimit)
                    current.afterAll.push(added)
                if (current.afterChanged.length < lineLimit)
                    current.afterChanged.push(added)
            } else if (line.indexOf("-") === 0 && line.indexOf("---") !== 0) {
                const removed = line.substring(1)
                ++current.deletions
                if (current.beforeAll.length < lineLimit)
                    current.beforeAll.push(removed)
                if (current.beforeChanged.length < lineLimit)
                    current.beforeChanged.push(removed)
            } else if (line.indexOf(" ") === 0) {
                const contextLine = line.substring(1)
                if (current.beforeAll.length < lineLimit)
                    current.beforeAll.push(contextLine)
                if (current.afterAll.length < lineLimit)
                    current.afterAll.push(contextLine)
            }
        }

        pushCurrent()
        return hunks
    }

    function approveCurrentCommand() {
        if (page.approvalTool !== "codex_run_command" || page.approvalCommand.length === 0) {
            page.appendAssistantNotice(qsTr("No command is available to run."))
            page.resetApproval()
            return
        }

        const command = page.approvalCommand
        const result = Runtime.runApprovedCommand(command, page.approvalCommandReason)
        page.appendAssistantNotice(page.formatCommandResult(command, result))
        page.resetApproval()
    }

    function openMenuNear(anchor, menu) {
        const pos = anchor.mapToItem(page, 0, 0)
        const desiredHeight = menu.popupHeight || menu.implicitHeight || 180
        menu.x = Math.max(8, Math.min(page.width - menu.width - 8, pos.x))
        menu.y = Math.max(8, pos.y - desiredHeight - 8)
        menu.open()
    }

    function hasModel(modelId) {
        for (let i = 0; i < availableModels.count; ++i) {
            if (availableModels.get(i).idValue === modelId)
                return true
        }
        return false
    }

    function pruneRuntimeModels() {
        for (let i = availableModels.count - 1; i >= 0; --i) {
            if (availableModels.get(i).provider === "Ollama")
                availableModels.remove(i)
        }
    }

    function syncRuntimeModels() {
        if (!Runtime || !Runtime.localModels)
            return

        pruneRuntimeModels()
        for (let i = 0; i < Runtime.localModels.length; ++i) {
            const model = Runtime.localModels[i]
            if (!hasModel(model.idValue))
                availableModels.append(model)
        }
    }

    function scrollToLatest() {
        Qt.callLater(function() {
            if (chatHistory.count > 0)
                msgList.positionViewAtEnd()
        })
    }

    Component.onCompleted: {
        syncRuntimeModels()
        if (Runtime && Runtime.refreshLocalModels)
            Runtime.refreshLocalModels()
    }

    ListModel { id: chatHistory }
    ListModel { id: mentionFilesModel }

    // Reload mention suggestions when query changes
    onMentionQueryChanged: {
        mentionFilesModel.clear()
        const files = Runtime.listWorkspaceFiles(page.mentionQuery)
        for (let i = 0; i < Math.min(files.length, 12); ++i)
            mentionFilesModel.append(files[i])
        page.mentionPickerIndex = 0
    }

    ListModel {
        id: availableModels
        ListElement { idValue: "gpt-5.5"; label: "GPT-5.5"; provider: "OpenAI"; detail: "Agentic coding and planning" }
        ListElement { idValue: "gpt-5.4"; label: "GPT-5.4"; provider: "OpenAI"; detail: "Balanced software tasks" }
        ListElement { idValue: "gpt-5.4-mini"; label: "GPT-5.4 Mini"; provider: "OpenAI"; detail: "Fast lightweight work" }
        ListElement { idValue: "o3"; label: "o3"; provider: "OpenAI"; detail: "Deep reasoning" }
        ListElement { idValue: "gpt-4.1"; label: "GPT-4.1"; provider: "OpenAI"; detail: "Compatibility" }
        ListElement { idValue: "claude-sonnet"; label: "Claude Sonnet"; provider: "Anthropic"; detail: "Long form coding" }
        ListElement { idValue: "qwen-max"; label: "Qwen Max"; provider: "Qwen"; detail: "Multilingual tasks" }
    }

    ListModel {
        id: effortOptions
        ListElement { label: "Low"; detail: "Quick" }
        ListElement { label: "Medium"; detail: "Balanced" }
        ListElement { label: "High"; detail: "Thorough" }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: "#111"
            border.color: page.palette.border

            Text {
                anchors.centerIn: parent
                text: qsTr("Codex Agent")
                font { pixelSize: 16; weight: Font.Medium }
                color: page.palette.textPrim
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: page.isWaiting ? 32 : 0
            visible: page.isWaiting
            color: "#1a2a1f"
            border.color: "#2f6b3a"
            clip: true
            Behavior on Layout.preferredHeight { NumberAnimation { duration: 150 } }

            RowLayout {
                anchors { fill: parent; margins: 6 }
                spacing: 8

                Text {
                    text: page.currentToolRunning ? "🔧" : "⏳"
                    font.pixelSize: 12
                }

                Text {
                    Layout.fillWidth: true
                    text: page.currentWaitingHint()
                    color: page.palette.textPrim
                    font { pixelSize: 11; weight: Font.Medium }
                    elide: Text.ElideRight
                }

                Rectangle {
                    width: 120
                    height: 4
                    color: "#2f6b3a"
                    radius: 2

                    Rectangle {
                        width: page.currentToolRunning
                             ? parent.width * (page.currentToolIteration / Math.max(1, page.maxToolIterations))
                             : parent.width * ((page.waitingDotPhase + 1) / 3)
                        height: parent.height
                        color: "#4ade80"
                        radius: 2
                        Behavior on width { NumberAnimation { duration: 200 } }
                    }
                }

                Text {
                    text: page.currentToolRunning
                        ? (page.currentToolIteration + "/" + page.maxToolIterations)
                        : (page.streamElapsedSec + "s")
                    color: page.palette.textSec
                    font.pixelSize: 10
                    Layout.preferredWidth: 30
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: msgList
                anchors.fill: parent
                model: chatHistory
                spacing: 0
                clip: true
                onCountChanged: page.scrollToLatest()
                onContentHeightChanged: {
                    if (page.isWaiting)
                        page.scrollToLatest()
                }

                topMargin: 16
                leftMargin: 16
                rightMargin: 16

                delegate: ChatBubble {
                    width: msgList.width - 32
                }

                footer: Column {
                    width: msgList.width - 32
                    spacing: 8
                    visible: page.showOperationSummary

                    Rectangle {
                        width: parent.width
                        height: summaryContent.implicitHeight + 12
                        color: "#1a2a1f"
                        border.color: "#2f6b3a"
                        radius: 8
                        visible: page.showOperationSummary

                        ColumnLayout {
                            id: summaryContent
                            anchors { fill: parent; margins: 8 }
                            spacing: 6

                            RowLayout {
                                spacing: 8

                                Text {
                                    text: "✓"
                                    color: "#4ade80"
                                    font.pixelSize: 14
                                    font.weight: Font.Bold
                                }

                                Text {
                                    text: qsTr("Operation complete")
                                    color: page.palette.textPrim
                                    font { pixelSize: 12; weight: Font.Medium }
                                }

                                Item { Layout.fillWidth: true }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                visible: page.lastOperationSummary && page.lastOperationSummary.toolCount > 0

                                Text {
                                    text: page.lastOperationSummary ? page.lastOperationSummary.toolCount + " " + qsTr("tools executed:") : ""
                                    color: page.palette.textSec
                                    font.pixelSize: 11
                                }

                                Repeater {
                                    model: page.lastOperationSummary && page.lastOperationSummary.toolsExecuted ? page.lastOperationSummary.toolsExecuted : []

                                    Text {
                                        text: modelData.name.replace("codex_", "") + (modelData.success ? " ✓" : " ✗")
                                        color: modelData.success ? "#4ade80" : "#f97316"
                                        font.pixelSize: 10
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        Layout.preferredHeight: 8
                    }
                }
            }

            // Empty-state hint
            Column {
                anchors.centerIn: parent
                spacing: 12
                visible: chatHistory.count === 0

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "💬"
                    font.pixelSize: 40
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Start a conversation")
                    font { pixelSize: 16; weight: Font.Medium }
                    color: page.palette.textPrim
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 320
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: {
                        const isOllama = page.selectedModelId.indexOf(":") !== -1
                        if (isOllama)
                            return qsTr("Local model selected. Make sure Ollama is running, then type a message below.")
                        if (!Runtime.chatApiKey)
                            return qsTr("No API key set. Go to Settings → API Key to add one, then come back and start chatting.")
                        return qsTr("Type a message below to chat with ") + page.selectedModelLabel + "."
                    }
                    font.pixelSize: 13
                    color: page.palette.textSec
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Math.max(122, composerColumn.implicitHeight + 24)
            color: "#111"
            border.color: page.palette.border

            Rectangle {
                id: composer
                anchors { fill: parent; margins: 12 }
                color: "#171717"
                border.color: msgInput.activeFocus ? page.palette.accent : page.palette.border
                radius: 10

                ColumnLayout {
                    id: composerColumn
                    anchors { fill: parent; margins: 8 }
                    spacing: 6

                    TextArea {
                        id: msgInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(96, Math.max(44, implicitHeight))
                        Layout.minimumHeight: 44
                        font { pixelSize: 14 }
                        color: page.palette.textPrim
                        placeholderText: qsTr("Message an agent... (/ for commands, @file to attach)")
                        background: null

                        onTextChanged: {
                            const txt = msgInput.text
                            const pos = msgInput.cursorPosition
                            // Find the token at cursor
                            let start = pos - 1
                            while (start > 0 && txt[start - 1] !== ' ' && txt[start - 1] !== '\n')
                                start--
                            if (start >= 0 && start < txt.length && txt[start] === '@') {
                                const query = txt.substring(start + 1, pos)
                                page.mentionQuery = query
                                page.mentionPickerIndex = 0
                                page.mentionPickerOpen = true
                                page.slashPickerOpen = false
                            } else if (start >= 0 && start < txt.length && txt[start] === '/' && start === 0) {
                                // Slash at start of input only
                                const query = txt.substring(start + 1, pos)
                                page.slashQuery = query
                                page.slashPickerIndex = 0
                                page.slashPickerOpen = true
                                page.mentionPickerOpen = false
                            } else {
                                page.mentionPickerOpen = false
                                page.slashPickerOpen = false
                            }
                        }

                        Keys.onPressed: function(event) {
                            if (page.mentionPickerOpen) {
                                if (event.key === Qt.Key_Down) {
                                    page.mentionPickerIndex = Math.min(page.mentionPickerIndex + 1, mentionList.count - 1)
                                    event.accepted = true; return
                                }
                                if (event.key === Qt.Key_Up) {
                                    page.mentionPickerIndex = Math.max(0, page.mentionPickerIndex - 1)
                                    event.accepted = true; return
                                }
                                if (event.key === Qt.Key_Return || event.key === Qt.Key_Tab) {
                                    const item = mentionFilesModel.get(page.mentionPickerIndex)
                                    if (item) page.insertMentionChip(item)
                                    event.accepted = true; return
                                }
                                if (event.key === Qt.Key_Escape) {
                                    page.mentionPickerOpen = false
                                    event.accepted = true; return
                                }
                            }
                            if (page.slashPickerOpen) {
                                const filtered = page.filteredSlashCommands()
                                if (event.key === Qt.Key_Down) {
                                    page.slashPickerIndex = Math.min(page.slashPickerIndex + 1, filtered.length - 1)
                                    event.accepted = true; return
                                }
                                if (event.key === Qt.Key_Up) {
                                    page.slashPickerIndex = Math.max(0, page.slashPickerIndex - 1)
                                    event.accepted = true; return
                                }
                                if (event.key === Qt.Key_Return || event.key === Qt.Key_Tab) {
                                    if (filtered.length > 0)
                                        page.applySlashCommand(filtered[page.slashPickerIndex])
                                    event.accepted = true; return
                                }
                                if (event.key === Qt.Key_Escape) {
                                    page.slashPickerOpen = false
                                    event.accepted = true; return
                                }
                            }
                        }

                        Keys.onReturnPressed: function(event) {
                            if (page.mentionPickerOpen || page.slashPickerOpen) {
                                event.accepted = true
                                return
                            }
                            if (event.modifiers & Qt.ShiftModifier) {
                                event.accepted = false
                                return
                            }
                            sendBtn.clicked()
                            event.accepted = true
                        }
                    }

                    // Mention chips row
                    Flow {
                        Layout.fillWidth: true
                        spacing: 4
                        visible: page.mentionedFiles.length > 0

                        Repeater {
                            model: page.mentionedFiles

                            Rectangle {
                                required property var modelData
                                required property int index
                                width: chipLabel.implicitWidth + 20
                                height: 20
                                radius: 10
                                color: "#1e3a2f"
                                border.color: "#2f6b3a"

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 4
                                    Text {
                                        text: "📎 " + modelData.name
                                        color: "#4ade80"
                                        font.pixelSize: 10
                                        id: chipLabel
                                    }
                                    Text {
                                        text: "×"
                                        color: "#9a9a9a"
                                        font.pixelSize: 10
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                const arr = page.mentionedFiles.slice()
                                                arr.splice(index, 1)
                                                page.mentionedFiles = arr
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        spacing: 8

                        ComposerButton {
                            text: "+"
                            compact: true
                            tooltip: qsTr("Attach context")
                        }

                        ComposerButton {
                            text: qsTr("Analyze file")
                            tooltip: qsTr("Analyze the file open in the editor")
                            enabled: page.currentFilePath.length > 0 && !page.isWaiting
                            onClicked: page.analyzeCurrentEditorFile()
                        }

                        ComposerButton {
                            id: permissionsButton
                            text: "◉ " + page.selectedPermissions + " v"
                            tooltip: qsTr("Permission mode")
                            onClicked: page.openMenuNear(permissionsButton, permissionsMenu)
                        }

                        ComposerButton {
                            id: locationButton
                            text: page.workLocally ? "▱ " + qsTr("Work locally") + " v" : "☁ " + qsTr("Cloud") + " v"
                            tooltip: qsTr("Execution location")
                            onClicked: page.openMenuNear(locationButton, localMenu)
                        }

                        Item { Layout.fillWidth: true }

                        ComposerButton {
                            id: modelButton
                            text: page.selectedModelLabel + "  " + page.selectedEffort + " v"
                            tooltip: qsTr("Select model and reasoning")
                            onClicked: page.openMenuNear(modelButton, modelMenu)
                        }

                        ComposerButton {
                            text: "✧ " + qsTr("IDE context")
                            checked: page.ideContextEnabled
                            tooltip: qsTr("Toggle IDE context")
                            onClicked: page.ideContextEnabled = !page.ideContextEnabled
                        }

                        // Stop button — visible only while waiting
                        Rectangle {
                            id: stopBtn
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 34
                            visible: page.isWaiting
                            color: "#3a1414"
                            border.color: "#8b1a1a"
                            radius: 17

                            Text {
                                anchors.centerIn: parent
                                text: "■"
                                font.pixelSize: 13
                                color: "#ef4444"
                            }

                            ToolTip.text: qsTr("Stop generating")
                            ToolTip.visible: stopBtnMa.containsMouse
                            ToolTip.delay: 600

                            MouseArea {
                                id: stopBtnMa
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                onClicked: {
                                    Runtime.cancelChat()
                                }
                            }
                        }

                        Rectangle {
                            id: sendBtn
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 34
                            visible: !page.isWaiting
                            color: msgInput.text.trim().length > 0 ? page.palette.textPrim : "#5f5f5f"
                            radius: 17

                            signal clicked()
                            onClicked: {
                                const text = msgInput.text.trim()
                                if (!text)
                                    return

                                msgInput.text = ""
                                page.beginAssistantTurn(text, true)
                            }

                            Text {
                                anchors.centerIn: parent
                                text: "↑"
                                font.pixelSize: 19
                                color: "#111"
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: sendBtn.clicked()
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            implicitWidth: localStatusText.implicitWidth + 18
                            implicitHeight: 24
                            radius: 12
                            color: Runtime.localModels.length > 0 ? "#13261c" : "#2a1d12"
                            border.color: Runtime.localModels.length > 0 ? "#28543a" : "#5f3b1d"
                            border.width: 1

                            Text {
                                id: localStatusText
                                anchors.centerIn: parent
                                text: Runtime.localModelStatus.length > 0 ? Runtime.localModelStatus : qsTr("Checking local models")
                                color: Runtime.localModels.length > 0 ? "#86efac" : "#fbbf24"
                                font.pixelSize: 11
                            }
                        }

                        ComposerButton {
                            text: qsTr("Refresh local models")
                            tooltip: Runtime.localModelSource.length > 0
                                ? qsTr("Source: ") + Runtime.localModelSource
                                : qsTr("Ollama executable not found")
                            onClicked: Runtime.refreshLocalModels()
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }
    }

    Menu {
        id: permissionsMenu
        property int popupHeight: 110
        width: 250
        background: MenuPanel {}
        Action { text: qsTr("Default permissions"); onTriggered: page.selectedPermissions = text }
        Action { text: qsTr("Read only"); onTriggered: page.selectedPermissions = text }
        Action { text: qsTr("Auto approve safe commands"); onTriggered: page.selectedPermissions = text }
    }

    Menu {
        id: localMenu
        property int popupHeight: 78
        width: 220
        background: MenuPanel {}
        Action { text: qsTr("Work locally"); onTriggered: page.workLocally = true }
        Action { text: qsTr("Use cloud runtime"); onTriggered: page.workLocally = false }
    }

    Popup {
        id: modelMenu
        property int popupHeight: 364
        width: 300
        height: popupHeight
        padding: 6
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: MenuPanel {}

        contentItem: ColumnLayout {
            spacing: 6

            ListView {
                id: modelList
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(230, contentHeight)
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: availableModels

                delegate: Rectangle {
                    id: modelItem
                    required property string idValue
                    required property string label
                    required property string provider
                    required property string detail

                    width: modelList.width
                    height: 46
                    radius: 6
                    color: modelMouse.containsMouse ? "#262626" : "transparent"

                    RowLayout {
                        anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                        spacing: 10

                        Text {
                            text: page.selectedModelId === modelItem.idValue ? "✓" : ""
                            color: page.palette.accent
                            font.pixelSize: 13
                            Layout.preferredWidth: 14
                        }

                        Column {
                            Layout.fillWidth: true
                            spacing: 1
                            Text {
                                text: modelItem.label
                                color: page.palette.textPrim
                                font { pixelSize: 13; weight: page.selectedModelId === modelItem.idValue ? Font.DemiBold : Font.Normal }
                            }
                            Text {
                                width: parent.width
                                text: modelItem.provider + " · " + modelItem.detail
                                color: page.palette.textSec
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }
                    }

                    MouseArea {
                        id: modelMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            page.selectedModelId = modelItem.idValue
                            page.selectedModelLabel = modelItem.label
                            Runtime.chatModel = modelItem.idValue
                            modelMenu.close()
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: page.palette.border
            }

            ListView {
                id: effortList
                Layout.fillWidth: true
                Layout.preferredHeight: contentHeight
                interactive: false
                model: effortOptions

                delegate: Rectangle {
                    id: effortItem
                    required property string label
                    required property string detail

                    width: effortList.width
                    height: 36
                    radius: 6
                    color: effortMouse.containsMouse ? "#262626" : "transparent"

                    RowLayout {
                        anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                        spacing: 10

                        Text {
                            text: page.selectedEffort === effortItem.label ? "✓" : ""
                            color: page.palette.accent
                            font.pixelSize: 13
                            Layout.preferredWidth: 14
                        }
                        Text {
                            text: effortItem.label + " · " + effortItem.detail
                            color: page.palette.textPrim
                            font.pixelSize: 12
                        }
                    }

                    MouseArea {
                        id: effortMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            page.selectedEffort = effortItem.label
                            modelMenu.close()
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: Runtime

        function onLocalModelsChanged() {
            page.syncRuntimeModels()
        }

        function onChatChunkReceived(delta) {
            page.pendingContent += delta
            if (page.pendingMsgIdx >= 0) {
                chatHistory.setProperty(page.pendingMsgIdx, "content", page.renderStreamingContent())
                page.scrollToLatest()
            }
        }

        function onChatStreamFinished(fullContent) {
            const approval = page.parseApprovalMarker(fullContent)
            const visibleContent = approval ? approval.content : fullContent

            if (page.pendingMsgIdx >= 0) {
                chatHistory.setProperty(page.pendingMsgIdx, "content", visibleContent)
                chatHistory.setProperty(page.pendingMsgIdx, "time",
                    Qt.formatTime(new Date(), "hh:mm"))
                page.scrollToLatest()
            }
            page.pendingMsgIdx = -1
            page.pendingContent = ""
            page.streamCursorOn = true
            page.streamElapsedSec = 0
            page.isWaiting = false

            if (approval) {
                // Check if this operation is auto-approvable
                const isAutoApprovable = Runtime.isToolAutoApprovable(approval.tool, approval.args)

                if (isAutoApprovable) {
                    // Auto-apply the operation
                    if (approval.tool === "codex_apply_patch") {
                        const result = Runtime.applyApprovedPatch(approval.path, approval.patch)
                        if (result && result.ok) {
                            page.appendAssistantNotice(qsTr("Auto-approved: Patch applied to ") + (result.path || approval.path))
                        } else {
                            const message = result && result.error ? result.error : qsTr("Patch failed.")
                            page.appendAssistantNotice(qsTr("Auto-approved patch failed: ") + message)
                        }
                    } else if (approval.tool === "codex_run_command") {
                        const result = Runtime.runApprovedCommand(approval.command, approval.reason)
                        page.appendAssistantNotice(page.formatCommandResult(approval.command, result))
                    } else {
                        // For other tools, just show a notice
                        page.appendAssistantNotice(qsTr("Auto-approved: ") + approval.tool)
                    }
                    return
                }

                // Show approval dialog for non-auto-approvable operations
                page.approvalTool = approval.tool
                page.approvalReason = approval.reason
                page.approvalArgsText = approval.argsText
                page.approvalPath = approval.path
                page.approvalPatch = approval.patch
                page.approvalSummary = approval.summary
                page.approvalCommand = approval.command
                page.approvalCommandReason = approval.commandReason
                page.patchShowContext = true
                page.patchCollapseHunks = false
                page.patchSelectedFile = qsTr("(all files)")
                page.patchCollapsedByKey = ({})
                page.patchSearchText = ""
                page.patchCurrentHunkIndex = 0
                page.patchSelectedHunkKeys = ({})
                page.approvalDialogOpen = true
                approvalPopup.open()
            }
        }

        function onChatErrorOccurred(error) {
            if (page.pendingMsgIdx >= 0) {
                chatHistory.setProperty(page.pendingMsgIdx, "content",
                    qsTr("Error: ") + error)
                chatHistory.setProperty(page.pendingMsgIdx, "time",
                    Qt.formatTime(new Date(), "hh:mm"))
                page.scrollToLatest()
            }
            page.pendingMsgIdx = -1
            page.pendingContent = ""
            page.streamCursorOn = true
            page.streamElapsedSec = 0
            page.isWaiting = false
            page.currentToolRunning = false
        }

        function onToolExecutionStarted(toolName, args, iteration, maxIterations) {
            page.currentToolName = toolName
            page.currentToolIteration = iteration
            page.maxToolIterations = maxIterations
            page.currentToolRunning = true
            page.refreshWaitingHint()
        }

        function onToolExecutionFinished(toolName, success) {
            page.currentToolRunning = false
            page.refreshWaitingHint()
        }

        function onOperationSummaryReady(summary) {
            page.lastOperationSummary = summary
            page.showOperationSummary = true
            summaryAutoHideTimer.restart()
        }

        function onChatCancelled() {
            page.isWaiting = false
            page.currentToolRunning = false
            page.currentToolName = ""
            if (page.pendingMsgIdx >= 0) {
                const current = chatHistory.get(page.pendingMsgIdx).content
                const stopped = page.pendingContent.length === 0 ? qsTr("(stopped)") : current + qsTr(" _(stopped)_")
                chatHistory.setProperty(page.pendingMsgIdx, "content", stopped)
                chatHistory.setProperty(page.pendingMsgIdx, "time", Qt.formatTime(new Date(), "hh:mm"))
            }
            page.pendingMsgIdx = -1
            page.pendingContent = ""
            page.streamCursorOn = true
            page.streamElapsedSec = 0
        }
    }

    Timer {
        id: summaryAutoHideTimer
        interval: 5000
        repeat: false
        onTriggered: page.showOperationSummary = false
    }

    Timer {
        id: waitingHintTimer
        interval: 650
        repeat: true
        running: page.isWaiting
        onTriggered: {
            if (!page.isWaiting || page.pendingMsgIdx < 0 || page.pendingContent.length > 0)
                return

            page.waitingDotPhase = (page.waitingDotPhase + 1) % 3
            if (!page.currentToolRunning)
                page.waitingHintIndex = (page.waitingHintIndex + 1) % page.waitingHints.length
            page.refreshWaitingHint()
        }
    }

    Timer {
        id: streamCursorTimer
        interval: 380
        repeat: true
        running: page.isWaiting && page.pendingContent.length > 0
        onTriggered: {
            if (!page.isWaiting || page.pendingMsgIdx < 0 || page.pendingContent.length === 0)
                return
            page.streamCursorOn = !page.streamCursorOn
            chatHistory.setProperty(page.pendingMsgIdx, "content", page.renderStreamingContent())
        }
    }

    Timer {
        id: streamElapsedTimer
        interval: 1000
        repeat: true
        running: page.isWaiting
        onTriggered: {
            if (page.isWaiting)
                page.streamElapsedSec += 1
        }
    }

    // Slash command picker popup (anchored above the composer)
    Popup {
        id: slashPopup
        modal: false
        focus: false
        closePolicy: Popup.NoAutoClose
        visible: page.slashPickerOpen && page.filteredSlashCommands().length > 0

        x: 8
        y: page.height - height - 112
        width: Math.min(400, page.width - 16)
        padding: 4

        background: Rectangle {
            color: "#1e1e1e"
            border.color: "#3a3a3a"
            radius: 8
        }

        Column {
            width: parent.width
            spacing: 0

            Text {
                leftPadding: 8
                topPadding: 4
                bottomPadding: 4
                text: qsTr("Commands") + (page.slashQuery.length > 0 ? " · /" + page.slashQuery : "")
                font { pixelSize: 10; weight: Font.Medium }
                color: "#6a6a6a"
            }

            Repeater {
                model: page.filteredSlashCommands()

                delegate: Item {
                    required property var modelData
                    required property int index
                    width: slashPopup.width - 8
                    height: 40

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 2
                        radius: 4
                        color: index === page.slashPickerIndex ? "#2a3a4a" : "transparent"

                        RowLayout {
                            anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                            spacing: 10

                            Text {
                                text: modelData.icon
                                font.pixelSize: 14
                            }

                            Column {
                                Layout.fillWidth: true
                                spacing: 1

                                Text {
                                    text: "/" + modelData.name
                                    color: "#f0f0f0"
                                    font { pixelSize: 12; weight: Font.Medium }
                                }
                                Text {
                                    text: modelData.desc
                                    color: "#8a8a8a"
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                    width: parent.width
                                }
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: page.applySlashCommand(modelData)
                        onContainsMouseChanged: {
                            if (containsMouse)
                                page.slashPickerIndex = index
                        }
                    }
                }
            }

            Text {
                leftPadding: 8
                bottomPadding: 4
                text: "↑↓ navigate · Enter select · Esc cancel"
                color: "#5a5a5a"
                font.pixelSize: 9
            }
        }
    }

    // @file mention picker popup (anchored above the composer)
    Popup {
        id: mentionPopup
        modal: false
        focus: false
        closePolicy: Popup.NoAutoClose
        visible: page.mentionPickerOpen && mentionFilesModel.count > 0

        x: 8
        y: page.height - height - 112
        width: Math.min(360, page.width - 16)
        padding: 4

        background: Rectangle {
            color: "#1e1e1e"
            border.color: "#3a3a3a"
            radius: 8
        }

        Column {
            width: parent.width
            spacing: 0

            Text {
                leftPadding: 8
                topPadding: 4
                bottomPadding: 4
                text: qsTr("Files") + (page.mentionQuery.length > 0 ? " · " + page.mentionQuery : "")
                font { pixelSize: 10; weight: Font.Medium }
                color: "#6a6a6a"
            }

            ListView {
                id: mentionList
                width: parent.width
                height: Math.min(contentHeight, 220)
                model: mentionFilesModel
                clip: true
                currentIndex: page.mentionPickerIndex

                delegate: Item {
                    id: mentionDelegate
                    required property string name
                    required property string path
                    required property string rel
                    required property int index
                    width: mentionList.width
                    height: 32

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 2
                        radius: 4
                        color: mentionDelegate.index === page.mentionPickerIndex ? "#2a3a4a" : "transparent"

                        RowLayout {
                            anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                            spacing: 8

                            Text {
                                text: "📄"
                                font.pixelSize: 11
                            }
                            Text {
                                Layout.fillWidth: true
                                text: mentionDelegate.name
                                color: "#f0f0f0"
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                            Text {
                                text: mentionDelegate.rel
                                color: "#6a6a6a"
                                font.pixelSize: 10
                                elide: Text.ElideLeft
                                Layout.maximumWidth: 160
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: page.insertMentionChip({
                            name: mentionDelegate.name,
                            path: mentionDelegate.path,
                            rel:  mentionDelegate.rel
                        })
                        onContainsMouseChanged: {
                            if (containsMouse)
                                page.mentionPickerIndex = mentionDelegate.index
                        }
                    }
                }
            }

            Text {
                leftPadding: 8
                bottomPadding: 4
                text: "↑↓ navigate · Enter/Tab select · Esc cancel"
                color: "#5a5a5a"
                font.pixelSize: 9
            }
        }
    }

    Popup {
        id: approvalPopup
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        width: Math.min(640, page.width - 40)
        padding: 12
        anchors.centerIn: parent

        onOpened: {
            page.patchSearchText = ""
            page.patchCurrentHunkIndex = 0
            page.patchSelectedHunkKeys = ({})
        }

        background: Rectangle {
            color: "#171717"
            border.color: page.palette.border
            radius: 10
        }

        contentItem: ColumnLayout {
            focus: true
            spacing: 10

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_J || event.text === "j") {
                    page.cyclePatchHunkSelection(1)
                    event.accepted = true
                } else if (event.key === Qt.Key_K || event.text === "k") {
                    page.cyclePatchHunkSelection(-1)
                    event.accepted = true
                } else if (event.key === Qt.Key_Q || event.text === "q") {
                    approvalPopup.close()
                    page.resetApproval()
                    event.accepted = true
                } else if (event.key === Qt.Key_N || event.text === "n") {
                    page.cyclePatchHunkSelection(1)
                    event.accepted = true
                } else if (event.key === Qt.Key_P || event.text === "p") {
                    page.cyclePatchHunkSelection(-1)
                    event.accepted = true
                } else if (event.key === Qt.Key_Space) {
                    page.toggleCurrentHunkCollapsed()
                    event.accepted = true
                } else if (event.key === Qt.Key_C || event.text === "c") {
                    page.copyCurrentHunk()
                    event.accepted = true
                } else if (event.key === Qt.Key_X || event.text === "x") {
                    page.toggleCurrentHunkSelected()
                    event.accepted = true
                } else if (event.key === Qt.Key_A || event.text === "a") {
                    page.selectAllHunks()
                    event.accepted = true
                } else if (event.key === Qt.Key_D || event.text === "d") {
                    page.deselectAllHunks()
                    event.accepted = true
                }
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Approval required")
                color: page.palette.textPrim
                font { pixelSize: 15; weight: Font.DemiBold }
            }

            Rectangle {
                Layout.fillWidth: true
                visible: page.approvalPatch.length > 0
                height: 32
                color: "#101316"
                border.color: page.palette.border
                radius: 6

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 4
                    spacing: 6

                    Text {
                        text: "🔍"
                        font.pixelSize: 12
                    }

                    TextInput {
                        id: searchInput
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        verticalAlignment: TextInput.AlignVCenter
                        text: page.patchSearchText
                        onTextChanged: page.patchSearchText = text
                        color: page.palette.textPrim
                        font { family: "Menlo"; pixelSize: 11 }
                        selectByMouse: true
                        clip: true
                    }

                    Text {
                        text: "j/k nav  space collapse  c copy  x toggle  a all  d none  q close"
                        color: page.palette.textSec
                        font.pixelSize: 8
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: page.approvalTool === "codex_apply_patch"
                      ? qsTr("Apply patch") + ": " + page.approvalPath
                      : page.approvalTool === "codex_run_command"
                        ? qsTr("Run command") + ": " + page.approvalCommand
                      : qsTr("Tool") + ": " + page.approvalTool
                color: page.palette.textSec
                font.pixelSize: 12
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: page.approvalReason
                color: page.palette.textPrim
                font.pixelSize: 12
            }

            Rectangle {
                Layout.fillWidth: true
                visible: page.approvalPatch.length > 0
                color: "#101316"
                border.color: page.palette.border
                radius: 8
                implicitHeight: 286

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        color: page.palette.textSec
                        font.pixelSize: 11
                        text: {
                            const s = page.patchStats(page.approvalPatch)
                            const h = page.getHunkSelectionStats()
                            const filesText = s.filesText.length > 0 ? s.filesText : qsTr("(single file)")
                            let result = qsTr("Files") + ": " + s.fileCount
                                + "   +" + s.additions + "   -" + s.deletions
                            if (h.total > 0)
                                result += "\nHunks: " + h.selected + " / " + h.total + " selected"
                            result += "\n" + filesText
                            return result
                        }
                        wrapMode: Text.WrapAnywhere
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        ComposerButton {
                            text: qsTr("File") + ": " + page.patchSelectedFile
                            tooltip: qsTr("Cycle file filter")
                            onClicked: page.cyclePatchFileFilter()
                        }

                        ComposerButton {
                            text: page.patchShowContext ? qsTr("Context: on") : qsTr("Context: off")
                            checked: page.patchShowContext
                            tooltip: qsTr("Toggle context lines in hunk preview")
                            onClicked: page.patchShowContext = !page.patchShowContext
                        }

                        ComposerButton {
                            text: page.patchCollapseHunks ? qsTr("Hunks: compact") : qsTr("Hunks: full")
                            checked: page.patchCollapseHunks
                            tooltip: qsTr("Compact mode shows fewer lines per hunk")
                            onClicked: page.patchCollapseHunks = !page.patchCollapseHunks
                        }

                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 0

                        Flickable {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            contentWidth: Math.max(width, hunkColumn.implicitWidth)
                            contentHeight: hunkColumn.implicitHeight
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds

                            Column {
                                id: hunkColumn
                                width: parent.width
                                spacing: 8

                                Repeater {
                                    model: page.patchHunksSearchFiltered(
                                        page.patchHunks(page.approvalPatch,
                                                       5,
                                                       36,
                                                       page.patchShowContext,
                                                       page.patchCollapseHunks,
                                                       10,
                                                       page.patchSelectedFile))

                                    delegate: Rectangle {
                                        required property var modelData
                                        required property int index

                                        width: hunkColumn.width
                                        color: index === page.patchCurrentHunkIndex ? "#1a3a3a" : "#0d1117"
                                        border.color: index === page.patchCurrentHunkIndex ? "#4ade80" : page.palette.border
                                        border.width: index === page.patchCurrentHunkIndex ? 2 : 1
                                        radius: 6
                                        implicitHeight: hunkBody.implicitHeight + 16
                                        Behavior on color { ColorAnimation { duration: 100 } }
                                        Behavior on border.color { ColorAnimation { duration: 100 } }

                                        Column {
                                            id: hunkBody
                                            anchors.fill: parent
                                            anchors.margins: 8
                                            spacing: 6

                                            Text {
                                                width: parent.width
                                                text: (modelData.file && modelData.file.length > 0 ? modelData.file + "  " : "")
                                                      + modelData.header
                                                      + "   -" + modelData.deletions + " +" + modelData.additions
                                                color: "#93c5fd"
                                                font { family: "Menlo"; pixelSize: 11; weight: Font.DemiBold }
                                                wrapMode: Text.WrapAnywhere
                                            }

                                            RowLayout {
                                                width: parent.width
                                                spacing: 6

                                                ComposerButton {
                                                    text: page.isHunkSelected(modelData.key) ? "☑ Include" : "☐ Exclude"
                                                    compact: true
                                                    checked: page.isHunkSelected(modelData.key)
                                                    tooltip: qsTr("x key to toggle inclusion")
                                                    onClicked: {
                                                        page.patchCurrentHunkIndex = index
                                                        page.toggleHunkSelected(modelData.key)
                                                    }
                                                }

                                                ComposerButton {
                                                    text: page.isHunkCollapsed(modelData.key)
                                                          ? qsTr("▶ Expand")
                                                          : qsTr("▼ Collapse")
                                                    compact: true
                                                    tooltip: qsTr("Space to toggle collapse")
                                                    onClicked: page.toggleHunkCollapsed(modelData.key)
                                                }

                                                ComposerButton {
                                                    text: qsTr("📋 Copy")
                                                    compact: true
                                                    tooltip: qsTr("c key to copy this hunk")
                                                    onClicked: {
                                                        page.patchCurrentHunkIndex = index
                                                        page.copyCurrentHunk()
                                                    }
                                                }

                                                Item { Layout.fillWidth: true }
                                            }

                                            RowLayout {
                                                visible: !page.isHunkCollapsed(modelData.key)
                                                width: parent.width
                                                spacing: 8

                                                Rectangle {
                                                    Layout.fillWidth: true
                                                    color: "#221417"
                                                    border.color: "#6b2c37"
                                                    radius: 4
                                                    implicitHeight: beforeText.implicitHeight + 12

                                                    Text {
                                                        id: beforeText
                                                        anchors.fill: parent
                                                        anchors.margins: 6
                                                        text: modelData.before
                                                        color: "#fecaca"
                                                        font.family: "Menlo"
                                                        font.pixelSize: 11
                                                        wrapMode: Text.WrapAnywhere
                                                    }
                                                }

                                                Rectangle {
                                                    Layout.fillWidth: true
                                                    color: "#142219"
                                                    border.color: "#2f6b3a"
                                                    radius: 4
                                                    implicitHeight: afterText.implicitHeight + 12

                                                    Text {
                                                        id: afterText
                                                        anchors.fill: parent
                                                        anchors.margins: 6
                                                        text: modelData.after
                                                        color: "#bbf7d0"
                                                        font.family: "Menlo"
                                                        font.pixelSize: 11
                                                        wrapMode: Text.WrapAnywhere
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                visible: page.approvalPatch.length === 0
                         && (page.approvalCommand.length > 0 || page.approvalArgsText.length > 0)
                color: "#101316"
                border.color: page.palette.border
                radius: 8
                implicitHeight: Math.min(300, Math.max(120, argsText.implicitHeight + 16))
                clip: true

                Flickable {
                    anchors.fill: parent
                    anchors.margins: 8
                    contentWidth: argsText.implicitWidth
                    contentHeight: argsText.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    clip: true

                    Text {
                        id: argsText
                        text: page.approvalCommand.length > 0
                              ? page.approvalCommand
                              : page.approvalArgsText
                        color: "#d6dde5"
                        font.family: "Menlo"
                        font.pixelSize: 11
                        wrapMode: Text.NoWrap
                    }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 8

                ComposerButton {
                    text: qsTr("Deny")
                    onClicked: {
                        approvalPopup.close()
                        page.resetApproval()
                    }
                }

                ComposerButton {
                    text: page.approvalTool === "codex_apply_patch"
                          ? qsTr("Apply")
                          : page.approvalTool === "codex_run_command"
                            ? qsTr("Run")
                          : qsTr("Approve and retry")
                    onClicked: {
                        approvalPopup.close()
                        if (page.approvalTool === "codex_apply_patch") {
                            page.approveCurrentPatch()
                        } else if (page.approvalTool === "codex_run_command") {
                            page.approveCurrentCommand()
                        } else {
                            page.selectedPermissions = qsTr("Auto approve safe commands")
                            page.resetApproval()
                            if (page.lastUserPrompt.length > 0)
                                page.beginAssistantTurn(page.lastUserPrompt, false)
                        }
                    }
                }
            }
        }
    }

    component MenuPanel: Rectangle {
        implicitWidth: 260
        implicitHeight: 36
        color: "#1a1a1a"
        border.color: page.palette.border
        radius: 8
    }

    component ComposerButton: Rectangle {
        id: control
        property alias text: label.text
        property string tooltip: ""
        property bool checked: false
        property bool compact: false
        signal clicked()

        Layout.preferredWidth: compact ? 30 : Math.min(220, Math.max(34, label.implicitWidth + 18))
        Layout.preferredHeight: 30
        color: checked ? "#24283a" : mouse.containsMouse ? "#242424" : "transparent"
        border.color: checked ? page.palette.accentDim : "transparent"
        border.width: checked ? 1 : 0
        radius: 6

        Text {
            id: label
            anchors.centerIn: parent
            width: parent.width - 12
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            color: control.checked ? page.palette.textPrim : page.palette.textSec
            font { pixelSize: 12 }
        }

        ToolTip.visible: mouse.containsMouse && control.tooltip.length > 0
        ToolTip.text: control.tooltip
        ToolTip.delay: 500

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: control.clicked()
        }
    }
}
