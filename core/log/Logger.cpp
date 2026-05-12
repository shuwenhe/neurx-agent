#include "Logger.h"
#include <QDebug>
#include <QMutexLocker>

namespace neurx {

namespace {
const char *levelStr(Logger::Level l)
{
    switch (l) {
    case Logger::Level::Debug:   return "DBG";
    case Logger::Level::Info:    return "INF";
    case Logger::Level::Warning: return "WRN";
    case Logger::Level::Error:   return "ERR";
    }
    return "???";
}
}

Logger &Logger::instance()
{
    static Logger s_instance;
    return s_instance;
}

Logger::Logger(QObject *parent) : QObject(parent) {}

void Logger::setLogFile(const QString &path)
{
    QMutexLocker lk(&m_mutex);
    if (m_file.isOpen()) m_file.close();
    m_file.setFileName(path);
    if (m_file.open(QIODevice::Append | QIODevice::Text))
        m_stream.setDevice(&m_file);
}

void Logger::setLevel(Level minLevel) { m_minLevel = minLevel; }

void Logger::debug  (const QString &tag, const QString &msg) { write(Level::Debug,   tag, msg); }
void Logger::info   (const QString &tag, const QString &msg) { write(Level::Info,    tag, msg); }
void Logger::warning(const QString &tag, const QString &msg) { write(Level::Warning, tag, msg); }
void Logger::error  (const QString &tag, const QString &msg) { write(Level::Error,   tag, msg); }

void Logger::write(Level level, const QString &tag, const QString &msg)
{
    if (static_cast<int>(level) < static_cast<int>(m_minLevel)) return;
    const auto ts  = QDateTime::currentDateTime();
    const auto line = QStringLiteral("[%1] [%2] [%3] %4")
                          .arg(ts.toString("yyyy-MM-dd hh:mm:ss.zzz"))
                          .arg(QLatin1String(levelStr(level)))
                          .arg(tag)
                          .arg(msg);
    {
        QMutexLocker lk(&m_mutex);
        qDebug().noquote() << line;
        if (m_file.isOpen()) {
            m_stream << line << '\n';
            m_stream.flush();
        }
    }
    emit logEntry(level, tag, msg, ts);
}

} // namespace neurx
