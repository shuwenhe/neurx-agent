#pragma once
#include "BaseTool.h"
#include <QHash>
#include <QSharedPointer>

namespace neurx {

/// Registry for all tools available to agents.
class ToolRegistry : public QObject
{
    Q_OBJECT
public:
    static ToolRegistry &instance();

    void registerTool(BaseTool *tool);
    BaseTool *findTool(const QString &toolId) const;
    QList<BaseTool *> allTools() const;

    /// Returns JSON Schema array of all tool definitions (for LLM function-calling).
    QVariantList toolSchemas() const;

signals:
    void toolRegistered(const QString &toolId);

private:
    explicit ToolRegistry(QObject *parent = nullptr);
    QHash<QString, BaseTool *> m_tools;
};

} // namespace neurx
