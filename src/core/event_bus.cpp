#include "aero/core/event_bus.h"

#include "aero/core/logging.h"

#include <QThread>

namespace Aero::Core {

QString const EventBus::LaunchApplication = QStringLiteral("app.launch");
QString const EventBus::OpenFile = QStringLiteral("file.open");
QString const EventBus::PostNotification = QStringLiteral("notification.post");
QString const EventBus::ThemeChanged = QStringLiteral("theme.changed");
QString const EventBus::WorkspaceChanged = QStringLiteral("workspace.changed");
QString const EventBus::SettingsChanged = QStringLiteral("settings.changed");

EventBus::EventBus(QObject* parent)
    : QObject(parent)
{
}

int EventBus::subscribe(const QString& topic, Handler handler)
{
    const int token = m_nextToken++;
    m_handlers.insert(token, Entry {topic, std::move(handler)});
    return token;
}

void EventBus::unsubscribe(int token)
{
    m_handlers.remove(token);
}

void EventBus::publish(const QString& topic, const QVariantMap& payload)
{
    if (QThread::currentThread() != thread()) {
        // The bus is intentionally main-thread-only.  Cross-thread producers
        // should invoke publish through QMetaObject::invokeMethod.
        qCWarning(aeroLog) << "EventBus::publish called from non-GUI thread for" << topic;
    }

    emit eventPublished(topic, payload);

    const QList<Entry> matching = [&]() {
        QList<Entry> result;
        for (const Entry& entry : std::as_const(m_handlers)) {
            if (entry.topic == topic) {
                result.append(entry);
            }
        }
        return result;
    }();

    for (const Entry& entry : matching) {
        try {
            entry.handler(payload);
        } catch (const std::exception& exception) {
            qCWarning(aeroLog) << "Event handler for" << topic
                               << "threw:" << exception.what();
        }
    }
}

} // namespace Aero::Core
