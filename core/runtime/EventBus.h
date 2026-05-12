#pragma once
#include <QObject>
#include <QVariantMap>
#include <QString>
#include <functional>

namespace neurx {

/// Publish/subscribe event bus for decoupled communication between subsystems.
class EventBus : public QObject
{
    Q_OBJECT
public:
    using Handler = std::function<void(const QString &topic, const QVariantMap &data)>;

    static EventBus &instance();

    void publish(const QString &topic, const QVariantMap &data = {});
    void subscribe(const QString &topic, QObject *owner, Handler handler);
    void unsubscribeAll(QObject *owner);

signals:
    void eventPublished(const QString &topic, const QVariantMap &data);

private:
    explicit EventBus(QObject *parent = nullptr);

    struct Subscription {
        QObject *owner;
        Handler  handler;
    };
    QMultiHash<QString, Subscription> m_subscriptions;
};

} // namespace neurx
