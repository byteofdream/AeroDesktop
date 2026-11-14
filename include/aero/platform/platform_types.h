#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QRect>
#include <QVariantMap>

namespace Aero::Platform {

enum class Backend {
    LinuxDev,
    DreamyOS,
};

enum class Capability {
    WindowManagement,
    DisplayManagement,
    InputManagement,
    Filesystem,
    ProcessManagement,
    NetworkManagement,
    BluetoothManagement,
    AudioManagement,
    PowerManagement,
    Notifications,
    UserSessions,
    SystemSettings,
};

QString capabilityName(Capability capability);
QString backendName(Backend backend);

struct PlatformError {
    int code = 0;
    QString message;

    bool failed() const { return code != 0; }
    explicit operator bool() const { return failed(); }
};

struct BatteryInfo {
    bool present = false;
    int percent = -1;
    bool charging = false;
    bool fullyCharged = false;
    int secondsRemaining = -1;
    QString status;
};

struct NetworkInfo {
    bool available = false;
    bool wifiEnabled = false;
    bool connected = false;
    bool wifi = false;
    QString interfaceName;
    QString ssid;
    QString ipAddress;
    int signalStrength = 0;
};

struct WifiNetwork {
    QString ssid;
    int strength = 0;
    bool secured = false;
    bool connected = false;
};

struct BluetoothDevice {
    QString address;
    QString name;
    bool connected = false;
    bool paired = false;
    int batteryPercent = -1;
};

struct BluetoothInfo {
    bool available = false;
    bool enabled = false;
    QList<BluetoothDevice> devices;
};

struct AudioInfo {
    int volume = 0;
    bool muted = false;
    QString outputDevice;
    QString inputDevice;
};

struct DisplayInfo {
    QString id;
    QString name;
    QRect geometry;
    qreal devicePixelRatio = 1.0;
    qreal refreshRate = 0.0;
    bool primary = false;
    bool builtIn = false;
    int brightness = -1;
};

struct MemoryInfo {
    quint64 totalBytes = 0;
    quint64 availableBytes = 0;
    quint64 usedBytes = 0;
    quint64 cachedBytes = 0;
    quint64 swapTotalBytes = 0;
    quint64 swapUsedBytes = 0;
};

struct ProcessInfo {
    int pid = 0;
    int parentPid = 0;
    QString name;
    QString user;
    QString commandLine;
    QString state;
    qint64 residentBytes = 0;
    qint64 virtualBytes = 0;
    double cpuPercent = 0.0;
    double memoryPercent = 0.0;
    qint64 startedAtMs = 0;
};

struct AudioDeviceInfo {
    QString id;
    QString name;
    bool isInput = false;
    bool isDefault = false;
};

struct UserInfo {
    quint32 uid = 0;
    QString userName;
    QString realName;
    QString homePath;
    QString shell;
    QString iconPath;
};

struct SessionInfo {
    QString sessionType;
    QString desktopName;
    QString hostName;
    QString seat;
    bool locked = false;
};

struct SystemInfo {
    QString osName;
    QString osVersion;
    QString kernelVersion;
    QString architecture;
    QString hostName;
    QString cpuModel;
    int cpuCores = 0;
    quint64 totalMemoryBytes = 0;
    QString gpuModel;
    QString desktopVersion;
    QString platformBackend;
    qint64 uptimeSeconds = 0;
};

struct WindowInfo {
    quintptr id = 0;
    QString title;
    QString applicationId;
    QRect geometry;
    bool active = false;
    bool minimized = false;
    bool maximized = false;
    int workspace = 0;
};

struct NotificationRequest {
    QString applicationName = QStringLiteral("Aero Desktop");
    QString applicationIcon;
    QString summary;
    QString body;
    int expireTimeoutMs = 5000;
    QStringList actions;
    QVariantMap hints;
};

struct PowerCapabilities {
    bool canSuspend = false;
    bool canReboot = false;
    bool canPowerOff = false;
    bool canLock = false;
    bool canHibernate = false;
};

} // namespace Aero::Platform

Q_DECLARE_METATYPE(Aero::Platform::BatteryInfo)
Q_DECLARE_METATYPE(Aero::Platform::NetworkInfo)
Q_DECLARE_METATYPE(Aero::Platform::AudioInfo)
Q_DECLARE_METATYPE(Aero::Platform::DisplayInfo)
Q_DECLARE_METATYPE(Aero::Platform::ProcessInfo)
Q_DECLARE_METATYPE(Aero::Platform::BluetoothInfo)
Q_DECLARE_METATYPE(Aero::Platform::SystemInfo)
