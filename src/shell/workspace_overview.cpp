#include "aero/shell/workspace_overview.h"

#include "aero/compositor/window.h"
#include "aero/compositor/window_manager.h"
#include "aero/compositor/workspace_model.h"
#include "aero/graphics/wallpaper.h"
#include "aero/settings/settings_manager.h"
#include "aero/theme/theme.h"

#include <QEnterEvent>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QStringList>
#include <QVBoxLayout>

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

QString workspaceTitle(Compositor::WorkspaceModel* model, int index)
{
    if (model == nullptr) {
        return QStringLiteral("Desktop %1").arg(index + 1);
    }
    const QString name = model->name(index);
    return name.isEmpty() ? QStringLiteral("Desktop %1").arg(index + 1) : name;
}

} // namespace

class WorkspaceOverview::WorkspaceCard final : public QWidget {
public:
    WorkspaceCard(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(260, 174);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
    }

    void configure(int index,
                   const QString& name,
                   bool active,
                   const QList<Compositor::AeroWindow*>& windows,
                   const QString& wallpaperId,
                   bool dark)
    {
        m_index = index;
        m_name = name;
        m_active = active;
        m_wallpaperId = wallpaperId;
        m_dark = dark;
        m_thumbnails.clear();
        m_titles.clear();

        const int limit = 5;
        for (int i = 0; i < windows.size() && i < limit; ++i) {
            Compositor::AeroWindow* window = windows.at(i);
            if (window == nullptr) {
                continue;
            }
            QPixmap thumbnail = window->thumbnail(QSize(220, 138));
            if (!thumbnail.isNull()) {
                m_thumbnails.append(thumbnail);
                m_titles.append(window->title());
            }
        }
        m_windowCount = windows.size();
        update();
    }

    void setActivatedHandler(std::function<void(int)> handler)
    {
        m_activated = std::move(handler);
    }

    QSize sizeHint() const override
    {
        return QSize(390, 250);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const Theme::ThemeTokens& tokens = Theme::ThemeManager::instance().tokens();
        const QRectF card = QRectF(rect()).adjusted(1, 1, -1, -1);
        const qreal radius = 17.0;

        QPainterPath clip;
        clip.addRoundedRect(card, radius, radius);

        painter.save();
        painter.setClipPath(clip);
        Aero::Graphics::Wallpaper::paintPreset(painter, card, m_wallpaperId, m_dark, true);
        QColor dim(0, 0, 0, m_dark ? 92 : 34);
        painter.fillRect(card, dim);
        painter.restore();

        QColor border = m_active ? tokens.color.accent : tokens.color.glassBorder;
        if (m_active) {
            border.setAlphaF(0.95);
        } else {
            border.setAlphaF(m_hovered ? 0.75 : 0.45);
        }
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(border, m_active ? 2.0 : 1.0));
        painter.drawRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

