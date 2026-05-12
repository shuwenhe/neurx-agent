#pragma once
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QDateTime>
#include <QUuid>
#include <functional>

namespace neurx {

struct Task
{
    enum class Priority { Low = 0, Normal = 1, High = 2, Critical = 3 };
    enum class Status   { Pending, Running, Completed, Failed, Cancelled };

    QString   id;
    QString   name;
    Priority  priority{Priority::Normal};
    Status    status{Status::Pending};
    QVariantMap params;
    QDateTime   createdAt;
    QDateTime   startedAt;
    QDateTime   finishedAt;

    static Task create(const QString &name,
                       const QVariantMap &params = {},
                       Priority priority = Priority::Normal)
    {
        return Task{
            QUuid::createUuid().toString(QUuid::WithoutBraces),
            name, priority, Status::Pending, params,
            QDateTime::currentDateTimeUtc()
        };
    }
};

/// Schedules and dispatches Tasks to registered agent workers.
class TaskScheduler : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY pendingCountChanged)

public:
    using TaskHandler = std::function<void(Task &)>;

    static TaskScheduler &instance();

    void enqueue(Task task);
    void cancel(const QString &taskId);
    void registerHandler(const QString &taskName, TaskHandler handler);

    int pendingCount() const;

signals:
    void taskEnqueued(const QString &taskId);
    void taskStarted(const QString &taskId);
    void taskCompleted(const QString &taskId, const QVariantMap &result);
    void taskFailed(const QString &taskId, const QString &error);
    void pendingCountChanged(int count);

private:
    explicit TaskScheduler(QObject *parent = nullptr);
    void processNext();

    QList<Task>                    m_queue;
    QHash<QString, TaskHandler>    m_handlers;
    bool                           m_running{false};
};

} // namespace neurx
