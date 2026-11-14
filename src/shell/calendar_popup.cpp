#include "aero/shell/calendar_popup.h"

#include "aero/services/calendar_store.h"
#include "aero/theme/controls.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/theme.h"

#include <QAbstractButton>
#include <QEnterEvent>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <utility>

namespace Aero::Shell {
namespace {

void clearLayout(QLayout* layout)
{
    if (layout == nullptr) {
        return;
    }
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        } else if (QLayout* child = item->layout()) {
            clearLayout(child);
            child->deleteLater();
        }
        delete item;
    }
}

QLabel* mutedLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setProperty("role", QStringLiteral("muted"));
    label->setFont(Theme::ThemeManager::instance().captionFont());
    return label;
}

QString eventTime(const Services::CalendarEvent& event)
{
    if (event.allDay) {
        return QStringLiteral("All day");
    }
    if (!event.start.isValid()) {
        return QString();
    }
    QString text = event.start.toString(QStringLiteral("HH:mm"));
    if (event.end.isValid() && event.end.date() == event.start.date()) {
        text += QStringLiteral(" – ") + event.end.toString(QStringLiteral("HH:mm"));
    }
    return text;
}

} // namespace

// ---------------------------------------------------------------------------
// Day cell
// ---------------------------------------------------------------------------

class CalendarPopup::DayCell final : public QWidget {
public:
    DayCell(CalendarPopup* owner, QWidget* parent)
        : QWidget(parent)
        , m_owner(owner)
    {
        setFixedHeight(42);
        setMinimumWidth(38);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
    }

    void setDay(const QDate& date, bool inCurrentMonth)
    {
        m_date = date;
        m_inMonth = inCurrentMonth;
        setToolTip(date.toString(QStringLiteral("dddd, d MMMM yyyy")));
        update();
    }

    void setSelected(bool selected)
    {
        if (m_selected == selected) {
            return;
        }
        m_selected = selected;
        update();
    }

