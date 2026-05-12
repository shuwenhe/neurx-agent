#include "AgentListModel.h"

namespace neurx {

AgentListModel::AgentListModel(AgentRuntime *runtime, QObject *parent)
    : QAbstractListModel(parent), m_runtime(runtime)
{
    refresh();
    connect(&runtime->registry(), &AgentRegistry::agentRegistered,
            this, [this](const QString &) { refresh(); });
    connect(&runtime->registry(), &AgentRegistry::agentUnregistered,
            this, [this](const QString &) { refresh(); });
}

void AgentListModel::refresh()
{
    beginResetModel();
    m_agents.clear();
    for (auto *a : m_runtime->registry().allAgents())
        m_agents.append(a);
    endResetModel();
}

int AgentListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_agents.size();
}

QVariant AgentListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_agents.size()) return {};
    const auto *a = m_agents.at(index.row());
    switch (role) {
    case IdRole:     return a->agentId();
    case NameRole:   return a->name();
    case StatusRole: return static_cast<int>(a->status());
    default:         return {};
    }
}

QHash<int, QByteArray> AgentListModel::roleNames() const
{
    return {
        {IdRole,     "agentId"},
        {NameRole,   "name"},
        {StatusRole, "status"},
    };
}

} // namespace neurx
