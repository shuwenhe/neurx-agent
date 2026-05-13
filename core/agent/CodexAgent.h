#pragma once

#include "Agent.h"
#include "llm/LlmClient.h"
#include "runtime/WorkspaceContext.h"

#include <QPointer>
#include <QString>
#include <QVariantMap>

namespace neurx {

class WorkspaceContext;

/// Codex-style coding assistant agent.
///
/// The first implementation focuses on the agent loop that makes Codex useful:
/// persistent chat context, workspace/editor context, model configuration, and
/// streaming responses. File writes and terminal execution remain explicit
/// future tool steps rather than hidden side effects.
class CodexAgent : public Agent
{
    Q_OBJECT

public:
    explicit CodexAgent(QObject *parent = nullptr);

    void setConfig(const LlmConfig &config);
    void setWorkspaceRoot(const QString &workspaceRoot);
    void setCurrentFile(const QString &filePath, const QString &content);

    void submitPrompt(const QString &text, const QVariantMap &options = {});
    void clearContext();
    void cancelTurn();

    bool isToolCallAutoApprovable(const QString &toolName,
                                  const QVariantMap &args) const;

public slots:
    void receiveMessage(const QVariantMap &message) override;

signals:
    void chunkReceived(const QString &delta);
    void responseFinished(const QString &content);
    void errorOccurred(const QString &error);
    void workspaceFileChanged(const QString &filePath);
    void toolExecutionStarted(const QString &toolName, const QString &args, int iteration, int maxIterations);
    void toolExecutionFinished(const QString &toolName, bool success);
    void operationSummaryReady(const QVariantMap &summary);
    void turnCancelled();

private:
    QString buildSystemPrompt(const QVariantMap &options) const;
    QString buildUserPrompt(const QString &text, const QVariantMap &options) const;
    QVariantList availableTools(const QVariantMap &options) const;
    QString normalizedPermissionMode(const QVariantMap &options) const;
    bool isToolCallAllowed(const QString &toolName,
                           const QVariantMap &args,
                           const QString &permissionMode,
                           QString *reason = nullptr) const;
    void continueToolLoop();
    void finishTurnWithContent(const QString &content);
    QString clippedFileContext() const;
    void trimContext();

    // Error recovery and diagnostics
    QString executeToolSafely(const QString &toolName, const QVariantMap &args);
    void compressContextIfNeeded();
    int estimateTokens(const QList<LlmMessage> &msgs) const;

    LlmConfig       m_config;
    QPointer<LlmClient> m_client;
    QList<LlmMessage>   m_context;
    QList<LlmMessage>   m_turnMessages;
    QVariantList        m_turnTools;
    int                 m_remainingToolIterations{0};
    QString             m_turnPermissionMode;
    QString             m_visibleTranscript;
    bool                m_turnNeedsApproval{false};
    QString             m_turnApprovalTool;
    QString             m_turnApprovalReason;
    QVariantMap         m_turnApprovalArgs;
    QString         m_workspaceRoot;
    QString         m_currentFilePath;
    QString         m_currentFileContent;
    bool            m_busy{false};
    bool            m_turnCancelled{false};  // Flag to distinguish user cancellation from errors
    QPointer<WorkspaceContext> m_workspaceContext;
};

} // namespace neurx