    void setEventCount(int count)
    {
        m_eventCount = qMax(0, count);
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        if (!m_date.isValid()) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const Theme::ThemeTokens& tokens = Theme::ThemeManager::instance().tokens();
        const QRectF box = rect().adjusted(2, 2, -2, -2);

        if (m_selected) {
            QColor fill = tokens.color.accent;
            fill.setAlphaF(0.20);
            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            painter.drawRoundedRect(box, 9, 9);

            QColor border = tokens.color.accent;
            border.setAlphaF(0.75);
            painter.setPen(QPen(border, 1.2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), 9, 9);
        } else if (m_hovered) {
            QColor fill = tokens.color.textPrimary;
            fill.setAlphaF(0.07);
            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            painter.drawRoundedRect(box, 9, 9);
        }

        if (!m_selected && m_today) {
            QColor border = tokens.color.accent;
            border.setAlphaF(0.85);
            painter.setPen(QPen(border, 1.2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), 9, 9);
        }

        QColor text = m_inMonth ? tokens.color.textPrimary : tokens.color.textMuted;
        if (!m_inMonth) {
            text.setAlphaF(0.55);
        }
        if (m_selected) {
            text = tokens.color.accent;
        }
        QFont font = Theme::ThemeManager::instance().font(11.5, m_today ? QFont::DemiBold : QFont::Medium);
        painter.setFont(font);
        painter.setPen(text);
        painter.drawText(QRectF(box.left(), box.top() + 3, box.width(), box.height() - 10),
                         Qt::AlignHCenter | Qt::AlignTop, QString::number(m_date.day()));

        const int dots = qMin(3, m_eventCount);
        if (dots > 0) {
            QColor dot = m_selected ? tokens.color.accent : tokens.color.accent;
            dot.setAlphaF(m_inMonth ? 0.9 : 0.4);
            painter.setPen(Qt::NoPen);
            painter.setBrush(dot);
            const qreal spacing = 5.0;
            const qreal total = (dots - 1) * spacing;
            const qreal startX = box.center().x() - total / 2.0;
            const qreal y = box.bottom() - 6.0;
            for (int i = 0; i < dots; ++i) {
                painter.drawEllipse(QPointF(startX + i * spacing, y), 1.35, 1.35);
            }
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && m_owner != nullptr && m_date.isValid()) {
            m_owner->selectDate(m_date);
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

    void enterEvent(QEnterEvent* event) override
    {
        m_hovered = true;
        update();
        QWidget::enterEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        m_hovered = false;
        update();
        QWidget::leaveEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && m_owner != nullptr && m_date.isValid()) {
            m_owner->selectDate(m_date);
            emit m_owner->newEventRequested(m_date);
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

private:
    CalendarPopup* m_owner = nullptr;
    QDate m_date;
    bool m_inMonth = true;
    bool m_selected = false;
    bool m_hovered = false;
    bool m_today = false;
    int m_eventCount = 0;

public:
    void updateFlags(const QDate& today)
    {
        m_today = m_date == today;
    }
};

// ---------------------------------------------------------------------------
// CalendarPopup
// ---------------------------------------------------------------------------

CalendarPopup::CalendarPopup(Services::CalendarStore* store, QWidget* parent)
    : Overlay(parent)
    , m_store(store)
    , m_month(QDate::currentDate().addDays(1 - QDate::currentDate().day()))
    , m_selected(QDate::currentDate())
{
    setCornerRadius(22);
    setShadowMargin(18);
    setTintStrength(1.12);
    resize(392, 548);
    build();

    if (m_store != nullptr) {
        connect(m_store, &Services::CalendarStore::eventsChanged, this, [this]() {
            if (isOpen()) {
                rebuildMonth();
                rebuildAgenda();
            }
        });
    }
}

void CalendarPopup::build()
{
    const int margin = shadowMargin();
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(margin + 16, margin + 12, margin + 16, margin + 14);
    root->setSpacing(8);

    auto* header = new QHBoxLayout;
    header->setSpacing(4);

    m_monthLabel = new QLabel(this);
    m_monthLabel->setFont(Theme::ThemeManager::instance().font(16.5, QFont::DemiBold));
    header->addWidget(m_monthLabel);
    header->addStretch(1);

    m_today = new Theme::AeroButton(QStringLiteral("Today"), this);
    m_today->setVariant(Theme::AeroButton::Variant::Ghost);
    m_today->setToolTip(QStringLiteral("Jump to today"));
    connect(m_today, &QAbstractButton::clicked, this, &CalendarPopup::goToToday);
    header->addWidget(m_today);

    m_previous = new Theme::IconButton(QStringLiteral("chevron-left"), this);
    m_previous->setIconSize(14);
    m_previous->setFixedSize(26, 26);
    m_previous->setToolTip(QStringLiteral("Previous month"));
    connect(m_previous, &QAbstractButton::clicked, this, [this]() { stepMonth(-1); });
    header->addWidget(m_previous);

    m_next = new Theme::IconButton(QStringLiteral("chevron-right"), this);
    m_next->setIconSize(14);
    m_next->setFixedSize(26, 26);
    m_next->setToolTip(QStringLiteral("Next month"));
    connect(m_next, &QAbstractButton::clicked, this, [this]() { stepMonth(1); });
    header->addWidget(m_next);

    root->addLayout(header);

    auto* weekdays = new QHBoxLayout;
    weekdays->setContentsMargins(0, 2, 0, 0);
    weekdays->setSpacing(2);
    const QStringList names = {QStringLiteral("Mo"), QStringLiteral("Tu"),
                               QStringLiteral("We"), QStringLiteral("Th"),
                               QStringLiteral("Fr"), QStringLiteral("Sa"),
                               QStringLiteral("Su")};
    for (const QString& name : names) {
        auto* label = new QLabel(name, this);
        label->setProperty("role", QStringLiteral("section"));
        label->setFont(Theme::ThemeManager::instance().font(9.5, QFont::DemiBold));
        label->setAlignment(Qt::AlignCenter);
        weekdays->addWidget(label, 1);
    }
    root->addLayout(weekdays);

    auto* gridHost = new QWidget(this);
    m_grid = new QGridLayout(gridHost);
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setHorizontalSpacing(2);
    m_grid->setVerticalSpacing(2);
    for (int column = 0; column < 7; ++column) {
        m_grid->setColumnStretch(column, 1);
    }
    root->addWidget(gridHost);

    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    line->setProperty("role", QStringLiteral("separator"));
    root->addWidget(line);

    auto* agendaHeader = new QHBoxLayout;
    agendaHeader->setSpacing(6);
    auto* agendaTitle = new QLabel(this);
    agendaTitle->setObjectName(QStringLiteral("agendaTitle"));
    agendaTitle->setFont(Theme::ThemeManager::instance().bodyFont(QFont::DemiBold));
    agendaHeader->addWidget(agendaTitle, 1);

    auto* addEvent = new Theme::IconButton(QStringLiteral("plus"), this);
    addEvent->setIconSize(13);
    addEvent->setFixedSize(26, 26);
    addEvent->setToolTip(QStringLiteral("New event"));
    connect(addEvent, &QAbstractButton::clicked, this, [this]() {
        emit newEventRequested(m_selected);
    });
    agendaHeader->addWidget(addEvent);
    root->addLayout(agendaHeader);

    auto* agendaHost = new QWidget(this);
    m_agendaLayout = new QVBoxLayout(agendaHost);
    m_agendaLayout->setContentsMargins(0, 0, 0, 0);
    m_agendaLayout->setSpacing(5);
    root->addWidget(agendaHost, 1);

    m_openCalendar = new Theme::AeroButton(QStringLiteral("Open Calendar"), this);
    m_openCalendar->setVariant(Theme::AeroButton::Variant::Primary);
    connect(m_openCalendar, &QAbstractButton::clicked, this, [this]() {
        emit openCalendarRequested(m_selected);
    });
    root->addWidget(m_openCalendar);
}

void CalendarPopup::prepareShow()
{
    refresh();
}

void CalendarPopup::refresh()
{
    const QDate today = QDate::currentDate();
    if (!m_selected.isValid()) {
        m_selected = today;
    }
    if (!m_month.isValid()) {
        m_month = QDate(today.year(), today.month(), 1);
    }
    rebuildMonth();
    rebuildAgenda();
}

void CalendarPopup::rebuildMonth()
{
    if (m_monthLabel == nullptr || m_grid == nullptr) {
        return;
    }
    m_monthLabel->setText(m_month.toString(QStringLiteral("MMMM yyyy")));

    clearLayout(m_grid);
    m_cells.clear();

    const QDate first(m_month.year(), m_month.month(), 1);
    const int offset = first.dayOfWeek() - 1; // Monday == 0
    const QDate gridStart = first.addDays(-offset);
    const QDate today = QDate::currentDate();

    for (int index = 0; index < 42; ++index) {
        const QDate date = gridStart.addDays(index);
        const bool inMonth = date.month() == m_month.month() && date.year() == m_month.year();
        auto* cell = new DayCell(this, this);
        cell->setDay(date, inMonth);
        cell->setSelected(date == m_selected);
        cell->updateFlags(today);
        const int count = m_store != nullptr ? m_store->eventsForDate(date).size() : 0;
        cell->setEventCount(count);
        m_grid->addWidget(cell, index / 7, index % 7);
        m_cells.append(cell);
    }
}

void CalendarPopup::rebuildAgenda()
{
    if (m_agendaLayout == nullptr) {
        return;
    }

    clearLayout(m_agendaLayout);

    auto* title = findChild<QLabel*>(QStringLiteral("agendaTitle"));
    if (title != nullptr) {
        title->setText(m_selected.toString(QStringLiteral("dddd, d MMMM")));
    }

    QList<Services::CalendarEvent> events;
    if (m_store != nullptr && m_selected.isValid()) {
        events = m_store->eventsForDate(m_selected);
    }

    if (events.isEmpty()) {
        auto* empty = mutedLabel(QStringLiteral("No events. Double-click a day or press + to add one."), this);
        empty->setAlignment(Qt::AlignCenter);
        empty->setWordWrap(true);
        empty->setMinimumHeight(70);
        m_agendaLayout->addWidget(empty);
        m_agendaLayout->addStretch(1);
        return;
    }

    const int visible = qMin(5, events.size());
    for (int i = 0; i < visible; ++i) {
        const Services::CalendarEvent& event = events.at(i);
        auto* row = new QWidget(this);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 2, 0, 2);
        rowLayout->setSpacing(8);

        auto* dot = new QLabel(row);
        dot->setFixedSize(8, 8);
        QColor color(event.color);
        if (!color.isValid()) {
            color = Theme::ThemeManager::instance().tokens().color.accent;
        }
        dot->setStyleSheet(QStringLiteral("QLabel { background: %1; border-radius: 4px; }")
                               .arg(color.name()));
        rowLayout->addWidget(dot, 0, Qt::AlignTop);

        auto* textHost = new QWidget(row);
        auto* textLayout = new QVBoxLayout(textHost);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(0);
        auto* name = new QLabel(event.title, textHost);
        name->setFont(Theme::ThemeManager::instance().bodyFont(QFont::Medium));
        name->setWordWrap(true);
        textLayout->addWidget(name);

        QString detail = eventTime(event);
        if (!event.location.isEmpty()) {
            detail += detail.isEmpty() ? event.location : QStringLiteral(" · ") + event.location;
        }
        if (!detail.isEmpty()) {
            auto* subtitle = mutedLabel(detail, textHost);
            subtitle->setWordWrap(true);
            textLayout->addWidget(subtitle);
        }
        rowLayout->addWidget(textHost, 1);
        m_agendaLayout->addWidget(row);
    }

    if (events.size() > visible) {
        auto* more = mutedLabel(QStringLiteral("+%1 more event(s)").arg(events.size() - visible), this);
        more->setAlignment(Qt::AlignHCenter);
        m_agendaLayout->addWidget(more);
    }
    m_agendaLayout->addStretch(1);
}

void CalendarPopup::selectDate(const QDate& date)
{
    if (!date.isValid()) {
        return;
    }
    m_selected = date;
    if (date.month() != m_month.month() || date.year() != m_month.year()) {
        m_month = QDate(date.year(), date.month(), 1);
    }
    rebuildMonth();
    rebuildAgenda();
}

void CalendarPopup::stepMonth(int delta)
{
    if (delta == 0) {
        return;
    }
    const QDate base = m_month.isValid() ? m_month : QDate::currentDate();
    m_month = QDate(base.year(), base.month(), 1).addMonths(delta);

    const int day = qBound(1, m_selected.day(), m_month.daysInMonth());
    m_selected = QDate(m_month.year(), m_month.month(), day);
    rebuildMonth();
    rebuildAgenda();
}

void CalendarPopup::goToToday()
{
    const QDate today = QDate::currentDate();
    m_selected = today;
    m_month = QDate(today.year(), today.month(), 1);
    rebuildMonth();
    rebuildAgenda();
}

void CalendarPopup::paintEvent(QPaintEvent* event)
{
    Overlay::paintEvent(event);
}

} // namespace Aero::Shell
