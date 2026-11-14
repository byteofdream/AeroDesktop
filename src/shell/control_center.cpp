#include "aero/shell/control_center.h"

#include "aero/platform/i_audio_manager.h"
#include "aero/platform/i_bluetooth_manager.h"
#include "aero/platform/i_display_manager.h"
#include "aero/platform/i_network_manager.h"
#include "aero/platform/i_power_manager.h"
#include "aero/platform/platform.h"
#include "aero/services/notification_bus.h"
#include "aero/settings/settings_keys.h"
#include "aero/settings/settings_manager.h"
#include "aero/theme/controls.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/theme.h"

#include <QAbstractButton>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

namespace Aero::Shell {
namespace {

QLabel* labelWithRole(const QString& text, const QString& role, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setProperty("role", role);
    return label;
}

QLabel* iconLabel(const QString& iconName, int size, QWidget* parent)
{
    auto* label = new QLabel(parent);
    label->setFixedSize(size, size);
    label->setPixmap(Theme::IconFactory::pixmap(iconName, size,
                                               Theme::ThemeManager::instance().tokens().color.textSecondary,
                                               parent != nullptr ? parent->devicePixelRatioF() : 2.0));
    return label;
}

QFrame* separatorLine(QWidget* parent)
{
    auto* line = new QFrame(parent);
    line->setProperty("role", QStringLiteral("separator"));
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    return line;
}

} // namespace

ControlCenter::ControlCenter(Platform::Platform* platform,
                             Settings::SettingsManager* settings,
                             Services::NotificationBus* notifications,
                             QWidget* parent)
    : Overlay(parent)
    , m_platform(platform)
    , m_settings(settings)
    , m_notifications(notifications)
{
    setCornerRadius(22);
    setShadowMargin(18);
    setTintStrength(1.10);
    resize(400, 584);
    build();
    connectSignals();
    refresh();
}

void ControlCenter::build()
{
    const int margin = shadowMargin();
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(margin + 18, margin + 14, margin + 18, margin + 16);
    root->setSpacing(12);

    auto* header = new QHBoxLayout;
    header->setSpacing(8);
    auto* title = new QLabel(QStringLiteral("Control Center"), this);
    title->setFont(Theme::ThemeManager::instance().font(19.0, QFont::DemiBold));
    header->addWidget(title);
    header->addStretch(1);
    auto* close = new Theme::IconButton(QStringLiteral("close"), this);
    close->setIconSize(13);
    close->setFixedSize(26, 26);
    close->setToolTip(QStringLiteral("Close"));
    connect(close, &QAbstractButton::clicked, this, &ControlCenter::dismiss);
    header->addWidget(close);
    root->addLayout(header);

    auto* connectivity = new Theme::Card(this);
    auto* connectivityLayout = new QVBoxLayout(connectivity);
    connectivityLayout->setContentsMargins(14, 10, 14, 10);
    connectivityLayout->setSpacing(10);
    connectivityLayout->addWidget(labelWithRole(QStringLiteral("CONNECTIVITY"), QStringLiteral("section"), connectivity));

    auto addToggleRow = [connectivity, connectivityLayout](const QString& icon,
                                                           const QString& title,
                                                           QLabel** status,
                                                           Theme::ToggleSwitch** toggle) {
        auto* row = new QWidget(connectivity);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 2, 0, 2);
        layout->setSpacing(10);
        layout->addWidget(iconLabel(icon, 20, row), 0, Qt::AlignVCenter);
        auto* textHost = new QWidget(row);
        auto* textLayout = new QVBoxLayout(textHost);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(0);
        auto* name = new QLabel(title, textHost);
        name->setFont(Theme::ThemeManager::instance().bodyFont(QFont::DemiBold));
        textLayout->addWidget(name);
        *status = new QLabel(QStringLiteral("Checking…"), textHost);
        (*status)->setProperty("role", QStringLiteral("muted"));
        (*status)->setFont(Theme::ThemeManager::instance().captionFont());
        textLayout->addWidget(*status);
        layout->addWidget(textHost, 1);
        *toggle = new Theme::ToggleSwitch(row);
        layout->addWidget(*toggle, 0, Qt::AlignVCenter);
        connectivityLayout->addWidget(row);
    };

