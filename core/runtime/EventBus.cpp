#include "EventBus.h"
#include <algorithm>

namespace neurx {

EventBus &EventBus::instance()
{
    static EventBus s_instance;
    return s_instance;
}

EventBus::EventBus(QObject *parent) : QObject(parent) {}

void EventBus::publish(const QString &topic, const QVariantMap &data)
{
    emit eventPublished(topic, data);
    const auto subs = m_subscriptions.values(topic);
    for (const auto &sub : subs) {
        if (sub.owner)
            sub.handler(topic, data);
    }
}

void EventBus::subscribe(const QString &topic, QObject *owner, Handler handler)
{
    // Auto-unsubscribe when owner is destroyed
    connect(owner, &QObject::destroyed, this, [this, owner]() {
        unsubscribeAll(owner);
    });
    m_subscriptions.insert(topic, {owner, std::move(handler)});
}

void EventBus::unsubscribeAll(QObject *owner)
{
    for (auto it = m_subscriptions.begin(); it != m_subscriptions.end(); ) {
        if (it->owner == owner)
            it = m_subscriptions.erase(it);
        else
            ++it;
    }
}

} // namespace neurx
