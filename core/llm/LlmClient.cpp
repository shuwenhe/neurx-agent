#include "LlmClient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QNetworkReply>

namespace neurx {

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
    for (const auto &m : messages)
        msgs.append(QVariantMap{{"role", m.role}, {"content", m.content}});

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
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        const auto choices = doc["choices"].toArray();
        if (choices.isEmpty()) { emit errorOccurred("Empty choices"); return; }

        const auto msg = choices[0]["message"].toObject();
        QString content = msg["content"].toString();
        QVariantMap toolCall;
        if (msg.contains("tool_calls")) {
            const auto tc = msg["tool_calls"].toArray()[0].toObject();
            toolCall = {
                {"id",   tc["id"].toString()},
                {"name", tc["function"]["name"].toString()},
                {"args", tc["function"]["arguments"].toString()}
            };
        }
        emit responseReceived(content, toolCall);
    });
}

void LlmClient::chatStream(const QList<LlmMessage> &messages,
                            const QVariantList &tools)
{
    const auto body = QJsonDocument::fromVariant(
                          buildRequestBody(messages, tools, true))
                          .toJson(QJsonDocument::Compact);

    auto *reply = m_nam->post(buildRequest(), body);
    QString accumulated;

    connect(reply, &QNetworkReply::readyRead, this, [this, reply, &accumulated]() {
        const auto lines = reply->readAll().split('\n');
        for (const auto &line : lines) {
            if (!line.startsWith("data: ")) continue;
            const auto data = line.mid(6).trimmed();
            if (data == "[DONE]") continue;
            const auto doc = QJsonDocument::fromJson(data);
            const auto delta = doc["choices"][0]["delta"]["content"].toString();
            if (!delta.isEmpty()) {
                accumulated += delta;
                emit chunkReceived(delta);
            }
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, accumulated]() mutable {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            emit errorOccurred(reply->errorString());
        else
            emit streamFinished(accumulated);
    });
}

} // namespace neurx