    addToggleRow(QStringLiteral("wifi"), QStringLiteral("Wi-Fi"), &m_networkLabel, &m_wifi);
    addToggleRow(QStringLiteral("bluetooth"), QStringLiteral("Bluetooth"), &m_bluetoothLabel, &m_bluetooth);

    auto* dndRow = new QWidget(connectivity);
    auto* dndLayout = new QHBoxLayout(dndRow);
    dndLayout->setContentsMargins(0, 2, 0, 2);
    dndLayout->setSpacing(10);
    dndLayout->addWidget(iconLabel(QStringLiteral("moon"), 20, dndRow), 0, Qt::AlignVCenter);
    auto* dndText = new QWidget(dndRow);
    auto* dndTextLayout = new QVBoxLayout(dndText);
    dndTextLayout->setContentsMargins(0, 0, 0, 0);
    dndTextLayout->setSpacing(0);
    auto* dndName = new QLabel(QStringLiteral("Do Not Disturb"), dndText);
    dndName->setFont(Theme::ThemeManager::instance().bodyFont(QFont::DemiBold));
    dndTextLayout->addWidget(dndName);
    auto* dndSub = new QLabel(QStringLiteral("Keep notifications quiet"), dndText);
    dndSub->setProperty("role", QStringLiteral("muted"));
    dndSub->setFont(Theme::ThemeManager::instance().captionFont());
    dndTextLayout->addWidget(dndSub);
    dndLayout->addWidget(dndText, 1);
    m_dnd = new Theme::ToggleSwitch(dndRow);
    dndLayout->addWidget(m_dnd, 0, Qt::AlignVCenter);
    connectivityLayout->addWidget(dndRow);

    root->addWidget(connectivity);

    auto* output = new Theme::Card(this);
    auto* outputLayout = new QVBoxLayout(output);
    outputLayout->setContentsMargins(14, 10, 14, 12);
    outputLayout->setSpacing(8);
    outputLayout->addWidget(labelWithRole(QStringLiteral("OUTPUT & DISPLAY"), QStringLiteral("section"), output));

    auto* volumeRow = new QWidget(output);
    auto* volumeLayout = new QHBoxLayout(volumeRow);
    volumeLayout->setContentsMargins(0, 0, 0, 0);
    volumeLayout->setSpacing(10);
    volumeLayout->addWidget(iconLabel(QStringLiteral("volume"), 20, volumeRow), 0, Qt::AlignVCenter);
    auto* volumeHost = new QWidget(volumeRow);
    auto* volumeHostLayout = new QVBoxLayout(volumeHost);
    volumeHostLayout->setContentsMargins(0, 0, 0, 0);
    volumeHostLayout->setSpacing(1);
    volumeHostLayout->addWidget(labelWithRole(QStringLiteral("Volume"), QStringLiteral("value"), volumeHost));
    m_audioLabel = labelWithRole(QStringLiteral("…"), QStringLiteral("muted"), volumeHost);
    m_audioLabel->setFont(Theme::ThemeManager::instance().captionFont());
    volumeHostLayout->addWidget(m_audioLabel);
    volumeLayout->addWidget(volumeHost, 0, Qt::AlignVCenter);
    m_volume = new Theme::AeroSlider(0, 100, 60, volumeRow);
    m_volume->setShowValueLabel(true);
    volumeLayout->addWidget(m_volume, 1);
    outputLayout->addWidget(volumeRow);

