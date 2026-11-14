#include "aero/apps/builtin_apps.h"

#include "aero/apps/common/app_widgets.h"
#include "aero/core/utils.h"
#include "aero/services/calendar_store.h"
#include "aero/theme/controls.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/theme.h"

#include <QCheckBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTimeEdit>
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace Aero::Applications {
namespace {

QPixmap colorDot(const QColor& color, int size = 12)
{
    QPixmap pixmap(size * 2, size * 2);
    pixmap.setDevicePixelRatio(2.0);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(QRectF(1.0, 1.0, size - 2.0, size - 2.0));
    return pixmap;
}

class MonthGrid : public QWidget {
public:
    explicit MonthGrid(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(420, 320);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMouseTracking(true);
        m_month = QDate(QDate::currentDate().year(), QDate::currentDate().month(), 1);
        m_selected = QDate::currentDate();
    }

    std::function<void(const QDate&)> onDateSelected;
    std::function<bool(const QDate&)> hasEvents;

    void setMonth(const QDate& month)
    {
        m_month = QDate(month.year(), month.month(), 1);
        update();
    }

    QDate month() const { return m_month; }

    void setSelected(const QDate& date)
    {
        m_selected = date;
        if (date.year() != m_month.year() || date.month() != m_month.month()) {
            m_month = QDate(date.year(), date.month(), 1);
        }
        update();
    }

    QDate selected() const { return m_selected; }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        const Theme::ThemeTokens& tokens = Theme::ThemeManager::instance().tokens();
        const Theme::ColorTokens& colors = tokens.color;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const qreal headerHeight = 28.0;
        const qreal cellWidth = width() / 7.0;
        const qreal cellHeight = qMax(1.0, (height() - headerHeight) / 6.0);

        painter.setFont(Theme::ThemeManager::instance().captionFont(QFont::DemiBold));
        painter.setPen(colors.textMuted);
        const QLocale locale = QLocale::system();
        for (int i = 0; i < 7; ++i) {
            const int day = ((i + 0) % 7) + 1;
            const QRectF headerRect(i * cellWidth, 0, cellWidth, headerHeight);
            painter.drawText(headerRect, Qt::AlignCenter, locale.dayName(day, QLocale::ShortFormat).toUpper());
        }

        const int offset = (m_month.dayOfWeek() + 6) % 7;
        const QDate firstCell = m_month.addDays(-offset);
        const QDate today = QDate::currentDate();

        for (int index = 0; index < 42; ++index) {
            const QDate date = firstCell.addDays(index);
            const int column = index % 7;
            const int row = index / 7;
            const QRectF cell(column * cellWidth, headerHeight + row * cellHeight, cellWidth, cellHeight);
            const bool inMonth = date.month() == m_month.month() && date.year() == m_month.year();
            const bool selected = date == m_selected;
            const bool isToday = date == today;
            const bool hovered = date == m_hovered;

            if (hovered && !selected) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(Core::Utils::withAlpha(colors.textPrimary, 0.05));
                painter.drawRoundedRect(cell.adjusted(3, 2, -3, -2), 9, 9);
            }

            const qreal circleSize = qMin(cellWidth, cellHeight) * 0.58;
            const QRectF circle(cell.center().x() - circleSize / 2.0,
                                cell.center().y() - circleSize / 2.0 - 3.0,
                                circleSize,
                                circleSize);

            if (selected) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(colors.accent);
                painter.drawEllipse(circle);
            } else if (isToday) {
                painter.setPen(QPen(Core::Utils::withAlpha(colors.accent, 0.65), 1.2));
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(circle);
            }

            QColor textColor = selected ? colors.onAccent
                                        : (inMonth ? colors.textPrimary : Core::Utils::withAlpha(colors.textMuted, 0.55));
            if (isToday && !selected) {
                textColor = colors.accent;
            }
            painter.setPen(textColor);
            painter.setFont(Theme::ThemeManager::instance().font(selected ? 14.0 : 13.0,
                                                                selected ? QFont::DemiBold : QFont::Normal));
            painter.drawText(cell.adjusted(0, -5, 0, -5), Qt::AlignCenter, QString::number(date.day()));

            if (hasEvents && hasEvents(date)) {
                const QColor dotColor = selected ? colors.onAccent : colors.accent;
                painter.setPen(Qt::NoPen);
                painter.setBrush(dotColor);
                painter.drawEllipse(QPointF(cell.center().x(), cell.bottom() - 9.0), 2.2, 2.2);
            }
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton) {
            QWidget::mousePressEvent(event);
            return;
        }
        const QDate date = dateAt(event->position());
        if (date.isValid()) {
            setSelected(date);
            if (onDateSelected) {
                onDateSelected(date);
            }
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        const QDate date = dateAt(event->position());
        if (date != m_hovered) {
            m_hovered = date;
            update();
        }
        QWidget::mouseMoveEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        m_hovered = QDate();
        update();
        QWidget::leaveEvent(event);
    }

private:
    QDate dateAt(const QPointF& position) const
    {
        const qreal headerHeight = 28.0;
        if (position.y() < headerHeight) {
            return {};
        }
        const qreal cellWidth = width() / 7.0;
        const qreal cellHeight = qMax(1.0, (height() - headerHeight) / 6.0);
        const int column = qBound(0, static_cast<int>(position.x() / cellWidth), 6);
        const int row = qBound(0, static_cast<int>((position.y() - headerHeight) / cellHeight), 5);
        const int offset = (m_month.dayOfWeek() + 6) % 7;
        return m_month.addDays(-offset + row * 7 + column);
    }

    QDate m_month;
    QDate m_selected;
    QDate m_hovered;
};

