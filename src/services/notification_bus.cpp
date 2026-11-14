#include "aero/services/notification_bus.h"

#include "aero/core/json_config.h"
#include "aero/core/logging.h"
#include "aero/core/paths.h"
#include "aero/settings/settings_keys.h"
#include "aero/settings/settings_manager.h"

#include <QUuid>

#include <algorithm>
#include <utility>

namespace Aero::Services {

QVariantMap Notification::toVariantMap() const
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("appId"), appId},
        {QStringLiteral("appName"), appName},
        {QStringLiteral("title"), title},
        {QStringLiteral("body"), body},
        {QStringLiteral("iconName"), iconName},
        {QStringLiteral("timestamp"), timestamp.toString(Qt::ISODateWithMs)},
        {QStringLiteral("read"), read},
        {QStringLiteral("actions"), actions},
    };
}

Notification Notification::fromVariantMap(const QVariantMap& map)
{
    Notification notification;
    notification.id = map.value(QStringLiteral("id")).toString();
    notification.appId = map.value(QStringLiteral("appId")).toString();
    notification.appName = map.value(QStringLiteral("appName")).toString();
    notification.title = map.value(QStringLiteral("title")).toString();
    notification.body = map.value(QStringLiteral("body")).toString();
    notification.iconName = map.value(QStringLiteral("iconName")).toString();
    notification.timestamp = QDateTime::fromString(
        map.value(QStringLiteral("timestamp")).toString(), Qt::ISODateWithMs);
    notification.read = map.value(QStringLiteral("read")).toBool();
    notification.actions = map.value(QStringLiteral("actions")).toStringList();
    return notification;
}

NotificationBus::NotificationBus(Settings::SettingsManager* settings, QObject* parent)
    : QObject(parent)
    , m_config(std::make_unique<Core::JsonConfig>(Core::Paths::notificationsFile()))
    , m_settings(settings)
{
    load();
    if (m_settings != nullptr) {
        connect(m_settings, &Settings::SettingsManager::settingChanged, this,
                [this](const QString& key, const QVariant&) {
                    if (key == QString::fromLatin1(Settings::Keys::NotificationsDnd)) {
                        emit changed();
                    }
                });
    }
}

NotificationBus::~NotificationBus()
{
    save();
}

QList<Notification> NotificationBus::notifications() const
{
    return m_notifications;
}

int NotificationBus::unreadCount() const
{
    int count = 0;
    for (const Notification& notification : m_notifications) {
        if (!notification.read) {
            ++count;
        }
    }
    return count;
}

int NotificationBus::count() const
{
    return m_notifications.size();
}

void NotificationBus::post(const Notification& input)
{
    Notification notification = input;
    if (notification.id.isEmpty()) {
        notification.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (!notification.timestamp.isValid()) {
        notification.timestamp = QDateTime::currentDateTime();
    }
    if (notification.appName.isEmpty()) {
        notification.appName = QStringLiteral("Aero Desktop");
    }

    m_notifications.prepend(notification);
    enforceHistoryLimit();
    save();

    emit posted(notification);
    emit changed();
    emit unreadCountChanged(unreadCount());

    if (!doNotDisturb()) {
        emit bannerRequested(notification);
    }
}

void NotificationBus::post(const QString& appId,
                           const QString& appName,
                           const QString& title,
                           const QString& body,
                           const QString& iconName)
{
    Notification notification;
    notification.appId = appId;
    notification.appName = appName;
    notification.title = title;
    notification.body = body;
    notification.iconName = iconName;
    post(notification);
}

void NotificationBus::dismiss(const QString& id)
{
    const auto it = std::find_if(m_notifications.begin(), m_notifications.end(),
                                 [&id](const Notification& n) { return n.id == id; });
    if (it == m_notifications.end()) {
        return;
    }

    const bool wasUnread = !it->read;
    m_notifications.erase(it);
    save();
    emit dismissed(id);
    emit changed();
    if (wasUnread) {
        emit unreadCountChanged(unreadCount());
    }
}

void NotificationBus::clearAll()
{
    if (m_notifications.isEmpty()) {
        return;
    }
    m_notifications.clear();
    save();
    emit changed();
    emit unreadCountChanged(0);
}

void NotificationBus::markAllRead()
{
    bool changedAny = false;
    for (Notification& notification : m_notifications) {
        if (!notification.read) {
            notification.read = true;
            changedAny = true;
        }
    }
    if (changedAny) {
        save();
        emit changed();
        emit unreadCountChanged(0);
    }
}

void NotificationBus::markRead(const QString& id)
{
    for (Notification& notification : m_notifications) {
        if (notification.id == id && !notification.read) {
            notification.read = true;
            save();
            emit changed();
            emit unreadCountChanged(unreadCount());
            return;
        }
    }
}

bool NotificationBus::doNotDisturb() const
{
    return m_settings != nullptr && m_settings->doNotDisturb();
}

void NotificationBus::setDoNotDisturb(bool enabled)
{
    if (m_settings == nullptr) {
        return;
    }
    m_settings->setDoNotDisturb(enabled);
    emit changed();
}

void NotificationBus::load()
{
    QString error;
    if (!m_config->load(&error)) {
        qCWarning(aeroLog) << "Unable to load notifications:" << error;
        return;
    }

    m_notifications.clear();
    const QVariantList list = m_config->value(QStringLiteral("items")).toList();
    for (const QVariant& item : list) {
        const Notification notification = Notification::fromVariantMap(item.toMap());
        if (!notification.id.isEmpty() && !notification.title.isEmpty()) {
            m_notifications.append(notification);
        }
    }
}

void NotificationBus::save()
{
    QVariantList list;
    list.reserve(m_notifications.size());
    for (const Notification& notification : std::as_const(m_notifications)) {
        list.append(notification.toVariantMap());
    }
    m_config->setValue(QStringLiteral("items"), list);
}

void NotificationBus::enforceHistoryLimit()
{
    int limit = 50;
    if (m_settings != nullptr) {
        limit = qBound(10, m_settings->value(
                                 QString::fromLatin1(Settings::Keys::NotificationsHistoryLimit), 50).toInt(), 200);
    }
    while (m_notifications.size() > limit) {
        m_notifications.removeLast();
    }
}

} // namespace Aero::Services
