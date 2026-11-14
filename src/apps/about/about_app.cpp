#include "aero/apps/builtin_apps.h"

#include "aero/apps/common/app_widgets.h"
#include "aero/core/utils.h"
#include "aero/core/version.h"
#include "aero/platform/i_session_manager.h"
#include "aero/platform/platform_service.h"
#include "aero/theme/controls.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/theme.h"

#include <QApplication>
#include <QClipboard>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace Aero::Applications {
namespace {

class AboutView final : public AppView {
public:
    explicit AboutView(const AppContext& context, QWidget* parent = nullptr)
        : AppView(context, parent)
    {
        buildUi();
        refresh();
    }

    QString windowTitle() const override { return QStringLiteral("About Aero"); }
    QString subtitle() const override { return QStringLiteral("Version %1").arg(Core::version()); }
    void activate() override { refresh(); }

private:
    void buildUi()
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        auto* scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto* content = new QWidget(scroll);
        auto* layout = new QVBoxLayout(content);
        layout->setContentsMargins(30, 24, 30, 28);
        layout->setSpacing(18);

        auto* hero = new QVBoxLayout;
        hero->setSpacing(6);
        hero->setAlignment(Qt::AlignHCenter);
        auto* icon = new QLabel(content);
        icon->setPixmap(Theme::IconFactory::appIcon(QString::fromLatin1(AppIds::About), 96).pixmap(96, 96));
        icon->setAlignment(Qt::AlignCenter);
        hero->addWidget(icon, 0, Qt::AlignHCenter);

        auto* name = new QLabel(QStringLiteral("Aero Desktop"), content);
        name->setProperty("role", QStringLiteral("title"));
        name->setFont(Theme::ThemeManager::instance().font(30.0, QFont::DemiBold));
        name->setAlignment(Qt::AlignCenter);
        hero->addWidget(name);

        auto* version = new QLabel(QStringLiteral("Version %1 · “%2”")
                                       .arg(Core::version(), QString::fromLatin1(Core::CodeName)),
                                   content);
        version->setProperty("role", QStringLiteral("muted"));
        version->setFont(Theme::ThemeManager::instance().captionFont(QFont::Medium));
        version->setAlignment(Qt::AlignCenter);
        hero->addWidget(version);

        auto* description = Widgets::mutedLabel(
            QStringLiteral("A liquid-glass desktop environment built with C++20 and Qt 6 Widgets. "
                           "The shell, window manager and applications render every surface with QPainter. "
                           "Aero runs on the platform abstraction so the same shell can target DreamyOS without "
                           "pretending that Linux services exist."),
            content);
        description->setAlignment(Qt::AlignCenter);
        description->setMaximumWidth(500);
        hero->addWidget(description, 0, Qt::AlignHCenter);
        layout->addLayout(hero);

        auto* actions = new QHBoxLayout;
        actions->setSpacing(8);
        actions->setAlignment(Qt::AlignHCenter);
        auto* copyButton = new Theme::AeroButton(QStringLiteral("Copy Diagnostics"),
                                                 QStringLiteral("copy"), content);
        connect(copyButton, &QAbstractButton::clicked, this, [this]() { copyDiagnostics(); });
        actions->addWidget(copyButton);
        auto* settingsButton = new Theme::AeroButton(QStringLiteral("Open Settings"),
                                                      QStringLiteral("gear"), content);
        connect(settingsButton, &QAbstractButton::clicked, this, [this]() {
            context().launch(QString::fromLatin1(AppIds::Settings));
        });
        actions->addWidget(settingsButton);
        auto* monitorButton = new Theme::AeroButton(QStringLiteral("System Monitor"),
                                                     QStringLiteral("monitor"), content);
        connect(monitorButton, &QAbstractButton::clicked, this, [this]() {
            context().launch(QString::fromLatin1(AppIds::Monitor));
        });
        actions->addWidget(monitorButton);
        layout->addLayout(actions);

