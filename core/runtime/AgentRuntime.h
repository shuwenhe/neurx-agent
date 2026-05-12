#pragma once
#include <QObject>
#include "agent/AgentRegistry.h"
#include "runtime/TaskScheduler.h"
#include "runtime/EventBus.h"

namespace neurx {

/// Top-level runtime that bootstraps and manages all AgentOS subsystems.
class AgentRuntime : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)

public:
    static AgentRuntime &instance();

    bool isRunning() const { return m_running; }

    AgentRegistry  &registry()  { return AgentRegistry::instance();  }
    TaskScheduler  &scheduler() { return TaskScheduler::instance();  }
    EventBus       &eventBus()  { return EventBus::instance();       }

public slots:
    void start();
    void shutdown();

signals:
    void runningChanged(bool running);
    void started();
    void shuttingDown();

private:
    explicit AgentRuntime(QObject *parent = nullptr);
    bool m_running{false};
};

} // namespace neurx
