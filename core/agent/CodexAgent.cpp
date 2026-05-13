#include "CodexAgent.h"

#include "tools/ToolRegistry.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QStringList>

namespace neurx {

namespace {
constexpr int kMaxContextMessages = 24;
constexpr int kMaxCurrentFileChars = 12000;
constexpr int kMaxToolIterations = 8;
constexpr int kMaxPreviewChars = 320;

QString clippedPreview(const QString &text)
{
    if (text.size() <= kMaxPreviewChars)
        return text;
    return text.left(kMaxPreviewChars) + QStringLiteral(" ...");
}

bool hasUnsafeShellToken(const QString &command)
{
    static const QStringList tokens = {
        QStringLiteral(";"),
        QStringLiteral("&&"),
        QStringLiteral("||"),
        QStringLiteral("|"),
        QStringLiteral(">"),
        QStringLiteral("<"),
        QStringLiteral("$()"),
        QStringLiteral("`")
    };

    for (const QString &token : tokens) {
        if (command.contains(token))
            return true;
    }
    return false;
}

bool isLikelySafeShellCommand(const QString &command)
{
    const QString normalized = command.trimmed().toLower();
    if (normalized.isEmpty())
        return false;

    if (hasUnsafeShellToken(normalized))
        return false;

    QStringList parts = QProcess::splitCommand(normalized);
    if (parts.isEmpty())
        return false;

    const QString program = QFileInfo(parts.takeFirst()).fileName();
    if (program == QLatin1String("ls") || program == QLatin1String("pwd")
        || program == QLatin1String("cat") || program == QLatin1String("head")
        || program == QLatin1String("tail") || program == QLatin1String("wc")
        || program == QLatin1String("rg")) {
        return true;
    }

    if (program == QLatin1String("git")) {
        if (parts.isEmpty())
            return false;
        const QString subcommand = parts.first();
        return subcommand == QLatin1String("status")
            || subcommand == QLatin1String("diff")
            || subcommand == QLatin1String("log")
            || subcommand == QLatin1String("show");
    }

    if (program == QLatin1String("cmake"))
        return parts.contains(QStringLiteral("--build"));
    if (program == QLatin1String("make") || program == QLatin1String("ctest"))
        return true;

    return false;
}
}

CodexAgent::CodexAgent(QObject *parent)
    : Agent(QStringLiteral("Codex"), parent)
    , m_workspaceContext(nullptr)
{
}

void CodexAgent::setConfig(const LlmConfig &config)
{
    m_config = config;
    if (m_client)
        m_client->setConfig(m_config);
}

void CodexAgent::setWorkspaceRoot(const QString &workspaceRoot)
{
    m_workspaceRoot = workspaceRoot;
    if (!m_workspaceContext) {
        m_workspaceContext = new WorkspaceContext(workspaceRoot, this);
    }
}

void CodexAgent::setCurrentFile(const QString &filePath, const QString &content)
{
    m_currentFilePath = filePath;
    m_currentFileContent = content;
    if (m_workspaceContext) {
        m_workspaceContext->setCurrentFile(filePath, content);
    }
}

void CodexAgent::clearContext()
{
    m_context.clear();
}

void CodexAgent::cancelTurn()
{
    if (!m_busy)
        return;
    if (m_client)
        m_client->abort();
    // Reset turn state
    m_turnMessages.clear();
    m_turnTools.clear();
    m_remainingToolIterations = 0;
    m_turnPermissionMode.clear();
    m_turnNeedsApproval = false;
    m_turnApprovalTool.clear();
    m_turnApprovalReason.clear();
    m_turnApprovalArgs.clear();
    m_visibleTranscript.clear();
    m_busy = false;
    setStatus(Status::Idle);
    emit turnCancelled();
}

void CodexAgent::receiveMessage(const QVariantMap &message)
{
    const QString type = message.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("clear")) {
        clearContext();
        return;
    }

    if (type == QLatin1String("prompt")) {
        submitPrompt(message.value(QStringLiteral("text")).toString(),
                     message.value(QStringLiteral("options")).toMap());
    }
}

