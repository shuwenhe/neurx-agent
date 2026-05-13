#include "LlmClient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <memory>

namespace neurx {

namespace {
constexpr int kRequestTimeoutMs = 120000;
}

LlmClient::LlmClient(const LlmConfig &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_nam(new QNetworkAccessManager(this))
{}

void LlmClient::setConfig(const LlmConfig &config)
{
    m_config = config;
}

QNetworkRequest LlmClient::buildRequest() const
{
    QNetworkRequest req(QUrl(m_config.endpoint));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization",
                     QStringLiteral("Bearer %1").arg(m_config.apiKey).toUtf8());
    return req;
}

QVariantMap LlmClient::buildRequestBody(const QList<LlmMessage> &messages,
                                        const QVariantList &tools,
                                        bool stream) const
{
    QVariantList msgs;
    for (const auto &m : messages) {
        QVariantMap msg{{"role", m.role}, {"content", m.content}};
        if (!m.name.isEmpty())
            msg["name"] = m.name;
        if (!m.toolCallId.isEmpty())
            msg["tool_call_id"] = m.toolCallId;
        if (!m.toolCalls.isEmpty())
            msg["tool_calls"] = m.toolCalls;
        msgs.append(msg);
    }

    QVariantMap body{
        {"model",       m_config.model},
        {"messages",    msgs},
        {"temperature", m_config.temperature},
        {"max_tokens",  m_config.maxTokens},
        {"stream",      stream}
    };
    if (!tools.isEmpty())
        body["tools"] = tools;
    return body;
}

void LlmClient::chat(const QList<LlmMessage> &messages,
                     const QVariantList &tools)
{
    const auto body = QJsonDocument::fromVariant(
                          buildRequestBody(messages, tools, false))
                          .toJson(QJsonDocument::Compact);

    auto *reply = m_nam->post(buildRequest(), body);
    m_activeReply = reply;
    // Shared flag so the timer and finished handler don't both emit.
    auto emitted = std::make_shared<bool>(false);

    // Abort and report error if the server doesn't respond within the timeout.
    auto *timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, reply, emitted]() {
        if (reply->isRunning()) {
            reply->abort();
            if (!*emitted) {
                *emitted = true;
                emit errorOccurred(QStringLiteral(
                    "Request timed out (120s). Check your model service, network, and API key."));
            }
        }
    });
    timer->start(kRequestTimeoutMs);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, messages, tools, emitted]() {
        if (m_activeReply == reply)
            m_activeReply = nullptr;
        const bool ignored = m_ignoredReplies.remove(reply) > 0;
        if (ignored || reply->error() == QNetworkReply::OperationCanceledError) {
            reply->deleteLater();
            return;
        }
        if (*emitted) {
            reply->deleteLater();
            return;
        }
        reply->deleteLater();
        *emitted = true;

        const QByteArray responseBody = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            // Try to extract the actual API error message from the body.
            const auto errDoc = QJsonDocument::fromJson(responseBody);
            const QString apiError = errDoc["error"]["message"].toString();

            // If the model doesn't support tools, auto-retry without them.
            if (!apiError.isEmpty()
                && apiError.contains(QLatin1String("does not support tools"))
                && !tools.isEmpty()) {
                *emitted = false; // allow the retry to emit
                chat(messages, {});
                return;
            }

            const QString msg = apiError.isEmpty() ? reply->errorString() : apiError;
            emit errorOccurred(msg);
            return;
        }

        const auto doc = QJsonDocument::fromJson(responseBody);
        // Handle error returned in a 200 body (some providers do this).
        const QString inlineError = doc["error"]["message"].toString();
        if (!inlineError.isEmpty()) {
            if (!inlineError.isEmpty()
                && inlineError.contains(QLatin1String("does not support tools"))
                && !tools.isEmpty()) {
                *emitted = false;
                chat(messages, {});
                return;
            }
            emit errorOccurred(inlineError);
            return;
        }

        const auto choices = doc["choices"].toArray();
        if (choices.isEmpty()) {
            emit errorOccurred(QStringLiteral("Empty response from server."));
            return;
        }

        const auto msg = choices[0]["message"].toObject();
        QString content = msg["content"].toString();
        QVariantMap toolCall;
        if (msg.contains("tool_calls")) {
            const auto tc = msg["tool_calls"].toArray()[0].toObject();
            const auto function = tc["function"].toObject();
            toolCall = {
                {"id",   tc["id"].toString()},
                {"name", function["name"].toString()},
                {"args", function["arguments"].toString()}
            };
        }
        emit responseReceived(content, toolCall);
    });
}

void LlmClient::abort()
{
    if (m_activeReply) {
        // Mark for ignored reads before aborting to prevent Qt warnings
        m_ignoredReplies.insert(m_activeReply);
        if (m_activeReply->isRunning())
            m_activeReply->abort();
    }
}

void LlmClient::chatStream(const QList<LlmMessage> &messages,
                            const QVariantList &tools)
{
    const auto body = QJsonDocument::fromVariant(
                          buildRequestBody(messages, tools, true))
                          .toJson(QJsonDocument::Compact);

    auto *reply = m_nam->post(buildRequest(), body);
    m_activeReply = reply;
    auto accumulated = std::make_shared<QString>();
    auto aborted = std::make_shared<bool>(false);

    connect(reply, &QNetworkReply::readyRead, this, [this, reply, accumulated]() {
        // Skip reads on replies that are being aborted
        if (m_ignoredReplies.contains(reply))
            return;

        const auto lines = reply->readAll().split('\n');
        for (const auto &line : lines) {
            if (!line.startsWith("data: ")) continue;
            const auto data = line.mid(6).trimmed();
            if (data == "[DONE]") continue;
            const auto doc = QJsonDocument::fromJson(data);
            const auto delta = doc["choices"][0]["delta"]["content"].toString();
            if (!delta.isEmpty()) {
                *accumulated += delta;
                emit chunkReceived(delta);
            }
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, accumulated, aborted]() {
        if (m_activeReply == reply)
            m_activeReply = nullptr;
        m_ignoredReplies.remove(reply);
        reply->deleteLater();
        if (reply->error() == QNetworkReply::OperationCanceledError || *aborted) {
            emit streamFinished(*accumulated); // emit what we have so far
        } else if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
        } else {
            emit streamFinished(*accumulated);
        }
    });
}

} // namespace neurx
