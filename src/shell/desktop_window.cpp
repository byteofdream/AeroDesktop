#include "aero/shell/desktop_window.h"

#include "aero/compositor/window.h"
#include "aero/compositor/window_manager.h"
#include "aero/compositor/workspace_model.h"
#include "aero/core/paths.h"
#include "aero/graphics/backdrop_provider.h"
#include "aero/graphics/glass_panel.h"
#include "aero/platform/i_file_system.h"
#include "aero/platform/platform.h"
#include "aero/services/notification_bus.h"
#include "aero/settings/settings_keys.h"
#include "aero/settings/settings_manager.h"
#include "aero/shell/dock.h"
#include "aero/shell/overlay.h"
#include "aero/shell/top_bar.h"
#include "aero/shell/wallpaper_view.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/style.h"
#include "aero/theme/theme.h"

#include <QApplication>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSet>
#include <QShowEvent>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace Aero::Shell {
namespace {
constexpr int kTopBarHeight = 40;
constexpr int kDockGap = 8;
constexpr int kWorkAreaGap = 8;
} // namespace

DesktopWindow::DesktopWindow(Platform::Platform* platform,
                             Settings::SettingsManager* settings,
                             Services::NotificationBus* notifications,
                             QWidget* parent)
    : QWidget(parent)
    , m_platform(platform)
    , m_settings(settings)
    , m_notifications(notifications)
{
    setWindowTitle(QStringLiteral("Aero Desktop"));
    setWindowIcon(Theme::IconFactory::appIcon(QStringLiteral("aero-about"), 64));
    setMinimumSize(900, 600);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAutoFillBackground(false);

    buildUi();

    if (QApplication::instance() != nullptr) {
        QApplication::instance()->installEventFilter(this);
    }

    if (m_settings != nullptr) {
        connect(m_settings, &Settings::SettingsManager::settingChanged, this,
                [this](const QString& key, const QVariant&) {
                    if (key.startsWith(QStringLiteral("wallpaper."))) {
                        if (m_wallpaper != nullptr) {
                            m_wallpaper->reload();
                        }
                        scheduleBackdrop();
                    } else if (key.startsWith(QStringLiteral("dock."))) {
                        updateDockMode();
                        layoutChildren();
                    } else if (key == QString::fromLatin1(Settings::Keys::DesktopShowIcons)) {
                        refreshDesktopIcons();
                        layoutChildren();
                    } else if (key == QString::fromLatin1(Settings::Keys::DesktopDoubleClick)) {
                        refreshDesktopIcons();
                    } else if (key == QString::fromLatin1(Settings::Keys::Blur)
                               || key == QString::fromLatin1(Settings::Keys::Transparency)) {
                        scheduleBackdrop();
                    }
                });
    }

    connect(&Theme::ThemeManager::instance(), &Theme::ThemeManager::themeChanged, this, [this]() {
        refreshDesktopIcons();
        scheduleBackdrop();
        update();
    });
}

DesktopWindow::~DesktopWindow()
{
    if (QApplication::instance() != nullptr) {
        QApplication::instance()->removeEventFilter(this);
    }
}

void DesktopWindow::buildUi()
{
    m_wallpaper = new WallpaperView(m_settings, this);
    m_wallpaper->setGeometry(rect());
    m_wallpaper->lower();

    m_workspaceHost = new QWidget(this);
    m_workspaceHost->setAttribute(Qt::WA_NoSystemBackground, true);
    m_workspaceHost->setAutoFillBackground(false);
    m_workspaceHost->setMouseTracking(true);
    m_workspaceHost->installEventFilter(this);
    m_workspaceHost->setGeometry(rect());

    m_windowManager = new Compositor::WindowManager(m_workspaceHost, this);

    buildDesktopIcons();

    m_topBar = new TopBar(m_platform, m_settings, m_notifications, m_windowManager, this);
    m_topBar->setGeometry(0, 0, width(), kTopBarHeight);
    m_topBar->raise();

    m_dock = new Dock(m_settings, m_windowManager, this);
    connect(m_dock, &Dock::preferredSizeChanged, this, [this]() {
        if (isVisible()) {
            layoutChildren();
        }
    });

    m_backdropTimer = new QTimer(this);
    m_backdropTimer->setSingleShot(true);
    m_backdropTimer->setInterval(100);
    connect(m_backdropTimer, &QTimer::timeout, this, &DesktopWindow::refreshBackdrop);

    m_dockHideTimer = new QTimer(this);
    m_dockHideTimer->setSingleShot(true);
    m_dockHideTimer->setInterval(1500);
    connect(m_dockHideTimer, &QTimer::timeout, this, [this]() {
        if (m_dock == nullptr || m_settings == nullptr || !m_settings->dockAutoHide()) {
            return;
        }
        if (m_dock->underMouse()) {
            scheduleDockHide();
            return;
        }
        hideDock();
    });

    m_dock->installEventFilter(this);
    updateDockMode();
    layoutChildren();
}

