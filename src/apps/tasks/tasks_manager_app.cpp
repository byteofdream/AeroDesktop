#include "aero/apps/builtin_apps.h"

#include "aero/apps/common/app_widgets.h"
#include "aero/core/utils.h"
#include "aero/platform/i_process_manager.h"
#include "aero/theme/controls.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/theme.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace Aero::Applications {
namespace {

class TaskManagerView final : public AppView {
public:
    explicit TaskManagerView(const AppContext& context, QWidget* parent = nullptr)
        : AppView(context, parent)
    {
        buildUi();
        m_timer = new QTimer(this);
        m_timer->setInterval(2500);
        connect(m_timer, &QTimer::timeout, this, [this]() { requestRefresh(); });
        m_timer->start();
        requestRefresh();

        Platform::IProcessManager* manager = context.processManager();
        if (manager != nullptr) {
            connect(manager, &Platform::IProcessManager::processesUpdated, this, [this]() { updateTable(); });
            connect(manager, &Platform::IProcessManager::memoryUpdated, this, [this]() { updateSummary(); });
            connect(manager, &Platform::IProcessManager::cpuUsageUpdated, this, [this](double) { updateSummary(); });
        }
    }

    QString windowTitle() const override { return QStringLiteral("Task Manager"); }
    QString subtitle() const override { return QStringLiteral("Processes and resource use"); }

    void activate() override { requestRefresh(); }

private:
    void buildUi()
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(18, 14, 18, 14);
        root->setSpacing(10);

        auto* headerRow = new QHBoxLayout;
        headerRow->setContentsMargins(0, 0, 0, 0);
        headerRow->addWidget(Widgets::pageHeader(QStringLiteral("Task Manager"),
                                                 QStringLiteral("Inspect, filter and end processes"),
                                                 this), 1);
        auto* refreshButton = new Theme::IconButton(QStringLiteral("refresh"), this);
        refreshButton->setToolTip(QStringLiteral("Refresh Now"));
        connect(refreshButton, &QAbstractButton::clicked, this, [this]() { requestRefresh(); });
        headerRow->addWidget(refreshButton, 0, Qt::AlignTop);
        root->addLayout(headerRow);

        auto* summary = new QWidget(this);
        auto* summaryLayout = new QHBoxLayout(summary);
        summaryLayout->setContentsMargins(0, 0, 0, 0);
        summaryLayout->setSpacing(10);
        summaryLayout->addWidget(summaryCard(QStringLiteral("Processor"), &m_cpuValue, summary), 1);
        summaryLayout->addWidget(summaryCard(QStringLiteral("Memory"), &m_memoryValue, summary), 1);
        summaryLayout->addWidget(summaryCard(QStringLiteral("Processes"), &m_countValue, summary), 1);
        root->addWidget(summary);

        auto* toolbar = new QHBoxLayout;
        toolbar->setContentsMargins(0, 0, 0, 0);
        toolbar->setSpacing(8);
        m_search = Widgets::searchField(QStringLiteral("Filter processes"), this);
        m_search->setMaximumWidth(320);
        toolbar->addWidget(m_search);
        toolbar->addStretch(1);
        m_endButton = new Theme::AeroButton(QStringLiteral("End Process"), QStringLiteral("close"), this);
        m_endButton->setVariant(Theme::AeroButton::Variant::Danger);
        m_endButton->setEnabled(false);
        toolbar->addWidget(m_endButton);
        root->addLayout(toolbar);

        m_table = new QTableWidget(this);
        m_table->setColumnCount(6);
        m_table->setHorizontalHeaderLabels({QStringLiteral("Process"), QStringLiteral("PID"),
                                            QStringLiteral("User"), QStringLiteral("CPU"),
                                            QStringLiteral("Memory"), QStringLiteral("State")});
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setShowGrid(false);
        m_table->setAlternatingRowColors(false);
        m_table->setWordWrap(false);
        m_table->verticalHeader()->setVisible(false);
        m_table->verticalHeader()->setDefaultSectionSize(28);
        m_table->horizontalHeader()->setStretchLastSection(false);
        m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
        m_table->setSortingEnabled(true);
        m_table->setContextMenuPolicy(Qt::CustomContextMenu);
        root->addWidget(m_table, 1);

