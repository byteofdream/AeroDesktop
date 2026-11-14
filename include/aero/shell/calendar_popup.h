#pragma once

#include "aero/shell/overlay.h"

#include <QDate>
#include <QList>

class QGridLayout;
class QLabel;
class QVBoxLayout;

namespace Aero::Services {
class CalendarStore;
}

namespace Aero::Theme {
class AeroButton;
class IconButton;
}

namespace Aero::Shell {

/// Compact month calendar and daily agenda shown from the top bar clock.
class CalendarPopup : public Overlay {
    Q_OBJECT
public:
    explicit CalendarPopup(Services::CalendarStore* store, QWidget* parent = nullptr);

    void refresh();

signals:
    void openCalendarRequested(const QDate& date);
    void newEventRequested(const QDate& date);

protected:
    void prepareShow() override;
    void paintEvent(QPaintEvent* event) override;

private:
    void build();
    void rebuildMonth();
    void rebuildAgenda();
    void selectDate(const QDate& date);
    void stepMonth(int delta);
    void goToToday();

    Services::CalendarStore* m_store = nullptr;
    QDate m_month;
    QDate m_selected;

    QLabel* m_monthLabel = nullptr;
    QGridLayout* m_grid = nullptr;
    QVBoxLayout* m_agendaLayout = nullptr;
    Theme::IconButton* m_previous = nullptr;
    Theme::IconButton* m_next = nullptr;
    Theme::AeroButton* m_today = nullptr;
    Theme::AeroButton* m_openCalendar = nullptr;

    class DayCell;
    QList<DayCell*> m_cells;
};

} // namespace Aero::Shell
