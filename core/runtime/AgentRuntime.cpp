#include "AgentRuntime.h"
#include "../log/Logger.h"

namespace neurx {

AgentRuntime &AgentRuntime::instance()
{
    static AgentRuntime s_instance;
    return s_instance;
}

AgentRuntime::AgentRuntime(QObject *parent) : QObject(parent) {}

void AgentRuntime::start()
{
    if (m_running) return;
    Logger::instance().info("AgentRuntime", "Starting NeurX AgentOS runtime…");
    m_running = true;
    emit runningChanged(true);
    emit started();
    eventBus().publish("runtime.started");
}

void AgentRuntime::shutdown()
{
    if (!m_running) return;
    Logger::instance().info("AgentRuntime", "Shutting down NeurX AgentOS runtime…");
    emit shuttingDown();
    eventBus().publish("runtime.shutdown");

    // Stop all agents gracefully
    for (auto *agent : registry().allAgents())
        agent->stop();

    m_running = false;
    emit runningChanged(false);
}

} // namespace neurx