void DesktopWindow::buildDesktopIcons()
{
    if (m_workspaceHost == nullptr) {
        return;
    }
    if (m_desktopIcons == nullptr) {
        m_desktopIcons = new QListWidget(m_workspaceHost);
        m_desktopIcons->setObjectName(QStringLiteral("aeroDesktopIcons"));
        m_desktopIcons->setFrameShape(QFrame::NoFrame);
        m_desktopIcons->setViewMode(QListView::IconMode);
        m_desktopIcons->setMovement(QListView::Static);
        m_desktopIcons->setResizeMode(QListView::Adjust);
        m_desktopIcons->setWrapping(false);
        m_desktopIcons->setUniformItemSizes(true);
        m_desktopIcons->setIconSize(QSize(40, 40));
        m_desktopIcons->setGridSize(QSize(98, 88));
        m_desktopIcons->setSpacing(2);
        m_desktopIcons->setWordWrap(true);
        m_desktopIcons->setTextElideMode(Qt::ElideRight);
        m_desktopIcons->setSelectionMode(QAbstractItemView::SingleSelection);
        m_desktopIcons->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_desktopIcons->setDragEnabled(false);
        m_desktopIcons->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_desktopIcons->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_desktopIcons->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        m_desktopIcons->setFocusPolicy(Qt::ClickFocus);
        m_desktopIcons->installEventFilter(this);

        connect(m_desktopIcons, &QListWidget::itemDoubleClicked, this,
                [this](QListWidgetItem* item) {
                    if (item == nullptr) {
                        return;
                    }
                    const qint64 now = QDateTime::currentMSecsSinceEpoch();
                    if (now - m_lastDesktopOpenMs < 250) {
                        return;
                    }
                    m_lastDesktopOpenMs = now;
                    const QString path = item->data(Qt::UserRole).toString();
                    if (!path.isEmpty()) {
                        emit openPathRequested(path);
                    }
                });
        connect(m_desktopIcons, &QListWidget::itemClicked, this,
                [this](QListWidgetItem* item) {
                    if (m_settings != nullptr && m_settings->value(
                            QString::fromLatin1(Settings::Keys::DesktopDoubleClick), true).toBool()) {
                        return;
                    }
                    if (item == nullptr) {
                        return;
                    }
                    const qint64 now = QDateTime::currentMSecsSinceEpoch();
                    if (now - m_lastDesktopOpenMs < 350) {
                        return;
                    }
                    m_lastDesktopOpenMs = now;
                    const QString path = item->data(Qt::UserRole).toString();
                    if (!path.isEmpty()) {
                        emit openPathRequested(path);
                    }
                });
    }

    refreshDesktopIcons();
}

