#include "ToolRegistry.h"

namespace neurx {

ToolRegistry &ToolRegistry::instance()
{
    static ToolRegistry s_instance;
    return s_instance;
}

ToolRegistry::ToolRegistry(QObject *parent) : QObject(parent) {}

void ToolRegistry::registerTool(BaseTool *tool)
{
    if (!tool || m_tools.contains(tool->toolId())) return;
    tool->setParent(this);
    m_tools.insert(tool->toolId(), tool);
    emit toolRegistered(tool->toolId());
}

BaseTool *ToolRegistry::findTool(const QString &toolId) const
{
    return m_tools.value(toolId, nullptr);
}

QList<BaseTool *> ToolRegistry::allTools() const
{
    return m_tools.values();
}

QVariantList ToolRegistry::toolSchemas() const
{
    QVariantList schemas;
    for (const auto *tool : m_tools) {
        schemas.append(QVariantMap{
            {"type", QStringLiteral("function")},
            {"function", QVariantMap{
                {"name",        tool->toolId()},
                {"description", tool->description()},
                {"parameters",  tool->inputSchema()}
            }}
        });
    }
    return schemas;
}

} // namespace neurx