        auto* details = new Theme::Card(this);
        details->setRadius(12);
        auto* detailsLayout = new QVBoxLayout(details);
        detailsLayout->setContentsMargins(14, 10, 14, 10);
        detailsLayout->setSpacing(3);
        m_detailsTitle = Widgets::mutedLabel(QStringLiteral("Select a process to inspect it."), details);
        m_detailsTitle->setFont(Theme::ThemeManager::instance().captionFont(QFont::DemiBold));
        detailsLayout->addWidget(m_detailsTitle);
        m_detailsBody = Widgets::mutedLabel(QString(), details);
        m_detailsBody->setFont(Theme::ThemeManager::instance().monospaceFont(11.0));
        m_detailsBody->setWordWrap(true);
        detailsLayout->addWidget(m_detailsBody);
        root->addWidget(details);

        connect(m_search, &QLineEdit::textChanged, this, [this]() { applyFilter(); });
        connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() { updateDetails(); });
        connect(m_table, &QTableWidget::customContextMenuRequested, this,
                [this](const QPoint& position) { showContextMenu(position); });
        connect(m_table, &QTableWidget::doubleClicked, this, [this](const QModelIndex&) { endSelected(); });
        connect(m_endButton, &QAbstractButton::clicked, this, [this]() { endSelected(); });