class EventDialog final : public QDialog {
public:
    explicit EventDialog(QWidget* parent = nullptr, const Aero::Services::CalendarEvent* event = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(event == nullptr ? QStringLiteral("New Event") : QStringLiteral("Edit Event"));
        setModal(true);
        setMinimumWidth(430);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(22, 20, 22, 18);
        root->setSpacing(12);

        auto* heading = new QLabel(event == nullptr ? QStringLiteral("Add an event") : QStringLiteral("Edit event"), this);
        heading->setProperty("role", QStringLiteral("title"));
        heading->setFont(Theme::ThemeManager::instance().font(21.0, QFont::DemiBold));
        root->addWidget(heading);

        auto* form = new QFormLayout;
        form->setHorizontalSpacing(14);
        form->setVerticalSpacing(9);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

        m_title = new QLineEdit(this);
        m_title->setPlaceholderText(QStringLiteral("Event title"));
        form->addRow(QStringLiteral("Title"), m_title);

        m_location = new QLineEdit(this);
        m_location->setPlaceholderText(QStringLiteral("Optional location"));
        form->addRow(QStringLiteral("Location"), m_location);

        m_date = new QDateEdit(this);
        m_date->setCalendarPopup(true);
        m_date->setDisplayFormat(QStringLiteral("ddd, d MMM yyyy"));
        form->addRow(QStringLiteral("Date"), m_date);

        m_allDay = new QCheckBox(QStringLiteral("All day"), this);
        form->addRow(QString(), m_allDay);

        auto* timeRow = new QWidget(this);
        auto* timeLayout = new QHBoxLayout(timeRow);
        timeLayout->setContentsMargins(0, 0, 0, 0);
        timeLayout->setSpacing(6);
        m_start = new QTimeEdit(this);
        m_start->setDisplayFormat(QStringLiteral("HH:mm"));
        m_end = new QTimeEdit(this);
        m_end->setDisplayFormat(QStringLiteral("HH:mm"));
        timeLayout->addWidget(m_start, 1);
        timeLayout->addWidget(new QLabel(QStringLiteral("to"), timeRow));
        timeLayout->addWidget(m_end, 1);
        form->addRow(QStringLiteral("Time"), timeRow);

        m_notes = new QTextEdit(this);
        m_notes->setPlaceholderText(QStringLiteral("Notes"));
        m_notes->setFixedHeight(80);
        form->addRow(QStringLiteral("Notes"), m_notes);

        root->addLayout(form);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
        buttons->button(QDialogButtonBox::Save)->setDefault(true);
        root->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
            if (m_title->text().trimmed().isEmpty()) {
                m_title->setFocus();
                return;
            }
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        const QDate today = QDate::currentDate();
        const QTime now = QTime::currentTime();
        const int roundedMinute = ((now.minute() + 14) / 15) * 15;
        QTime start(now.hour(), 0);
        if (roundedMinute >= 60) {
            start = start.addSecs(3600);
        } else {
            start = QTime(now.hour(), roundedMinute);
        }
        m_date->setDate(today);
        m_start->setTime(start);
        m_end->setTime(start.addSecs(3600));

        if (event != nullptr) {
            m_title->setText(event->title);
            m_location->setText(event->location);
            m_date->setDate(event->start.date());
            m_start->setTime(event->start.time());
            m_end->setTime(event->end.isValid() ? event->end.time() : event->start.time().addSecs(3600));
            m_allDay->setChecked(event->allDay);
            m_notes->setPlainText(event->notes);
            m_existingId = event->id;
            m_color = event->color;
        }

        auto updateTimes = [this]() {
            const bool allDay = m_allDay->isChecked();
            m_start->setEnabled(!allDay);
            m_end->setEnabled(!allDay);
        };
        connect(m_allDay, &QCheckBox::toggled, this, updateTimes);
        updateTimes();

        connect(m_start, &QTimeEdit::timeChanged, this, [this](const QTime& time) {
            if (m_end->time() <= time) {
                m_end->setTime(time.addSecs(3600));
            }
        });

        m_title->setFocus();
    }