void DesktopWindow::refreshDesktopIcons()
{
    if (m_desktopIcons == nullptr) {
        return;
    }
    const bool enabled = m_settings == nullptr
        || m_settings->value(QString::fromLatin1(Settings::Keys::DesktopShowIcons), true).toBool();
    m_desktopIcons->setVisible(enabled);
    if (!enabled) {
        return;
    }

    const Theme::ThemeTokens& tokens = Theme::ThemeManager::instance().tokens();
    QColor textColor = tokens.color.textPrimary;
    if (!Theme::ThemeManager::instance().isDark()) {
        textColor = QColor(0x2B, 0x24, 0x3D);
    }
    QColor selection = tokens.color.accent;
    selection.setAlphaF(0.18);
    m_desktopIcons->setStyleSheet(
        QStringLiteral("QListWidget#aeroDesktopIcons { background: transparent; border: none; padding: 4px; }"
                       "QListWidget::item { color: %1; border-radius: 9px; padding: 5px; }"
                       "QListWidget::item:selected { background: %2; color: %3; }"
                       "QListWidget::item:hover { background: rgba(255,255,255,0.10); }")
            .arg(Theme::Style::cssColor(textColor),
                 Theme::Style::cssColor(selection),
                 Theme::Style::cssColor(tokens.color.textPrimary)));

    m_desktopIcons->clear();
    const Platform::IFileSystem* fileSystem = m_platform != nullptr ? m_platform->fileSystem() : nullptr;

    struct Entry {
        QString label;
        QString path;
        QString icon;
    };
    QList<Entry> entries;

    const QString home = fileSystem != nullptr ? fileSystem->homePath() : QDir::homePath();
    entries.append({QStringLiteral("Home"), home, QStringLiteral("home")});

    auto addLocation = [&entries, fileSystem](Platform::StandardLocation location,
                                              const QString& fallbackName,
                                              const QString& icon) {
        QString path;
        if (fileSystem != nullptr) {
            path = fileSystem->standardLocation(location);
        } else {
            path = QDir::homePath() + QStringLiteral("/") + fallbackName;
        }
        if (!path.isEmpty() && QFileInfo(path).isDir()) {
            entries.append({fallbackName, QDir::cleanPath(path), icon});
        }
    };

    addLocation(Platform::StandardLocation::Documents, QStringLiteral("Documents"), QStringLiteral("documents"));
    addLocation(Platform::StandardLocation::Downloads, QStringLiteral("Downloads"), QStringLiteral("downloads"));
    addLocation(Platform::StandardLocation::Pictures, QStringLiteral("Pictures"), QStringLiteral("pictures"));
    addLocation(Platform::StandardLocation::Music, QStringLiteral("Music"), QStringLiteral("music"));
    addLocation(Platform::StandardLocation::Videos, QStringLiteral("Videos"), QStringLiteral("video"));

    const QString trashPath = QDir::homePath() + QStringLiteral("/.local/share/Trash/files");
    if (QFileInfo(trashPath).isDir()) {
        entries.append({QStringLiteral("Trash"), trashPath, QStringLiteral("trash")});
    }

    QSet<QString> seen;
    for (const Entry& entry : std::as_const(entries)) {
        if (entry.path.isEmpty() || seen.contains(entry.path)) {
            continue;
        }
        seen.insert(entry.path);
        auto* item = new QListWidgetItem(Theme::IconFactory::icon(entry.icon, 40, textColor),
                                         entry.label, m_desktopIcons);
        item->setData(Qt::UserRole, entry.path);
        item->setToolTip(entry.path);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        item->setForeground(textColor);
    }
}

void DesktopWindow::layoutChildren()
{
    if (m_wallpaper != nullptr) {
        m_wallpaper->setGeometry(rect());
    }
    if (m_workspaceHost != nullptr) {
        m_workspaceHost->setGeometry(rect());
    }
    if (m_topBar != nullptr) {
        m_topBar->setGeometry(0, 0, width(), kTopBarHeight);
        m_topBar->raise();
    }

    updateDockGeometry();

    const QRect area = workArea();
    if (m_desktopIcons != nullptr) {
        const int iconsWidth = qMin(132, qMax(96, area.width() / 7));
        const int iconsTop = area.top() + 10;
        m_desktopIcons->setGeometry(area.left() + 8,
                                    iconsTop,
                                    iconsWidth,
                                    qMax(80, area.bottom() - iconsTop + 1));
        m_desktopIcons->lower();
    }

    positionOverlays();
    updateWorkArea();
}

