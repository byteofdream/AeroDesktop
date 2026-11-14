#include "aero/apps/builtin_apps.h"

#include "aero/apps/common/app_widgets.h"
#include "aero/core/utils.h"
#include "aero/core/version.h"
#include "aero/platform/i_audio_manager.h"
#include "aero/platform/i_network_manager.h"
#include "aero/platform/i_power_manager.h"
#include "aero/platform/i_process_manager.h"
#include "aero/platform/i_session_manager.h"
#include "aero/platform/i_window_manager.h"
#include "aero/theme/controls.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/theme.h"

#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>

#include <algorithm>

namespace Aero::Applications {
namespace {

class MeterWidget : public QWidget {
public:
    explicit MeterWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(10);
        setMaximumHeight(10);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setValue(qreal value)
    {
        m_value = Core::Utils::clamp(value, 0.0, 1.0);
        update();
    }

    void setColor(const QColor& color)
    {
        m_color = color;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        const Theme::ColorTokens& colors = Theme::ThemeManager::instance().tokens().color;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QRectF area = QRectF(rect()).adjusted(0, 2, 0, -2);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Core::Utils::withAlpha(colors.textPrimary, 0.10));
        painter.drawRoundedRect(area, area.height() / 2.0, area.height() / 2.0);
        if (m_value > 0.0) {
            const QColor color = m_color.isValid() ? m_color : colors.accent;
            painter.setBrush(color);
            painter.drawRoundedRect(QRectF(area.left(), area.top(), area.width() * m_value, area.height()),
                                    area.height() / 2.0, area.height() / 2.0);
        }
    }

private:
    qreal m_value = 0.0;
    QColor m_color;
};

class CpuGraph : public QWidget {
public:
    explicit CpuGraph(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(96);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_samples.fill(0.0, 60);
    }

