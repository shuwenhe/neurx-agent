#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>

namespace neurx {

/// Simple in-process memory store backed by SQLite.
class MemoryStore : public QObject
{
    Q_OBJECT
public:
    explicit MemoryStore(const QString &dbPath, QObject *parent = nullptr);

    bool open();
    void close();

    bool store(const QString &key, const QVariantMap &value,
               const QString &namespace_ = "default");
    QVariantMap retrieve(const QString &key,
                         const QString &namespace_ = "default") const;
    QVariantList search(const QString &query,
                        const QString &namespace_ = "default",
                        int limit = 10) const;
    bool remove(const QString &key, const QString &namespace_ = "default");
    void clear(const QString &namespace_ = "default");

private:
    void ensureSchema();
    QString  m_dbPath;
    QString  m_connectionName;
};

} // namespace neurx