        auto* systemCard = new Theme::Card(content);
        systemCard->setRadius(16);
        auto* systemLayout = new QVBoxLayout(systemCard);
        systemLayout->setContentsMargins(20, 16, 20, 18);
        systemLayout->setSpacing(8);
        systemLayout->addWidget(Widgets::sectionLabel(QStringLiteral("System"), systemCard));
        m_systemGrid = new QGridLayout;
        m_systemGrid->setHorizontalSpacing(24);
        m_systemGrid->setVerticalSpacing(7);
        systemLayout->addLayout(m_systemGrid);
        layout->addWidget(systemCard);

        auto* capabilityCard = new Theme::Card(content);
        capabilityCard->setRadius(16);
        auto* capabilityLayout = new QVBoxLayout(capabilityCard);
        capabilityLayout->setContentsMargins(20, 16, 20, 18);
        capabilityLayout->setSpacing(8);
        capabilityLayout->addWidget(Widgets::sectionLabel(QStringLiteral("Platform capabilities"), capabilityCard));
        m_capabilityGrid = new QGridLayout;
        m_capabilityGrid->setHorizontalSpacing(18);
        m_capabilityGrid->setVerticalSpacing(6);
        capabilityLayout->addLayout(m_capabilityGrid);
        auto* note = Widgets::mutedLabel(
            QStringLiteral("Unavailable services are reported honestly. Aero hides or disables controls instead of "
                           "simulating a feature the backend cannot provide."),
            capabilityCard);
        note->setFont(Theme::ThemeManager::instance().font(11.5));
        capabilityLayout->addWidget(note);
        layout->addWidget(capabilityCard);

        auto* credits = Widgets::mutedLabel(
            QStringLiteral("Aero Desktop is an independent project. Built with Qt 6 (Core, Gui, Widgets, Network, "
                           "DBus, Concurrent). Every icon is drawn with QPainter; no icon theme, QML or web view is required."),
            content);
        credits->setAlignment(Qt::AlignCenter);
        credits->setFont(Theme::ThemeManager::instance().captionFont());
        layout->addWidget(credits);
        layout->addStretch(1);

        scroll->setWidget(content);
        root->addWidget(scroll, 1);
    }

