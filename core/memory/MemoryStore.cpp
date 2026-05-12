#include "MemoryStore.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QDebug>
#include <QUuid>

namespace neurx {

MemoryStore::MemoryStore(const QString &dbPath, QObject *parent)
    : QObject(parent)
    , m_dbPath(dbPath)
    , m_connectionName(QUuid::createUuid().toString())
{}

bool MemoryStore::open()
{
    auto db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
        qWarning() << "MemoryStore: cannot open database" << db.lastError().text();
        return false;
    }
    ensureSchema();
    return true;
}

void MemoryStore::close()
{
    {
        auto db = QSqlDatabase::database(m_connectionName);
        if (db.isOpen()) db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

void MemoryStore::ensureSchema()
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS memory (
            namespace TEXT NOT NULL,
            key       TEXT NOT NULL,
            value     TEXT NOT NULL,
            created_at INTEGER DEFAULT (strftime('%s','now')),
            PRIMARY KEY (namespace, key)
        )
    )");
}

bool MemoryStore::store(const QString &key, const QVariantMap &value,
                        const QString &namespace_)
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare("INSERT OR REPLACE INTO memory (namespace, key, value) VALUES (?,?,?)");
    q.addBindValue(namespace_);
    q.addBindValue(key);
    q.addBindValue(QString::fromUtf8(
        QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact)));
    return q.exec();
}

QVariantMap MemoryStore::retrieve(const QString &key,
                                  const QString &namespace_) const
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare("SELECT value FROM memory WHERE namespace=? AND key=?");
    q.addBindValue(namespace_);
    q.addBindValue(key);
    if (!q.exec() || !q.next()) return {};
    return QJsonDocument::fromJson(q.value(0).toString().toUtf8())
               .toVariant().toMap();
}

QVariantList MemoryStore::search(const QString &query,
                                 const QString &namespace_,
                                 int limit) const
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare("SELECT key, value FROM memory WHERE namespace=? AND (key LIKE ? OR value LIKE ?) LIMIT ?");
    const QString pattern = QStringLiteral("%%1%").arg(query);
    q.addBindValue(namespace_);
    q.addBindValue(pattern);
    q.addBindValue(pattern);
    q.addBindValue(limit);
    QVariantList results;
    if (q.exec()) {
        while (q.next()) {
            auto item = QJsonDocument::fromJson(
                            q.value(1).toString().toUtf8()).toVariant().toMap();
            item["_key"] = q.value(0).toString();
            results.append(item);
        }
    }
    return results;
}

bool MemoryStore::remove(const QString &key, const QString &namespace_)
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare("DELETE FROM memory WHERE namespace=? AND key=?");
    q.addBindValue(namespace_);
    q.addBindValue(key);
    return q.exec();
}

void MemoryStore::clear(const QString &namespace_)
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare("DELETE FROM memory WHERE namespace=?");
    q.addBindValue(namespace_);
    q.exec();
}

} // namespace neurx