    void addSample(double percent)
    {
        m_samples.removeFirst();
        m_samples.append(Core::Utils::clamp(percent, 0.0, 100.0));
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        const Theme::ColorTokens& colors = Theme::ThemeManager::instance().tokens().color;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF area = QRectF(rect()).adjusted(0.5, 4.0, -0.5, -4.0);
        painter.setPen(QPen(colors.separator, 1.0));
        for (int i = 1; i < 4; ++i) {
            const qreal y = area.top() + area.height() * i / 4.0;
            painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
        }

        if (m_samples.size() < 2) {
            return;
        }

        QPainterPath line;
        QPainterPath fill;
        for (int i = 0; i < m_samples.size(); ++i) {
            const qreal x = area.left() + area.width() * i / (m_samples.size() - 1);
            const qreal y = area.bottom() - area.height() * (m_samples.at(i) / 100.0);
            if (i == 0) {
                line.moveTo(x, y);
                fill.moveTo(x, area.bottom());
                fill.lineTo(x, y);
            } else {
                line.lineTo(x, y);
                fill.lineTo(x, y);
            }
        }
        fill.lineTo(area.right(), area.bottom());
        fill.closeSubpath();

        QLinearGradient gradient(area.topLeft(), area.bottomLeft());
        gradient.setColorAt(0.0, Core::Utils::withAlpha(colors.accent, 0.34));
        gradient.setColorAt(1.0, Core::Utils::withAlpha(colors.accent, 0.015));
        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawPath(fill);

        painter.setPen(QPen(colors.accent, 2.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(line);
    }

private:
    QVector<double> m_samples;
};

class MonitorView final : public AppView {
public:
    explicit MonitorView(const AppContext& context, QWidget* parent = nullptr)
        : AppView(context, parent)
    {
        buildUi();
        m_timer = new QTimer(this);
        m_timer->setInterval(1200);
        connect(m_timer, &QTimer::timeout, this, [this]() { refresh(); });
        m_timer->start();
        connect(&Theme::ThemeManager::instance(), &Theme::ThemeManager::themeChanged,
                this, [this]() { repaint(); });
        refresh();
    }

    QString windowTitle() const override { return QStringLiteral("Monitor"); }
    QString subtitle() const override { return QStringLiteral("System activity"); }

    void activate() override { refresh(); }

private:
    void buildUi()
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(20, 16, 20, 18);
        root->setSpacing(12);
        root->addWidget(Widgets::pageHeader(QStringLiteral("System Monitor"),
                                            QStringLiteral("Live resource usage reported by the platform backend"),
                                            this));

        auto* scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto* content = new QWidget(scroll);
        auto* grid = new QGridLayout(content);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(12);

        auto* cpuCard = makeCard(QStringLiteral("Processor"), &m_cpuValue,
                                 &m_cpuDetail, nullptr, grid, 0, 0);
        m_cpuGraph = new CpuGraph(cpuCard);
        qobject_cast<QVBoxLayout*>(cpuCard->layout())->addWidget(m_cpuGraph, 1);

        auto* memoryCard = makeCard(QStringLiteral("Memory"), &m_memoryValue,
                                    &m_memoryDetail, nullptr, grid, 0, 1);
        m_memoryBar = new MeterWidget(memoryCard);
        qobject_cast<QVBoxLayout*>(memoryCard->layout())->addWidget(m_memoryBar);

        auto* uptimeCard = makeCard(QStringLiteral("Uptime"), &m_uptimeValue,
                                    &m_uptimeDetail, nullptr, grid, 0, 2);

        auto* batteryCard = makeCard(QStringLiteral("Battery"), &m_batteryValue,
                                     &m_batteryDetail, nullptr, grid, 1, 0);
        m_batteryBar = new MeterWidget(batteryCard);
        qobject_cast<QVBoxLayout*>(batteryCard->layout())->addWidget(m_batteryBar);

        auto* networkCard = makeCard(QStringLiteral("Network"), &m_networkValue,
                                     &m_networkDetail, nullptr, grid, 1, 1);

        auto* activityCard = makeCard(QStringLiteral("Activity"), &m_activityValue,
                                      &m_activityDetail, nullptr, grid, 1, 2);

        auto* systemCard = new Theme::Card(content);
        systemCard->setRadius(16);
        auto* systemLayout = new QVBoxLayout(systemCard);
        systemLayout->setContentsMargins(18, 14, 18, 16);
        systemLayout->setSpacing(6);
        auto* systemTitle = Widgets::sectionLabel(QStringLiteral("System"), systemCard);
        systemLayout->addWidget(systemTitle);
        m_systemRows = new QGridLayout;
        m_systemRows->setHorizontalSpacing(20);
        m_systemRows->setVerticalSpacing(5);
        systemLayout->addLayout(m_systemRows);

        grid->addWidget(systemCard, 2, 0, 1, 3);
        grid->setRowStretch(0, 2);
        grid->setRowStretch(1, 2);
        grid->setRowStretch(2, 0);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);
        grid->setColumnStretch(2, 1);
        scroll->setWidget(content);
        root->addWidget(scroll, 1);
    }

    Theme::Card* makeCard(const QString& title,
                          QLabel** valueLabel,
                          QLabel** detailLabel,
                          QWidget* footer,
                          QGridLayout* grid,
                          int row,
                          int column)
    {
        auto* card = new Theme::Card(grid->parentWidget());
        card->setRadius(16);
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(18, 14, 18, 16);
        layout->setSpacing(6);

        auto* titleLabel = Widgets::sectionLabel(title, card);
        layout->addWidget(titleLabel);

        auto* value = new QLabel(QStringLiteral("—"), card);
        value->setProperty("role", QStringLiteral("title"));
        value->setFont(Theme::ThemeManager::instance().font(28.0, QFont::DemiBold));
        layout->addWidget(value);
        *valueLabel = value;

        auto* detail = Widgets::mutedLabel(QStringLiteral("Unavailable"), card);
        detail->setFont(Theme::ThemeManager::instance().captionFont());
        detail->setWordWrap(true);
        layout->addWidget(detail);
        *detailLabel = detail;

        if (footer != nullptr) {
            layout->addWidget(footer);
        }
        layout->addStretch(1);
        grid->addWidget(card, row, column);
        return card;
    }