void DesktopWindow::updateDockGeometry()
{
    if (m_dock == nullptr || m_settings == nullptr || width() <= 0 || height() <= 0) {
        return;
    }

    const QString position = m_settings->dockPosition();
    const Qt::Orientation orientation = (position == QStringLiteral("left")
                                         || position == QStringLiteral("right"))
        ? Qt::Vertical
        : Qt::Horizontal;
    m_dock->setOrientation(orientation);

    const QSize dockSize = m_dock->preferredSize();
    const int margin = m_dock->shadowMargin();
    int x = 0;
    int y = 0;

    if (orientation == Qt::Horizontal) {
        x = (width() - dockSize.width()) / 2;
        y = height() - dockSize.height();
        if (m_dockConcealed) {
            y = height() - margin;
        }
    } else {
        y = (height() - dockSize.height()) / 2;
        y = qMax(kTopBarHeight + 12, y);
        if (position == QStringLiteral("left")) {
            x = m_dockConcealed ? (-dockSize.width() + margin) : 0;
        } else {
            x = m_dockConcealed ? (width() - margin) : (width() - dockSize.width());
        }
    }

    m_dock->setGeometry(x, y, dockSize.width(), dockSize.height());
    m_dock->raise();
}

void DesktopWindow::updateDockMode()
{
    if (m_dock == nullptr || m_settings == nullptr) {
        return;
    }
    const bool autoHide = m_settings->dockAutoHide();
    if (!autoHide) {
        m_dockConcealed = false;
        m_dock->setConcealed(false);
        m_dock->show();
        if (m_dockHideTimer != nullptr) {
            m_dockHideTimer->stop();
        }
        return;
    }
    if (m_dock->underMouse()) {
        m_dockConcealed = false;
        m_dock->setConcealed(false);
    } else {
        m_dockConcealed = true;
        m_dock->setConcealed(true);
    }
    m_dock->show();
    updateDockGeometry();
}

void DesktopWindow::hideDock()
{
    if (m_dock == nullptr || m_settings == nullptr || !m_settings->dockAutoHide()) {
        return;
    }
    if (m_dock->underMouse()) {
        scheduleDockHide();
        return;
    }
    m_dockConcealed = true;
    m_dock->setConcealed(true);
    updateDockGeometry();
}

void DesktopWindow::revealDock()
{
    if (m_dock == nullptr || m_settings == nullptr || !m_settings->dockAutoHide()) {
        return;
    }
    if (m_dockConcealed) {
        m_dockConcealed = false;
        m_dock->setConcealed(false);
        updateDockGeometry();
    }
    scheduleDockHide();
}

void DesktopWindow::scheduleDockHide()
{
    if (m_dockHideTimer == nullptr || m_settings == nullptr || !m_settings->dockAutoHide()) {
        return;
    }
    m_dockHideTimer->start();
}

void DesktopWindow::handleDockEdgePointer(const QPoint& globalPosition)
{
    if (m_settings == nullptr || !m_settings->dockAutoHide() || m_dock == nullptr) {
        return;
    }
    const QPoint local = mapFromGlobal(globalPosition);
    const QString position = m_settings->dockPosition();
    constexpr int edge = 5;
    bool nearEdge = false;
    if (position == QStringLiteral("left")) {
        nearEdge = local.x() <= edge;
    } else if (position == QStringLiteral("right")) {
        nearEdge = local.x() >= width() - edge;
    } else {
        nearEdge = local.y() >= height() - edge;
    }

    if (nearEdge) {
        revealDock();
        return;
    }
    if (!m_dockConcealed && !m_dock->geometry().contains(local)) {
        scheduleDockHide();
    }
}

QRect DesktopWindow::workArea() const
{
    int left = 0;
    int right = 0;
    int top = kTopBarHeight + kWorkAreaGap;
    int bottom = kWorkAreaGap;

    if (m_dock != nullptr && m_settings != nullptr) {
        const QRect dock = m_dock->geometry();
        const bool autoHide = m_settings->dockAutoHide();
        const int margin = m_dock->shadowMargin();
        const QString position = m_settings->dockPosition();
        if (!autoHide) {
            if (position == QStringLiteral("left")) {
                left = dock.x() + dock.width() - margin + kDockGap;
            } else if (position == QStringLiteral("right")) {
                right = width() - (dock.x() + margin) + kDockGap;
            } else {
                bottom = height() - (dock.y() + margin) + kDockGap;
            }
        }
    }

    const QRect area(left, top,
                     qMax(1, width() - left - right),
                     qMax(1, height() - top - bottom));
    return area;
}

QRect DesktopWindow::topBarRect() const
{
    return QRect(0, 0, width(), kTopBarHeight);
}