    auto* brightnessRow = new QWidget(output);
    auto* brightnessLayout = new QHBoxLayout(brightnessRow);
    brightnessLayout->setContentsMargins(0, 0, 0, 0);
    brightnessLayout->setSpacing(10);
    brightnessLayout->addWidget(iconLabel(QStringLiteral("sun"), 20, brightnessRow), 0, Qt::AlignVCenter);
    auto* brightnessHost = new QWidget(brightnessRow);
    auto* brightnessHostLayout = new QVBoxLayout(brightnessHost);
    brightnessHostLayout->setContentsMargins(0, 0, 0, 0);
    brightnessHostLayout->setSpacing(1);
    brightnessHostLayout->addWidget(labelWithRole(QStringLiteral("Display brightness"), QStringLiteral("value"), brightnessHost));
    m_displayLabel = labelWithRole(QStringLiteral("…"), QStringLiteral("muted"), brightnessHost);
    m_displayLabel->setFont(Theme::ThemeManager::instance().captionFont());
    brightnessHostLayout->addWidget(m_displayLabel);
    brightnessLayout->addWidget(brightnessHost, 0, Qt::AlignVCenter);
    m_brightness = new Theme::AeroSlider(5, 100, 80, brightnessRow);
    m_brightness->setShowValueLabel(true);
    brightnessLayout->addWidget(m_brightness, 1);
    outputLayout->addWidget(brightnessRow);

    outputLayout->addWidget(separatorLine(output));

    auto* appearanceRow = new QWidget(output);
    auto* appearanceLayout = new QHBoxLayout(appearanceRow);
    appearanceLayout->setContentsMargins(0, 0, 0, 0);
    appearanceLayout->setSpacing(10);
    appearanceLayout->addWidget(iconLabel(QStringLiteral("palette"), 20, appearanceRow), 0, Qt::AlignVCenter);
    appearanceLayout->addWidget(labelWithRole(QStringLiteral("Appearance"), QStringLiteral("value"), appearanceRow), 0,
                                Qt::AlignVCenter);
    appearanceLayout->addStretch(1);
    m_appearance = new Theme::SegmentedControl({QStringLiteral("Light"),
                                                QStringLiteral("Dark"),
                                                QStringLiteral("Auto")},
                                               appearanceRow);
    appearanceLayout->addWidget(m_appearance, 0, Qt::AlignVCenter);
    outputLayout->addWidget(appearanceRow);

    root->addWidget(output);

    auto* powerCard = new Theme::Card(this);
    auto* powerLayout = new QHBoxLayout(powerCard);
    powerLayout->setContentsMargins(14, 10, 14, 10);
    powerLayout->setSpacing(10);
    powerLayout->addWidget(iconLabel(QStringLiteral("battery"), 20, powerCard));
    m_powerLabel = labelWithRole(QStringLiteral("Checking power…"), QStringLiteral("value"), powerCard);
    powerLayout->addWidget(m_powerLabel, 1);
    root->addWidget(powerCard);

    root->addStretch(1);

    auto* actions = new QHBoxLayout;
    actions->setSpacing(8);
    m_settingsButton = new Theme::AeroButton(QStringLiteral("Settings"), QStringLiteral("gear"), this);
    m_settingsButton->setVariant(Theme::AeroButton::Variant::Primary);
    m_lockButton = new Theme::AeroButton(QStringLiteral("Lock"), QStringLiteral("lock"), this);
    actions->addWidget(m_settingsButton, 1);
    actions->addWidget(m_lockButton, 1);
    root->addLayout(actions);

    auto* displaySettings = new Theme::AeroButton(QStringLiteral("Display settings"), QStringLiteral("display"), this);
    displaySettings->setVariant(Theme::AeroButton::Variant::Ghost);
    connect(displaySettings, &QAbstractButton::clicked, this, [this]() {
        emit displaySettingsRequested();
        dismiss();
    });
    root->addWidget(displaySettings);
}

