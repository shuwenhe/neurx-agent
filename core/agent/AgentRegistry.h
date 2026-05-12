#pragma once
#include "Agent.h"
#include <QObject>
#include <QHash>
#include <QSharedPointer>

namespace neurx {

/// Central registry that owns all agents in the system.
class AgentRegistry : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    static AgentRegistry &instance();

    /// Register an agent. The registry takes ownership.
    void registerAgent(Agent *agent);

    /// Unregister and destroy an agent by id.
    void unregisterAgent(const QString &agentId);

    Agent *findAgent(const QString &agentId) const;

    QList<Agent *> allAgents() const;

    int count() const { return m_agents.size(); }

signals:
    void agentRegistered(const QString &agentId);
    void agentUnregistered(const QString &agentId);
    void countChanged(int count);

private:
    explicit AgentRegistry(QObject *parent = nullptr);
    QHash<QString, Agent *> m_agents;
};

} // namespace neurx