QRect DesktopWindow::dockRect() const
{
    return m_dock != nullptr ? m_dock->geometry() : QRect();
}

Compositor::WindowManager* DesktopWindow::windowManager() const
{
    return m_windowManager;
}

QWidget* DesktopWindow::workspaceHost() const
{
    return m_workspaceHost;
}

TopBar* DesktopWindow::topBar() const
{
    return m_topBar;
}

Dock* DesktopWindow::dock() const
{
    return m_dock;
}

void DesktopWindow::updateWorkArea()
{
    if (m_windowManager != nullptr) {
        m_windowManager->setWorkArea(workArea());
    }
}

void DesktopWindow::addOverlay(QWidget* overlay, OverlayPlacement placement)
{
    if (overlay == nullptr) {
        return;
    }
    overlay->setParent(this);
    overlay->hide();
    for (OverlayEntry& entry : m_overlays) {
        if (entry.widget == overlay) {
            entry.placement = placement;
            return;
        }
    }
    m_overlays.append({overlay, placement});
}

void DesktopWindow::showOverlay(QWidget* overlay)
{
    if (overlay == nullptr) {
        return;
    }
    hideOverlays(overlay);
    refreshBackdrop();
    positionOverlays();
    overlay->raise();
    if (auto* animated = qobject_cast<Overlay*>(overlay)) {
        animated->present();
    } else {
        overlay->show();
        overlay->raise();
    }
}

void DesktopWindow::hideOverlays(QWidget* except)
{
    for (const OverlayEntry& entry : std::as_const(m_overlays)) {
        if (entry.widget != nullptr && entry.widget != except && entry.widget->isVisible()) {
            entry.widget->hide();
        }
    }
}

bool DesktopWindow::isOverlayVisible(QWidget* overlay) const
{
    return overlay != nullptr && overlay->isVisible();
}

void DesktopWindow::positionOverlays()
{
    for (const OverlayEntry& entry : std::as_const(m_overlays)) {
        QWidget* widget = entry.widget;
        if (widget == nullptr) {
            continue;
        }

        QSize target = widget->size();
        if (!target.isValid() || target.isEmpty()) {
            target = widget->sizeHint();
        }
        if (target.isEmpty()) {
            target = QSize(360, 480);
        }

        QRect geometry;
        switch (entry.placement) {
        case OverlayPlacement::Center:
            geometry = QRect(QPoint((width() - target.width()) / 2,
                                    (height() - target.height()) / 2 - 32),
                             target);
            break;
        case OverlayPlacement::TopRight:
            geometry = QRect(QPoint(width() - target.width() - 20,
                                    kTopBarHeight + 10),
                             target);
            break;
        case OverlayPlacement::TopRightOffset:
            geometry = QRect(QPoint(width() - target.width() - 150,
                                    kTopBarHeight + 8),
                             target);
            break;
        case OverlayPlacement::BottomCenter: {
            const int dockReserve = (m_dock != nullptr && m_settings != nullptr
                                     && !m_settings->dockAutoHide())
                ? (height() - workArea().bottom() + 12)
                : 24;
            geometry = QRect(QPoint((width() - target.width()) / 2,
                                    height() - target.height() - dockReserve),
                             target);
            break;
        }
        case OverlayPlacement::FullScreen:
            geometry = rect();
            break;
        }

        geometry.moveLeft(qBound(-20, geometry.left(), qMax(-20, width() - geometry.width() + 20)));
        geometry.moveTop(qBound(0, geometry.top(), qMax(0, height() - geometry.height())));
        widget->setGeometry(geometry);
    }
}

void DesktopWindow::scheduleBackdrop()
{
    if (m_backdropTimer != nullptr) {
        m_backdropTimer->start();
    }
}