void ControlCenter::connectSignals()
{
    connect(m_wifi, &QAbstractButton::toggled, this, [this](bool enabled) {
        if (m_updating) {
            return;
        }
        Platform::INetworkManager* network = m_platform != nullptr ? m_platform->networkManager() : nullptr;
        if (network != nullptr && network->isAvailable()) {
            network->setWifiEnabled(enabled);
            if (enabled) {
                network->scanWifi();
            }
            QTimer::singleShot(700, this, &ControlCenter::refreshNetwork);
        }
    });

    connect(m_bluetooth, &QAbstractButton::toggled, this, [this](bool enabled) {
        if (m_updating) {
            return;
        }
        Platform::IBluetoothManager* bluetooth = m_platform != nullptr ? m_platform->bluetoothManager() : nullptr;
        if (bluetooth != nullptr && bluetooth->isAvailable()) {
            bluetooth->setEnabled(enabled);
            bluetooth->refresh();
            QTimer::singleShot(700, this, &ControlCenter::refreshBluetooth);
        }
    });

    connect(m_dnd, &QAbstractButton::toggled, this, [this](bool enabled) {
        if (m_updating) {
            return;
        }
        if (m_notifications != nullptr) {
            m_notifications->setDoNotDisturb(enabled);
        } else if (m_settings != nullptr) {
            m_settings->setDoNotDisturb(enabled);
        }
    });

    connect(m_volume, &Theme::AeroSlider::valueChanged, this, [this](int value) {
        if (m_updating) {
            return;
        }
        if (m_settings != nullptr) {
            m_settings->setValue(QString::fromLatin1(Settings::Keys::SoundVolume), value);
            m_settings->setValue(QString::fromLatin1(Settings::Keys::SoundMuted), false);
        }
        Platform::IAudioManager* audio = m_platform != nullptr ? m_platform->audioManager() : nullptr;
        if (audio != nullptr && audio->isAvailable()) {
            audio->setMuted(false);
            audio->setVolume(value);
        }
    });

    connect(m_brightness, &Theme::AeroSlider::valueChanged, this, [this](int value) {
        if (m_updating) {
            return;
        }
        if (m_settings != nullptr) {
            m_settings->setValue(QString::fromLatin1(Settings::Keys::DisplayBrightness), value);
        }
        Platform::IDisplayManager* display = m_platform != nullptr ? m_platform->displayManager() : nullptr;
        if (display != nullptr && display->isAvailable() && display->supportsBrightness()) {
            const QList<Platform::DisplayInfo> displays = display->displays();
            for (const Platform::DisplayInfo& info : displays) {
                if (info.primary || displays.size() == 1) {
                    display->setBrightness(info.id, value);
                }
            }
        }
    });

    connect(m_appearance, &Theme::SegmentedControl::currentIndexChanged, this, [this](int index) {
        if (m_updating || m_settings == nullptr) {
            return;
        }
        const QString mode = index == 0 ? QStringLiteral("light")
            : (index == 1 ? QStringLiteral("dark") : QStringLiteral("auto"));
        m_settings->setAppearanceMode(mode);
    });

    connect(m_settingsButton, &QAbstractButton::clicked, this, [this]() {
        emit settingsRequested();
        dismiss();
    });
    connect(m_lockButton, &QAbstractButton::clicked, this, [this]() {
        emit lockRequested();
        dismiss();
    });

    if (m_platform != nullptr) {
        if (auto* network = m_platform->networkManager()) {
            connect(network, &Platform::INetworkManager::networkChanged,
                    this, [this](const Platform::NetworkInfo&) { refreshNetwork(); });
            connect(network, &Platform::INetworkManager::wifiNetworksChanged,
                    this, [this](const QList<Platform::WifiNetwork>&) { refreshNetwork(); });
        }
        if (auto* bluetooth = m_platform->bluetoothManager()) {
            connect(bluetooth, &Platform::IBluetoothManager::stateChanged,
                    this, [this](const Platform::BluetoothInfo&) { refreshBluetooth(); });
        }
        if (auto* audio = m_platform->audioManager()) {
            connect(audio, &Platform::IAudioManager::stateChanged,
                    this, [this](const Platform::AudioInfo&) { refreshAudio(); });
            connect(audio, &Platform::IAudioManager::devicesChanged,
                    this, &ControlCenter::refreshAudio);
        }
        if (auto* power = m_platform->powerManager()) {
            connect(power, &Platform::IPowerManager::batteryChanged,
                    this, [this](const Platform::BatteryInfo&) { refreshPower(); });
        }
        if (auto* display = m_platform->displayManager()) {
            connect(display, &Platform::IDisplayManager::displaysChanged,
                    this, &ControlCenter::refreshDisplay);
            connect(display, &Platform::IDisplayManager::brightnessChanged,
                    this, [this](const QString&, int) { refreshDisplay(); });
        }
    }

    if (m_notifications != nullptr) {
        connect(m_notifications, &Services::NotificationBus::changed,
                this, &ControlCenter::refresh);
    }
    if (m_settings != nullptr) {
        connect(m_settings, &Settings::SettingsManager::settingChanged, this,
                [this](const QString& key, const QVariant&) {
                    if (key == QString::fromLatin1(Settings::Keys::AppearanceMode)) {
                        refreshAppearance();
                    }
                });
    }

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(2500);
    connect(m_refreshTimer, &QTimer::timeout, this, &ControlCenter::refresh);
}

