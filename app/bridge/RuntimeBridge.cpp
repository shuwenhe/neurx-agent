#include "RuntimeBridge.h"

namespace neurx {

RuntimeBridge::RuntimeBridge(AgentRuntime *runtime, QObject *parent)
    : QObject(parent), m_runtime(runtime)
{
    connect(m_runtime, &AgentRuntime::runningChanged,
            this, &RuntimeBridge::runningChanged);
    connect(&m_runtime->registry(), &AgentRegistry::countChanged,
            this, &RuntimeBridge::agentCountChanged);
}

bool RuntimeBridge::isRunning() const { return m_runtime->isRunning(); }

int RuntimeBridge::agentCount() const
{
    return m_runtime->registry().count();
}

void RuntimeBridge::startRuntime()    { m_runtime->start();    }
void RuntimeBridge::shutdownRuntime() { m_runtime->shutdown(); }

} // namespace neurx
