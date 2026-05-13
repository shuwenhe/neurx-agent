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
                          buildRequestBody(messages, tools, true))
                          .toJson(QJsonDocument::Compact);

    auto *reply = m_nam->post(buildRequest(), body);
    m_activeReply = reply;
    auto accumulated = std::make_shared<QString>();
    auto streamBuffer = std::make_shared<QByteArray>();
    auto streamedToolId = std::make_shared<QString>();
    auto streamedToolName = std::make_shared<QString>();
    auto streamedToolArgs = std::make_shared<QString>();
    auto streamApiError = std::make_shared<QString>();
    auto emitted = std::make_shared<bool>(false);

    auto consumeDataChunk = [this, accumulated, streamedToolId, streamedToolName,
                             streamedToolArgs, streamApiError](const QByteArray &data) {
        const QByteArray trimmed = data.trimmed();
        if (trimmed.isEmpty() || trimmed == "[DONE]")
            return;

        const auto doc = QJsonDocument::fromJson(trimmed);
        if (doc.isNull() || !doc.isObject())
            return;

        const auto root = doc.object();
        const QString inlineError = root.value(QStringLiteral("error")).toObject()
            .value(QStringLiteral("message")).toString();
        if (!inlineError.isEmpty()) {
            *streamApiError = inlineError;
            return;
        }

        const auto choices = root.value(QStringLiteral("choices")).toArray();
        if (choices.isEmpty())
            return;

        const auto delta = choices.first().toObject().value(QStringLiteral("delta")).toObject();
        const QString contentDelta = delta.value(QStringLiteral("content")).toString();
        if (!contentDelta.isEmpty()) {
            *accumulated += contentDelta;
            emit chunkReceived(contentDelta);
        }

        const auto toolCalls = delta.value(QStringLiteral("tool_calls")).toArray();
        for (const auto &toolVar : toolCalls) {
            const auto toolObj = toolVar.toObject();
            const QString id = toolObj.value(QStringLiteral("id")).toString();
            if (!id.isEmpty())
                *streamedToolId = id;

            const auto functionObj = toolObj.value(QStringLiteral("function")).toObject();
            const QString name = functionObj.value(QStringLiteral("name")).toString();
            if (!name.isEmpty())
                *streamedToolName = name;

            const QString argsChunk = functionObj.value(QStringLiteral("arguments")).toString();
            if (!argsChunk.isEmpty())
                *streamedToolArgs += argsChunk;
        }
    };

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

    connect(reply, &QNetworkReply::readyRead, this,
            [this, reply, streamBuffer, consumeDataChunk]() {
        if (m_ignoredReplies.contains(reply) || !reply->isOpen())
            return;

        *streamBuffer += reply->readAll();
        while (true) {
            const int newline = streamBuffer->indexOf('\n');
            if (newline < 0)
                break;

            const QByteArray line = streamBuffer->left(newline).trimmed();
            streamBuffer->remove(0, newline + 1);
            if (!line.startsWith("data: "))
                continue;
            consumeDataChunk(line.mid(6));
        }
    });

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, messages, tools, emitted, streamBuffer,
             streamedToolId, streamedToolName, streamedToolArgs,
             streamApiError, consumeDataChunk, accumulated]() {
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

        if (!streamBuffer->isEmpty()) {
            const QByteArray tail = streamBuffer->trimmed();
            if (tail.startsWith("data: "))
                consumeDataChunk(tail.mid(6));
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

        // Handle stream-level errors returned in chunk payloads.
        const QString inlineError = *streamApiError;
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

        QVariantMap toolCall;
        if (!streamedToolName->isEmpty()) {
            toolCall = {
                {"id",   *streamedToolId},
                {"name", *streamedToolName},
                {"args", *streamedToolArgs}
            };
        }

        emit responseReceived(*accumulated, toolCall);
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