void ControlCenter::refresh()
{
    refreshNetwork();
    refreshBluetooth();
    refreshAudio();
    refreshDisplay();
    refreshPower();
    refreshAppearance();

    if (m_dnd != nullptr) {
        m_updating = true;
        const bool enabled = m_notifications != nullptr
            ? m_notifications->doNotDisturb()
            : (m_settings != nullptr && m_settings->doNotDisturb());
        m_dnd->setChecked(enabled);
        m_updating = false;
    }
}

void ControlCenter::refreshNetwork()
{
    if (m_wifi == nullptr || m_networkLabel == nullptr) {
        return;
    }
    Platform::INetworkManager* network = m_platform != nullptr ? m_platform->networkManager() : nullptr;
    const bool available = network != nullptr && network->isAvailable();
    m_wifi->setEnabled(available);
    if (!available) {
        m_updating = true;
        m_wifi->setChecked(false);
        m_updating = false;
        m_networkLabel->setText(QStringLiteral("Unavailable on this backend"));
        return;
    }

    const Platform::NetworkInfo info = network->network();
    m_updating = true;
    m_wifi->setChecked(info.wifiEnabled);
    m_updating = false;

    if (!info.wifiEnabled) {
        m_networkLabel->setText(QStringLiteral("Wi-Fi is off"));
    } else if (info.connected) {
        m_networkLabel->setText(info.wifi
                                    ? QStringLiteral("%1 · %2%").arg(info.ssid).arg(info.signalStrength)
                                    : QStringLiteral("Wired · %1").arg(info.ipAddress));
    } else {
        m_networkLabel->setText(QStringLiteral("Not connected"));
    }
}

void ControlCenter::refreshBluetooth()
{
    if (m_bluetooth == nullptr || m_bluetoothLabel == nullptr) {
        return;
    }
    Platform::IBluetoothManager* bluetooth = m_platform != nullptr ? m_platform->bluetoothManager() : nullptr;
    const bool available = bluetooth != nullptr && bluetooth->isAvailable();
    m_bluetooth->setEnabled(available);
    if (!available) {
        m_updating = true;
        m_bluetooth->setChecked(false);
        m_updating = false;
        m_bluetoothLabel->setText(QStringLiteral("Unavailable on this backend"));
        return;
    }

    const Platform::BluetoothInfo info = bluetooth->state();
    m_updating = true;
    m_bluetooth->setChecked(info.enabled);
    m_updating = false;
    if (!info.enabled) {
        m_bluetoothLabel->setText(QStringLiteral("Off"));
    } else if (info.devices.isEmpty()) {
        m_bluetoothLabel->setText(QStringLiteral("On · no devices"));
    } else {
        m_bluetoothLabel->setText(QStringLiteral("On · %1 device(s)").arg(info.devices.size()));
    }
}