    void refresh()
    {
        while (QLayoutItem* item = m_systemGrid->takeAt(0)) {
            if (item->widget() != nullptr) {
                item->widget()->deleteLater();
            }
            delete item;
        }
        while (QLayoutItem* item = m_capabilityGrid->takeAt(0)) {
            if (item->widget() != nullptr) {
                item->widget()->deleteLater();
            }
            if (item->layout() != nullptr) {
                delete item->layout();
            }
            delete item;
        }

        Platform::SystemInfo info;
        if (context().sessionManager() != nullptr) {
            info = context().sessionManager()->systemInfo();
        }
        if (context().platform != nullptr) {
            info.platformBackend = context().platform->backendDisplayName();
        }

        const QList<QPair<QString, QString>> rows = {
            {QStringLiteral("Operating system"), info.osName.isEmpty() ? QStringLiteral("Unknown") : info.osName},
            {QStringLiteral("OS version"), info.osVersion.isEmpty() ? QStringLiteral("Unknown") : info.osVersion},
            {QStringLiteral("Kernel"), info.kernelVersion.isEmpty() ? QStringLiteral("Unknown") : info.kernelVersion},
            {QStringLiteral("Architecture"), info.architecture.isEmpty() ? QStringLiteral("Unknown") : info.architecture},
            {QStringLiteral("Host name"), info.hostName.isEmpty() ? QStringLiteral("Unknown") : info.hostName},
            {QStringLiteral("Processor"), info.cpuModel.isEmpty() ? QStringLiteral("Unknown") : info.cpuModel},
            {QStringLiteral("CPU cores"), info.cpuCores > 0 ? QString::number(info.cpuCores) : QStringLiteral("Unknown")},
            {QStringLiteral("Memory"), info.totalMemoryBytes > 0 ? Core::Utils::formatBytes(info.totalMemoryBytes)
                                                                 : QStringLiteral("Unknown")},
            {QStringLiteral("Graphics"), info.gpuModel.isEmpty() ? QStringLiteral("Unknown") : info.gpuModel},
            {QStringLiteral("Uptime"), info.uptimeSeconds > 0 ? Core::Utils::formatDuration(info.uptimeSeconds)
                                                              : QStringLiteral("Unknown")},
            {QStringLiteral("Platform backend"), info.platformBackend.isEmpty() ? QStringLiteral("None") : info.platformBackend},
        };

        int row = 0;
        for (const auto& pair : rows) {
            auto* name = Widgets::mutedLabel(pair.first, m_systemGrid->parentWidget());
            name->setFont(Theme::ThemeManager::instance().captionFont());
            auto* value = new QLabel(pair.second, m_systemGrid->parentWidget());
            value->setProperty("role", QStringLiteral("value"));
            value->setFont(Theme::ThemeManager::instance().captionFont(QFont::Medium));
            value->setTextInteractionFlags(Qt::TextSelectableByMouse);
            m_systemGrid->addWidget(name, row, 0);
            m_systemGrid->addWidget(value, row, 1);
            ++row;
        }
        m_systemGrid->setColumnStretch(1, 1);

        if (context().platform == nullptr) {
            m_capabilityGrid->addWidget(Widgets::mutedLabel(QStringLiteral("No platform backend is attached."),
                                                            m_capabilityGrid->parentWidget()),
                                        0, 0, 1, 2);
            return;
        }

        const QList<Platform::PlatformService*> services = context().platform->services();
        int rowIndex = 0;
        for (Platform::PlatformService* service : services) {
            if (service == nullptr) {
                continue;
            }
            const QString name = Platform::capabilityName(service->capability());
            auto* label = new QLabel(name, m_capabilityGrid->parentWidget());
            label->setProperty("role", QStringLiteral("subtitle"));
            label->setFont(Theme::ThemeManager::instance().bodyFont(QFont::Medium));
            m_capabilityGrid->addWidget(label, rowIndex, 0);

            auto* pill = new Theme::StatusPill(service->isAvailable() ? QStringLiteral("Available")
                                                                      : QStringLiteral("Unavailable"),
                                               m_capabilityGrid->parentWidget());
            pill->setTone(service->isAvailable()
                              ? Theme::ThemeManager::instance().tokens().color.success
                              : Theme::ThemeManager::instance().tokens().color.textMuted);
            pill->setFilled(service->isAvailable());
            m_capabilityGrid->addWidget(pill, rowIndex, 1, Qt::AlignRight);
            ++rowIndex;
        }
        m_capabilityGrid->setColumnStretch(0, 1);
    }

    void copyDiagnostics()
    {
        QStringList lines;
        lines << QStringLiteral("Aero Desktop %1 (%2)").arg(Core::version(), QString::fromLatin1(Core::CodeName));
        lines << QStringLiteral("Backend: %1").arg(context().platform != nullptr
                                                       ? context().platform->backendDisplayName()
                                                       : QStringLiteral("none"));
        if (context().sessionManager() != nullptr) {
            const Platform::SystemInfo info = context().sessionManager()->systemInfo();
            lines << QStringLiteral("OS: %1 %2").arg(info.osName, info.osVersion);
            lines << QStringLiteral("Kernel: %1").arg(info.kernelVersion);
            lines << QStringLiteral("Architecture: %1").arg(info.architecture);
            lines << QStringLiteral("CPU: %1 (%2 cores)").arg(info.cpuModel).arg(info.cpuCores);
            lines << QStringLiteral("Memory: %1").arg(Core::Utils::formatBytes(info.totalMemoryBytes));
        }
        if (context().platform != nullptr) {
            for (Platform::PlatformService* service : context().platform->services()) {
                if (service != nullptr) {
                    lines << QStringLiteral("%1: %2")
                                 .arg(Platform::capabilityName(service->capability()),
                                      service->isAvailable() ? QStringLiteral("available")
                                                             : QStringLiteral("unavailable"));
                }
            }
        }
        QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
        emit statusMessage(QStringLiteral("Diagnostics copied to the clipboard."));
    }

    QGridLayout* m_systemGrid = nullptr;
    QGridLayout* m_capabilityGrid = nullptr;
};

} // namespace

AppView* createAboutApp(const AppContext& context, QWidget* parent)
{
    return new AboutView(context, parent);
}

} // namespace Aero::Applications
