#pragma once
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace neurx {

/// Abstract base for all agent tools.
class BaseTool : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString toolId      READ toolId      CONSTANT)
    Q_PROPERTY(QString description READ description CONSTANT)

public:
    explicit BaseTool(const QString &toolId,
                      const QString &description,
                      QObject *parent = nullptr);
    virtual ~BaseTool() = default;

    QString toolId()      const { return m_toolId;      }
    QString description() const { return m_description; }

    /// Execute the tool with the given parameters.
    /// Returns the result as a QVariantMap.
    virtual QVariantMap execute(const QVariantMap &params) = 0;

    /// JSON Schema describing input parameters (for LLM function-calling).
    virtual QVariantMap inputSchema() const { return {}; }

signals:
    void executed(const QVariantMap &result);
    void failed(const QString &error);

private:
    QString m_toolId;
    QString m_description;
};

} // namespace neurx
