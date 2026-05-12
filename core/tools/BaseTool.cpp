#include "BaseTool.h"

namespace neurx {

BaseTool::BaseTool(const QString &toolId,
                   const QString &description,
                   QObject *parent)
    : QObject(parent)
    , m_toolId(toolId)
    , m_description(description)
{}

} // namespace neurx
