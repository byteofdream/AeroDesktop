#pragma once

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QString>

#include <memory>

namespace Aero::Core {
class JsonConfig;
}

namespace Aero::Services {

struct CalendarEvent {
    QString id;
    QString title;
    QString notes;
    QString location;
    QDateTime start;
    QDateTime end;
    bool allDay = false;
    QString color = QStringLiteral("#7C5CFF");
    QString recurrence; // reserved: "none", "daily", "weekly", "monthly"

    bool isValid() const { return !title.isEmpty() && start.isValid(); }
    QDate date() const { return start.date(); }
};

/// Calendar model shared by the top-bar calendar popup and Aero Calendar.
/// Events are persisted in Aero's data directory through JsonConfig.
class CalendarStore : public QObject {
    Q_OBJECT
public:
    explicit CalendarStore(QObject* parent = nullptr);
    explicit CalendarStore(const QString& filePath, QObject* parent = nullptr);
    ~CalendarStore() override;

    QList<CalendarEvent> events() const;
    QList<CalendarEvent> eventsForDate(const QDate& date) const;
    QList<CalendarEvent> eventsForRange(const QDateTime& from, const QDateTime& to) const;
    QList<CalendarEvent> upcoming(int days = 7, int limit = 8) const;

    QString addEvent(CalendarEvent event);
    bool updateEvent(const CalendarEvent& event);
    bool removeEvent(const QString& id);
    void clear();

    QString filePath() const;

signals:
    void eventsChanged();

private:
    void load();
    void save();
    static int compareEvents(const CalendarEvent& a, const CalendarEvent& b);
    static QVariantMap toMap(const CalendarEvent& event);
    static CalendarEvent fromMap(const QVariantMap& map);

    std::unique_ptr<Core::JsonConfig> m_config;
    QList<CalendarEvent> m_events;
};

} // namespace Aero::Services

Q_DECLARE_METATYPE(Aero::Services::CalendarEvent)
