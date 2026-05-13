#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>
#include "runtime/AgentRuntime.h"
#include "llm/LlmClient.h"

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
    Q_PROPERTY(QString chatApiKey  READ chatApiKey  WRITE setChatApiKey  NOTIFY chatApiKeyChanged)
    Q_PROPERTY(QString chatEndpoint READ chatEndpoint WRITE setChatEndpoint NOTIFY chatEndpointChanged)

public:
    explicit RuntimeBridge(AgentRuntime *runtime, QObject *parent = nullptr);

    bool isRunning()  const;
    int  agentCount() const;
    QVariantList localModels() const;
    QString localModelStatus() const;
    QString localModelSource() const;
    QString chatApiKey() const;
    void    setChatApiKey(const QString &key);
    QString chatEndpoint() const;
    void    setChatEndpoint(const QString &ep);

public slots:
    Q_INVOKABLE void startRuntime();
    Q_INVOKABLE void shutdownRuntime();
    Q_INVOKABLE void refreshLocalModels();
    Q_INVOKABLE void sendChatMessage(const QString &modelId, const QString &text);
    Q_INVOKABLE void clearChatContext();
    Q_INVOKABLE QString readLocalFile(const QString &filePath);
    Q_INVOKABLE QString lastOpenedEditorFile() const;
    Q_INVOKABLE void rememberOpenedEditorFile(const QString &filePath);
    Q_INVOKABLE void copyToClipboard(const QString &text);
    Q_INVOKABLE void revealInFinder(const QString &filePath);

signals:
    void runningChanged(bool running);
    void agentCountChanged(int count);
    void localModelsChanged();
    void chatApiKeyChanged();
    void chatEndpointChanged();
    void chatChunkReceived(const QString &delta);
    void chatStreamFinished(const QString &fullContent);
    void chatErrorOccurred(const QString &error);

private:
    QString resolveOllamaExecutable() const;
    QVariantList detectOllamaModels() const;
    QString resolveEndpointForModel(const QString &modelId) const;

    AgentRuntime     *m_runtime;
    QVariantList      m_localModels;
    QString           m_localModelStatus;
    QString           m_localModelSource;
    QString           m_chatApiKey;
    QString           m_chatEndpoint;
    LlmClient        *m_chatClient{nullptr};
    QList<LlmMessage> m_chatContext;
};

} // namespace neurx