        // Header strip: workspace name and window count.
        QFont titleFont = Theme::ThemeManager::instance().font(14.5, QFont::DemiBold);
        painter.setFont(titleFont);
        painter.setPen(tokens.dark ? QColor(255, 255, 255, 235) : QColor(31, 26, 44, 235));
        const QRectF titleRect(card.left() + 16, card.top() + 12, card.width() - 100, 24);
        painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, m_name);

        painter.setFont(Theme::ThemeManager::instance().font(10.5, QFont::Medium));
        painter.setPen(QColor(255, 255, 255, 170));
        const QString countText = m_windowCount == 0
            ? QStringLiteral("Empty")
            : (m_windowCount == 1 ? QStringLiteral("1 window")
                                  : QStringLiteral("%1 windows").arg(m_windowCount));
        painter.drawText(QRectF(card.right() - 118, card.top() + 12, 100, 24),
                         Qt::AlignRight | Qt::AlignVCenter, countText);

        // Miniature window stack.
        const QRectF content = card.adjusted(18, 46, -18, -18);
        if (m_thumbnails.isEmpty()) {
            painter.setPen(QColor(255, 255, 255, 105));
            painter.setFont(Theme::ThemeManager::instance().font(11.0, QFont::Normal));
            painter.drawText(content, Qt::AlignCenter, QStringLiteral("No windows on this desktop"));
        } else {
            const int count = qMin(3, m_thumbnails.size());
            for (int i = count - 1; i >= 0; --i) {
                const QPixmap& thumbnail = m_thumbnails.at(i);
                const qreal inset = i * 13.0;
                QRectF target(content.left() + inset + 26,
                              content.top() + inset + 8,
                              content.width() - 56 - inset,
                              content.height() - 16 - inset);
                if (target.width() < 60 || target.height() < 34) {
                    continue;
                }
                const qreal thumbRatio = thumbnail.width() / qreal(qMax(1, thumbnail.height()));
                QSizeF size(target.width(), target.width() / thumbRatio);
                if (size.height() > target.height()) {
                    size = QSizeF(target.height() * thumbRatio, target.height());
                }
                QRectF destination(QPointF(target.center().x() - size.width() / 2.0,
                                           target.center().y() - size.height() / 2.0),
                                   size);

                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(0, 0, 0, 60));
                painter.drawRoundedRect(destination.adjusted(-2, 2, 2, 3), 7, 7);

                QPainterPath thumbClip;
                thumbClip.addRoundedRect(destination, 6, 6);
                painter.save();
                painter.setClipPath(thumbClip);
                painter.drawPixmap(destination, thumbnail, thumbnail.rect());
                painter.restore();

                painter.setPen(QPen(QColor(255, 255, 255, 42), 1));
                painter.setBrush(Qt::NoBrush);
                painter.drawRoundedRect(destination.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
            }

            if (m_titles.size() > 0) {
                painter.setFont(Theme::ThemeManager::instance().font(9.5, QFont::Normal));
                painter.setPen(QColor(255, 255, 255, 145));
                const QString caption = m_titles.first();
                painter.drawText(QRectF(card.left() + 18, card.bottom() - 22, card.width() - 36, 16),
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 painter.fontMetrics().elidedText(caption, Qt::ElideRight,
                                                                  qMax(40, int(card.width() - 36))));
            }
        }

        if (m_active) {
            QColor badge = tokens.color.accent;
            badge.setAlphaF(0.94);
            const QRectF pill(card.left() + 16, card.bottom() - 30, 58, 18);
            painter.setPen(Qt::NoPen);
            painter.setBrush(badge);
            painter.drawRoundedRect(pill, 9, 9);
            painter.setPen(Qt::white);
            painter.setFont(Theme::ThemeManager::instance().font(9.0, QFont::DemiBold));
            painter.drawText(pill, Qt::AlignCenter, QStringLiteral("CURRENT"));
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && m_activated) {
            m_activated(m_index);
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

private:
    int m_index = 0;
    QString m_name;
    QString m_wallpaperId;
    bool m_active = false;
    bool m_dark = false;
    bool m_hovered = false;
    int m_windowCount = 0;
    QList<QPixmap> m_thumbnails;
    QStringList m_titles;
    std::function<void(int)> m_activated;
};

WorkspaceOverview::WorkspaceOverview(Compositor::WindowManager* windowManager,
                                     Settings::SettingsManager* settings,
                                     QWidget* parent)
    : Overlay(parent)
    , m_windowManager(windowManager)
    , m_settings(settings)
{
    setCornerRadius(0);
    setShadowMargin(0);
    setTintStrength(0.9);
    setFocusPolicy(Qt::StrongFocus);
    resize(1200, 720);
    build();

    if (m_windowManager != nullptr) {
        connect(m_windowManager, &Compositor::WindowManager::windowListChanged,
                this, [this]() {
                    if (isOpen()) {
                        refresh();
                    }
                });
        connect(m_windowManager, &Compositor::WindowManager::workspaceChanged,
                this, [this](int) {
                    if (isOpen()) {
                        refresh();
                    }
                });
        if (m_windowManager->workspaces() != nullptr) {
            connect(m_windowManager->workspaces(), &Compositor::WorkspaceModel::countChanged,
                    this, [this](int) {
                        if (isOpen()) {
                            refresh();
                        }
                    });
        }
    }
}

