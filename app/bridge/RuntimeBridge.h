#pragma once
#include <QObject>
#include <QString>
#include "runtime/AgentRuntime.h"

namespace neurx {

/// QML-facing bridge that exposes AgentRuntime to the UI layer.
class RuntimeBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool  running  READ isRunning  NOTIFY runningChanged)
    Q_PROPERTY(int   agentCount READ agentCount NOTIFY agentCountChanged)

public:
    explicit RuntimeBridge(AgentRuntime *runtime, QObject *parent = nullptr);

    bool isRunning()  const;
    int  agentCount() const;

public slots:
    Q_INVOKABLE void startRuntime();
    Q_INVOKABLE void shutdownRuntime();

signals:
    void runningChanged(bool running);
    void agentCountChanged(int count);

private:
    AgentRuntime *m_runtime;
};

} // namespace neurx
