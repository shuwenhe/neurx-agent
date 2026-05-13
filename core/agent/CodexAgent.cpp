#include "CodexAgent.h"

#include <QFileInfo>

namespace neurx {

namespace {
constexpr int kMaxContextMessages = 24;
constexpr int kMaxCurrentFileChars = 12000;
}

CodexAgent::CodexAgent(QObject *parent)
    : Agent(QStringLiteral("Codex"), parent)
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
}

void CodexAgent::setCurrentFile(const QString &filePath, const QString &content)
{
    m_currentFilePath = filePath;
    m_currentFileContent = content;
}

void CodexAgent::clearContext()
{
    m_context.clear();
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
        connect(m_client, &LlmClient::chunkReceived,
                this, &CodexAgent::chunkReceived);
        connect(m_client, &LlmClient::streamFinished,
                this, [this](const QString &content) {
            m_context.append({QStringLiteral("assistant"), content});
            trimContext();
            m_busy = false;
            setStatus(Status::Idle);
            emit responseFinished(content);
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
    QList<LlmMessage> messages;
    messages.append({QStringLiteral("system"), buildSystemPrompt(options)});
    messages.append(m_context);
    messages.append({QStringLiteral("user"), userPrompt});

    m_context.append({QStringLiteral("user"), userPrompt});
    trimContext();

    m_busy = true;
    setStatus(Status::Running);
    m_client->chatStream(messages);
}

QString CodexAgent::buildSystemPrompt(const QVariantMap &options) const
{
    const QString effort = options.value(QStringLiteral("effort"), QStringLiteral("Medium")).toString();
    const QString permissions = options.value(QStringLiteral("permissions"), QStringLiteral("Default permissions")).toString();
    const bool ideContext = options.value(QStringLiteral("ideContext"), true).toBool();

    return QStringLiteral(
        "You are the NeurX Codex agent, a senior coding assistant embedded in a local IDE.\n"
        "Act like OpenAI Codex: inspect the given context, reason about implementation, "
        "explain concise plans when useful, and produce concrete code-oriented answers.\n"
        "Respect the current permission mode. Do not claim that files or terminal commands "
        "were changed unless a tool explicitly performed that action.\n"
        "When asked to implement, provide exact edits or patches that fit the existing codebase.\n\n"
        "Workspace root: %1\n"
        "Reasoning effort: %2\n"
        "Permission mode: %3\n"
        "IDE context enabled: %4")
        .arg(m_workspaceRoot.isEmpty() ? QStringLiteral("(unknown)") : m_workspaceRoot,
             effort,
             permissions,
             ideContext ? QStringLiteral("yes") : QStringLiteral("no"));
}

QString CodexAgent::buildUserPrompt(const QString &text, const QVariantMap &options) const
{
    const bool ideContext = options.value(QStringLiteral("ideContext"), true).toBool();
    QString prompt;

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

} // namespace neurx