void CodexAgent::submitPrompt(const QString &text, const QVariantMap &options)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return;

    if (m_busy) {
        emit errorOccurred(QStringLiteral("Codex agent is still responding."));
        return;
    }

    if (m_config.endpoint.isEmpty() || m_config.model.isEmpty()) {
        emit errorOccurred(QStringLiteral("Codex agent is missing model configuration."));
        return;
    }

    if (!m_client) {
        m_client = new LlmClient(m_config, this);
        connect(m_client, &LlmClient::responseReceived,
                this, [this](const QString &content, const QVariantMap &toolCall) {
            if (!m_busy)
                return;

            if (!toolCall.isEmpty() && m_remainingToolIterations > 0) {
                const QString toolName = toolCall.value(QStringLiteral("name")).toString();
                const QString argsStr = toolCall.value(QStringLiteral("args")).toString();
                const QVariantMap args = QJsonDocument::fromJson(argsStr.toUtf8()).toVariant().toMap();
                const int currentIteration = kMaxToolIterations - m_remainingToolIterations + 1;

                emit toolExecutionStarted(toolName, argsStr, currentIteration, kMaxToolIterations);

                QVariantMap toolResult;

                const QString toolCallChunk = QStringLiteral("\n[tool] %1 args=%2\n")
                    .arg(toolName, clippedPreview(argsStr));
                m_visibleTranscript += toolCallChunk;
                emit chunkReceived(toolCallChunk);

                QString blockReason;
                bool toolSucceeded = false;
                if (!isToolCallAllowed(toolName, args, m_turnPermissionMode, &blockReason)) {
                    if (m_turnPermissionMode == QLatin1String("default")
                        && blockReason.contains(QStringLiteral("requires approval"))) {
                        m_turnNeedsApproval = true;
                        m_turnApprovalTool = toolName;
                        m_turnApprovalReason = blockReason;
                        m_turnApprovalArgs = args;
                    }
                    toolResult = {{QStringLiteral("error"),
                                   blockReason.isEmpty()
                                   ? (QStringLiteral("tool call blocked by permission mode: ")
                                      + m_turnPermissionMode)
                                   : blockReason}};
                } else if (auto *tool = ToolRegistry::instance().findTool(toolName)) {
                    toolResult = tool->execute(args);
                    toolSucceeded = !toolResult.contains(QStringLiteral("error"));
                    if ((toolName == QLatin1String("codex_create_file")
                         || toolName == QLatin1String("codex_edit_file"))
                        && toolResult.value(QStringLiteral("ok")).toBool()) {
                        const QString changedPath = toolResult.value(QStringLiteral("path")).toString();
                        if (!changedPath.isEmpty())
                            emit workspaceFileChanged(changedPath);
                    }
                    if (toolName == QLatin1String("codex_propose_patch")
                        && toolResult.value(QStringLiteral("requiresApproval")).toBool()) {
                        m_turnNeedsApproval = true;
                        m_turnApprovalTool = QStringLiteral("codex_apply_patch");
                        m_turnApprovalReason = toolResult.value(QStringLiteral("summary")).toString();
                        if (m_turnApprovalReason.trimmed().isEmpty())
                            m_turnApprovalReason = QStringLiteral("Review and apply the proposed workspace patch.");
                        m_turnApprovalArgs = {
                            {QStringLiteral("path"), toolResult.value(QStringLiteral("path"))},
                            {QStringLiteral("patch"), toolResult.value(QStringLiteral("patch"))},
                            {QStringLiteral("summary"), toolResult.value(QStringLiteral("summary"))}
                        };
                    }
                } else {
                    toolResult = {{QStringLiteral("error"), QStringLiteral("tool not found: ") + toolName}};
                }

                emit toolExecutionFinished(toolName, toolSucceeded);

                const QString resultJson = QString::fromUtf8(QJsonDocument::fromVariant(toolResult)
                    .toJson(QJsonDocument::Compact));
                const QString toolResultChunk = QStringLiteral("[tool_result] %1\n")
                    .arg(clippedPreview(resultJson));
                m_visibleTranscript += toolResultChunk;
                emit chunkReceived(toolResultChunk);

                const QString toolCallId = toolCall.value(QStringLiteral("id")).toString();
                const QVariantList toolCalls{
                    QVariantMap{
                        {QStringLiteral("id"), toolCallId},
                        {QStringLiteral("type"), QStringLiteral("function")},
                        {QStringLiteral("function"), QVariantMap{
                            {QStringLiteral("name"), toolName},
                            {QStringLiteral("arguments"), argsStr}
                        }}
                    }
                };
                m_turnMessages.append({QStringLiteral("assistant"),
                    content,
                    QString(),
                    QString(),
                    toolCalls});
                m_turnMessages.append({QStringLiteral("tool"),
                    resultJson,
                    toolName,
                    toolCallId});

                --m_remainingToolIterations;
                continueToolLoop();
                return;
            }

            if (!toolCall.isEmpty() && m_remainingToolIterations <= 0) {
                const QString capped = QStringLiteral("\n[tool] Reached max tool iterations, returning best effort answer.\n");
                m_visibleTranscript += capped;
                emit chunkReceived(capped);
            }

            finishTurnWithContent(content);
        });
        connect(m_client, &LlmClient::errorOccurred,
                this, [this](const QString &error) {
            m_busy = false;
            setStatus(Status::Error);
            emit errorOccurred(error);
        });
    } else {
        m_client->setConfig(m_config);
    }

    const QString userPrompt = buildUserPrompt(trimmed, options);
    m_turnMessages.clear();
    m_turnMessages.append({QStringLiteral("system"), buildSystemPrompt(options)});
    m_turnMessages.append(m_context);
    m_turnMessages.append({QStringLiteral("user"), userPrompt});
    m_turnPermissionMode = normalizedPermissionMode(options);
    m_turnTools = availableTools(options);
    m_remainingToolIterations = kMaxToolIterations;
    m_visibleTranscript.clear();
    m_turnNeedsApproval = false;
    m_turnApprovalTool.clear();
    m_turnApprovalReason.clear();
    m_turnApprovalArgs.clear();

    m_context.append({QStringLiteral("user"), userPrompt});
    trimContext();

    m_busy = true;
    setStatus(Status::Running);
    continueToolLoop();
}