void DesktopWindow::refreshBackdrop()
{
    if (m_settings != nullptr
        && (!m_settings->blurEnabled() || !m_settings->transparencyEnabled())) {
        for (const OverlayEntry& entry : std::as_const(m_overlays)) {
            if (auto* panel = qobject_cast<Graphics::GlassPanel*>(entry.widget)) {
                panel->clearBackdrop();
            }
        }
        if (m_topBar != nullptr) {
            m_topBar->clearBackdrop();
        }
        if (m_dock != nullptr) {
            m_dock->clearBackdrop();
        }
        return;
    }

    if (!isVisible() || size().isEmpty()) {
        return;
    }

    // Temporarily remove the shell chrome from the snapshot: a glass panel
    // must never sample itself, otherwise repeated overlays create recursive
    // smearing along the screen edges.
    const bool topVisible = m_topBar != nullptr && m_topBar->isVisible();
    const bool dockVisible = m_dock != nullptr && m_dock->isVisible();
    if (m_topBar != nullptr) {
        m_topBar->hide();
    }
    if (m_dock != nullptr) {
        m_dock->hide();
    }

    const QPixmap backdrop = Graphics::BackdropProvider::capture(this, 30);

    if (m_topBar != nullptr && topVisible) {
        m_topBar->show();
        m_topBar->raise();
    }
    if (m_dock != nullptr && dockVisible) {
        m_dock->show();
        m_dock->raise();
    }

    updateGlassBackdrops(backdrop);
}

void DesktopWindow::updateGlassBackdrops(const QPixmap& backdrop)
{
    const QPoint origin = mapToGlobal(QPoint(0, 0));
    auto apply = [&backdrop, &origin](Graphics::GlassPanel* panel) {
        if (panel == nullptr) {
            return;
        }
        if (backdrop.isNull()) {
            panel->clearBackdrop();
        } else {
            panel->setBackdrop(backdrop, origin);
        }
    };

    apply(m_topBar);
    apply(m_dock);
    for (const OverlayEntry& entry : std::as_const(m_overlays)) {
        apply(qobject_cast<Graphics::GlassPanel*>(entry.widget));
    }
}

void DesktopWindow::applySettings()
{
    if (m_wallpaper != nullptr) {
        m_wallpaper->reload();
    }
    if (m_topBar != nullptr) {
        m_topBar->refresh();
    }
    if (m_dock != nullptr) {
        m_dock->refresh();
    }
    refreshDesktopIcons();
    updateDockMode();
    layoutChildren();
    scheduleBackdrop();
    update();
}

bool DesktopWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_workspaceHost && event->type() == QEvent::ContextMenu) {
        auto* context = static_cast<QContextMenuEvent*>(event);
        emit desktopContextMenuRequested(context->globalPos());
        return false;
    }

    if (watched == m_dock) {
        if (event->type() == QEvent::Enter) {
            revealDock();
        } else if (event->type() == QEvent::Leave) {
            scheduleDockHide();
        }
    }

    if (event->type() == QEvent::MouseMove) {
        handleDockEdgePointer(QCursor::pos());
    } else if (event->type() == QEvent::MouseButtonPress) {
        bool anyVisible = false;
        for (const OverlayEntry& entry : std::as_const(m_overlays)) {
            if (entry.widget != nullptr && entry.widget->isVisible()) {
                anyVisible = true;
                break;
            }
        }
        if (anyVisible) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            const QPoint global = mouse->globalPosition().toPoint();
            bool insideOverlay = false;
            for (const OverlayEntry& entry : std::as_const(m_overlays)) {
                QWidget* widget = entry.widget;
                if (widget == nullptr || !widget->isVisible()) {
                    continue;
                }
                if (widget->rect().contains(widget->mapFromGlobal(global))) {
                    insideOverlay = true;
                    break;
                }
            }
            if (!insideOverlay) {
                hideOverlays();
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}

void DesktopWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutChildren();
    scheduleBackdrop();
}

void DesktopWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_firstShow) {
        m_firstShow = false;
        layoutChildren();
        QTimer::singleShot(40, this, &DesktopWindow::refreshBackdrop);
    }
}

void DesktopWindow::hideEvent(QHideEvent* event)
{
    hideOverlays();
    QWidget::hideEvent(event);
}

void DesktopWindow::closeEvent(QCloseEvent* event)
{
    if (m_settings != nullptr) {
        m_settings->flush();
    }
    event->accept();
}

void DesktopWindow::mousePressEvent(QMouseEvent* event)
{
    hideOverlays();
    QWidget::mousePressEvent(event);
}

void DesktopWindow::contextMenuEvent(QContextMenuEvent* event)
{
    emit desktopContextMenuRequested(event->globalPos());
    event->accept();
}

} // namespace Aero::Shell
