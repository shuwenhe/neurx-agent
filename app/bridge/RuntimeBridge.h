#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>
#include "runtime/AgentRuntime.h"

namespace neurx {

/// QML-facing bridge that exposes AgentRuntime to the UI layer.
class RuntimeBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool  running  READ isRunning  NOTIFY runningChanged)
    Q_PROPERTY(int   agentCount READ agentCount NOTIFY agentCountChanged)
    Q_PROPERTY(QVariantList localModels READ localModels NOTIFY localModelsChanged)
    Q_PROPERTY(QString localModelStatus READ localModelStatus NOTIFY localModelsChanged)
    Q_PROPERTY(QString localModelSource READ localModelSource NOTIFY localModelsChanged)

public:
    explicit RuntimeBridge(AgentRuntime *runtime, QObject *parent = nullptr);

    bool isRunning()  const;
    int  agentCount() const;
    QVariantList localModels() const;
    QString localModelStatus() const;
    QString localModelSource() const;

public slots:
    Q_INVOKABLE void startRuntime();
    Q_INVOKABLE void shutdownRuntime();
    Q_INVOKABLE void refreshLocalModels();

signals:
    void runningChanged(bool running);
    void agentCountChanged(int count);
    void localModelsChanged();

private:
    QString resolveOllamaExecutable() const;
    QVariantList detectOllamaModels() const;

    AgentRuntime *m_runtime;
    QVariantList  m_localModels;
    QString       m_localModelStatus;
    QString       m_localModelSource;
};

} // namespace neurx