QVariantList CodexAgent::availableTools(const QVariantMap &) const
{
    QVariantList tools;
    const QVariantList schemas = ToolRegistry::instance().toolSchemas();
    for (const QVariant &schemaVar : schemas) {
        const QVariantMap schema = schemaVar.toMap();
        const QVariantMap function = schema.value(QStringLiteral("function")).toMap();
        const QString name = function.value(QStringLiteral("name")).toString();
        if (name.startsWith(QStringLiteral("codex_")))
            tools.append(schema);
    }
    return tools;
}

QString CodexAgent::normalizedPermissionMode(const QVariantMap &options) const
{
    const QString raw = options.value(QStringLiteral("permissions"),
                                      QStringLiteral("Default permissions")).toString().trimmed().toLower();

    if (raw == QLatin1String("read only"))
        return QStringLiteral("read_only");
    if (raw == QLatin1String("auto approve safe commands"))
        return QStringLiteral("auto_safe");
    return QStringLiteral("default");
}

bool CodexAgent::isToolCallAllowed(const QString &toolName,
                                   const QVariantMap &args,
                                   const QString &permissionMode,
                                   QString *reason) const
{
    auto setReason = [reason](const QString &value) {
        if (reason)
            *reason = value;
    };

    if (permissionMode == QLatin1String("read_only")) {
        return toolName == QLatin1String("codex_read_file")
            || toolName == QLatin1String("codex_search_workspace")
            || toolName == QLatin1String("codex_find_symbol")
            || toolName == QLatin1String("codex_find_references");
    }

    if (permissionMode == QLatin1String("default")) {
        if (toolName == QLatin1String("codex_run_command")) {
            setReason(QStringLiteral("codex_run_command requires approval in Default permissions."));
            return false;
        }
        if (toolName == QLatin1String("codex_create_file")) {
            const QString path = args.value(QStringLiteral("path")).toString().trimmed();
            if (path.isEmpty()) {
                setReason(QStringLiteral("codex_create_file requires a path in Default permissions."));
                return false;
            }

            const QString resolvedPath = QDir(m_workspaceRoot).absoluteFilePath(path);
            if (QFileInfo::exists(resolvedPath)) {
                setReason(QStringLiteral("codex_create_file would overwrite an existing file and requires approval in Default permissions."));
                return false;
            }

            return true;
        }
        if (toolName == QLatin1String("codex_edit_file")) {
            setReason(QStringLiteral("%1 requires approval in Default permissions.").arg(toolName));
            return false;
        }
        return toolName.startsWith(QStringLiteral("codex_"));
    }

    if (permissionMode == QLatin1String("auto_safe")) {
        if (toolName == QLatin1String("codex_run_command")) {
            const QString command = args.value(QStringLiteral("command")).toString();
            const bool safe = isLikelySafeShellCommand(command);
            if (!safe)
                setReason(QStringLiteral("command is outside the auto-approved safe command allowlist."));
            return safe;
        }
        return toolName.startsWith(QStringLiteral("codex_"));
    }

    return true;
}

