#pragma once

#include <QHash>
#include <QObject>
#include <QVariantMap>

#include <functional>

namespace Aero::Core {

/// Lightweight, main-thread-only publish/subscribe bus for cross-module events.
///
/// Subscriptions return a token which can be released with unsubscribe().
/// Handlers are copied while dispatching so a handler may unsubscribe safely.
/// The bus intentionally has no compile-time dependency on any module; payloads
/// are validated by the publisher and consumer.
class EventBus : public QObject {
    Q_OBJECT
public:
    using Handler = std::function<void(const QVariantMap&)>;

    explicit EventBus(QObject* parent = nullptr);

    int subscribe(const QString& topic, Handler handler);
    void unsubscribe(int token);

    void publish(const QString& topic, const QVariantMap& payload = {});

    static QString const LaunchApplication;
    static QString const OpenFile;
    static QString const PostNotification;
    static QString const ThemeChanged;
    static QString const WorkspaceChanged;
    static QString const SettingsChanged;

signals:
    void eventPublished(const QString& topic, const QVariantMap& payload);

private:
    struct Entry {
        QString topic;
        Handler handler;
    };

    int m_nextToken = 1;
    QHash<int, Entry> m_handlers;
};

} // namespace Aero::Core
