#pragma once
#include <QAbstractListModel>
#include "runtime/AgentRuntime.h"

namespace neurx {

/// List model exposing registered agents to QML.
class AgentListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        StatusRole,
    };

    explicit AgentListModel(AgentRuntime *runtime, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    void refresh();
    AgentRuntime        *m_runtime;
    QList<const Agent*>  m_agents;
};

} // namespace neurx