bool CodexAgent::isToolCallAutoApprovable(const QString &toolName,
                                          const QVariantMap &args) const
{
    // File operations are always auto-approvable in auto_safe mode
    if (toolName == QLatin1String("codex_create_file")
        || toolName == QLatin1String("codex_edit_file")) {
        return true;
    }

    // Workspace inspection tools are auto-approvable
    if (toolName == QLatin1String("codex_read_file")
        || toolName == QLatin1String("codex_search_workspace")
        || toolName == QLatin1String("codex_find_symbol")
        || toolName == QLatin1String("codex_find_references")) {
        return true;
    }

    // Patches must always be approved by user (not auto-approvable)
    if (toolName == QLatin1String("codex_propose_patch")
        || toolName == QLatin1String("codex_apply_patch")) {
        return false;
    }

    // Commands: auto-approvable only if safe
    if (toolName == QLatin1String("codex_run_command")) {
        const QString command = args.value(QStringLiteral("command")).toString();
        return isLikelySafeShellCommand(command);
    }

    // Other codex tools are not auto-approvable
    return false;
}

void CodexAgent::continueToolLoop()
{
    if (!m_client)
        return;
    m_client->chat(m_turnMessages, m_turnTools);
}

void CodexAgent::finishTurnWithContent(const QString &content)
{
    // Build operation summary from this turn
    QVariantMap summary;
    QVariantList toolsExecuted;

    // Count tool executions from messages
    for (int i = 0; i < m_turnMessages.size(); ++i) {
        const auto &msg = m_turnMessages[i];
        if (msg.role == QStringLiteral("assistant") && !msg.toolCalls.isEmpty()) {
            for (const auto &toolCall : msg.toolCalls) {
                const QVariantMap callMap = toolCall.toMap();
                const QString toolName = callMap.value(QStringLiteral("function")).toMap().value(QStringLiteral("name")).toString();
                if (!toolName.isEmpty()) {
                    toolsExecuted.append(QVariantMap{
                        {QStringLiteral("name"), toolName},
                        {QStringLiteral("success"), true}
                    });
                }
            }
        }
    }

    if (!toolsExecuted.isEmpty()) {
        summary.insert(QStringLiteral("toolsExecuted"), toolsExecuted);
        summary.insert(QStringLiteral("toolCount"), toolsExecuted.size());
        emit operationSummaryReady(summary);
    }

    QString assistantContent = content;
    if (m_turnNeedsApproval) {
        const QVariantMap payload{
            {QStringLiteral("tool"), m_turnApprovalTool},
            {QStringLiteral("reason"), m_turnApprovalReason},
            {QStringLiteral("args"), m_turnApprovalArgs}
        };
        const QString payloadJson = QString::fromUtf8(
            QJsonDocument::fromVariant(payload).toJson(QJsonDocument::Compact));
        assistantContent.prepend(QStringLiteral("[approval_required_json] %1\n")
            .arg(payloadJson));
    }

    m_context.append({QStringLiteral("assistant"), assistantContent});
    trimContext();
    m_turnMessages.clear();
    m_turnTools.clear();
    m_remainingToolIterations = 0;
    m_turnPermissionMode.clear();
    m_turnNeedsApproval = false;
    m_turnApprovalTool.clear();
    m_turnApprovalReason.clear();
    m_turnApprovalArgs.clear();
    const QString finalContent = m_visibleTranscript.isEmpty()
        ? assistantContent
        : (assistantContent + QStringLiteral("\n") + m_visibleTranscript);
    m_visibleTranscript.clear();
    m_busy = false;
    setStatus(Status::Idle);
    emit responseFinished(finalContent);
}

