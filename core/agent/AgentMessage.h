#pragma once
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QDateTime>
#include <QUuid>

namespace neurx {

/// Immutable message envelope passed between agents.
struct AgentMessage
{
    QString     id;
    QString     fromAgentId;
    QString     toAgentId;   // empty = broadcast
    QString     type;        // e.g. "task.request", "task.result", "event"
    QVariantMap payload;
    QDateTime   timestamp;

    static AgentMessage create(const QString &from,
                               const QString &to,
                               const QString &type,
                               const QVariantMap &payload = {})
    {
        return AgentMessage{
            QUuid::createUuid().toString(QUuid::WithoutBraces),
            from, to, type, payload,
            QDateTime::currentDateTimeUtc()
        };
    }

    QVariantMap toVariantMap() const;
    static AgentMessage fromVariantMap(const QVariantMap &map);
};

} // namespace neurx
