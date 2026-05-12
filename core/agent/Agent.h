#pragma once
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QUuid>

namespace neurx {

/// Base class for all agents in the NeurX AgentOS.
/// Subclass this and override onMessage() / onTick() to implement agent logic.
class Agent : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString agentId   READ agentId   CONSTANT)
    Q_PROPERTY(QString name      READ name      WRITE setName  NOTIFY nameChanged)
    Q_PROPERTY(Status  status    READ status    NOTIFY statusChanged)

public:
    enum class Status {
        Idle,
        Running,
        Paused,
        Error,
        Stopped
    };
    Q_ENUM(Status)

    explicit Agent(const QString &name, QObject *parent = nullptr);
    virtual ~Agent() = default;

    QString agentId() const { return m_id; }
    QString name()    const { return m_name; }
    Status  status()  const { return m_status; }

    void setName(const QString &name);

public slots:
    virtual void start();
    virtual void stop();
    virtual void pause();
    virtual void resume();

    /// Called by the runtime to deliver a message to this agent.
    virtual void receiveMessage(const QVariantMap &message) = 0;

signals:
    void nameChanged(const QString &name);
    void statusChanged(Status status);
    void messageSent(const QString &targetAgentId, const QVariantMap &message);
    void logEmitted(const QString &level, const QString &text);

protected:
    void setStatus(Status s);
    void sendMessage(const QString &targetAgentId, const QVariantMap &msg);
    void emitLog(const QString &level, const QString &text);

    QString   m_id;
    QString   m_name;
    Status    m_status{Status::Idle};
};

} // namespace neurx
