#include "aero/apps/builtin_apps.h"

#include "aero/apps/common/app_widgets.h"
#include "aero/core/paths.h"
#include "aero/core/utils.h"
#include "aero/core/version.h"
#include "aero/graphics/wallpaper.h"
#include "aero/platform/i_audio_manager.h"
#include "aero/platform/i_bluetooth_manager.h"
#include "aero/platform/i_display_manager.h"
#include "aero/platform/i_file_system.h"
#include "aero/platform/i_input_manager.h"
#include "aero/platform/i_network_manager.h"
#include "aero/platform/i_power_manager.h"
#include "aero/platform/i_session_manager.h"
#include "aero/services/notification_bus.h"
#include "aero/settings/settings_keys.h"
#include "aero/theme/controls.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/theme.h"

#include <QAbstractButton>
#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace Aero::Applications {
namespace {

QPixmap avatarPixmap(const QString& name, int size)
{
    QPixmap pixmap(size * 2, size * 2);
    pixmap.setDevicePixelRatio(2.0);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QColor accent = Theme::ThemeManager::instance().tokens().color.accent;
    painter.setPen(Qt::NoPen);
    painter.setBrush(accent);
    painter.drawEllipse(QRectF(1.0, 1.0, size - 2.0, size - 2.0));
    painter.setPen(Core::Utils::contrastColor(accent));
    QFont font = Theme::ThemeManager::instance().font(size * 0.34, QFont::DemiBold);
    painter.setFont(font);
    painter.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, Core::Utils::initials(name));
    return pixmap;
}

class AccentSwatch : public QAbstractButton {
public:
    explicit AccentSwatch(const QColor& color, QWidget* parent = nullptr)
        : QAbstractButton(parent)
        , m_color(color)
    {
        setCheckable(true);
        setAutoExclusive(true);
        setCursor(Qt::PointingHandCursor);
        setFixedSize(30, 30);
    }

    QColor color() const { return m_color; }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QRectF circle = QRectF(rect()).adjusted(3.5, 3.5, -3.5, -3.5);
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_color);
        painter.drawEllipse(circle);

        if (isChecked()) {
            const Theme::ColorTokens& colors = Theme::ThemeManager::instance().tokens().color;
            painter.setPen(QPen(colors.surfaceRaised, 2.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(QRectF(rect()).adjusted(0.8, 0.8, -0.8, -0.8));
            QColor checkColor = Core::Utils::contrastColor(m_color);
            Theme::IconFactory::paint(painter, QStringLiteral("check"),
                                      circle.adjusted(6, 6, -6, -6), checkColor, 2.2);
        }
    }

private:
    QColor m_color;
};

