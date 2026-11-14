#pragma once

#include "aero/shell/overlay.h"

class QLabel;
class QTimer;

namespace Aero::Platform {
class Platform;
}
namespace Aero::Services {
class NotificationBus;
}
namespace Aero::Settings {
class SettingsManager;
}

namespace Aero::Theme {
class AeroButton;
class AeroSlider;
class SegmentedControl;
class ToggleSwitch;
}

namespace Aero::Shell {

/// Quick system controls modelled after a desktop Control Center: connectivity,
/// output volume, display brightness, Do Not Disturb, appearance and session
/// actions.  Controls are disabled when the platform service is absent; the
/// panel never claims a change succeeded when no service handled it.
class ControlCenter : public Overlay {
    Q_OBJECT
public:
    ControlCenter(Platform::Platform* platform,
                  Settings::SettingsManager* settings,
                  Services::NotificationBus* notifications,
                  QWidget* parent = nullptr);

    void refresh();

signals:
    void settingsRequested();
    void lockRequested();
    void displaySettingsRequested();

protected:
    void prepareShow() override;
    void prepareHide() override;
    void paintEvent(QPaintEvent* event) override;

private:
    void build();
    void refreshNetwork();
    void refreshAudio();
    void refreshDisplay();
    void refreshBluetooth();
    void refreshPower();
    void refreshAppearance();
    void connectSignals();

    Platform::Platform* m_platform = nullptr;
    Settings::SettingsManager* m_settings = nullptr;
    Services::NotificationBus* m_notifications = nullptr;

    Theme::ToggleSwitch* m_wifi = nullptr;
    Theme::ToggleSwitch* m_bluetooth = nullptr;
    Theme::ToggleSwitch* m_dnd = nullptr;
    Theme::AeroSlider* m_volume = nullptr;
    Theme::AeroSlider* m_brightness = nullptr;
    Theme::SegmentedControl* m_appearance = nullptr;
    Theme::AeroButton* m_settingsButton = nullptr;
    Theme::AeroButton* m_lockButton = nullptr;
    QLabel* m_networkLabel = nullptr;
    QLabel* m_audioLabel = nullptr;
    QLabel* m_displayLabel = nullptr;
    QLabel* m_bluetoothLabel = nullptr;
    QLabel* m_powerLabel = nullptr;

    QTimer* m_refreshTimer = nullptr;
    bool m_updating = false;
};

} // namespace Aero::Shell