        Platform::IProcessManager* manager = context().processManager();
        if (manager == nullptr || !manager->isAvailable()) {
            auto* unavailable = Widgets::emptyState(
                QStringLiteral("activity"),
                QStringLiteral("Process service unavailable"),
                QStringLiteral("This platform backend does not expose a process table. Aero does not invent one."),
                this);
            root->addWidget(unavailable, 1);
            m_table->setVisible(false);
            m_search->setEnabled(false);
            m_endButton->setVisible(false);
        }
    }

    QWidget* summaryCard(const QString& title, QLabel** value, QWidget* parent)
    {
        auto* card = new Theme::Card(parent);
        card->setRadius(12);
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(14, 10, 14, 10);
        layout->setSpacing(2);
        auto* name = Widgets::mutedLabel(title, card);
        name->setFont(Theme::ThemeManager::instance().captionFont());
        layout->addWidget(name);
        auto* number = new QLabel(QStringLiteral("—"), card);
        number->setProperty("role", QStringLiteral("title"));
        number->setFont(Theme::ThemeManager::instance().font(21.0, QFont::DemiBold));
        layout->addWidget(number);
        *value = number;
        return card;
    }

    void requestRefresh()
    {
        Platform::IProcessManager* manager = context().processManager();
        if (manager == nullptr || !manager->isAvailable()) {
            return;
        }
        manager->refresh();
    }

    void updateSummary()
    {
        Platform::IProcessManager* manager = context().processManager();
        if (manager == nullptr || !manager->isAvailable()) {
            return;
        }
        m_cpuValue->setText(QStringLiteral("%1%").arg(QString::number(manager->cpuUsagePercent(), 'f', 0)));
        const Platform::MemoryInfo memory = manager->memory();
        m_memoryValue->setText(QStringLiteral("%1 / %2")
                                   .arg(Core::Utils::formatBytes(memory.usedBytes),
                                        Core::Utils::formatBytes(memory.totalBytes)));
        m_countValue->setText(QString::number(manager->processes().size()));
    }

    void updateTable()
    {
        Platform::IProcessManager* manager = context().processManager();
        if (manager == nullptr || !manager->isAvailable()) {
            return;
        }

        const int selectedPid = selectedProcessId();
        const QList<Platform::ProcessInfo> processes = manager->processes();

        m_table->setSortingEnabled(false);
        m_table->setRowCount(processes.size());

        const Theme::ColorTokens& colors = Theme::ThemeManager::instance().tokens().color;
        int row = 0;
        for (const Platform::ProcessInfo& process : processes) {
            auto* name = new QTableWidgetItem(process.name.isEmpty()
                                                  ? QStringLiteral("(unnamed)")
                                                  : process.name);
            name->setData(Qt::UserRole, process.pid);
            name->setToolTip(process.commandLine);
            auto* pid = new QTableWidgetItem;
            pid->setData(Qt::DisplayRole, process.pid);
            auto* user = new QTableWidgetItem(process.user);
            auto* cpu = new QTableWidgetItem;
            cpu->setData(Qt::DisplayRole, process.cpuPercent);
            cpu->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            auto* memory = new QTableWidgetItem(Core::Utils::formatBytes(static_cast<quint64>(qMax<qint64>(0, process.residentBytes))));
            memory->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            auto* state = new QTableWidgetItem(process.state.isEmpty() ? QStringLiteral("running") : process.state);
            if (process.cpuPercent > 40.0) {
                cpu->setForeground(colors.danger);
            }

            m_table->setItem(row, 0, name);
            m_table->setItem(row, 1, pid);
            m_table->setItem(row, 2, user);
            m_table->setItem(row, 3, cpu);
            m_table->setItem(row, 4, memory);
            m_table->setItem(row, 5, state);
            ++row;
        }

        m_table->setSortingEnabled(true);
        m_table->sortItems(3, Qt::DescendingOrder);

        if (selectedPid > 0) {
            for (int rowIndex = 0; rowIndex < m_table->rowCount(); ++rowIndex) {
                QTableWidgetItem* item = m_table->item(rowIndex, 0);
                if (item != nullptr && item->data(Qt::UserRole).toInt() == selectedPid) {
                    m_table->selectRow(rowIndex);
                    break;
                }
            }
        }

        applyFilter();
        updateSummary();
        updateDetails();
    }

    void applyFilter()
    {
        const QString needle = m_search->text().trimmed();
        for (int row = 0; row < m_table->rowCount(); ++row) {
            bool match = needle.isEmpty();
            if (!match) {
                for (int column = 0; column < m_table->columnCount(); ++column) {
                    QTableWidgetItem* item = m_table->item(row, column);
                    if (item != nullptr && item->text().contains(needle, Qt::CaseInsensitive)) {
                        match = true;
                        break;
                    }
                }
            }
            m_table->setRowHidden(row, !match);
        }
    }

    int selectedProcessId() const
    {
        const int row = m_table->currentRow();
        if (row < 0) {
            return -1;
        }
        QTableWidgetItem* item = m_table->item(row, 0);
        return item != nullptr ? item->data(Qt::UserRole).toInt() : -1;
    }

    Platform::ProcessInfo selectedProcess() const
    {
        const int pid = selectedProcessId();
        Platform::IProcessManager* manager = context().processManager();
        if (pid <= 0 || manager == nullptr) {
            return {};
        }
        for (const Platform::ProcessInfo& process : manager->processes()) {
            if (process.pid == pid) {
                return process;
            }
        }
        return {};
    }

    void updateDetails()
    {
        const Platform::ProcessInfo process = selectedProcess();
        const bool hasSelection = process.pid > 0;
        m_endButton->setEnabled(hasSelection);
        if (!hasSelection) {
            m_detailsTitle->setText(QStringLiteral("Select a process to inspect it."));
            m_detailsBody->setText(QString());
            return;
        }
        m_detailsTitle->setText(QStringLiteral("%1 · PID %2 · %3")
                                    .arg(process.name.isEmpty() ? QStringLiteral("(unnamed)") : process.name)
                                    .arg(process.pid)
                                    .arg(process.user));
        m_detailsBody->setText(QStringLiteral("%1\nResident %2 · Virtual %3 · Started %4")
                                   .arg(process.commandLine.isEmpty() ? QStringLiteral("(no command line)") : process.commandLine,
                                        Core::Utils::formatBytes(static_cast<quint64>(qMax<qint64>(0, process.residentBytes))),
                                        Core::Utils::formatBytes(static_cast<quint64>(qMax<qint64>(0, process.virtualBytes))),
                                        process.startedAtMs > 0
                                            ? QDateTime::fromMSecsSinceEpoch(process.startedAtMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                                            : QStringLiteral("unknown")));
    }

    void endSelected()
    {
        const Platform::ProcessInfo process = selectedProcess();
        if (process.pid <= 0) {
            return;
        }
        if (process.pid == 1 || process.pid == QCoreApplication::applicationPid()) {
            QMessageBox::warning(this, QStringLiteral("Process Protected"),
                                 QStringLiteral("Aero will not terminate this critical process."));
            return;
        }
        const auto answer = QMessageBox::question(
            this,
            QStringLiteral("End Process"),
            QStringLiteral("End “%1” (PID %2)? Unsaved work in that process will be lost.")
                .arg(process.name.isEmpty() ? QStringLiteral("process") : process.name)
                .arg(process.pid),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (answer != QMessageBox::Yes) {
            return;
        }

        Platform::IProcessManager* manager = context().processManager();
        if (manager == nullptr) {
            return;
        }
        if (!manager->killProcess(process.pid, false)) {
            const auto force = QMessageBox::question(
                this,
                QStringLiteral("Process Did Not Exit"),
                QStringLiteral("The process ignored the termination request. Force kill it?"),
                QMessageBox::Yes | QMessageBox::Cancel,
                QMessageBox::Cancel);
            if (force == QMessageBox::Yes) {
                manager->killProcess(process.pid, true);
            }
        }
        QTimer::singleShot(400, this, [this]() { requestRefresh(); });
    }

    void showContextMenu(const QPoint& position)
    {
        const QModelIndex index = m_table->indexAt(position);
        if (!index.isValid()) {
            return;
        }
        m_table->selectRow(index.row());
        const Platform::ProcessInfo process = selectedProcess();
        if (process.pid <= 0) {
            return;
        }

        QMenu menu(this);
        QAction* end = menu.addAction(QStringLiteral("End Process"));
        QAction* force = menu.addAction(QStringLiteral("Force Kill"));
        menu.addSeparator();
        QAction* copyPid = menu.addAction(QStringLiteral("Copy PID"));
        QAction* copyCommand = menu.addAction(QStringLiteral("Copy Command Line"));
        QAction* chosen = menu.exec(m_table->viewport()->mapToGlobal(position));
        if (chosen == nullptr) {
            return;
        }
        if (chosen == end) {
            endSelected();
        } else if (chosen == force) {
            const auto answer = QMessageBox::question(this, QStringLiteral("Force Kill"),
                                                       QStringLiteral("Force kill PID %1?").arg(process.pid),
                                                       QMessageBox::Yes | QMessageBox::Cancel,
                                                       QMessageBox::Cancel);
            if (answer == QMessageBox::Yes && context().processManager() != nullptr) {
                context().processManager()->killProcess(process.pid, true);
                QTimer::singleShot(400, this, [this]() { requestRefresh(); });
            }
        } else if (chosen == copyPid) {
            QApplication::clipboard()->setText(QString::number(process.pid));
        } else if (chosen == copyCommand) {
            QApplication::clipboard()->setText(process.commandLine);
        }
    }

    QTableWidget* m_table = nullptr;
    QLineEdit* m_search = nullptr;
    QLabel* m_cpuValue = nullptr;
    QLabel* m_memoryValue = nullptr;
    QLabel* m_countValue = nullptr;
    QLabel* m_detailsTitle = nullptr;
    QLabel* m_detailsBody = nullptr;
    Theme::AeroButton* m_endButton = nullptr;
    QTimer* m_timer = nullptr;
};

} // namespace

AppView* createTaskManagerApp(const AppContext& context, QWidget* parent)
{
    return new TaskManagerView(context, parent);
}

} // namespace Aero::Applications
