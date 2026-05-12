#include "Agent.h"
#include <QUuid>

namespace neurx {

Agent::Agent(const QString &name, QObject *parent)
    : QObject(parent)
    , m_id(QUuid::createUuid().toString(QUuid::WithoutBraces))
    , m_name(name)
{}

void Agent::setName(const QString &name)
{
    if (m_name == name) return;
    m_name = name;
    emit nameChanged(m_name);
}

void Agent::start()
{
    setStatus(Status::Running);
}

void Agent::stop()
{
    setStatus(Status::Stopped);
}

void Agent::pause()
{
    if (m_status == Status::Running)
        setStatus(Status::Paused);
}

void Agent::resume()
{
    if (m_status == Status::Paused)
        setStatus(Status::Running);
}

void Agent::setStatus(Status s)
{
    if (m_status == s) return;
    m_status = s;
    emit statusChanged(m_status);
}

void Agent::sendMessage(const QString &targetAgentId, const QVariantMap &msg)
{
    emit messageSent(targetAgentId, msg);
}

void Agent::emitLog(const QString &level, const QString &text)
{
    emit logEmitted(level, text);
}

} // namespace neurx
