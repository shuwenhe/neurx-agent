#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>
#include "agent/CodexAgent.h"
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
    Q_PROPERTY(QString chatModel    READ chatModel    WRITE setChatModel    NOTIFY chatModelChanged)

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
    QString chatModel() const;
    void    setChatModel(const QString &model);

public slots:
    Q_INVOKABLE void startRuntime();
    Q_INVOKABLE void shutdownRuntime();
    Q_INVOKABLE void refreshLocalModels();
    Q_INVOKABLE void sendChatMessage(const QString &modelId,
                                     const QString &text,
                                     const QString &currentFilePath = {},
                                     const QString &currentFileContent = {},
                                     const QString &effort = QStringLiteral("Medium"),
                                     const QString &permissions = QStringLiteral("Default permissions"),
                                     bool ideContext = true,
                                     bool workLocally = true);
    Q_INVOKABLE void clearChatContext();
    Q_INVOKABLE void cancelChat();
    Q_INVOKABLE QString readLocalFile(const QString &filePath);
    Q_INVOKABLE QString readRemoteUrl(const QString &url);
    Q_INVOKABLE QVariantList addressHistory() const;
    Q_INVOKABLE void setAddressHistory(const QVariantList &entries);
    Q_INVOKABLE void clearAddressHistory();
    Q_INVOKABLE QVariantList pinnedAddressHistory() const;
    Q_INVOKABLE void setPinnedAddressHistory(const QVariantList &entries);
    Q_INVOKABLE QString lastOpenedEditorFile() const;
    Q_INVOKABLE void rememberOpenedEditorFile(const QString &filePath);
    Q_INVOKABLE void copyToClipboard(const QString &text);
    Q_INVOKABLE void revealInFinder(const QString &filePath);
    Q_INVOKABLE QVariantMap applyApprovedPatch(const QString &filePath, const QString &patch);
    Q_INVOKABLE QVariantMap runApprovedCommand(const QString &command, const QString &reason = {});
    Q_INVOKABLE bool isToolAutoApprovable(const QString &toolName, const QVariantMap &args);
    Q_INVOKABLE QVariantList listWorkspaceFiles(const QString &filter = {});

signals:
    void runningChanged(bool running);
    void agentCountChanged(int count);
    void localModelsChanged();
    void chatApiKeyChanged();
    void chatEndpointChanged();
    void chatModelChanged();
    void chatChunkReceived(const QString &delta);
    void chatStreamFinished(const QString &fullContent);
    void chatErrorOccurred(const QString &error);
    void fileChanged(const QString &filePath);
    void toolExecutionStarted(const QString &toolName, const QString &args, int iteration, int maxIterations);
    void toolExecutionFinished(const QString &toolName, bool success);
    void operationSummaryReady(const QVariantMap &summary);
    void chatCancelled();

private:
    QString resolveOllamaExecutable() const;
    QVariantList detectOllamaModels() const;
    QString resolveEndpointForModel(const QString &modelId) const;
    LlmConfig buildLlmConfig(const QString &modelId) const;

    AgentRuntime     *m_runtime;
    CodexAgent       *m_codexAgent{nullptr};
    QVariantList      m_localModels;
    QString           m_localModelStatus;
    QString           m_localModelSource;
    QString           m_chatApiKey;
    QString           m_chatEndpoint;
    QString           m_chatModel;
};

} // namespace neurx