    Aero::Services::CalendarEvent event() const
    {
        Aero::Services::CalendarEvent result;
        result.id = m_existingId;
        result.title = m_title->text().trimmed();
        result.location = m_location->text().trimmed();
        result.notes = m_notes->toPlainText();
        result.allDay = m_allDay->isChecked();
        result.color = m_color;

        const QDate date = m_date->date();
        if (result.allDay) {
            result.start = QDateTime(date.startOfDay());
            result.end = QDateTime(date.addDays(1).startOfDay());
        } else {
            result.start = QDateTime(date, m_start->time(), QTimeZone::systemTimeZone());
            result.end = QDateTime(date, m_end->time(), QTimeZone::systemTimeZone());
            if (result.end <= result.start) {
                result.end = result.start.addSecs(3600);
            }
        }
        return result;
    }

private:
    QLineEdit* m_title = nullptr;
    QLineEdit* m_location = nullptr;
    QDateEdit* m_date = nullptr;
    QTimeEdit* m_start = nullptr;
    QTimeEdit* m_end = nullptr;
    QCheckBox* m_allDay = nullptr;
    QTextEdit* m_notes = nullptr;
    QString m_existingId;
    QString m_color = QStringLiteral("#7C5CFF");
};

class CalendarView final : public AppView {
public:
    explicit CalendarView(const AppContext& context, QWidget* parent = nullptr)
        : AppView(context, parent)
    {
        m_selected = QDate::currentDate();
        buildUi();
        if (context.calendar != nullptr) {
            connect(context.calendar, &Services::CalendarStore::eventsChanged, this, [this]() {
                m_grid->update();
                refreshAgenda();
            });
        }
        refreshAgenda();
        setSelectedDate(m_selected);
    }

    QString windowTitle() const override { return QStringLiteral("Calendar"); }
    QString subtitle() const override
    {
        return QLocale::system().toString(QDate::currentDate(), QStringLiteral("MMMM yyyy"));
    }

    void activate() override { refreshAgenda(); }

private:
    void buildUi()
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(18, 14, 18, 16);
        root->setSpacing(10);

        auto* header = new QHBoxLayout;
        header->setContentsMargins(0, 0, 0, 0);
        auto* heading = new QLabel(QStringLiteral("Calendar"), this);
        heading->setProperty("role", QStringLiteral("title"));
        heading->setFont(Theme::ThemeManager::instance().font(23.0, QFont::DemiBold));
        header->addWidget(heading);
        header->addStretch(1);
        m_addButton = new Theme::AeroButton(QStringLiteral("New Event"), QStringLiteral("plus"), this);
        m_addButton->setVariant(Theme::AeroButton::Variant::Primary);
        connect(m_addButton, &QAbstractButton::clicked, this, [this]() { addEvent(); });
        header->addWidget(m_addButton);
        root->addLayout(header);

        auto* splitter = new QSplitter(Qt::Horizontal, this);
        splitter->setChildrenCollapsible(false);
        splitter->setHandleWidth(1);

        auto* left = new QWidget(splitter);
        auto* leftLayout = new QVBoxLayout(left);
        leftLayout->setContentsMargins(0, 0, 10, 0);
        leftLayout->setSpacing(8);

