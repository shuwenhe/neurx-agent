#pragma once

#include "Agent.h"
#include "llm/LlmClient.h"

#include <QPointer>
#include <QString>
#include <QVariantMap>

namespace neurx {

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

public slots:
    void receiveMessage(const QVariantMap &message) override;

signals:
    void chunkReceived(const QString &delta);
    void responseFinished(const QString &content);
    void errorOccurred(const QString &error);

private:
    QString buildSystemPrompt(const QVariantMap &options) const;
    QString buildUserPrompt(const QString &text, const QVariantMap &options) const;
    QString clippedFileContext() const;
    void trimContext();

    LlmConfig       m_config;
    QPointer<LlmClient> m_client;
    QList<LlmMessage>   m_context;
    QString         m_workspaceRoot;
    QString         m_currentFilePath;
    QString         m_currentFileContent;
    bool            m_busy{false};
};

} // namespace neurx