class WallpaperTile : public QAbstractButton {
public:
    WallpaperTile(const QString& id, QWidget* parent = nullptr)
        : QAbstractButton(parent)
        , m_id(id)
    {
        setCheckable(true);
        setAutoExclusive(true);
        setCursor(Qt::PointingHandCursor);
        setMinimumSize(150, 94);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    QString id() const { return m_id; }
    void setSelected(bool selected) { setChecked(selected); }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QRectF area = QRectF(rect()).adjusted(2.0, 2.0, -2.0, -2.0);
        QPainterPath clip;
        clip.addRoundedRect(area, 12, 12);
        painter.save();
        painter.setClipPath(clip);
        Graphics::Wallpaper::paintPreset(painter, area, m_id,
                                         Theme::ThemeManager::instance().isDark(), true);
        painter.restore();

        const Theme::ColorTokens& colors = Theme::ThemeManager::instance().tokens().color;
        painter.setPen(QPen(isChecked() ? colors.accent : colors.glassBorder, isChecked() ? 2.0 : 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(area, 12, 12);

        painter.setPen(Core::Utils::contrastColor(colors.textPrimary).rgba() == 0 ? QColor(255, 255, 255)
                                                                                  : QColor(255, 255, 255));
        QFont font = Theme::ThemeManager::instance().captionFont(QFont::DemiBold);
        painter.setFont(font);
        painter.setPen(QColor(255, 255, 255, 220));
        painter.drawText(area.adjusted(10, 0, -10, -8), Qt::AlignLeft | Qt::AlignBottom,
                         Graphics::Wallpaper::displayName(m_id));
    }

private:
    QString m_id;
};

class SettingsView final : public AppView {
public:
    explicit SettingsView(const AppContext& context, QWidget* parent = nullptr)
        : AppView(context, parent)
    {
        buildUi();
        refreshRecent();
        refreshDisplays();
        refreshNetwork();
        refreshBluetooth();
        refreshPower();
        refreshSound();
        refreshNotifications();
        refreshUsers();
    }

    QString windowTitle() const override { return QStringLiteral("Settings"); }
    QString subtitle() const override { return QStringLiteral("Aero preferences"); }
    void activate() override { refreshPower(); refreshNetwork(); refreshNotifications(); }

    void setLaunchArgument(const QString& argument) override
    {
        AppView::setLaunchArgument(argument);
        if (argument.isEmpty() || m_sidebar == nullptr) {
            return;
        }
        const QString needle = argument.trimmed().toLower();
        if (needle.isEmpty()) {
            return;
        }
        for (int row = 0; row < m_sidebar->count(); ++row) {
            QListWidgetItem* item = m_sidebar->item(row);
            if (item == nullptr) {
                continue;
            }
            const QString label = item->text().toLower();
            if (label == needle || label.startsWith(needle) || label.contains(needle)) {
                m_sidebar->setCurrentRow(row);
                return;
            }
        }
    }

private:
    void buildUi()
    {
        auto* root = new QHBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        m_sidebar = new QListWidget(this);
        m_sidebar->setFixedWidth(218);
        m_sidebar->setFrameShape(QFrame::NoFrame);
        m_sidebar->setIconSize(QSize(18, 18));
        m_sidebar->setSpacing(1);
        m_sidebar->setFocusPolicy(Qt::NoFocus);
        root->addWidget(m_sidebar);
        root->addWidget(Widgets::separator(this));

        m_stack = new QStackedWidget(this);
        root->addWidget(m_stack, 1);

        buildAppearancePage();
        buildWallpaperPage();
        buildDockPage();
        buildDesktopPage();
        buildDisplaysPage();
        buildShortcutsPage();
        buildMousePage();
        buildSoundPage();
        buildNetworkPage();
        buildBluetoothPage();
        buildPowerPage();
        buildNotificationsPage();
        buildUsersPage();
        buildAccessibilityPage();
        buildAboutPage();

        connect(m_sidebar, &QListWidget::currentRowChanged, this, [this](int row) {
            if (row >= 0) {
                m_stack->setCurrentIndex(row);
            }
        });
        m_sidebar->setCurrentRow(0);
    }

    QWidget* makePage(const QString& title, const QString& subtitle, const QString& icon)
    {
        auto* page = new QWidget;
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(26, 22, 26, 26);
        layout->setSpacing(14);
        layout->addWidget(Widgets::pageHeader(title, subtitle, page));

        auto* scroll = new QScrollArea(m_stack);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidget(page);
        m_stack->addWidget(scroll);

        auto* item = new QListWidgetItem(Theme::IconFactory::icon(icon, 18,
                                                                  Theme::ThemeManager::instance().tokens().color.textSecondary),
                                         title);
        item->setSizeHint(QSize(0, 34));
        m_sidebar->addItem(item);
        return page;
    }

    QVBoxLayout* addCard(QVBoxLayout* pageLayout, const QString& title)
    {
        auto* card = new Theme::Card;
        card->setRadius(16);
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(18, 14, 18, 16);
        layout->setSpacing(2);
        if (!title.isEmpty()) {
            layout->addWidget(Widgets::sectionLabel(title, card));
            layout->addSpacing(3);
        }
        pageLayout->addWidget(card);
        return layout;
    }

    void addToggleRow(QVBoxLayout* layout,
                      const QString& title,
                      const QString& description,
                      bool checked,
                      const std::function<void(bool)>& onChange,
                      bool enabled = true)
    {
        auto* toggle = new Theme::ToggleSwitch;
        toggle->setChecked(checked);
        toggle->setEnabled(enabled);
        if (enabled) {
            connect(toggle, &QAbstractButton::toggled, this, [onChange](bool value) { onChange(value); });
        }
        layout->addWidget(Widgets::formRow(title, toggle, description));
    }

    Theme::AeroSlider* addSliderRow(QVBoxLayout* layout,
                                   const QString& title,
                                   const QString& description,
                                   int minimum,
                                   int maximum,
                                   int value,
                                   const std::function<void(int)>& onChange,
                                   bool enabled = true)
    {
        auto* slider = new Theme::AeroSlider(minimum, maximum, value);
        slider->setShowValueLabel(true);
        slider->setEnabled(enabled);
        if (enabled) {
            connect(slider, &Theme::AeroSlider::valueChanged, this,
                    [onChange](int newValue) { onChange(newValue); });
        }
        layout->addWidget(Widgets::formRow(title, slider, description));
        return slider;
    }

    QComboBox* addComboRow(QVBoxLayout* layout,
                           const QString& title,
                           const QString& description,
                           const QList<QPair<QString, QVariant>>& options,
                           const QVariant& current,
                           const std::function<void(const QVariant&)>& onChange,
                           bool enabled = true)
    {
        auto* combo = new QComboBox;
        for (const auto& option : options) {
            combo->addItem(option.first, option.second);
        }
        const int index = combo->findData(current);
        if (index >= 0) {
            combo->setCurrentIndex(index);
        }
        combo->setEnabled(enabled);
        if (enabled) {
            connect(combo, &QComboBox::currentIndexChanged, this, [combo, onChange](int) {
                onChange(combo->currentData());
            });
        }
        layout->addWidget(Widgets::formRow(title, combo, description));
        return combo;
    }

    QWidget* statusRow(const QString& text, const QColor& tone)
    {
        auto* pill = new Theme::StatusPill(text);
        pill->setTone(tone);
        pill->setFilled(tone == Theme::ThemeManager::instance().tokens().color.success);
        return pill;
    }

    QString settingsKey(const char* key) const
    {
        return QString::fromLatin1(key);
    }

    void buildAppearancePage()
    {
        auto* page = makePage(QStringLiteral("Appearance"),
                              QStringLiteral("Theme, accent color and visual effects"),
                              QStringLiteral("palette"));
        auto* layout = qobject_cast<QVBoxLayout*>(page->layout());

        auto* themeCard = addCard(layout, QStringLiteral("Theme"));
        auto* mode = new Theme::SegmentedControl({QStringLiteral("Light"), QStringLiteral("Dark"),
                                                  QStringLiteral("Auto")});
        const QString modeValue = context().settings != nullptr ? context().settings->appearanceMode()
                                                                : QStringLiteral("auto");
        mode->setCurrentIndex(modeValue == QStringLiteral("light") ? 0
                             : modeValue == QStringLiteral("dark") ? 1 : 2);
        mode->setMaximumWidth(260);
        connect(mode, &Theme::SegmentedControl::currentIndexChanged, this, [this](int index) {
            if (context().settings == nullptr) {
                return;
            }
            context().settings->setAppearanceMode(index == 0 ? QStringLiteral("light")
                                                          : index == 1 ? QStringLiteral("dark")
                                                                       : QStringLiteral("auto"));
        });
        themeCard->addWidget(Widgets::formRow(QStringLiteral("Appearance"),
                                              mode,
                                              QStringLiteral("Auto follows the system color scheme.")));

        addToggleRow(themeCard, QStringLiteral("Reduce transparency"),
                     QStringLiteral("Use more opaque surfaces for readability."),
                     context().settings != nullptr && !context().settings->transparencyEnabled(),
                     [this](bool reduced) {
                         if (context().settings != nullptr) {
                             context().settings->setTransparencyEnabled(!reduced);
                         }
                     });
        addToggleRow(themeCard, QStringLiteral("Background blur"),
                     QStringLiteral("Sample and blur the wallpaper behind glass surfaces."),
                     context().settings != nullptr && context().settings->blurEnabled(),
                     [this](bool enabled) {
                         if (context().settings != nullptr) {
                             context().settings->setBlurEnabled(enabled);
                         }
                     });

        auto* accentCard = addCard(layout, QStringLiteral("Accent color"));
        auto* swatchHost = new QWidget;
        auto* swatchLayout = new QHBoxLayout(swatchHost);
        swatchLayout->setContentsMargins(0, 4, 0, 4);
        swatchLayout->setSpacing(8);
        const QStringList accents = {
            QStringLiteral("#7C5CFF"), QStringLiteral("#3D8BFF"), QStringLiteral("#12B5A8"),
            QStringLiteral("#32C984"), QStringLiteral("#FF9B42"), QStringLiteral("#FF5D6C"),
            QStringLiteral("#FF73B0"), QStringLiteral("#8E8E93"), QStringLiteral("#5C6BC0"),
        };
        const QString currentAccent = context().settings != nullptr ? context().settings->accentColor()
                                                                    : QStringLiteral("#7C5CFF");
        for (const QString& accent : accents) {
            auto* swatch = new AccentSwatch(QColor(accent), swatchHost);
            swatch->setChecked(accent.compare(currentAccent, Qt::CaseInsensitive) == 0);
            connect(swatch, &QAbstractButton::clicked, this, [this, accent]() {
                if (context().settings != nullptr) {
                    context().settings->setAccentColor(accent);
                }
            });
            swatchLayout->addWidget(swatch);
        }
        swatchLayout->addStretch(1);
        auto* custom = new Theme::AeroButton(QStringLiteral("Custom…"), QStringLiteral("palette"), swatchHost);
        connect(custom, &QAbstractButton::clicked, this, [this]() {
            const QColor color = QColorDialog::getColor(
                QColor(context().settings != nullptr ? context().settings->accentColor() : QStringLiteral("#7C5CFF")),
                this,
                QStringLiteral("Accent Color"));
            if (color.isValid() && context().settings != nullptr) {
                context().settings->setAccentColor(color.name());
            }
        });
        swatchLayout->addWidget(custom);
        accentCard->addWidget(swatchHost);

        auto* effectsCard = addCard(layout, QStringLiteral("Text and motion"));
        addSliderRow(effectsCard, QStringLiteral("Text size"),
                     QStringLiteral("Scale interface text without changing the layout grid."),
                     80, 150,
                     context().settings != nullptr ? qRound(context().settings->fontScale() * 100.0) : 100,
                     [this](int value) {
                         if (context().settings != nullptr) {
                             context().settings->setFontScale(value / 100.0);
                         }
                     });
        addToggleRow(effectsCard, QStringLiteral("Reduce motion"),
                     QStringLiteral("Shorten or disable window and dock animations."),
                     context().settings != nullptr && context().settings->reducedMotion(),
                     [this](bool enabled) {
                         if (context().settings != nullptr) {
                             context().settings->setReducedMotion(enabled);
                         }
                     });
    }

    void buildWallpaperPage()
    {
        auto* page = makePage(QStringLiteral("Wallpaper"),
                              QStringLiteral("Desktop background and scaling"),
                              QStringLiteral("wallpaper"));
        auto* layout = qobject_cast<QVBoxLayout*>(page->layout());

        auto* presetsCard = addCard(layout, QStringLiteral("Built-in wallpapers"));
        auto* grid = new QGridLayout;
        grid->setSpacing(10);
        const QString currentId = context().settings != nullptr ? context().settings->wallpaperId()
                                                                : QStringLiteral("aurora");
        const QString currentPath = context().settings != nullptr ? context().settings->wallpaperPath() : QString();
        int index = 0;
        for (const QString& id : Graphics::Wallpaper::presetIds()) {
            auto* tile = new WallpaperTile(id);
            tile->setChecked(currentPath.isEmpty() && id == currentId);
            connect(tile, &QAbstractButton::clicked, this, [this, id]() {
                if (context().settings != nullptr) {
                    context().settings->setWallpaper(id, QString(), context().settings->wallpaperFit());
                    refreshWallpaperSelection();
                }
            });
            m_wallpaperTiles.append(tile);
            grid->addWidget(tile, index / 2, index % 2);
            ++index;
        }
        presetsCard->addLayout(grid);

        auto* customCard = addCard(layout, QStringLiteral("Custom image"));
        m_wallpaperPathLabel = Widgets::mutedLabel(
            currentPath.isEmpty() ? QStringLiteral("Using a built-in wallpaper.") : Core::Paths::elideHome(currentPath));
        m_wallpaperPathLabel->setFont(Theme::ThemeManager::instance().captionFont());
        customCard->addWidget(m_wallpaperPathLabel);
        auto* chooseRow = new QHBoxLayout;
        chooseRow->setContentsMargins(0, 4, 0, 0);
        auto* choose = new Theme::AeroButton(QStringLiteral("Choose Image…"), QStringLiteral("image"));
        connect(choose, &QAbstractButton::clicked, this, [this]() {
            const QString start = context().fileSystem() != nullptr
                ? context().fileSystem()->standardLocation(Platform::StandardLocation::Pictures)
                : QString();
            const QString path = QFileDialog::getOpenFileName(
                this,
                QStringLiteral("Choose Wallpaper"),
                start,
                QStringLiteral("Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
            if (!path.isEmpty() && context().settings != nullptr) {
                context().settings->setWallpaper(QStringLiteral("custom"), path,
                                                 context().settings->wallpaperFit());
                m_wallpaperPathLabel->setText(Core::Paths::elideHome(path));
                refreshWallpaperSelection();
            }
        });
        chooseRow->addWidget(choose);
        chooseRow->addStretch(1);
        customCard->addLayout(chooseRow);

        auto* fitCard = addCard(layout, QStringLiteral("Scaling"));
        const QStringList fitOptions = {QStringLiteral("Fill"), QStringLiteral("Fit"),
                                        QStringLiteral("Stretch"), QStringLiteral("Tile")};
        auto* fit = new Theme::SegmentedControl(fitOptions);
        const QString currentFit = context().settings != nullptr ? context().settings->wallpaperFit()
                                                                 : QStringLiteral("fill");
        fit->setCurrentIndex(qMax(0, fitOptions.indexOf(
            currentFit.left(1).toUpper() + currentFit.mid(1))));
        fit->setMaximumWidth(360);
        connect(fit, &Theme::SegmentedControl::currentIndexChanged, this, [this, fitOptions](int selected) {
            if (context().settings == nullptr || selected < 0) {
                return;
            }
            const QString id = context().settings->wallpaperId();
            context().settings->setWallpaper(id, context().settings->wallpaperPath(),
                                             fitOptions.at(selected).toLower());
        });
        fitCard->addWidget(Widgets::formRow(QStringLiteral("Scale to screen"), fit,
                                           QStringLiteral("Tile repeats the image at its native size.")));
    }

    void refreshWallpaperSelection()
    {
        if (context().settings == nullptr) {
            return;
        }
        const QString id = context().settings->wallpaperId();
        const bool custom = !context().settings->wallpaperPath().isEmpty();
        for (WallpaperTile* tile : m_wallpaperTiles) {
            tile->setSelected(!custom && tile->id() == id);
        }
        if (m_wallpaperPathLabel != nullptr) {
            const QString path = context().settings->wallpaperPath();
            m_wallpaperPathLabel->setText(path.isEmpty() ? QStringLiteral("Using a built-in wallpaper.")
                                                          : Core::Paths::elideHome(path));
        }
    }

    void buildDockPage()
    {
        auto* page = makePage(QStringLiteral("Dock"),
                              QStringLiteral("Position, size and magnification"),
                              QStringLiteral("dock"));
        auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
        auto* card = addCard(layout, QStringLiteral("Dock"));

        addComboRow(card, QStringLiteral("Position"),
                    QStringLiteral("Move the dock to another screen edge."),
                    {{QStringLiteral("Bottom"), QStringLiteral("bottom")},
                     {QStringLiteral("Left"), QStringLiteral("left")},
                     {QStringLiteral("Right"), QStringLiteral("right")}},
                    context().settings != nullptr ? QVariant(context().settings->dockPosition())
                                                  : QVariant(QStringLiteral("bottom")),
                    [this](const QVariant& value) {
                        if (context().settings != nullptr) {
                            context().settings->setValue(settingsKey(Settings::Keys::DockPosition), value);
                        }
                    });

        addSliderRow(card, QStringLiteral("Icon size"), QStringLiteral("Pixels at rest."),
                     32, 80,
                     context().settings != nullptr ? context().settings->dockIconSize() : 48,
                     [this](int value) {
                         if (context().settings != nullptr) {
                             context().settings->setValue(settingsKey(Settings::Keys::DockIconSize), value);
                         }
                     });

        addSliderRow(card, QStringLiteral("Magnification"),
                     QStringLiteral("How much icons grow under the pointer."),
                     100, 200,
                     context().settings != nullptr ? qRound(context().settings->dockMagnification() * 100.0) : 130,
                     [this](int value) {
                         if (context().settings != nullptr) {
                             context().settings->setValue(settingsKey(Settings::Keys::DockMagnification),
                                                          value / 100.0);
                         }
                     });

        addToggleRow(card, QStringLiteral("Automatically hide"),
                     QStringLiteral("Reveal the dock when the pointer reaches the screen edge."),
                     context().settings != nullptr && context().settings->dockAutoHide(),
                     [this](bool enabled) {
                         if (context().settings != nullptr) {
                             context().settings->setValue(settingsKey(Settings::Keys::DockAutoHide), enabled);
                         }
                     });

        addToggleRow(card, QStringLiteral("Show running applications"),
                     QStringLiteral("Keep an indicator under applications with open windows."),
                     context().settings == nullptr
                         || context().settings->value(settingsKey(Settings::Keys::DockShowRunning), true).toBool(),
                     [this](bool enabled) {
                         if (context().settings != nullptr) {
                             context().settings->setValue(settingsKey(Settings::Keys::DockShowRunning), enabled);
                         }
                     });
    }

    void buildDesktopPage()
    {
        auto* page = makePage(QStringLiteral("Desktop"),
                              QStringLiteral("Desktop icons and recent files"),
                              QStringLiteral("desktop"));
        auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
        auto* card = addCard(layout, QStringLiteral("Desktop"));

        addToggleRow(card, QStringLiteral("Show desktop icons"),
                     QStringLiteral("Display files from the Desktop folder on the wallpaper."),
                     context().settings == nullptr
                         || context().settings->value(settingsKey(Settings::Keys::DesktopShowIcons), true).toBool(),
                     [this](bool enabled) {
                         if (context().settings != nullptr) {
                             context().settings->setValue(settingsKey(Settings::Keys::DesktopShowIcons), enabled);
                         }
                     });
        addToggleRow(card, QStringLiteral("Open with double-click"),
                     QStringLiteral("When disabled, a single click opens desktop items."),
                     context().settings == nullptr
                         || context().settings->value(settingsKey(Settings::Keys::DesktopDoubleClick), true).toBool(),
                     [this](bool enabled) {
                         if (context().settings != nullptr) {
                             context().settings->setValue(settingsKey(Settings::Keys::DesktopDoubleClick), enabled);
                         }
                     });

        auto* recentCard = addCard(layout, QStringLiteral("Recent files"));
        m_recentList = new QListWidget;
        m_recentList->setFrameShape(QFrame::NoFrame);
        m_recentList->setMaximumHeight(190);
        m_recentList->setIconSize(QSize(16, 16));
        recentCard->addWidget(m_recentList);
        auto* actions = new QHBoxLayout;
        actions->setContentsMargins(0, 6, 0, 0);
        auto* clear = new Theme::AeroButton(QStringLiteral("Clear Recent Files"), QStringLiteral("trash"));
        clear->setVariant(Theme::AeroButton::Variant::Ghost);
        connect(clear, &QAbstractButton::clicked, this, [this]() {
            if (context().settings != nullptr) {
                context().settings->clearRecentFiles();
                refreshRecent();
            }
        });
        actions->addWidget(clear);
        actions->addStretch(1);
        recentCard->addLayout(actions);
        connect(m_recentList, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
            if (item != nullptr) {
                context().openFile(item->data(Qt::UserRole).toString());
            }
        });
    }

    void refreshRecent()
    {
        if (m_recentList == nullptr) {
            return;
        }
        m_recentList->clear();
        const QStringList recent = context().settings != nullptr ? context().settings->recentFiles() : QStringList();
        if (recent.isEmpty()) {
            auto* item = new QListWidgetItem(QStringLiteral("No recent files"));
            item->setFlags(Qt::NoItemFlags);
            m_recentList->addItem(item);
            return;
        }
        for (const QString& path : recent) {
            const QFileInfo info(path);
            auto* item = new QListWidgetItem(Theme::IconFactory::icon(
                                                 info.suffix() == QStringLiteral("png")
                                                     || info.suffix() == QStringLiteral("jpg")
                                                     ? QStringLiteral("image")
                                                     : QStringLiteral("file"),
                                                 16,
                                                 Theme::ThemeManager::instance().tokens().color.textMuted),
                                             info.fileName());
            item->setData(Qt::UserRole, path);
            item->setToolTip(path);
            m_recentList->addItem(item);
        }
    }

    void buildDisplaysPage()
    {
        auto* page = makePage(QStringLiteral("Displays"),
                              QStringLiteral("Connected outputs, resolution and brightness"),
                              QStringLiteral("display"));
        auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
        auto* card = addCard(layout, QStringLiteral("Outputs"));
        m_displayLayout = new QVBoxLayout;
        m_displayLayout->setSpacing(4);
        card->addLayout(m_displayLayout);

        Platform::IDisplayManager* manager = context().displayManager();
        if (manager != nullptr) {
            connect(manager, &Platform::IDisplayManager::displaysChanged, this, [this]() { refreshDisplays(); });
        }
    }

    void refreshDisplays()
    {
        if (m_displayLayout == nullptr) {
            return;
        }
        Widgets::clearLayout(m_displayLayout);
        Platform::IDisplayManager* manager = context().displayManager();
        if (manager == nullptr || !manager->isAvailable()) {
            m_displayLayout->addWidget(Widgets::mutedLabel(
                QStringLiteral("Display management is not available on this platform backend.")));
            return;
        }

        const QList<Platform::DisplayInfo> displays = manager->displays();
        if (displays.isEmpty()) {
            m_displayLayout->addWidget(Widgets::mutedLabel(
                QStringLiteral("No display outputs were reported.")));
            return;
        }

        for (const Platform::DisplayInfo& display : displays) {
            auto* header = new QWidget;
            auto* headerLayout = new QHBoxLayout(header);
            headerLayout->setContentsMargins(0, 8, 0, 2);
            headerLayout->setSpacing(8);
            auto* name = new QLabel(QStringLiteral("%1%2")
                                        .arg(display.name.isEmpty() ? display.id : display.name,
                                             display.primary ? QStringLiteral(" · Primary") : QString()));
            name->setProperty("role", QStringLiteral("title"));
            name->setFont(Theme::ThemeManager::instance().bodyFont(QFont::DemiBold));
            headerLayout->addWidget(name);
            headerLayout->addStretch(1);
            auto* geometry = Widgets::mutedLabel(QStringLiteral("%1 × %2 · %3 Hz")
                                                     .arg(display.geometry.width())
                                                     .arg(display.geometry.height())
                                                     .arg(QString::number(display.refreshRate, 'f', 0)));
            geometry->setFont(Theme::ThemeManager::instance().captionFont());
            headerLayout->addWidget(geometry);
            m_displayLayout->addWidget(header);

            if (manager->supportsBrightness()) {
                const int current = manager->brightness(display.id) >= 0
                    ? manager->brightness(display.id)
                    : (context().settings != nullptr ? context().settings->displayBrightness() : 80);
                auto* slider = new Theme::AeroSlider(5, 100, current);
                slider->setShowValueLabel(true);
                connect(slider, &Theme::AeroSlider::valueChanged, this, [this, manager, display](int value) {
                    manager->setBrightness(display.id, value);
                    if (context().settings != nullptr) {
                        context().settings->setValue(settingsKey(Settings::Keys::DisplayBrightness), value);
                    }
                });
                m_displayLayout->addWidget(Widgets::formRow(QStringLiteral("Brightness"), slider));
            } else {
                m_displayLayout->addWidget(Widgets::mutedLabel(
                    QStringLiteral("Brightness control is not exposed for this output.")));
            }
            m_displayLayout->addWidget(Widgets::separator());
        }
    }

    void buildShortcutsPage()
    {
        auto* page = makePage(QStringLiteral("Shortcuts"),
                              QStringLiteral("Keyboard commands handled by the shell"),
                              QStringLiteral("keyboard"));
        auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
        auto* card = addCard(layout, QStringLiteral("Shell shortcuts"));

        const QHash<QString, QString> names = {
            {QStringLiteral("spotlight"), QStringLiteral("Spotlight search")},
            {QStringLiteral("control-center"), QStringLiteral("Control Center")},
            {QStringLiteral("notification-center"), QStringLiteral("Notification Center")},
            {QStringLiteral("workspace-overview"), QStringLiteral("Workspace overview")},
            {QStringLiteral("workspace-next"), QStringLiteral("Next workspace")},
            {QStringLiteral("workspace-previous"), QStringLiteral("Previous workspace")},
            {QStringLiteral("window-close"), QStringLiteral("Close window")},
            {QStringLiteral("window-minimize"), QStringLiteral("Minimize window")},
            {QStringLiteral("window-maximize"), QStringLiteral("Maximize window")},
            {QStringLiteral("window-switch"), QStringLiteral("Switch applications")},
            {QStringLiteral("show-desktop"), QStringLiteral("Show desktop")},
            {QStringLiteral("lock-screen"), QStringLiteral("Lock screen")},
        };

        const QHash<QString, QKeySequence> shortcuts = context().settings != nullptr
            ? context().settings->shortcuts()
            : Settings::SettingsManager::defaultShortcuts();
        const QHash<QString, QKeySequence> defaults = Settings::SettingsManager::defaultShortcuts();

        QStringList ordered = names.keys();
        std::sort(ordered.begin(), ordered.end(), [&names](const QString& a, const QString& b) {
            return names.value(a) < names.value(b);
        });

        for (const QString& action : ordered) {
            auto* row = new QWidget;
            auto* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 5, 0, 5);
            rowLayout->setSpacing(8);
            auto* label = new QLabel(names.value(action), row);
            label->setProperty("role", QStringLiteral("title"));
            label->setFont(Theme::ThemeManager::instance().bodyFont());
            rowLayout->addWidget(label, 1);

            auto* edit = new QKeySequenceEdit(shortcuts.value(action), row);
            edit->setMaximumWidth(190);
            rowLayout->addWidget(edit);
            connect(edit, &QKeySequenceEdit::editingFinished, this, [this, action, edit]() {
                if (context().settings != nullptr) {
                    context().settings->setShortcut(action, edit->keySequence());
                }
            });

            auto* reset = new Theme::IconButton(QStringLiteral("refresh"), row);
            reset->setToolTip(QStringLiteral("Restore default"));
            connect(reset, &QAbstractButton::clicked, this, [this, action, edit, defaults]() {
                const QKeySequence sequence = defaults.value(action);
                edit->setKeySequence(sequence);
                if (context().settings != nullptr) {
                    context().settings->setShortcut(action, sequence);
                }
            });
            rowLayout->addWidget(reset);
            card->addWidget(row);
        }

        auto* note = Widgets::mutedLabel(
            QStringLiteral("Shortcuts are active while the Aero shell has keyboard focus. "
                           "System-wide grabs require a compositor that implements AeroPlatform input management."));
        note->setFont(Theme::ThemeManager::instance().captionFont());
        card->addWidget(note);
    }

    void buildMousePage()
    {
        auto* page = makePage(QStringLiteral("Mouse & Trackpad"),
                              QStringLiteral("Pointer speed and gestures"),
                              QStringLiteral("mouse"));
        auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
        auto* card = addCard(layout, QStringLiteral("Pointer"));

        Platform::IInputManager* input = context().inputManager();
        const bool available = input != nullptr && input->isAvailable();
        if (!available) {
            card->addWidget(Widgets::mutedLabel(
                QStringLiteral("This platform backend does not expose host input configuration. "
                               "Controls are disabled rather than pretending to apply changes.")));
        }

        const int speed = context().settings != nullptr
            ? qRound(context().settings->value(settingsKey(Settings::Keys::MouseSpeed), 0.0).toDouble() * 100.0)
            : 0;
        addSliderRow(card, QStringLiteral("Pointer speed"),
                     QStringLiteral("Relative speed for host pointing devices."),
                     -100, 100, speed,
                     [this, input](int value) {
                         if (context().settings != nullptr) {
                             context().settings->setValue(settingsKey(Settings::Keys::MouseSpeed), value / 100.0);
                         }
                         if (input != nullptr) {
                             input->setPointerSpeed(value / 100.0);
                         }
                     },
                     available);

        addToggleRow(card, QStringLiteral("Natural scrolling"),
                     QStringLiteral("Move content in the same direction as your fingers."),
                     context().settings != nullptr
                         && context().settings->value(settingsKey(Settings::Keys::MouseNaturalScroll), false).toBool(),
                     [this, input](bool enabled) {
                         if (context().settings != nullptr) {
                             context().settings->setValue(settingsKey(Settings::Keys::MouseNaturalScroll), enabled);
                         }
                         if (input != nullptr) {
                             input->setNaturalScroll(enabled);
                         }
                     },
                     available);

        addToggleRow(card, QStringLiteral("Tap to click"),
                     QStringLiteral("Treat a tap on the trackpad as a primary click."),
                     context().settings != nullptr
                         && context().settings->value(settingsKey(Settings::Keys::MouseTapToClick), false).toBool(),
                     [this, input](bool enabled) {
                         if (context().settings != nullptr) {
                             context().settings->setValue(settingsKey(Settings::Keys::MouseTapToClick), enabled);
                         }
                         if (input != nullptr) {
                             input->setTapToClick(enabled);
                         }
                     },
                     available);
    }

    void buildSoundPage()
    {
        auto* page = makePage(QStringLiteral("Sound"),
                              QStringLiteral("Output volume and devices"),
                              QStringLiteral("volume"));
        auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
        auto* card = addCard(layout, QStringLiteral("Output"));
        Platform::IAudioManager* audio = context().audioManager();
        const bool available = audio != nullptr && audio->isAvailable();

        if (!available) {
            card->addWidget(Widgets::mutedLabel(
                QStringLiteral("The platform does not expose an audio service. Volume controls are disabled.")));
        }

        m_soundVolume = addSliderRow(card, QStringLiteral("Volume"),
                     QStringLiteral("Master output volume."),
                     0, 100,
                     context().settings != nullptr ? context().settings->soundVolume() : 60,
                     [this, audio](int value) {
                         if (m_updating) {
                             return;
                         }
                         if (context().settings != nullptr) {
                             context().settings->setValue(settingsKey(Settings::Keys::SoundVolume), value);
                             context().settings->setValue(settingsKey(Settings::Keys::SoundMuted), false);
                         }
                         if (audio != nullptr) {
                             audio->setMuted(false);
                             audio->setVolume(value);
                         }
                     },
                     available);

        m_soundMute = new Theme::ToggleSwitch;
        m_soundMute->setChecked(context().settings != nullptr && context().settings->soundMuted());
        m_soundMute->setEnabled(available);
        connect(m_soundMute, &QAbstractButton::toggled, this, [this, audio](bool muted) {
            if (m_updating) {
                return;
            }
            if (context().settings != nullptr) {
                context().settings->setValue(settingsKey(Settings::Keys::SoundMuted), muted);
            }
            if (audio != nullptr) {
                audio->setMuted(muted);
            }
        });
        card->addWidget(Widgets::formRow(QStringLiteral("Mute"), m_soundMute,
                                         QStringLiteral("Silence all output until unmuted.")));

        m_outputDevice = new QComboBox;
        m_outputDevice->setEnabled(available);
        connect(m_outputDevice, &QComboBox::currentIndexChanged, this, [this, audio](int) {
            if (m_updating || audio == nullptr) {
                return;
            }
            const QString id = m_outputDevice->currentData().toString();
            if (!id.isEmpty()) {
                audio->setDefaultDevice(id, false);
            }
        });
        card->addWidget(Widgets::formRow(QStringLiteral("Output device"), m_outputDevice,
                                         QStringLiteral("Devices are reported by the platform audio service.")));

        auto* refresh = new Theme::AeroButton(QStringLiteral("Refresh Devices"), QStringLiteral("refresh"));
        refresh->setEnabled(available);
        connect(refresh, &QAbstractButton::clicked, this, [this]() { refreshSound(); });
        card->addWidget(refresh);

        if (audio != nullptr) {
            connect(audio, &Platform::IAudioManager::stateChanged, this, [this](const Platform::AudioInfo&) {
                refreshSound();
            });
            connect(audio, &Platform::IAudioManager::devicesChanged, this, [this]() { refreshSound(); });
        }
    }

    void refreshSound()
    {
        if (m_soundVolume == nullptr || m_soundMute == nullptr || m_outputDevice == nullptr) {
            return;
        }
        Platform::IAudioManager* audio = context().audioManager();
        if (audio == nullptr || !audio->isAvailable()) {
            return;
        }
        m_updating = true;
        const Platform::AudioInfo info = audio->state();
        m_soundVolume->setValue(info.volume);
        m_soundMute->setChecked(info.muted);
        m_outputDevice->clear();
        int selectedIndex = -1;
        for (const Platform::AudioDeviceInfo& device : audio->devices()) {
            if (device.isInput) {
                continue;
            }
            m_outputDevice->addItem(device.name.isEmpty() ? device.id : device.name, device.id);
            if (device.isDefault) {
                selectedIndex = m_outputDevice->count() - 1;
            }
        }
        if (selectedIndex >= 0) {
            m_outputDevice->setCurrentIndex(selectedIndex);
        }
        m_updating = false;
    }

    void buildNetworkPage()
    {
        auto* page = makePage(QStringLiteral("Network"),
                              QStringLiteral("Wi-Fi and connectivity"),
                              QStringLiteral("wifi"));
        auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
        auto* card = addCard(layout, QStringLiteral("Connection"));

        m_networkStatus = Widgets::subtitleLabel(QStringLiteral("Checking…"));
        card->addWidget(m_networkStatus);

        auto* wifiHost = new QWidget;
        auto* wifiRow = new QHBoxLayout(wifiHost);
        wifiRow->setContentsMargins(0, 4, 0, 4);
        m_wifiToggle = new Theme::ToggleSwitch;
        connect(m_wifiToggle, &QAbstractButton::toggled, this, [this](bool enabled) {
            Platform::INetworkManager* network = context().networkManager();
            if (network != nullptr && !m_updating) {
                network->setWifiEnabled(enabled);
                network->scanWifi();
            }
        });
        auto* scan = new Theme::AeroButton(QStringLiteral("Scan"), QStringLiteral("refresh"));
        connect(scan, &QAbstractButton::clicked, this, [this]() {
            if (context().networkManager() != nullptr) {
                context().networkManager()->scanWifi();
                m_networkStatus->setText(QStringLiteral("Scanning for Wi-Fi networks…"));
            }
        });
        wifiRow->addWidget(new QLabel(QStringLiteral("Wi-Fi")));
        wifiRow->addWidget(m_wifiToggle);
        wifiRow->addStretch(1);
        wifiRow->addWidget(scan);
        card->addWidget(wifiHost);

        m_networkList = new QListWidget;
        m_networkList->setFrameShape(QFrame::NoFrame);
        m_networkList->setMaximumHeight(260);
        m_networkList->setIconSize(QSize(18, 18));
        card->addWidget(m_networkList);

        auto* connectRow = new QHBoxLayout;
        connectRow->setContentsMargins(0, 4, 0, 0);
        auto* connectButton = new Theme::AeroButton(QStringLiteral("Connect"), QStringLiteral("wifi"));
        auto* disconnectButton = new Theme::AeroButton(QStringLiteral("Disconnect"), QStringLiteral("close"));
        disconnectButton->setVariant(Theme::AeroButton::Variant::Ghost);
        connect(connectButton, &QAbstractButton::clicked, this, [this]() {
            QListWidgetItem* item = m_networkList->currentItem();
            if (item == nullptr) {
                return;
            }
            connectToNetwork(item->data(Qt::UserRole).toString(), item->data(Qt::UserRole + 1).toBool());
        });
        connect(disconnectButton, &QAbstractButton::clicked, this, [this]() {
            if (context().networkManager() != nullptr) {
                context().networkManager()->disconnectWifi();
            }
        });
        connectRow->addWidget(connectButton);
        connectRow->addWidget(disconnectButton);
        connectRow->addStretch(1);
        card->addLayout(connectRow);

        Platform::INetworkManager* network = context().networkManager();
        if (network == nullptr || !network->isAvailable()) {
            m_networkStatus->setText(QStringLiteral("Network management is not available on this backend."));
            m_wifiToggle->setEnabled(false);
            scan->setEnabled(false);
            connectButton->setEnabled(false);
            disconnectButton->setEnabled(false);
        } else {
            connect(network, &Platform::INetworkManager::networkChanged, this,
                    [this](const Platform::NetworkInfo&) { refreshNetwork(); });
            connect(network, &Platform::INetworkManager::wifiNetworksChanged, this,
                    [this](const QList<Platform::WifiNetwork>&) { refreshNetwork(); });
        }

        connect(m_networkList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
            if (item != nullptr) {
                connectToNetwork(item->data(Qt::UserRole).toString(), item->data(Qt::UserRole + 1).toBool());
            }
        });
    }

    void refreshNetwork()
    {
        if (m_networkStatus == nullptr || m_networkList == nullptr || m_wifiToggle == nullptr) {
            return;
        }
        Platform::INetworkManager* network = context().networkManager();
        if (network == nullptr || !network->isAvailable()) {
            return;
        }
        m_updating = true;
        const Platform::NetworkInfo info = network->network();
        m_wifiToggle->setChecked(info.wifiEnabled);
        m_updating = false;

        m_networkStatus->setText(info.connected
                                     ? QStringLiteral("Connected%1%2%3")
                                           .arg(info.wifi ? QStringLiteral(" to ") + info.ssid : QString(),
                                                info.ipAddress.isEmpty() ? QString()
                                                                         : QStringLiteral(" · ") + info.ipAddress,
                                                info.wifi ? QStringLiteral(" · %1% signal").arg(info.signalStrength)
                                                          : QString())
                                     : (info.wifiEnabled ? QStringLiteral("Not connected. Choose a network below.")
                                                         : QStringLiteral("Wi-Fi is turned off.")));

        m_networkList->clear();
        const QList<Platform::WifiNetwork> networks = network->wifiNetworks();
        for (const Platform::WifiNetwork& wifi : networks) {
            auto* item = new QListWidgetItem(
                Theme::IconFactory::icon(wifi.secured ? QStringLiteral("lock") : QStringLiteral("wifi"), 18,
                                         Theme::ThemeManager::instance().tokens().color.textSecondary),
                QStringLiteral("%1%2   %3%")
                    .arg(wifi.ssid,
                         wifi.connected ? QStringLiteral("  ✓") : QString(),
                         QString::number(wifi.strength)));
            item->setData(Qt::UserRole, wifi.ssid);
            item->setData(Qt::UserRole + 1, wifi.secured);
            if (wifi.connected) {
                item->setForeground(Theme::ThemeManager::instance().tokens().color.accent);
            }
            m_networkList->addItem(item);
        }
        if (networks.isEmpty()) {
            auto* item = new QListWidgetItem(QStringLiteral("No networks found. Press Scan to search again."));
            item->setFlags(Qt::NoItemFlags);
            m_networkList->addItem(item);
        }
    }

    void connectToNetwork(const QString& ssid, bool secured)
    {
        Platform::INetworkManager* network = context().networkManager();
        if (network == nullptr || ssid.isEmpty()) {
            return;
        }
        QString password;
        if (secured) {
            bool accepted = false;
            password = QInputDialog::getText(this, QStringLiteral("Join “%1”").arg(ssid),
                                             QStringLiteral("Password:"), QLineEdit::Password,
                                             QString(), &accepted);
            if (!accepted) {
                return;
            }
        }
        m_networkStatus->setText(QStringLiteral("Connecting to %1…").arg(ssid));
        if (!network->connectToWifi(ssid, password)) {
            m_networkStatus->setText(QStringLiteral("The platform rejected the connection request."));
        }
        QTimer::singleShot(1200, this, [this]() { refreshNetwork(); });
    }

    void buildBluetoothPage()
    {
        auto* page = makePage(QStringLiteral("Bluetooth"),
                              QStringLiteral("Adapters and paired devices"),
                              QStringLiteral("bluetooth"));
        auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
        auto* card = addCard(layout, QStringLiteral("Bluetooth"));

        m_bluetoothStatus = Widgets::mutedLabel(QStringLiteral("Checking…"));
        card->addWidget(m_bluetoothStatus);

        m_bluetoothToggle = new Theme::ToggleSwitch;
        connect(m_bluetoothToggle, &QAbstractButton::toggled, this, [this](bool enabled) {
            Platform::IBluetoothManager* bluetooth = context().bluetoothManager();
            if (bluetooth != nullptr && !m_updating) {
                bluetooth->setEnabled(enabled);
                bluetooth->refresh();
            }
        });
        card->addWidget(Widgets::formRow(QStringLiteral("Bluetooth"),
                                         m_bluetoothToggle,
                                         QStringLiteral("Turn the adapter on or off.")));

        m_bluetoothList = new QListWidget;
        m_bluetoothList->setFrameShape(QFrame::NoFrame);
        m_bluetoothList->setMaximumHeight(250);
        m_bluetoothList->setIconSize(QSize(18, 18));
        card->addWidget(m_bluetoothList);

        auto* actions = new QHBoxLayout;
        actions->setContentsMargins(0, 4, 0, 0);
        auto* refresh = new Theme::AeroButton(QStringLiteral("Refresh"), QStringLiteral("refresh"));
        auto* connectButton = new Theme::AeroButton(QStringLiteral("Connect"), QStringLiteral("bluetooth"));
        auto* disconnectButton = new Theme::AeroButton(QStringLiteral("Disconnect"), QStringLiteral("close"));
        disconnectButton->setVariant(Theme::AeroButton::Variant::Ghost);
        connect(refresh, &QAbstractButton::clicked, this, [this]() {
            if (context().bluetoothManager() != nullptr) {
                context().bluetoothManager()->refresh();
            }
        });
        connect(connectButton, &QAbstractButton::clicked, this, [this]() {
            QListWidgetItem* item = m_bluetoothList->currentItem();
            if (item != nullptr && context().bluetoothManager() != nullptr) {
                context().bluetoothManager()->connectDevice(item->data(Qt::UserRole).toString());
            }
        });
        connect(disconnectButton, &QAbstractButton::clicked, this, [this]() {
            QListWidgetItem* item = m_bluetoothList->currentItem();
            if (item != nullptr && context().bluetoothManager() != nullptr) {
                context().bluetoothManager()->disconnectDevice(item->data(Qt::UserRole).toString());
            }
        });
        actions->addWidget(refresh);
        actions->addWidget(connectButton);
        actions->addWidget(disconnectButton);
        actions->addStretch(1);
        card->addLayout(actions);

        Platform::IBluetoothManager* bluetooth = context().bluetoothManager();
        if (bluetooth == nullptr || !bluetooth->isAvailable()) {
            m_bluetoothStatus->setText(QStringLiteral("Bluetooth is not available on this backend."));
            m_bluetoothToggle->setEnabled(false);
            refresh->setEnabled(false);
            connectButton->setEnabled(false);
            disconnectButton->setEnabled(false);
        } else {
            connect(bluetooth, &Platform::IBluetoothManager::stateChanged, this,
                    [this](const Platform::BluetoothInfo&) { refreshBluetooth(); });
        }
    }

    void refreshBluetooth()
    {
        if (m_bluetoothStatus == nullptr || m_bluetoothList == nullptr || m_bluetoothToggle == nullptr) {
            return;
        }
        Platform::IBluetoothManager* bluetooth = context().bluetoothManager();
        if (bluetooth == nullptr || !bluetooth->isAvailable()) {
            return;
        }
        const Platform::BluetoothInfo info = bluetooth->state();
        m_updating = true;
        m_bluetoothToggle->setChecked(info.enabled);
        m_updating = false;
        m_bluetoothStatus->setText(info.enabled
                                       ? QStringLiteral("%1 device(s) known").arg(info.devices.size())
                                       : QStringLiteral("Bluetooth is turned off."));

        m_bluetoothList->clear();
        for (const Platform::BluetoothDevice& device : info.devices) {
            QString label = device.name.isEmpty() ? device.address : device.name;
            if (device.connected) {
                label += QStringLiteral("  · Connected");
            } else if (device.paired) {
                label += QStringLiteral("  · Paired");
            }
            auto* item = new QListWidgetItem(Theme::IconFactory::icon(QStringLiteral("bluetooth"), 18,
                                                                     Theme::ThemeManager::instance().tokens().color.textSecondary),
                                             label);
            item->setData(Qt::UserRole, device.address);
            if (device.connected) {
                item->setForeground(Theme::ThemeManager::instance().tokens().color.accent);
            }
            m_bluetoothList->addItem(item);
        }
        if (info.devices.isEmpty()) {
            auto* item = new QListWidgetItem(info.enabled ? QStringLiteral("No devices found.")
                                                           : QStringLiteral("Enable Bluetooth to discover devices."));
            item->setFlags(Qt::NoItemFlags);
            m_bluetoothList->addItem(item);
        }
    }

    void buildPowerPage()
    {
        auto* page = makePage(QStringLiteral("Power"),
                              QStringLiteral("Battery and system power actions"),
                              QStringLiteral("battery"));
        auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
        auto* card = addCard(layout, QStringLiteral("Battery"));
        m_batteryLabel = new QLabel(QStringLiteral("Checking…"));
        m_batteryLabel->setProperty("role", QStringLiteral("title"));
        m_batteryLabel->setFont(Theme::ThemeManager::instance().font(24.0, QFont::DemiBold));
        card->addWidget(m_batteryLabel);
        m_batteryDetail = Widgets::mutedLabel(QString());
        card->addWidget(m_batteryDetail);

        auto* actionsCard = addCard(layout, QStringLiteral("System"));
        auto* grid = new QGridLayout;
        grid->setSpacing(8);
        m_lockButton = new Theme::AeroButton(QStringLiteral("Lock"), QStringLiteral("lock"));
        m_suspendButton = new Theme::AeroButton(QStringLiteral("Suspend"), QStringLiteral("moon"));
        m_rebootButton = new Theme::AeroButton(QStringLiteral("Restart"), QStringLiteral("refresh"));
        m_powerOffButton = new Theme::AeroButton(QStringLiteral("Shut Down"), QStringLiteral("power"));
        m_powerOffButton->setVariant(Theme::AeroButton::Variant::Danger);
        grid->addWidget(m_lockButton, 0, 0);
        grid->addWidget(m_suspendButton, 0, 1);
        grid->addWidget(m_rebootButton, 1, 0);
        grid->addWidget(m_powerOffButton, 1, 1);
        actionsCard->addLayout(grid);

        auto* note = Widgets::mutedLabel(
            QStringLiteral("Actions are only enabled when the platform reports the capability. "
                           "Restart and shut down require privileged host integration."));
        note->setFont(Theme::ThemeManager::instance().captionFont());
        actionsCard->addWidget(note);

        connect(m_lockButton, &QAbstractButton::clicked, this, [this]() {
            Platform::IPowerManager* power = context().powerManager();
            if (power != nullptr && power->lock()) {
                return;
            }
            if (context().sessionManager() != nullptr) {
                context().sessionManager()->lockSession();
            }
        });
        connect(m_suspendButton, &QAbstractButton::clicked, this, [this]() {
            if (confirm(QStringLiteral("Suspend"), QStringLiteral("Suspend the system now?"))) {
                if (context().powerManager() != nullptr) {
                    context().powerManager()->suspend();
                }
            }
        });
        connect(m_rebootButton, &QAbstractButton::clicked, this, [this]() {
            if (confirm(QStringLiteral("Restart"),
                        QStringLiteral("Restart the system now? Unsaved work will be lost."))) {
                if (context().powerManager() != nullptr) {
                    context().powerManager()->reboot();
                }
            }
        });
        connect(m_powerOffButton, &QAbstractButton::clicked, this, [this]() {
            if (confirm(QStringLiteral("Shut Down"),
                        QStringLiteral("Shut down the system now? Unsaved work will be lost."))) {
                if (context().powerManager() != nullptr) {
                    context().powerManager()->powerOff();
                }
            }
        });

        Platform::IPowerManager* power = context().powerManager();
        if (power != nullptr) {
            connect(power, &Platform::IPowerManager::batteryChanged, this,
                    [this](const Platform::BatteryInfo&) { refreshPower(); });
            connect(power, &Platform::IPowerManager::capabilitiesChanged, this,
                    [this]() { refreshPower(); });
        }
    }

    bool confirm(const QString& title, const QString& text)
    {
        return QMessageBox::question(this, title, text,
                                     QMessageBox::Yes | QMessageBox::Cancel,
                                     QMessageBox::Cancel) == QMessageBox::Yes;
    }

    void refreshPower()
    {
        if (m_batteryLabel == nullptr) {
            return;
        }
        Platform::IPowerManager* power = context().powerManager();
        if (power == nullptr || !power->isAvailable()) {
            m_batteryLabel->setText(QStringLiteral("Unavailable"));
            m_batteryDetail->setText(QStringLiteral("The platform does not expose power management."));
            m_lockButton->setEnabled(context().sessionManager() != nullptr);
            m_suspendButton->setEnabled(false);
            m_rebootButton->setEnabled(false);
            m_powerOffButton->setEnabled(false);
            return;
        }

        const Platform::BatteryInfo battery = power->battery();
        if (battery.present) {
            m_batteryLabel->setText(QStringLiteral("%1%").arg(battery.percent));
            m_batteryDetail->setText(battery.charging
                                         ? QStringLiteral("Charging%1")
                                               .arg(battery.secondsRemaining > 0
                                                        ? QStringLiteral(" · %1 until full")
                                                              .arg(Core::Utils::formatDuration(battery.secondsRemaining))
                                                        : QString())
                                         : (battery.fullyCharged
                                                ? QStringLiteral("Fully charged")
                                                : QStringLiteral("About %1 remaining")
                                                      .arg(Core::Utils::formatDuration(battery.secondsRemaining))));
        } else {
            m_batteryLabel->setText(QStringLiteral("AC Power"));
            m_batteryDetail->setText(QStringLiteral("No battery is present on this system."));
        }

        const Platform::PowerCapabilities capabilities = power->capabilities();
        m_lockButton->setEnabled(capabilities.canLock || context().sessionManager() != nullptr);
        m_suspendButton->setEnabled(capabilities.canSuspend);
        m_rebootButton->setEnabled(capabilities.canReboot);
        m_powerOffButton->setEnabled(capabilities.canPowerOff);
    }

    void buildNotificationsPage()
    {
        auto* page = makePage(QStringLiteral("Notifications"),
                              QStringLiteral("Alerts, banners and history"),
                              QStringLiteral("bell"));
        auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
        auto* card = addCard(layout, QStringLiteral("Delivery"));
        Services::NotificationBus* bus = context().notifications;
        if (bus == nullptr) {
            card->addWidget(Widgets::mutedLabel(
                QStringLiteral("The notification bus is not attached to this session.")));
        }

        addToggleRow(card, QStringLiteral("Do Not Disturb"),
                     QStringLiteral("Collect notifications in the center without showing banners."),
                     bus != nullptr ? bus->doNotDisturb()
                                    : (context().settings != nullptr && context().settings->doNotDisturb()),
                     [this](bool enabled) {
                         if (context().settings != nullptr) {
                             context().settings->setDoNotDisturb(enabled);
                         }
                         if (context().notifications != nullptr) {
                             context().notifications->setDoNotDisturb(enabled);
                         }
                     });
        addToggleRow(card, QStringLiteral("Show message previews"),
                     QStringLiteral("Include the notification body in banners."),
                     context().settings == nullptr
                         || context().settings->value(settingsKey(Settings::Keys::NotificationsPreview), true).toBool(),
                     [this](bool enabled) {
                         if (context().settings != nullptr) {
                             context().settings->setValue(settingsKey(Settings::Keys::NotificationsPreview), enabled);
                         }
                     });

        auto* historyCard = addCard(layout, QStringLiteral("History"));
        m_notificationCount = Widgets::mutedLabel(QStringLiteral("No notifications."));
        historyCard->addWidget(m_notificationCount);

        auto* limitHost = new QWidget;
        auto* limitLayout = new QHBoxLayout(limitHost);
        limitLayout->setContentsMargins(0, 4, 0, 4);
        auto* limit = new QSpinBox;
        limit->setRange(10, 500);
        limit->setSingleStep(10);
        limit->setValue(context().settings != nullptr
                            ? context().settings->value(settingsKey(Settings::Keys::NotificationsHistoryLimit), 100).toInt()
                            : 100);
        connect(limit, &QSpinBox::valueChanged, this, [this](int value) {
            if (context().settings != nullptr) {
                context().settings->setValue(settingsKey(Settings::Keys::NotificationsHistoryLimit), value);
            }
        });
        limitLayout->addWidget(limit);
        limitLayout->addStretch(1);
        historyCard->addWidget(Widgets::formRow(QStringLiteral("History limit"), limitHost,
                                               QStringLiteral("Older notifications are trimmed first.")));

        auto* actions = new QHBoxLayout;
        actions->setContentsMargins(0, 6, 0, 0);
        auto* test = new Theme::AeroButton(QStringLiteral("Send Test Notification"), QStringLiteral("bell"));
        auto* markRead = new Theme::AeroButton(QStringLiteral("Mark All Read"));
        auto* clear = new Theme::AeroButton(QStringLiteral("Clear History"), QStringLiteral("trash"));
        clear->setVariant(Theme::AeroButton::Variant::Ghost);
        connect(test, &QAbstractButton::clicked, this, [this]() {
            if (context().notifications != nullptr) {
                context().notifications->post(QStringLiteral("aero.desktop"),
                                              QStringLiteral("Aero Desktop"),
                                              QStringLiteral("Notifications are working"),
                                              QStringLiteral("This is a test notification from Settings."),
                                              QStringLiteral("bell"));
            }
        });
        connect(markRead, &QAbstractButton::clicked, this, [this]() {
            if (context().notifications != nullptr) {
                context().notifications->markAllRead();
            }
        });
        connect(clear, &QAbstractButton::clicked, this, [this]() {
            if (context().notifications != nullptr) {
                context().notifications->clearAll();
            }
        });
        actions->addWidget(test);
        actions->addWidget(markRead);
        actions->addWidget(clear);
        actions->addStretch(1);
        historyCard->addLayout(actions);

        if (bus != nullptr) {
            connect(bus, &Services::NotificationBus::changed, this, [this]() { refreshNotifications(); });
        }
    }

    void refreshNotifications()
    {
        if (m_notificationCount == nullptr) {
            return;
        }
        Services::NotificationBus* bus = context().notifications;
        if (bus == nullptr) {
            m_notificationCount->setText(QStringLiteral("Notification service unavailable."));
            return;
        }
        m_notificationCount->setText(bus->count() == 0
                                         ? QStringLiteral("No notifications.")
                                         : QStringLiteral("%1 notifications · %2 unread")
                                               .arg(bus->count())
                                               .arg(bus->unreadCount()));
    }

    void buildUsersPage()
    {
        auto* page = makePage(QStringLiteral("Users"),
                              QStringLiteral("Accounts and session"),
                              QStringLiteral("user"));
        auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
        auto* card = addCard(layout, QStringLiteral("Current user"));

        m_userAvatar = new QLabel;
        m_userAvatar->setFixedSize(64, 64);
        m_userName = new QLabel(QStringLiteral("—"));
        m_userName->setProperty("role", QStringLiteral("title"));
        m_userName->setFont(Theme::ThemeManager::instance().font(19.0, QFont::DemiBold));
        m_userDetail = Widgets::mutedLabel(QString());

        auto* userRow = new QHBoxLayout;
        userRow->setContentsMargins(0, 4, 0, 8);
        userRow->setSpacing(14);
        userRow->addWidget(m_userAvatar);
        auto* info = new QVBoxLayout;
        info->setSpacing(2);
        info->addWidget(m_userName);
        info->addWidget(m_userDetail);
        userRow->addLayout(info, 1);
        card->addLayout(userRow);

        auto* actions = new QHBoxLayout;
        actions->setContentsMargins(0, 2, 0, 0);
        auto* lock = new Theme::AeroButton(QStringLiteral("Lock Session"), QStringLiteral("lock"));
        auto* logout = new Theme::AeroButton(QStringLiteral("Log Out"), QStringLiteral("power"));
        logout->setVariant(Theme::AeroButton::Variant::Ghost);
        connect(lock, &QAbstractButton::clicked, this, [this]() {
            if (context().sessionManager() != nullptr) {
                context().sessionManager()->lockSession();
            }
        });
        connect(logout, &QAbstractButton::clicked, this, [this]() {
            if (context().sessionManager() == nullptr) {
                return;
            }
            if (!confirm(QStringLiteral("Log Out"),
                         QStringLiteral("End the current session and log out?"))) {
                return;
            }
            if (!context().sessionManager()->logoutSession()) {
                QMessageBox::warning(this, QStringLiteral("Log Out"),
                                     QStringLiteral("The platform backend could not end this session."));
            }
        });
        actions->addWidget(lock);
        actions->addWidget(logout);
        actions->addStretch(1);
        card->addLayout(actions);

        auto* accountsCard = addCard(layout, QStringLiteral("Accounts"));
        m_usersList = new QListWidget;
        m_usersList->setFrameShape(QFrame::NoFrame);
        m_usersList->setIconSize(QSize(28, 28));
        m_usersList->setMaximumHeight(220);
        accountsCard->addWidget(m_usersList);

        if (context().sessionManager() == nullptr || !context().sessionManager()->isAvailable()) {
            m_userDetail->setText(QStringLiteral("Session management is not available on this backend."));
            lock->setEnabled(false);
            logout->setEnabled(false);
            m_userName->setText(QStringLiteral("Unknown user"));
        }
    }

    void refreshUsers()
    {
        if (m_userAvatar == nullptr || context().sessionManager() == nullptr) {
            return;
        }
        const Platform::UserInfo user = context().sessionManager()->currentUser();
        const Platform::SessionInfo session = context().sessionManager()->session();
        const QString display = user.realName.isEmpty() ? user.userName : user.realName;
        m_userAvatar->setPixmap(avatarPixmap(display.isEmpty() ? QStringLiteral("Aero") : display, 64));
        m_userName->setText(display.isEmpty() ? QStringLiteral("Unknown user") : display);
        m_userDetail->setText(QStringLiteral("%1 · %2 · %3")
                                  .arg(user.userName.isEmpty() ? QStringLiteral("user") : user.userName,
                                       session.sessionType.isEmpty() ? QStringLiteral("session")
                                                                     : session.sessionType,
                                       session.hostName.isEmpty() ? QStringLiteral("local")
                                                                  : session.hostName));

        m_usersList->clear();
        for (const Platform::UserInfo& account : context().sessionManager()->users()) {
            auto* item = new QListWidgetItem(QIcon(avatarPixmap(account.realName.isEmpty() ? account.userName
                                                                                           : account.realName, 28)),
                                             QStringLiteral("%1  ·  %2")
                                                 .arg(account.userName,
                                                      account.realName.isEmpty() ? QStringLiteral("Account")
                                                                                 : account.realName));
            if (account.uid == user.uid) {
                item->setForeground(Theme::ThemeManager::instance().tokens().color.accent);
            }
            m_usersList->addItem(item);
        }
        if (m_usersList->count() == 0) {
            auto* item = new QListWidgetItem(QStringLiteral("No accounts reported."));
            item->setFlags(Qt::NoItemFlags);
            m_usersList->addItem(item);
        }
    }

    void buildAccessibilityPage()
    {
        auto* page = makePage(QStringLiteral("Accessibility"),
                              QStringLiteral("Readability and motion"),
                              QStringLiteral("accessibility"));
        auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
        auto* card = addCard(layout, QStringLiteral("Display and motion"));

        addSliderRow(card, QStringLiteral("Text size"),
                     QStringLiteral("Scale all interface text."),
                     80, 150,
                     context().settings != nullptr ? qRound(context().settings->fontScale() * 100.0) : 100,
                     [this](int value) {
                         if (context().settings != nullptr) {
                             context().settings->setFontScale(value / 100.0);
                         }
                     });
        addToggleRow(card, QStringLiteral("Reduce motion"),
                     QStringLiteral("Disable animated window transitions."),
                     context().settings != nullptr && context().settings->reducedMotion(),
                     [this](bool enabled) {
                         if (context().settings != nullptr) {
                             context().settings->setReducedMotion(enabled);
                         }
                     });
        addToggleRow(card, QStringLiteral("Reduce transparency"),
                     QStringLiteral("Increase surface opacity and disable backdrop sampling."),
                     context().settings != nullptr && !context().settings->transparencyEnabled(),
                     [this](bool reduced) {
                         if (context().settings != nullptr) {
                             context().settings->setTransparencyEnabled(!reduced);
                         }
                     });

        auto* note = Widgets::mutedLabel(
            QStringLiteral("Cursor scaling, screen readers and keyboard navigation policies are provided by a "
                           "DreamyOS accessibility service. Aero keeps these controls disabled until such a service "
                           "is attached through AeroPlatform."));
        note->setFont(Theme::ThemeManager::instance().captionFont());
        card->addWidget(note);
    }

    void buildAboutPage()
    {
        auto* page = makePage(QStringLiteral("About"),
                              QStringLiteral("Version, backend and configuration"),
                              QStringLiteral("info"));
        auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
        auto* card = addCard(layout, QStringLiteral("Aero Desktop"));
        auto* version = new QLabel(QStringLiteral("Version %1 · “%2”")
                                       .arg(Core::version(), QString::fromLatin1(Core::CodeName)));
        version->setProperty("role", QStringLiteral("title"));
        version->setFont(Theme::ThemeManager::instance().font(20.0, QFont::DemiBold));
        card->addWidget(version);
        card->addWidget(Widgets::mutedLabel(
            QStringLiteral("A liquid-glass desktop built with C++20 and Qt 6 Widgets. "
                           "The shell and every application paint with QPainter; no QML or web UI is used.")));

        auto* infoCard = addCard(layout, QStringLiteral("Runtime"));
        infoCard->addWidget(Widgets::formRow(
            QStringLiteral("Platform backend"),
            new QLabel(context().platform != nullptr ? context().platform->backendDisplayName()
                                                     : QStringLiteral("None")),
            QStringLiteral("Selected at startup.")));
        infoCard->addWidget(Widgets::formRow(
            QStringLiteral("Settings file"),
            new QLabel(Core::Paths::elideHome(Core::Paths::settingsFile())),
            QStringLiteral("Debounced, atomic JSON storage.")));

        auto* actions = new QHBoxLayout;
        actions->setContentsMargins(0, 6, 0, 0);
        auto* about = new Theme::AeroButton(QStringLiteral("Open About Aero"), QStringLiteral("info"));
        connect(about, &QAbstractButton::clicked, this, [this]() {
            context().launch(QString::fromLatin1(AppIds::About));
        });
        auto* reset = new Theme::AeroButton(QStringLiteral("Reset All Settings"), QStringLiteral("refresh"));
        reset->setVariant(Theme::AeroButton::Variant::Danger);
        connect(reset, &QAbstractButton::clicked, this, [this]() {
            if (!confirm(QStringLiteral("Reset Settings"),
                         QStringLiteral("Reset every Aero preference to its default value? "
                                        "This cannot be undone."))) {
                return;
            }
            if (context().settings != nullptr) {
                context().settings->resetToDefaults();
                context().settings->flush();
                context().notify(QStringLiteral("Settings reset"),
                                 QStringLiteral("Defaults restored. Reopen Settings to see every value."),
                                 QStringLiteral("gear"));
            }
        });
        actions->addWidget(about);
        actions->addWidget(reset);
        actions->addStretch(1);
        card->addLayout(actions);
    }

    QListWidget* m_sidebar = nullptr;
    QStackedWidget* m_stack = nullptr;
    QList<WallpaperTile*> m_wallpaperTiles;
    QLabel* m_wallpaperPathLabel = nullptr;
    QListWidget* m_recentList = nullptr;
    QVBoxLayout* m_displayLayout = nullptr;
    QLabel* m_networkStatus = nullptr;
    Theme::ToggleSwitch* m_wifiToggle = nullptr;
    QListWidget* m_networkList = nullptr;
    QLabel* m_bluetoothStatus = nullptr;
    Theme::ToggleSwitch* m_bluetoothToggle = nullptr;
    QListWidget* m_bluetoothList = nullptr;
    Theme::AeroSlider* m_soundVolume = nullptr;
    Theme::ToggleSwitch* m_soundMute = nullptr;
    QComboBox* m_outputDevice = nullptr;
    QLabel* m_batteryLabel = nullptr;
    QLabel* m_batteryDetail = nullptr;
    Theme::AeroButton* m_lockButton = nullptr;
    Theme::AeroButton* m_suspendButton = nullptr;
    Theme::AeroButton* m_rebootButton = nullptr;
    Theme::AeroButton* m_powerOffButton = nullptr;
    QLabel* m_notificationCount = nullptr;
    QLabel* m_userAvatar = nullptr;
    QLabel* m_userName = nullptr;
    QLabel* m_userDetail = nullptr;
    QListWidget* m_usersList = nullptr;
    bool m_updating = false;
};

} // namespace

AppView* createSettingsApp(const AppContext& context, QWidget* parent)
{
    return new SettingsView(context, parent);
}

} // namespace Aero::Applications