QString CodexAgent::buildSystemPrompt(const QVariantMap &options) const
{
    const QString effort = options.value(QStringLiteral("effort"), QStringLiteral("Medium")).toString();
    const QString permissions = options.value(QStringLiteral("permissions"), QStringLiteral("Default permissions")).toString();
    const bool ideContext = options.value(QStringLiteral("ideContext"), true).toBool();
    const bool workLocally = options.value(QStringLiteral("workLocally"), true).toBool();

    QString contextInfo = QStringLiteral("Workspace root: %1\n")
        .arg(m_workspaceRoot.isEmpty() ? QStringLiteral("(unknown)") : m_workspaceRoot);

    if (m_workspaceContext && ideContext) {
        const QVariantMap ctx = m_workspaceContext->contextMap();
        contextInfo += QStringLiteral(
            "Project: %1\n"
            "Files in project: %2 total\n"
            "  - %3 .cpp, %4 .h, %5 .qml files\n"
            "Current file: %6\n")
            .arg(ctx.value(QStringLiteral("projectName")).toString(),
                 ctx.value(QStringLiteral("fileCount")).toString(),
                 ctx.value(QStringLiteral("cppFileCount")).toString(),
                 ctx.value(QStringLiteral("headerFileCount")).toString(),
                 ctx.value(QStringLiteral("qmlFileCount")).toString(),
                 ctx.value(QStringLiteral("currentFile")).toString().isEmpty()
                     ? QStringLiteral("(none)")
                     : ctx.value(QStringLiteral("currentFile")).toString());
    }

    return QStringLiteral(
        "You are the NeurX Codex agent, a senior coding assistant embedded in a local IDE.\n"
        "Act like OpenAI Codex / GitHub Copilot Agent: inspect the given context, "
        "propose practical implementation steps, and produce concrete code-oriented answers.\n"
        "Prefer minimal, high-confidence edits over broad rewrites.\n"
        "Respect the current permission mode. Do not claim that files or terminal commands "
        "were changed unless a tool explicitly performed that action.\n"
        "When asked to implement, inspect files with codex_read_file/codex_search_workspace. "
        "Use codex_find_symbol/codex_find_references for symbol-level navigation before editing. "
        "Create new files with codex_create_file and perform exact text edits with codex_edit_file. "
        "In Default permissions, creating a brand-new file can proceed directly, but overwriting an existing file must be approved first. "
        "If the change is broad or uncertain, prefer codex_propose_patch for user approval first.\n"
        "After proposing or applying edits, use codex_run_command for safe validation commands "
        "such as git status, git diff, rg, ctest, or cmake --build when permission mode allows it.\n"
        "When the request is ambiguous, state assumptions briefly before solution details.\n"
        "Default response structure for coding tasks:\n"
        "1) Approach\n"
        "2) Proposed edits\n"
        "3) Validation steps\n"
        "Keep responses concise and actionable.\n\n"
        "%1"
        "Reasoning effort: %2\n"
        "Permission mode: %3\n"
        "IDE context enabled: %4\n"
        "Execution location: %5")
        .arg(contextInfo,
             effort,
             permissions,
             ideContext ? QStringLiteral("yes") : QStringLiteral("no"),
             workLocally ? QStringLiteral("local") : QStringLiteral("cloud"));
}