    void refresh()
    {
        Platform::IProcessManager* processes = context().processManager();
        if (processes != nullptr && processes->isAvailable()) {
            processes->refresh();

            const double cpu = processes->cpuUsagePercent();
            m_cpuValue->setText(QStringLiteral("%1%").arg(QString::number(cpu, 'f', 0)));
            const Platform::SystemInfo info = context().sessionManager() != nullptr
                ? context().sessionManager()->systemInfo()
                : Platform::SystemInfo();
            m_cpuDetail->setText(info.cpuModel.isEmpty()
                                     ? QStringLiteral("Processor load across all cores")
                                     : QStringLiteral("%1 · %2 cores").arg(info.cpuModel).arg(info.cpuCores));
            if (m_cpuGraph != nullptr) {
                m_cpuGraph->addSample(cpu);
            }

            const Platform::MemoryInfo memory = processes->memory();
            const double usedRatio = memory.totalBytes > 0
                ? static_cast<double>(memory.usedBytes) / static_cast<double>(memory.totalBytes)
                : 0.0;
            m_memoryValue->setText(Core::Utils::formatBytes(memory.usedBytes));
            m_memoryDetail->setText(QStringLiteral("of %1 · %2% in use")
                                        .arg(Core::Utils::formatBytes(memory.totalBytes))
                                        .arg(QString::number(usedRatio * 100.0, 'f', 0)));
            if (m_memoryBar != nullptr) {
                m_memoryBar->setValue(usedRatio);
            }

            const qint64 uptime = processes->uptimeSeconds();
            m_uptimeValue->setText(Core::Utils::formatDuration(uptime));
            m_uptimeDetail->setText(QStringLiteral("Booted %1")
                                        .arg(QDateTime::currentDateTime().addSecs(-uptime)
                                                 .toString(QStringLiteral("ddd, d MMM HH:mm"))));

            const int count = processes->processes().size();
            m_activityValue->setText(QString::number(count));
            m_activityDetail->setText(QStringLiteral("processes · %1 active")
                                          .arg(context().hostWindowManager() != nullptr
                                                   ? context().hostWindowManager()->windows().size()
                                                   : 0));
        } else {
            m_cpuValue->setText(QStringLiteral("N/A"));
            m_cpuDetail->setText(QStringLiteral("The platform has no process service."));
            m_memoryValue->setText(QStringLiteral("N/A"));
            m_memoryDetail->setText(QStringLiteral("Memory information requires the process service."));
            m_uptimeValue->setText(QStringLiteral("N/A"));
            m_uptimeDetail->setText(QStringLiteral("Uptime is not exposed."));
            m_activityValue->setText(QStringLiteral("—"));
            m_activityDetail->setText(QStringLiteral("Process list unavailable."));
        }

        Platform::IPowerManager* power = context().powerManager();
        if (power != nullptr && power->isAvailable()) {
            const Platform::BatteryInfo battery = power->battery();
            if (battery.present) {
                m_batteryValue->setText(QStringLiteral("%1%").arg(battery.percent));
                m_batteryDetail->setText(battery.charging ? QStringLiteral("Charging%1")
                                                                .arg(battery.secondsRemaining > 0
                                                                         ? QStringLiteral(" · %1 until full")
                                                                               .arg(Core::Utils::formatDuration(battery.secondsRemaining))
                                                                         : QString())
                                                          : (battery.fullyCharged
                                                                 ? QStringLiteral("Fully charged")
                                                                 : QStringLiteral("%1 remaining")
                                                                       .arg(Core::Utils::formatDuration(battery.secondsRemaining))));
                if (m_batteryBar != nullptr) {
                    m_batteryBar->setValue(battery.percent / 100.0);
                    m_batteryBar->setColor(battery.percent <= 20 && !battery.charging
                                               ? Theme::ThemeManager::instance().tokens().color.danger
                                               : Theme::ThemeManager::instance().tokens().color.success);
                }
            } else {
                m_batteryValue->setText(QStringLiteral("AC"));
                m_batteryDetail->setText(QStringLiteral("No battery detected."));
                if (m_batteryBar != nullptr) {
                    m_batteryBar->setValue(1.0);
                }
            }
        } else {
            m_batteryValue->setText(QStringLiteral("N/A"));
            m_batteryDetail->setText(QStringLiteral("Power information is unavailable."));
        }

        Platform::INetworkManager* network = context().networkManager();
        if (network != nullptr && network->isAvailable()) {
            const Platform::NetworkInfo info = network->network();
            if (info.connected) {
                m_networkValue->setText(info.wifi ? QStringLiteral("Wi-Fi") : QStringLiteral("Wired"));
                m_networkDetail->setText(QStringLiteral("%1 · %2% signal")
                                             .arg(info.ssid.isEmpty() ? info.interfaceName : info.ssid)
                                             .arg(info.signalStrength));
            } else {
                m_networkValue->setText(QStringLiteral("Offline"));
                m_networkDetail->setText(info.wifiEnabled
                                             ? QStringLiteral("Wi-Fi is enabled but not connected.")
                                             : QStringLiteral("No active network connection."));
            }
        } else {
            m_networkValue->setText(QStringLiteral("N/A"));
            m_networkDetail->setText(QStringLiteral("Network information is unavailable."));
        }

        refreshSystemRows();
    }

