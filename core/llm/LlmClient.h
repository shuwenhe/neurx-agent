#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <functional>

namespace neurx {

struct LlmConfig
{
    QString endpoint;       // e.g. "https://api.openai.com/v1/chat/completions"
    QString apiKey;
    QString model;          // e.g. "gpt-4o", "claude-3-5-sonnet", "qwen-max"
    double  temperature{0.7};
    int     maxTokens{4096};
};

struct LlmMessage
{
    QString role;    // "system" | "user" | "assistant" | "tool"
    QString content;
    QString name;
    QString toolCallId;
    QVariantList toolCalls;
};

/// Async LLM client supporting OpenAI-compatible APIs.
/// Supports streaming via the chunkReceived signal.
class LlmClient : public QObject
{
    Q_OBJECT
public:
    explicit LlmClient(const LlmConfig &config, QObject *parent = nullptr);

    void setConfig(const LlmConfig &config);
    const LlmConfig &config() const { return m_config; }

    /// Abort any in-flight request immediately.
    void abort();

    /// Send a chat completion request (non-streaming).
    void chat(const QList<LlmMessage> &messages,
              const QVariantList &tools = {});

    /// Send a streaming chat completion request.
    void chatStream(const QList<LlmMessage> &messages,
                    const QVariantList &tools = {});

signals:
    void responseReceived(const QString &content, const QVariantMap &toolCall);
    void chunkReceived(const QString &delta);
    void streamFinished(const QString &fullContent);
    void errorOccurred(const QString &error);

private:
    QVariantMap buildRequestBody(const QList<LlmMessage> &messages,
                                 const QVariantList &tools,
                                 bool stream) const;
    QNetworkRequest buildRequest() const;

    LlmConfig              m_config;
    QNetworkAccessManager *m_nam;
    QPointer<QNetworkReply> m_activeReply;
};

} // namespace neurx
