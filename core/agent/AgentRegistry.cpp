#include "AgentRegistry.h"

namespace neurx {

AgentRegistry &AgentRegistry::instance()
{
    static AgentRegistry s_instance;
    return s_instance;
}

AgentRegistry::AgentRegistry(QObject *parent) : QObject(parent) {}

void AgentRegistry::registerAgent(Agent *agent)
{
    if (!agent || m_agents.contains(agent->agentId()))
        return;

    agent->setParent(this);
    m_agents.insert(agent->agentId(), agent);

    // Wire inter-agent messaging through the registry
    connect(agent, &Agent::messageSent, this,
        [this](const QString &targetId, const QVariantMap &msg) {
            if (auto *target = findAgent(targetId))
                target->receiveMessage(msg);
        });

    emit agentRegistered(agent->agentId());
    emit countChanged(m_agents.size());
}

void AgentRegistry::unregisterAgent(const QString &agentId)
{
    auto *agent = m_agents.take(agentId);
    if (!agent) return;
    agent->stop();
    agent->deleteLater();
    emit agentUnregistered(agentId);
    emit countChanged(m_agents.size());
}

Agent *AgentRegistry::findAgent(const QString &agentId) const
{
    return m_agents.value(agentId, nullptr);
}

QList<Agent *> AgentRegistry::allAgents() const
{
    return m_agents.values();
}

} // namespace neurx
