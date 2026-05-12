#include "AgentMessage.h"

namespace neurx {

QVariantMap AgentMessage::toVariantMap() const
{
    return {
        {"id",          id},
        {"from",        fromAgentId},
        {"to",          toAgentId},
        {"type",        type},
        {"payload",     payload},
        {"timestamp",   timestamp.toString(Qt::ISODateWithMs)}
    };
}

AgentMessage AgentMessage::fromVariantMap(const QVariantMap &map)
{
    AgentMessage m;
    m.id          = map.value("id").toString();
    m.fromAgentId = map.value("from").toString();
    m.toAgentId   = map.value("to").toString();
    m.type        = map.value("type").toString();
    m.payload     = map.value("payload").toMap();
    m.timestamp   = QDateTime::fromString(
                        map.value("timestamp").toString(),
                        Qt::ISODateWithMs);
    return m;
}

} // namespace neurx
