#include "LogModel.h"
#include "log/Logger.h"

namespace neurx {

LogModel::LogModel(QObject *parent) : QAbstractListModel(parent)
{
    // Automatically pipe Logger output into this model
    connect(&Logger::instance(), &Logger::logEntry, this,
        [this](Logger::Level level, const QString &tag,
               const QString &msg, const QDateTime &ts) {
            static const QHash<Logger::Level, QString> levelNames{
                {Logger::Level::Debug,   "debug"},
                {Logger::Level::Info,    "info"},
                {Logger::Level::Warning, "warning"},
                {Logger::Level::Error,   "error"},
            };
            append(levelNames.value(level, "info"), tag, msg, ts);
        });
}

int LogModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_entries.size();
}

QVariant LogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size()) return {};
    const auto &e = m_entries.at(index.row());
    switch (role) {
    case TimeRole:    return e.timestamp.toString("hh:mm:ss.zzz");
    case LevelRole:   return e.level;
    case TagRole:     return e.tag;
    case MessageRole: return e.message;
    default:          return {};
    }
}

QHash<int, QByteArray> LogModel::roleNames() const
{
    return {
        {TimeRole,    "time"},
        {LevelRole,   "level"},
        {TagRole,     "tag"},
        {MessageRole, "message"},
    };
}

void LogModel::append(const QString &level, const QString &tag,
                      const QString &message, const QDateTime &ts)
{
    const QDateTime now = ts.isValid() ? ts : QDateTime::currentDateTime();
    if (m_entries.size() >= m_maxLines) {
        beginRemoveRows({}, 0, 0);
        m_entries.removeFirst();
        endRemoveRows();
    }
    beginInsertRows({}, m_entries.size(), m_entries.size());
    m_entries.append({now, level, tag, message});
    endInsertRows();
}

void LogModel::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

} // namespace neurx