    void refreshSystemRows()
    {
        while (QLayoutItem* item = m_systemRows->takeAt(0)) {
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
        info.desktopVersion = QStringLiteral("Aero Desktop %1").arg(Core::version());
        if (context().platform != nullptr) {
            info.platformBackend = context().platform->backendDisplayName();
        }

        const QList<QPair<QString, QString>> rows = {
            {QStringLiteral("Operating system"), info.osName.isEmpty() ? QStringLiteral("Unknown") : info.osName},
            {QStringLiteral("Version"), info.osVersion.isEmpty() ? QStringLiteral("Unknown") : info.osVersion},
            {QStringLiteral("Kernel"), info.kernelVersion.isEmpty() ? QStringLiteral("Unknown") : info.kernelVersion},
            {QStringLiteral("Architecture"), info.architecture.isEmpty() ? QStringLiteral("Unknown") : info.architecture},
            {QStringLiteral("Host name"), info.hostName.isEmpty() ? QStringLiteral("Unknown") : info.hostName},
            {QStringLiteral("Processor"), info.cpuModel.isEmpty() ? QStringLiteral("Unknown") : info.cpuModel},
            {QStringLiteral("Graphics"), info.gpuModel.isEmpty() ? QStringLiteral("Unknown") : info.gpuModel},
            {QStringLiteral("Platform backend"), info.platformBackend.isEmpty() ? QStringLiteral("Unknown") : info.platformBackend},
            {QStringLiteral("Aero version"), info.desktopVersion},
        };

        int row = 0;
        for (const auto& pair : rows) {
            auto* name = Widgets::mutedLabel(pair.first, m_systemRows->parentWidget());
            name->setFont(Theme::ThemeManager::instance().captionFont());
            auto* value = new QLabel(pair.second, m_systemRows->parentWidget());
            value->setProperty("role", QStringLiteral("value"));
            value->setFont(Theme::ThemeManager::instance().captionFont(QFont::Medium));
            value->setTextInteractionFlags(Qt::TextSelectableByMouse);
            m_systemRows->addWidget(name, row, 0);
            m_systemRows->addWidget(value, row, 1);
            ++row;
        }
        m_systemRows->setColumnStretch(1, 1);
    }

    QTimer* m_timer = nullptr;
    QLabel* m_cpuValue = nullptr;
    QLabel* m_cpuDetail = nullptr;
    QLabel* m_memoryValue = nullptr;
    QLabel* m_memoryDetail = nullptr;
    QLabel* m_uptimeValue = nullptr;
    QLabel* m_uptimeDetail = nullptr;
    QLabel* m_batteryValue = nullptr;
    QLabel* m_batteryDetail = nullptr;
    QLabel* m_networkValue = nullptr;
    QLabel* m_networkDetail = nullptr;
    QLabel* m_activityValue = nullptr;
    QLabel* m_activityDetail = nullptr;
    MeterWidget* m_memoryBar = nullptr;
    MeterWidget* m_batteryBar = nullptr;
    CpuGraph* m_cpuGraph = nullptr;
    QGridLayout* m_systemRows = nullptr;
};

} // namespace

AppView* createMonitorApp(const AppContext& context, QWidget* parent)
{
    return new MonitorView(context, parent);
}

} // namespace Aero::Applications