        auto* monthBar = new QHBoxLayout;
        monthBar->setContentsMargins(0, 0, 0, 0);
        monthBar->setSpacing(4);
        auto* previous = new Theme::IconButton(QStringLiteral("chevron-left"), left);
        previous->setToolTip(QStringLiteral("Previous Month"));
        connect(previous, &QAbstractButton::clicked, this, [this]() { navigateMonth(-1); });
        m_monthLabel = new QLabel(left);
        m_monthLabel->setProperty("role", QStringLiteral("title"));
        m_monthLabel->setFont(Theme::ThemeManager::instance().font(16.0, QFont::DemiBold));
        auto* next = new Theme::IconButton(QStringLiteral("chevron-right"), left);
        next->setToolTip(QStringLiteral("Next Month"));
        connect(next, &QAbstractButton::clicked, this, [this]() { navigateMonth(1); });
        auto* today = new Theme::AeroButton(QStringLiteral("Today"), left);
        today->setVariant(Theme::AeroButton::Variant::Ghost);
        connect(today, &QAbstractButton::clicked, this, [this]() {
            setSelectedDate(QDate::currentDate());
            m_grid->setMonth(QDate::currentDate());
        });
        monthBar->addWidget(previous);
        monthBar->addWidget(m_monthLabel);
        monthBar->addWidget(next);
        monthBar->addStretch(1);
        monthBar->addWidget(today);
        leftLayout->addLayout(monthBar);

        m_grid = new MonthGrid(left);
        m_grid->hasEvents = [this](const QDate& date) {
            return context().calendar != nullptr && !context().calendar->eventsForDate(date).isEmpty();
        };
        m_grid->onDateSelected = [this](const QDate& date) { setSelectedDate(date); };
        leftLayout->addWidget(m_grid, 1);
        splitter->addWidget(left);

        auto* right = new QWidget(splitter);
        auto* rightLayout = new QVBoxLayout(right);
        rightLayout->setContentsMargins(10, 0, 0, 0);
        rightLayout->setSpacing(8);

        m_agendaTitle = new QLabel(right);
        m_agendaTitle->setProperty("role", QStringLiteral("title"));
        m_agendaTitle->setFont(Theme::ThemeManager::instance().font(16.0, QFont::DemiBold));
        rightLayout->addWidget(m_agendaTitle);

        m_agendaSubtitle = Widgets::mutedLabel(QString(), right);
        m_agendaSubtitle->setFont(Theme::ThemeManager::instance().captionFont());
        rightLayout->addWidget(m_agendaSubtitle);

        m_agendaStack = new QStackedWidget(right);
        m_eventList = new QListWidget(m_agendaStack);
        m_eventList->setFrameShape(QFrame::NoFrame);
        m_eventList->setIconSize(QSize(12, 12));
        m_eventList->setSpacing(2);
        m_eventList->setWordWrap(true);
        m_agendaStack->addWidget(m_eventList);
        m_agendaStack->addWidget(Widgets::emptyState(QStringLiteral("calendar"),
                                                      QStringLiteral("Nothing scheduled"),
                                                      QStringLiteral("Add an event for this day, or choose another date in the month."),
                                                      m_agendaStack));
        rightLayout->addWidget(m_agendaStack, 1);

        auto* actions = new QHBoxLayout;
        actions->setContentsMargins(0, 0, 0, 0);
        actions->setSpacing(6);
        m_editButton = new Theme::AeroButton(QStringLiteral("Edit"), right);
        m_editButton->setEnabled(false);
        m_deleteButton = new Theme::AeroButton(QStringLiteral("Delete"), QStringLiteral("trash"), right);
        m_deleteButton->setVariant(Theme::AeroButton::Variant::Ghost);
        m_deleteButton->setEnabled(false);
        actions->addWidget(m_editButton);
        actions->addWidget(m_deleteButton);
        actions->addStretch(1);
        rightLayout->addLayout(actions);

        splitter->addWidget(right);
        splitter->setStretchFactor(0, 3);
        splitter->setStretchFactor(1, 2);
        root->addWidget(splitter, 1);

        connect(m_eventList, &QListWidget::itemSelectionChanged, this, [this]() {
            const bool has = currentEventId().isEmpty() == false;
            m_editButton->setEnabled(has);
            m_deleteButton->setEnabled(has);
        });
        connect(m_eventList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { editEvent(); });
        connect(m_editButton, &QAbstractButton::clicked, this, [this]() { editEvent(); });
        connect(m_deleteButton, &QAbstractButton::clicked, this, [this]() { deleteEvent(); });

