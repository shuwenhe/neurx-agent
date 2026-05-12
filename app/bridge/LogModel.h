#pragma once
#include <QAbstractListModel>
#include <QDateTime>

namespace neurx {

struct LogEntry {
    QDateTime timestamp;
    QString   level;
    QString   tag;
    QString   message;
};

/// Real-time log model for the QML console view.
class LogModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int maxLines READ maxLines WRITE setMaxLines)

public:
    enum Roles { TimeRole = Qt::UserRole+1, LevelRole, TagRole, MessageRole };

    explicit LogModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int maxLines() const  { return m_maxLines; }
    void setMaxLines(int n) { m_maxLines = n;  }

public slots:
    void append(const QString &level, const QString &tag,
                const QString &message, const QDateTime &ts = {});
    Q_INVOKABLE void clear();

private:
    QList<LogEntry> m_entries;
    int             m_maxLines{2000};
};

} // namespace neurx
