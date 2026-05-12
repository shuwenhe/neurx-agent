#include "ReActAgent.h"
#include "../../core/tools/ToolRegistry.h"
#include "../../core/log/Logger.h"
#include <QJsonDocument>

namespace neurx {

static const char *SYSTEM_PROMPT = R"(
You are NeurX, an intelligent AI agent running inside the NeurX AgentOS.
You have access to tools. Use them to fulfill the user's request.
Always think step-by-step. When you have the final answer, respond with plain text without a tool call.
)";

ReActAgent::ReActAgent(const QString &name, QObject *parent)
    : Agent(name, parent)
{}

void ReActAgent::setLlmConfig(const LlmConfig &cfg)
{
    delete m_llm;
    m_llm = new LlmClient(cfg, this);
    connect(m_llm, &LlmClient::responseReceived, this,
        &ReActAgent::handleLlmResponse);
    connect(m_llm, &LlmClient::errorOccurred, this, [this](const QString &err) {
        emitLog("error", "LLM error: " + err);
        setStatus(Status::Idle);
    });
}

void ReActAgent::receiveMessage(const QVariantMap &message)
{
    const QString query = message.value("content").toString();
    if (query.isEmpty()) return;

    m_history.clear();
    m_history.append({.role="system", .content=SYSTEM_PROMPT});
    m_history.append({.role="user",   .content=query});

    runReActLoop(query);
}

void ReActAgent::runReActLoop(const QString &)
{
    if (!m_llm) {
        emitLog("warning", "No LLM configured for ReActAgent. Set API key in Settings.");
        return;
    }
    setStatus(Status::Running);
    const auto schemas = ToolRegistry::instance().toolSchemas();
    m_llm->chat(m_history, schemas);
}

void ReActAgent::handleLlmResponse(const QString &content,
                                   const QVariantMap &toolCall)
{
    if (!toolCall.isEmpty()) {
        // Execute the requested tool
        const QString toolName = toolCall.value("name").toString();
        auto *tool = ToolRegistry::instance().findTool(toolName);

        QVariantMap toolResult;
        if (tool) {
            const auto argsStr = toolCall.value("args").toString();
            const auto args    = QJsonDocument::fromJson(argsStr.toUtf8())
                                     .toVariant().toMap();
            toolResult = tool->execute(args);
            emitLog("info", QStringLiteral("Tool [%1] called").arg(toolName));
        } else {
            toolResult = {{"error", "tool not found: " + toolName}};
            emitLog("warning", "Tool not found: " + toolName);
        }

        // Feed observation back
        m_history.append({.role="assistant", .content=content});
        m_history.append({.role="tool",
                          .content=QString::fromUtf8(
                              QJsonDocument::fromVariant(toolResult)
                                  .toJson(QJsonDocument::Compact))});

        // Continue loop
        if (m_history.size() < m_maxIterations * 2 + 2) {
            const auto schemas = ToolRegistry::instance().toolSchemas();
            m_llm->chat(m_history, schemas);
        } else {
            emitLog("warning", "ReAct max iterations reached");
            setStatus(Status::Idle);
        }
    } else {
        // Final answer
        m_history.append({.role="assistant", .content=content});
        emit messageSent("", {{"role","assistant"}, {"content", content}});
        setStatus(Status::Idle);
    }
}

} // namespace neurx