        if (context().calendar == nullptr) {
            m_addButton->setEnabled(false);
            m_agendaSubtitle->setText(QStringLiteral("The calendar store is not available in this session."));
        }
    }

    void navigateMonth(int delta)
    {
        const QDate month = m_grid->month().addMonths(delta);
        m_grid->setMonth(month);
        updateMonthLabel();
    }

    void updateMonthLabel()
    {
        m_monthLabel->setText(QLocale::system().toString(m_grid->month(), QStringLiteral("MMMM yyyy")));
    }

    void setSelectedDate(const QDate& date)
    {
        m_selected = date;
        m_grid->setSelected(date);
        updateMonthLabel();
        refreshAgenda();
    }

    void refreshAgenda()
    {
        m_eventList->clear();
        if (m_agendaTitle == nullptr) {
            return;
        }
        m_agendaTitle->setText(QLocale::system().toString(m_selected, QStringLiteral("dddd, d MMMM")));
        if (context().calendar == nullptr) {
            m_agendaSubtitle->setText(QStringLiteral("Calendar service unavailable"));
            m_agendaStack->setCurrentIndex(1);
            return;
        }

        QList<Services::CalendarEvent> events = context().calendar->eventsForDate(m_selected);
        std::sort(events.begin(), events.end(), [](const Services::CalendarEvent& a, const Services::CalendarEvent& b) {
            if (a.allDay != b.allDay) {
                return a.allDay;
            }
            return a.start < b.start;
        });

        m_agendaSubtitle->setText(events.isEmpty()
                                      ? QStringLiteral("No events")
                                      : (events.size() == 1 ? QStringLiteral("1 event") : QStringLiteral("%1 events").arg(events.size())));

        for (const Services::CalendarEvent& event : events) {
            QString time;
            if (event.allDay) {
                time = QStringLiteral("All day");
            } else {
                time = event.start.time().toString(QStringLiteral("HH:mm"));
                if (event.end.isValid()) {
                    time += QStringLiteral("–") + event.end.time().toString(QStringLiteral("HH:mm"));
                }
            }
            QString label = QStringLiteral("%1\n%2").arg(event.title, time);
            if (!event.location.isEmpty()) {
                label += QStringLiteral(" · %1").arg(event.location);
            }
            auto* item = new QListWidgetItem(label, m_eventList);
            item->setData(Qt::UserRole, event.id);
            item->setIcon(colorDot(QColor(event.color)));
            item->setSizeHint(QSize(0, 48));
            item->setToolTip(event.notes.isEmpty() ? event.title : event.notes);
        }

        m_agendaStack->setCurrentIndex(events.isEmpty() ? 1 : 0);
        m_editButton->setEnabled(false);
        m_deleteButton->setEnabled(false);
    }

    QString currentEventId() const
    {
        QListWidgetItem* item = m_eventList->currentItem();
        return item != nullptr ? item->data(Qt::UserRole).toString() : QString();
    }

    Services::CalendarEvent findEvent(const QString& id) const
    {
        if (context().calendar == nullptr) {
            return {};
        }
        for (const Services::CalendarEvent& event : context().calendar->events()) {
            if (event.id == id) {
                return event;
            }
        }
        return {};
    }

    void addEvent()
    {
        if (context().calendar == nullptr) {
            return;
        }
        EventDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        Services::CalendarEvent event = dialog.event();
        if (!event.isValid()) {
            return;
        }
        context().calendar->addEvent(event);
        context().notify(QStringLiteral("Event added"), event.title, QStringLiteral("calendar"));
        setSelectedDate(event.start.date());
    }

    void editEvent()
    {
        const QString id = currentEventId();
        if (id.isEmpty() || context().calendar == nullptr) {
            return;
        }
        const Services::CalendarEvent existing = findEvent(id);
        if (!existing.isValid()) {
            return;
        }
        EventDialog dialog(this, &existing);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        Services::CalendarEvent updated = dialog.event();
        updated.id = id;
        if (context().calendar->updateEvent(updated)) {
            setSelectedDate(updated.start.date());
        }
    }

    void deleteEvent()
    {
        const QString id = currentEventId();
        if (id.isEmpty() || context().calendar == nullptr) {
            return;
        }
        const Services::CalendarEvent existing = findEvent(id);
        const auto answer = QMessageBox::question(
            this,
            QStringLiteral("Delete Event"),
            QStringLiteral("Delete “%1”?").arg(existing.title),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (answer == QMessageBox::Yes) {
            context().calendar->removeEvent(id);
        }
    }

    MonthGrid* m_grid = nullptr;
    QLabel* m_monthLabel = nullptr;
    QLabel* m_agendaTitle = nullptr;
    QLabel* m_agendaSubtitle = nullptr;
    QStackedWidget* m_agendaStack = nullptr;
    QListWidget* m_eventList = nullptr;
    Theme::AeroButton* m_addButton = nullptr;
    Theme::AeroButton* m_editButton = nullptr;
    Theme::AeroButton* m_deleteButton = nullptr;
    QDate m_selected;
};

} // namespace

AppView* createCalendarApp(const AppContext& context, QWidget* parent)
{
    return new CalendarView(context, parent);
}

} // namespace Aero::Applications
