#pragma once
#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QDateTime>

namespace neurx {

class Logger : public QObject
{
    Q_OBJECT
public:
    enum class Level { Debug, Info, Warning, Error };
    Q_ENUM(Level)

    static Logger &instance();

    void setLogFile(const QString &path);
    void setLevel(Level minLevel);

    void debug  (const QString &tag, const QString &msg);
    void info   (const QString &tag, const QString &msg);
    void warning(const QString &tag, const QString &msg);
    void error  (const QString &tag, const QString &msg);

signals:
    void logEntry(Level level, const QString &tag,
                  const QString &msg, const QDateTime &ts);

private:
    explicit Logger(QObject *parent = nullptr);
    void write(Level level, const QString &tag, const QString &msg);

    Level        m_minLevel{Level::Debug};
    QFile        m_file;
    QTextStream  m_stream;
    QMutex       m_mutex;
};

} // namespace neurx