void ControlCenter::refreshAudio()
{
    if (m_volume == nullptr || m_audioLabel == nullptr) {
        return;
    }
    Platform::IAudioManager* audio = m_platform != nullptr ? m_platform->audioManager() : nullptr;
    const bool available = audio != nullptr && audio->isAvailable();
    m_volume->setEnabled(available);
    if (!available) {
        m_audioLabel->setText(QStringLiteral("Audio unavailable"));
        return;
    }

    const Platform::AudioInfo info = audio->state();
    m_updating = true;
    m_volume->setValue(info.volume);
    m_updating = false;
    m_audioLabel->setText(info.muted
                              ? QStringLiteral("Muted")
                              : (info.outputDevice.isEmpty() ? QStringLiteral("Output volume")
                                                             : info.outputDevice));
}

void ControlCenter::refreshDisplay()
{
    if (m_brightness == nullptr || m_displayLabel == nullptr) {
        return;
    }
    Platform::IDisplayManager* display = m_platform != nullptr ? m_platform->displayManager() : nullptr;
    const bool available = display != nullptr && display->isAvailable();
    const bool supports = available && display->supportsBrightness();
    m_brightness->setEnabled(supports);

    if (!available) {
        m_displayLabel->setText(QStringLiteral("Display service unavailable"));
        return;
    }

    const QList<Platform::DisplayInfo> displays = display->displays();
    if (displays.isEmpty()) {
        m_displayLabel->setText(QStringLiteral("No outputs reported"));
        return;
    }

    const Platform::DisplayInfo* primary = nullptr;
    for (const Platform::DisplayInfo& info : displays) {
        if (info.primary) {
            primary = &info;
            break;
        }
    }
    if (primary == nullptr) {
        primary = &displays.first();
    }

    m_displayLabel->setText(QStringLiteral("%1 × %2")
                                .arg(primary->geometry.width())
                                .arg(primary->geometry.height()));
    if (supports) {
        int value = display->brightness(primary->id);
        if (value < 0 && m_settings != nullptr) {
            value = m_settings->displayBrightness();
        }
        if (value >= 0) {
            m_updating = true;
            m_brightness->setValue(value);
            m_updating = false;
        }
    } else {
        m_displayLabel->setText(m_displayLabel->text() + QStringLiteral(" · no brightness control"));
    }
}

void ControlCenter::refreshPower()
{
    if (m_powerLabel == nullptr) {
        return;
    }
    Platform::IPowerManager* power = m_platform != nullptr ? m_platform->powerManager() : nullptr;
    if (power == nullptr || !power->isAvailable()) {
        m_powerLabel->setText(QStringLiteral("Power information unavailable"));
        if (m_lockButton != nullptr) {
            m_lockButton->setEnabled(m_platform != nullptr && m_platform->sessionManager() != nullptr);
        }
        return;
    }

    const Platform::BatteryInfo battery = power->battery();
    if (battery.present) {
        m_powerLabel->setText(battery.charging
                                  ? QStringLiteral("Battery %1% · charging").arg(battery.percent)
                                  : QStringLiteral("Battery %1%").arg(battery.percent));
    } else {
        m_powerLabel->setText(QStringLiteral("AC power · no battery"));
    }
    const Platform::PowerCapabilities capabilities = power->capabilities();
    if (m_lockButton != nullptr) {
        m_lockButton->setEnabled(capabilities.canLock
                                 || (m_platform != nullptr && m_platform->sessionManager() != nullptr));
    }
}

void ControlCenter::refreshAppearance()
{
    if (m_appearance == nullptr || m_settings == nullptr) {
        return;
    }
    const QString mode = m_settings->appearanceMode().toLower();
    const int index = mode == QStringLiteral("light") ? 0 : (mode == QStringLiteral("dark") ? 1 : 2);
    m_updating = true;
    m_appearance->setCurrentIndex(index);
    m_updating = false;
}

void ControlCenter::prepareShow()
{
    refresh();
    if (m_refreshTimer != nullptr) {
        m_refreshTimer->start();
    }
}

void ControlCenter::prepareHide()
{
    if (m_refreshTimer != nullptr) {
        m_refreshTimer->stop();
    }
}

void ControlCenter::paintEvent(QPaintEvent* event)
{
    Graphics::GlassPanel::paintEvent(event);
}

} // namespace Aero::Shell