void WorkspaceOverview::build()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(58, 46, 58, 42);
    root->setSpacing(18);

    auto* header = new QHBoxLayout;
    header->setSpacing(12);
    auto* titleHost = new QWidget(this);
    auto* titleLayout = new QVBoxLayout(titleHost);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(2);
    auto* title = new QLabel(QStringLiteral("Workspaces"), titleHost);
    title->setFont(Theme::ThemeManager::instance().font(28.0, QFont::Bold));
    titleLayout->addWidget(title);
    auto* subtitle = new QLabel(QStringLiteral("Click a desktop to switch. Press 1–9 to jump directly."), titleHost);
    subtitle->setProperty("role", QStringLiteral("muted"));
    subtitle->setFont(Theme::ThemeManager::instance().captionFont());
    titleLayout->addWidget(subtitle);
    header->addWidget(titleHost, 1);

    auto* close = new QLabel(QStringLiteral("esc"), this);
    close->setProperty("role", QStringLiteral("muted"));
    close->setFont(Theme::ThemeManager::instance().font(10.0, QFont::DemiBold));
    close->setAlignment(Qt::AlignCenter);
    close->setFixedSize(38, 24);
    close->setStyleSheet(QStringLiteral("QLabel { border: 1px solid %1; border-radius: 7px; }")
                             .arg(Theme::ThemeManager::instance().tokens().color.separator.name()));
    header->addWidget(close, 0, Qt::AlignTop);
    root->addLayout(header);

    auto* gridHost = new QWidget(this);
    m_grid = new QGridLayout(gridHost);
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setHorizontalSpacing(18);
    m_grid->setVerticalSpacing(18);
    root->addWidget(gridHost, 1);
}

void WorkspaceOverview::prepareShow()
{
    refresh();
}

void WorkspaceOverview::refresh()
{
    if (m_grid == nullptr || m_windowManager == nullptr || m_windowManager->workspaces() == nullptr) {
        return;
    }

    clearLayout(m_grid);
    m_cards.clear();

    Compositor::WorkspaceModel* model = m_windowManager->workspaces();
    const int count = qMax(1, model->count());
    const int active = model->activeIndex();
    const bool dark = Theme::ThemeManager::instance().isDark();
    const QString wallpaperId = m_settings != nullptr && !m_settings->wallpaperId().isEmpty()
        ? m_settings->wallpaperId()
        : QStringLiteral("aurora");

    const int columns = count <= 4 ? 2 : 3;
    for (int index = 0; index < count; ++index) {
        auto* card = new WorkspaceCard(this);
        card->configure(index,
                        workspaceTitle(model, index),
                        index == active,
                        m_windowManager->windowsOnWorkspace(index),
                        wallpaperId,
                        dark);
        card->setActivatedHandler([this](int workspace) {
            emit workspaceActivated(workspace);
            dismiss();
        });
        m_grid->addWidget(card, index / columns, index % columns);
        m_cards.append(card);
    }

    for (int column = 0; column < columns; ++column) {
        m_grid->setColumnStretch(column, 1);
    }
    for (int row = 0; row < (count + columns - 1) / columns; ++row) {
        m_grid->setRowStretch(row, 1);
    }
}

void WorkspaceOverview::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        event->accept();
        dismiss();
        return;
    }

    if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_9) {
        const int index = event->key() - Qt::Key_1;
        if (m_windowManager != nullptr && m_windowManager->workspaces() != nullptr
            && index < m_windowManager->workspaces()->count()) {
            emit workspaceActivated(index);
            dismiss();
            event->accept();
            return;
        }
    }

    Overlay::keyPressEvent(event);
}

void WorkspaceOverview::paintEvent(QPaintEvent* event)
{
    Overlay::paintEvent(event);

    QPainter painter(this);
    const Theme::ThemeTokens& tokens = Theme::ThemeManager::instance().tokens();
    QColor base = tokens.dark ? QColor(10, 8, 16) : QColor(245, 242, 252);
    base.setAlphaF(0.72);
    painter.fillRect(rect(), base);
}

} // namespace Aero::Shell
