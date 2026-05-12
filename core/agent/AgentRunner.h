#pragma once
#include <QObject>
#include <QThread>
#include "Agent.h"

namespace neurx {

/// Runs an agent on a dedicated QThread.
class AgentRunner : public QObject
{
    Q_OBJECT
public:
    explicit AgentRunner(Agent *agent, QObject *parent = nullptr);
    ~AgentRunner() override;

    void start();
    void stop();

    Agent *agent() const { return m_agent; }

private:
    Agent   *m_agent;
    QThread *m_thread;
};

} // namespace neurx
