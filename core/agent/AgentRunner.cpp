#include "AgentRunner.h"

namespace neurx {

AgentRunner::AgentRunner(Agent *agent, QObject *parent)
    : QObject(parent)
    , m_agent(agent)
    , m_thread(new QThread(this))
{
    m_agent->moveToThread(m_thread);
    connect(m_thread, &QThread::started, m_agent, &Agent::start);
    connect(m_thread, &QThread::finished, m_agent, &Agent::stop);
}

AgentRunner::~AgentRunner()
{
    stop();
}

void AgentRunner::start()
{
    if (!m_thread->isRunning())
        m_thread->start();
}

void AgentRunner::stop()
{
    if (m_thread->isRunning()) {
        m_agent->stop();
        m_thread->quit();
        m_thread->wait(3000);
    }
}

} // namespace neurx
