#pragma once
#include "../../core/agent/Agent.h"
#include "../../core/llm/LlmClient.h"

namespace neurx {

/// ReAct (Reasoning + Acting) agent.
/// Iterates: Think → Act (tool call) → Observe → until done.
class ReActAgent : public Agent
{
    Q_OBJECT
public:
    explicit ReActAgent(const QString &name, QObject *parent = nullptr);

    void setLlmConfig(const LlmConfig &cfg);
    void receiveMessage(const QVariantMap &message) override;

private:
    void runReActLoop(const QString &userQuery);
    void handleLlmResponse(const QString &content, const QVariantMap &toolCall);

    LlmClient           *m_llm{nullptr};
    QList<LlmMessage>    m_history;
    int                  m_maxIterations{10};
};

} // namespace neurx
