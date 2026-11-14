#include "aero/services/calendar_store.h"

#include "aero/core/json_config.h"
#include "aero/core/logging.h"
#include "aero/core/paths.h"

#include <QUuid>

#include <algorithm>

namespace Aero::Services {

CalendarStore::CalendarStore(QObject* parent)
    : CalendarStore(Core::Paths::calendarFile(), parent)
{
}

CalendarStore::CalendarStore(const QString& filePath, QObject* parent)
    : QObject(parent)
    , m_config(std::make_unique<Core::JsonConfig>(filePath))
{
    load();
}

CalendarStore::~CalendarStore() = default;

QString CalendarStore::filePath() const
{
    return m_config->filePath();
}

QList<CalendarEvent> CalendarStore::events() const
{
    return m_events;
}

QList<CalendarEvent> CalendarStore::eventsForDate(const QDate& date) const
{
    QList<CalendarEvent> result;
    for (const CalendarEvent& event : m_events) {
        if (event.start.date() == date) {
            result.append(event);
        }
    }
    std::sort(result.begin(), result.end(), compareEvents);
    return result;
}

QList<CalendarEvent> CalendarStore::eventsForRange(const QDateTime& from, const QDateTime& to) const
{
    QList<CalendarEvent> result;
    for (const CalendarEvent& event : m_events) {
        if (event.start <= to && event.end >= from) {
            result.append(event);
        }
    }
    std::sort(result.begin(), result.end(), compareEvents);
    return result;
}

QList<CalendarEvent> CalendarStore::upcoming(int days, int limit) const
{
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime limitTime = now.addDays(days);
    QList<CalendarEvent> result;
    for (const CalendarEvent& event : m_events) {
        if (event.start >= now.addDays(-1) && event.start <= limitTime) {
            result.append(event);
        }
    }
    std::sort(result.begin(), result.end(), compareEvents);
    if (result.size() > limit) {
        result = result.mid(0, limit);
    }
    return result;
}

QString CalendarStore::addEvent(CalendarEvent event)
{
    if (!event.isValid()) {
        return {};
    }
    if (event.id.isEmpty()) {
        event.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (!event.end.isValid() || event.end < event.start) {
        event.end = event.allDay
            ? QDateTime(event.start.date().addDays(1), QTime(0, 0))
            : event.start.addSecs(3600);
    }

    m_events.append(event);
    std::sort(m_events.begin(), m_events.end(), compareEvents);
    save();
    emit eventsChanged();
    return event.id;
}

bool CalendarStore::updateEvent(const CalendarEvent& event)
{
    for (CalendarEvent& existing : m_events) {
        if (existing.id == event.id) {
            existing = event;
            if (!existing.end.isValid() || existing.end < existing.start) {
                existing.end = existing.start.addSecs(3600);
            }
            std::sort(m_events.begin(), m_events.end(), compareEvents);
            save();
            emit eventsChanged();
            return true;
        }
    }
    return false;
}

bool CalendarStore::removeEvent(const QString& id)
{
    const auto it = std::find_if(m_events.begin(), m_events.end(),
                                 [&id](const CalendarEvent& event) { return event.id == id; });
    if (it == m_events.end()) {
        return false;
    }
    m_events.erase(it);
    save();
    emit eventsChanged();
    return true;
}

void CalendarStore::clear()
{
    if (m_events.isEmpty()) {
        return;
    }
    m_events.clear();
    save();
    emit eventsChanged();
}

int CalendarStore::compareEvents(const CalendarEvent& a, const CalendarEvent& b)
{
    if (a.start != b.start) {
        return a.start < b.start ? -1 : 1;
    }
    return a.title.compare(b.title, Qt::CaseInsensitive);
}

QVariantMap CalendarStore::toMap(const CalendarEvent& event)
{
    return {
        {QStringLiteral("id"), event.id},
        {QStringLiteral("title"), event.title},
        {QStringLiteral("notes"), event.notes},
        {QStringLiteral("location"), event.location},
        {QStringLiteral("start"), event.start.toString(Qt::ISODate)},
        {QStringLiteral("end"), event.end.toString(Qt::ISODate)},
        {QStringLiteral("allDay"), event.allDay},
        {QStringLiteral("color"), event.color},
        {QStringLiteral("recurrence"), event.recurrence},
    };
}

CalendarEvent CalendarStore::fromMap(const QVariantMap& map)
{
    CalendarEvent event;
    event.id = map.value(QStringLiteral("id")).toString();
    event.title = map.value(QStringLiteral("title")).toString();
    event.notes = map.value(QStringLiteral("notes")).toString();
    event.location = map.value(QStringLiteral("location")).toString();
    event.start = QDateTime::fromString(map.value(QStringLiteral("start")).toString(), Qt::ISODate);
    event.end = QDateTime::fromString(map.value(QStringLiteral("end")).toString(), Qt::ISODate);
    event.allDay = map.value(QStringLiteral("allDay")).toBool();
    event.color = map.value(QStringLiteral("color"), QStringLiteral("#7C5CFF")).toString();
    event.recurrence = map.value(QStringLiteral("recurrence"), QStringLiteral("none")).toString();
    return event;
}

void CalendarStore::load()
{
    QString error;
    if (!m_config->load(&error)) {
        qCWarning(aeroLog) << "Unable to load calendar:" << error;
        return;
    }

    m_events.clear();
    const QVariantList list = m_config->value(QStringLiteral("events")).toList();
    for (const QVariant& item : list) {
        const CalendarEvent event = fromMap(item.toMap());
        if (event.isValid()) {
            m_events.append(event);
        }
    }
    std::sort(m_events.begin(), m_events.end(), compareEvents);
}

void CalendarStore::save()
{
    QVariantList list;
    list.reserve(m_events.size());
    for (const CalendarEvent& event : std::as_const(m_events)) {
        list.append(toMap(event));
    }
    m_config->setValue(QStringLiteral("events"), list);
}

} // namespace Aero::Services