QString CodexAgent::buildUserPrompt(const QString &text, const QVariantMap &options) const
{
    const bool ideContext = options.value(QStringLiteral("ideContext"), true).toBool();
    const QString effort = options.value(QStringLiteral("effort"), QStringLiteral("Medium")).toString();
    const QString permissions = options.value(QStringLiteral("permissions"), QStringLiteral("Default permissions")).toString();
    const bool workLocally = options.value(QStringLiteral("workLocally"), true).toBool();
    QString prompt;

    prompt += QStringLiteral("<agent_options>\n");
    prompt += QStringLiteral("effort=%1\n").arg(effort);
    prompt += QStringLiteral("permissions=%1\n").arg(permissions);
    prompt += QStringLiteral("execution_location=%1\n").arg(workLocally ? QStringLiteral("local") : QStringLiteral("cloud"));
    prompt += QStringLiteral("</agent_options>\n\n");

    if (ideContext) {
        prompt += QStringLiteral("<ide_context>\n");
        if (!m_currentFilePath.isEmpty()) {
            prompt += QStringLiteral("Current file: %1\n").arg(m_currentFilePath);
            prompt += QStringLiteral("Current file preview:\n```%1\n%2\n```\n")
                .arg(QFileInfo(m_currentFilePath).suffix(), clippedFileContext());
        } else {
            prompt += QStringLiteral("Current file: (none)\n");
        }
        prompt += QStringLiteral("</ide_context>\n\n");
    }

    prompt += text;
    return prompt;
}

QString CodexAgent::clippedFileContext() const
{
    if (m_currentFileContent.size() <= kMaxCurrentFileChars)
        return m_currentFileContent;

    return m_currentFileContent.left(kMaxCurrentFileChars)
        + QStringLiteral("\n\n... [current file clipped]");
}

void CodexAgent::trimContext()
{
    while (m_context.size() > kMaxContextMessages)
        m_context.removeFirst();
}

QString CodexAgent::executeToolSafely(const QString &toolName, const QVariantMap &args)
{
    auto *tool = ToolRegistry::instance().findTool(toolName);
    if (!tool) {
        return QJsonDocument::fromVariant(QVariantMap{
            {QStringLiteral("error"), QStringLiteral("tool not found: ") + toolName}
        }).toJson(QJsonDocument::Compact);
    }

    try {
        const QVariantMap result = tool->execute(args);
        return QString::fromUtf8(QJsonDocument::fromVariant(result)
            .toJson(QJsonDocument::Compact));
    } catch (const std::exception &e) {
        return QJsonDocument::fromVariant(QVariantMap{
            {QStringLiteral("error"),
             QStringLiteral("tool exception: ") + QString::fromUtf8(e.what())}
        }).toJson(QJsonDocument::Compact);
    } catch (...) {
        return QJsonDocument::fromVariant(QVariantMap{
            {QStringLiteral("error"),
             QStringLiteral("tool crashed: unknown exception")}
        }).toJson(QJsonDocument::Compact);
    }
}

void CodexAgent::compressContextIfNeeded()
{
    // Estimate token count (rough: ~4 chars per token)
    const int estimatedTokens = estimateTokens(m_context);

    // If context is > 70% of max (e.g., 70% of 4096 = ~2867 tokens)
    constexpr int kContextLimit = 4096;
    constexpr int kCompressionThreshold = (kContextLimit * 70) / 100;

    if (estimatedTokens > kCompressionThreshold) {
        // Summarize older messages (keep system + last 5 exchanges)
        QList<LlmMessage> compressed;

        // Keep system prompt
        for (const auto &msg : m_context) {
            if (msg.role == QLatin1String("system")) {
                compressed.append(msg);
                break;
            }
        }

        // Keep last 5 user-assistant pairs
        int pairsKept = 0;
        for (int i = m_context.size() - 1; i >= 0 && pairsKept < 5; --i) {
            compressed.prepend(m_context.at(i));
            if (m_context.at(i).role == QLatin1String("user"))
                ++pairsKept;
        }

        // Add a summary message if we removed anything
        if (compressed.size() < m_context.size()) {
            compressed.insert(1, {
                QStringLiteral("system"),
                QStringLiteral("[context compressed: previous conversation history summarized]")
            });
        }

        m_context = compressed;
    }
}

int CodexAgent::estimateTokens(const QList<LlmMessage> &msgs) const
{
    int totalChars = 0;
    for (const auto &msg : msgs) {
        totalChars += msg.content.size();
        if (!msg.name.isEmpty())
            totalChars += msg.name.size();
    }
    // Rough estimate: ~4 characters per token
    return (totalChars + 3) / 4;
}

} // namespace neurx
