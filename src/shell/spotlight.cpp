#include "aero/shell/spotlight.h"

#include "aero/apps/app_registry.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/style.h"
#include "aero/theme/theme.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace Aero::Shell {
namespace {

constexpr int kKindRole = Qt::UserRole;
constexpr int kAppRole = Qt::UserRole + 1;
constexpr int kPathRole = Qt::UserRole + 2;
constexpr int kActionRole = Qt::UserRole + 3;

struct QuickAction {
    QString id;
    QString title;
    QString subtitle;
    QString icon;
    QStringList keywords;
};

QList<QuickAction> quickActions()
{
    return {
        {QStringLiteral("show-desktop"), QStringLiteral("Show Desktop"),
         QStringLiteral("Minimize every window on this desktop"), QStringLiteral("desktop"),
         {QStringLiteral("desktop"), QStringLiteral("minimize"), QStringLiteral("hide")}},
        {QStringLiteral("workspace-overview"), QStringLiteral("Workspace Overview"),
         QStringLiteral("See all desktops and their windows"), QStringLiteral("grid"),
         {QStringLiteral("spaces"), QStringLiteral("mission control"), QStringLiteral("windows")}},
        {QStringLiteral("control-center"), QStringLiteral("Control Center"),
         QStringLiteral("Wi-Fi, Bluetooth, sound and display"), QStringLiteral("palette"),
         {QStringLiteral("wifi"), QStringLiteral("volume"), QStringLiteral("brightness")}},
        {QStringLiteral("settings"), QStringLiteral("Open Settings"),
         QStringLiteral("Appearance, dock, network and more"), QStringLiteral("gear"),
         {QStringLiteral("preferences"), QStringLiteral("appearance"), QStringLiteral("system")}},
        {QStringLiteral("lock-screen"), QStringLiteral("Lock Screen"),
         QStringLiteral("Secure the current session"), QStringLiteral("lock"),
         {QStringLiteral("lock"), QStringLiteral("secure"), QStringLiteral("session")}},
        {QStringLiteral("about"), QStringLiteral("About Aero Desktop"),
         QStringLiteral("Version, backend and diagnostics"), QStringLiteral("info"),
         {QStringLiteral("version"), QStringLiteral("system"), QStringLiteral("credits")}},
    };
}

bool matches(const QString& query, const QStringList& fields)
{
    if (query.isEmpty()) {
        return true;
    }
    for (const QString& field : fields) {
        if (field.contains(query, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

} // namespace

Spotlight::Spotlight(Services::SearchService* searchService, QWidget* parent)
    : Overlay(parent)
    , m_searchService(searchService)
{
    setCornerRadius(24);
    setShadowMargin(24);
    setTintStrength(1.12);
    resize(680, 500);
    build();

    if (m_searchService != nullptr) {
        connect(m_searchService, &Services::SearchService::resultsReady, this,
                [this](const QString& query, const QList<Services::SearchResult>& results) {
                    if (!isOpen() || query != m_currentQuery) {
                        return;
                    }
                    addFileResults(query, results);
                });
    }
}

void Spotlight::build()
{
    const int margin = shadowMargin();
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(margin + 18, margin + 16, margin + 18, margin + 14);
    root->setSpacing(10);

    auto* searchRow = new QHBoxLayout;
    searchRow->setSpacing(10);
    auto* searchIcon = new QLabel(this);
    searchIcon->setPixmap(Theme::IconFactory::pixmap(
        QStringLiteral("search"), 22,
        Theme::ThemeManager::instance().tokens().color.textSecondary,
        devicePixelRatioF()));
    searchIcon->setFixedSize(24, 24);
    searchRow->addWidget(searchIcon, 0, Qt::AlignVCenter);

    m_field = new QLineEdit(this);
    m_field->setPlaceholderText(QStringLiteral("Search applications, settings and files"));
    m_field->setClearButtonEnabled(true);
    m_field->setFont(Theme::ThemeManager::instance().font(18.0, QFont::Normal));
    m_field->setMinimumHeight(42);
    m_field->installEventFilter(this);
    connect(m_field, &QLineEdit::textChanged, this, &Spotlight::setQuery);
    searchRow->addWidget(m_field, 1);

    auto* hint = new QLabel(QStringLiteral("esc"), this);
    hint->setProperty("role", QStringLiteral("muted"));
    hint->setFont(Theme::ThemeManager::instance().font(10.0, QFont::DemiBold));
    hint->setStyleSheet(QStringLiteral("QLabel { border: 1px solid %1; border-radius: 5px; padding: 2px 6px; }")
                            .arg(Theme::Style::cssColor(
                                Theme::ThemeManager::instance().tokens().color.separator)));
    searchRow->addWidget(hint, 0, Qt::AlignVCenter);
    root->addLayout(searchRow);

    m_results = new QListWidget(this);
    m_results->setFrameShape(QFrame::NoFrame);
    m_results->setSelectionMode(QAbstractItemView::SingleSelection);
    m_results->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_results->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_results->setSpacing(2);
    m_results->setUniformItemSizes(true);
    m_results->setMouseTracking(true);
    m_results->installEventFilter(this);
    connect(m_results, &QListWidget::itemActivated, this, [this](QListWidgetItem*) {
        activateCurrent();
    });
    connect(m_results, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        activateCurrent();
    });
    root->addWidget(m_results, 1);

    auto* footer = new QLabel(
        QStringLiteral("↑ ↓ Navigate     ↵ Open     esc Close"), this);
    footer->setProperty("role", QStringLiteral("muted"));
    footer->setFont(Theme::ThemeManager::instance().captionFont());
    footer->setAlignment(Qt::AlignCenter);
    root->addWidget(footer);
}

void Spotlight::prepareShow()
{
    m_currentQuery.clear();
    if (m_field != nullptr) {
        const QSignalBlocker blocker(m_field);
        m_field->clear();
    }
    rebuildResults();
    QTimer::singleShot(0, this, [this]() {
        if (m_field != nullptr) {
            m_field->setFocus(Qt::PopupFocusReason);
        }
        if (m_results != nullptr && m_results->count() > 0) {
            m_results->setCurrentRow(0);
        }
    });
}

void Spotlight::prepareHide()
{
    if (m_searchService != nullptr) {
        m_searchService->cancel();
    }
}

void Spotlight::setQuery(const QString& query)
{
    m_currentQuery = query.trimmed();
    rebuildResults();
    if (m_searchService != nullptr) {
        if (m_currentQuery.isEmpty()) {
            m_searchService->cancel();
        } else {
            m_searchService->search(m_currentQuery);
        }
    }
}

void Spotlight::clearRows()
{
    if (m_results != nullptr) {
        m_results->clear();
    }
}

void Spotlight::rebuildResults()
{
    if (m_results == nullptr) {
        return;
    }
    clearRows();
    addStaticResults(m_currentQuery);
}

void Spotlight::addStaticResults(const QString& query)
{
    const Theme::ThemeTokens& tokens = Theme::ThemeManager::instance().tokens();
    const QColor secondary = tokens.color.textSecondary;

    const QList<Applications::AppDefinition> apps = Applications::AppRegistry::instance().apps();
    for (const Applications::AppDefinition& app : apps) {
        QStringList fields = {app.name, app.description, app.category};
        fields.append(app.keywords);
        if (!matches(query, fields)) {
            continue;
        }

        Services::SearchResult result;
        result.kind = Services::SearchKind::Application;
        result.title = app.name;
        result.subtitle = app.description;
        result.appId = app.id;
        result.iconName = Theme::IconFactory::glyphForApp(app.id);

        auto* item = new QListWidgetItem(m_results);
        item->setData(kKindRole, static_cast<int>(Services::SearchKind::Application));
        item->setData(kAppRole, app.id);
        item->setSizeHint(QSize(0, 56));

        auto* row = new QWidget(m_results);
        row->setAttribute(Qt::WA_TranslucentBackground, true);
        row->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(10, 6, 10, 6);
        rowLayout->setSpacing(12);
        auto* iconLabel = new QLabel(row);
        iconLabel->setFixedSize(32, 32);
        iconLabel->setPixmap(Theme::IconFactory::appIcon(app.id, 32).pixmap(32, 32));
        rowLayout->addWidget(iconLabel);
        auto* textHost = new QWidget(row);
        auto* textLayout = new QVBoxLayout(textHost);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(0);
        auto* title = new QLabel(app.name, textHost);
        title->setFont(Theme::ThemeManager::instance().bodyFont(QFont::DemiBold));
        auto* subtitle = new QLabel(app.description, textHost);
        subtitle->setProperty("role", QStringLiteral("muted"));
        subtitle->setFont(Theme::ThemeManager::instance().captionFont());
        textLayout->addWidget(title);
        textLayout->addWidget(subtitle);
        rowLayout->addWidget(textHost, 1);
        m_results->setItemWidget(item, row);
    }

    for (const QuickAction& action : quickActions()) {
        if (!matches(query, {action.title, action.subtitle, action.keywords.join(QLatin1Char(' '))})) {
            continue;
        }

        auto* item = new QListWidgetItem(m_results);
        item->setData(kKindRole, static_cast<int>(Services::SearchKind::Action));
        item->setData(kActionRole, action.id);
        item->setSizeHint(QSize(0, 48));

        auto* row = new QWidget(m_results);
        row->setAttribute(Qt::WA_TranslucentBackground, true);
        row->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(10, 4, 10, 4);
        rowLayout->setSpacing(12);
        auto* iconLabel = new QLabel(row);
        iconLabel->setFixedSize(26, 26);
        iconLabel->setPixmap(Theme::IconFactory::pixmap(action.icon, 22, secondary, devicePixelRatioF()));
        rowLayout->addWidget(iconLabel);
        auto* textHost = new QWidget(row);
        auto* textLayout = new QVBoxLayout(textHost);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(0);
        auto* title = new QLabel(action.title, textHost);
        title->setFont(Theme::ThemeManager::instance().bodyFont());
        auto* subtitle = new QLabel(action.subtitle, textHost);
        subtitle->setProperty("role", QStringLiteral("muted"));
        subtitle->setFont(Theme::ThemeManager::instance().captionFont());
        textLayout->addWidget(title);
        textLayout->addWidget(subtitle);
        rowLayout->addWidget(textHost, 1);
        m_results->setItemWidget(item, row);
    }

    if (m_results->count() == 0 && !query.isEmpty()) {
        auto* item = new QListWidgetItem(QStringLiteral("No matching applications or actions"), m_results);
        item->setFlags(Qt::NoItemFlags);
        item->setSizeHint(QSize(0, 40));
    }

    if (m_results->count() > 0) {
        m_results->setCurrentRow(0);
    }
}

void Spotlight::addFileResults(const QString& query, const QList<Services::SearchResult>& results)
{
    if (m_results == nullptr || query != m_currentQuery || results.isEmpty()) {
        return;
    }

    QSet<QString> existing;
    for (int i = 0; i < m_results->count(); ++i) {
        const QString path = m_results->item(i)->data(kPathRole).toString();
        if (!path.isEmpty()) {
            existing.insert(path);
        }
    }

    const Theme::ThemeTokens& tokens = Theme::ThemeManager::instance().tokens();
    for (const Services::SearchResult& result : results) {
        if (result.kind != Services::SearchKind::File || existing.contains(result.path)) {
            continue;
        }
        existing.insert(result.path);

        auto* item = new QListWidgetItem(m_results);
        item->setData(kKindRole, static_cast<int>(Services::SearchKind::File));
        item->setData(kPathRole, result.path);
        item->setSizeHint(QSize(0, 48));

        auto* row = new QWidget(m_results);
        row->setAttribute(Qt::WA_TranslucentBackground, true);
        row->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(10, 4, 10, 4);
        rowLayout->setSpacing(12);
        auto* iconLabel = new QLabel(row);
        iconLabel->setFixedSize(26, 26);
        const QString glyph = result.iconName.isEmpty() ? QStringLiteral("file") : result.iconName;
        iconLabel->setPixmap(Theme::IconFactory::pixmap(glyph, 22, tokens.color.textSecondary,
                                                        devicePixelRatioF()));
        rowLayout->addWidget(iconLabel);
        auto* textHost = new QWidget(row);
        auto* textLayout = new QVBoxLayout(textHost);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(0);
        auto* title = new QLabel(result.title, textHost);
        title->setFont(Theme::ThemeManager::instance().bodyFont());
        auto* subtitle = new QLabel(result.subtitle, textHost);
        subtitle->setProperty("role", QStringLiteral("muted"));
        subtitle->setFont(Theme::ThemeManager::instance().captionFont());
        textLayout->addWidget(title);
        textLayout->addWidget(subtitle);
        rowLayout->addWidget(textHost, 1);
        m_results->setItemWidget(item, row);
    }
}

void Spotlight::activateCurrent()
{
    if (m_results == nullptr) {
        return;
    }
    QListWidgetItem* item = m_results->currentItem();
    if (item == nullptr) {
        return;
    }

    const int kind = item->data(kKindRole).toInt();
    if (kind == static_cast<int>(Services::SearchKind::Application)) {
        const QString appId = item->data(kAppRole).toString();
        if (!appId.isEmpty()) {
            dismiss();
            emit applicationRequested(appId);
        }
    } else if (kind == static_cast<int>(Services::SearchKind::File)) {
        const QString path = item->data(kPathRole).toString();
        if (!path.isEmpty()) {
            dismiss();
            emit fileRequested(path);
        }
    } else if (kind == static_cast<int>(Services::SearchKind::Action)) {
        const QString action = item->data(kActionRole).toString();
        if (!action.isEmpty()) {
            dismiss();
            emit actionRequested(action);
        }
    }
}

void Spotlight::moveSelection(int delta)
{
    if (m_results == nullptr || m_results->count() == 0) {
        return;
    }
    int row = m_results->currentRow();
    if (row < 0) {
        row = 0;
    } else {
        row = (row + delta + m_results->count()) % m_results->count();
    }
    m_results->setCurrentRow(row);
    if (QListWidgetItem* item = m_results->item(row)) {
        m_results->scrollToItem(item, QAbstractItemView::EnsureVisible);
    }
}

bool Spotlight::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::KeyPress) {
        return Overlay::eventFilter(watched, event);
    }
    if (watched != m_field && watched != m_results) {
        return Overlay::eventFilter(watched, event);
    }

    auto* key = static_cast<QKeyEvent*>(event);
    switch (key->key()) {
    case Qt::Key_Down:
        moveSelection(1);
        return true;
    case Qt::Key_Up:
        moveSelection(-1);
        return true;
    case Qt::Key_PageDown:
        moveSelection(6);
        return true;
    case Qt::Key_PageUp:
        moveSelection(-6);
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        activateCurrent();
        return true;
    case Qt::Key_Escape:
        dismiss();
        return true;
    default:
        break;
    }

    return Overlay::eventFilter(watched, event);
}

} // namespace Aero::Shell
