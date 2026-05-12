#include "TaskScheduler.h"
#include <QtConcurrent>
#include <algorithm>

namespace neurx {

TaskScheduler &TaskScheduler::instance()
{
    static TaskScheduler s_instance;
    return s_instance;
}

TaskScheduler::TaskScheduler(QObject *parent) : QObject(parent) {}

void TaskScheduler::registerHandler(const QString &taskName, TaskHandler handler)
{
    m_handlers.insert(taskName, std::move(handler));
}

void TaskScheduler::enqueue(Task task)
{
    m_queue.append(std::move(task));
    // Sort by priority descending
    std::stable_sort(m_queue.begin(), m_queue.end(),
        [](const Task &a, const Task &b) {
            return static_cast<int>(a.priority) > static_cast<int>(b.priority);
        });
    emit taskEnqueued(m_queue.back().id);
    emit pendingCountChanged(m_queue.size());
    processNext();
}

void TaskScheduler::cancel(const QString &taskId)
{
    m_queue.removeIf([&](const Task &t) {
        return t.id == taskId && t.status == Task::Status::Pending;
    });
    emit pendingCountChanged(m_queue.size());
}

int TaskScheduler::pendingCount() const
{
    return static_cast<int>(std::count_if(m_queue.begin(), m_queue.end(),
        [](const Task &t){ return t.status == Task::Status::Pending; }));
}

void TaskScheduler::processNext()
{
    if (m_running || m_queue.isEmpty()) return;

    auto it = std::find_if(m_queue.begin(), m_queue.end(),
        [](const Task &t){ return t.status == Task::Status::Pending; });
    if (it == m_queue.end()) return;

    Task &task = *it;
    auto handler = m_handlers.value(task.name);
    if (!handler) {
        task.status = Task::Status::Failed;
        emit taskFailed(task.id, QStringLiteral("No handler for task: ") + task.name);
        return;
    }

    task.status    = Task::Status::Running;
    task.startedAt = QDateTime::currentDateTimeUtc();
    m_running = true;
    emit taskStarted(task.id);

    const QString taskId = task.id;
    Task taskCopy = task;

    QtConcurrent::run([this, taskCopy, handler]() mutable {
        handler(taskCopy);
        QMetaObject::invokeMethod(this, [this, taskCopy]() {
            taskCopy.status     == Task::Status::Failed
                ? emit taskFailed(taskCopy.id, taskCopy.params.value("error").toString())
                : emit taskCompleted(taskCopy.id, taskCopy.params);
            m_queue.removeIf([&](const Task &t){ return t.id == taskCopy.id; });
            m_running = false;
            emit pendingCountChanged(m_queue.size());
            processNext();
        }, Qt::QueuedConnection);
    });
}

} // namespace neurx
