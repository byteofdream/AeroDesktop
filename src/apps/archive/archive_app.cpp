#include "aero/apps/builtin_apps.h"

#include "aero/apps/common/app_widgets.h"
#include "aero/core/paths.h"
#include "aero/core/utils.h"
#include "aero/platform/i_file_system.h"
#include "aero/theme/controls.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/theme.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QProcess>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QUrl>

namespace Aero::Applications {
namespace {

enum class ArchiveKind {
    None,
    Zip,
    Tar,
};

struct ArchiveEntry {
    QString name;
    qint64 size = 0;
    QDateTime modified;
    bool directory = false;
};

class ArchiveView final : public AppView {
public:
    explicit ArchiveView(const AppContext& context, QWidget* parent = nullptr)
        : AppView(context, parent)
    {
        buildUi();
        setAcceptDrops(true);
    }

    ~ArchiveView() override
    {
        if (m_extractProcess != nullptr && m_extractProcess->state() != QProcess::NotRunning) {
            m_extractProcess->kill();
            m_extractProcess->waitForFinished(1000);
        }
    }

    QString windowTitle() const override
    {
        return m_path.isEmpty() ? QStringLiteral("Archive Manager") : QFileInfo(m_path).fileName();
    }

    QString subtitle() const override
    {
        return m_path.isEmpty() ? QStringLiteral("Open a tar or zip archive")
                                : Core::Paths::elideHome(m_path);
    }

    void setLaunchArgument(const QString& argument) override
    {
        AppView::setLaunchArgument(argument);
        if (!argument.isEmpty()) {
            loadArchive(Core::Utils::absolutePath(argument));
        }
    }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override
    {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        }
    }

    void dropEvent(QDropEvent* event) override
    {
        const QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty() && urls.first().isLocalFile()) {
            loadArchive(urls.first().toLocalFile());
            event->acceptProposedAction();
        }
    }

private:
    void buildUi()
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(16, 14, 16, 14);
        root->setSpacing(10);

        auto* header = new QHBoxLayout;
        header->setContentsMargins(0, 0, 0, 0);
        header->addWidget(Widgets::pageHeader(QStringLiteral("Archive Manager"),
                                              QStringLiteral("Inspect and extract tar or zip archives"), this), 1);
        auto* openButton = new Theme::AeroButton(QStringLiteral("Open Archive"), QStringLiteral("folder"), this);
        connect(openButton, &QAbstractButton::clicked, this, [this]() { openDialog(); });
        header->addWidget(openButton, 0, Qt::AlignTop);
        root->addLayout(header);

        auto* infoCard = new Theme::Card(this);
        infoCard->setRadius(12);
        auto* infoLayout = new QHBoxLayout(infoCard);
        infoLayout->setContentsMargins(14, 10, 14, 10);
        infoLayout->setSpacing(10);
        auto* icon = new QLabel(infoCard);
        icon->setPixmap(Theme::IconFactory::pixmap(QStringLiteral("archive"), 34,
                                                   Theme::ThemeManager::instance().tokens().color.accent, 2.0));
        infoLayout->addWidget(icon);
        auto* textColumn = new QVBoxLayout;
        textColumn->setSpacing(2);
        m_pathLabel = new QLabel(QStringLiteral("No archive open"), infoCard);
        m_pathLabel->setProperty("role", QStringLiteral("title"));
        m_pathLabel->setFont(Theme::ThemeManager::instance().bodyFont(QFont::DemiBold));
        textColumn->addWidget(m_pathLabel);
        m_infoLabel = Widgets::mutedLabel(QStringLiteral("Drag an archive here or choose Open Archive."), infoCard);
        m_infoLabel->setFont(Theme::ThemeManager::instance().captionFont());
        textColumn->addWidget(m_infoLabel);
        infoLayout->addLayout(textColumn, 1);
        m_extractButton = new Theme::AeroButton(QStringLiteral("Extract…"), QStringLiteral("downloads"), this);
        m_extractButton->setEnabled(false);
        connect(m_extractButton, &QAbstractButton::clicked, this, [this]() { extract(); });
        infoLayout->addWidget(m_extractButton);
        root->addWidget(infoCard);

        m_stack = new QStackedWidget(this);
        m_table = new QTableWidget(m_stack);
        m_table->setColumnCount(4);
        m_table->setHorizontalHeaderLabels({QStringLiteral("Name"), QStringLiteral("Size"),
                                            QStringLiteral("Type"), QStringLiteral("Modified")});
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setShowGrid(false);
        m_table->verticalHeader()->setVisible(false);
        m_table->verticalHeader()->setDefaultSectionSize(27);
        m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        m_table->setSortingEnabled(true);
        m_table->setContextMenuPolicy(Qt::CustomContextMenu);
        m_stack->addWidget(m_table);
        m_stack->addWidget(Widgets::emptyState(
            QStringLiteral("archive"),
            QStringLiteral("No archive open"),
            QStringLiteral("Aero reads tar and zip archives through the platform's tar and unzip tools. "
                           "Drag a file here, or choose Open Archive."),
            m_stack));
        m_stack->setCurrentIndex(1);
        root->addWidget(m_stack, 1);

        m_status = Widgets::mutedLabel(QString(), this);
        m_status->setFont(Theme::ThemeManager::instance().captionFont());
        root->addWidget(m_status);

        connect(m_table, &QTableWidget::customContextMenuRequested, this,
                [this](const QPoint& position) {
                    const QModelIndex index = m_table->indexAt(position);
                    if (!index.isValid()) {
                        return;
                    }
                    QTableWidgetItem* item = m_table->item(index.row(), 0);
                    if (item == nullptr) {
                        return;
                    }
                    QMenu menu(this);
                    QAction* copy = menu.addAction(QStringLiteral("Copy Entry Name"));
                    if (menu.exec(m_table->viewport()->mapToGlobal(position)) == copy) {
                        QApplication::clipboard()->setText(item->text());
                    }
                });
    }

    void openDialog()
    {
        const QString start = context().fileSystem() != nullptr
            ? context().fileSystem()->standardLocation(Platform::StandardLocation::Downloads)
            : QString();
        const QString path = QFileDialog::getOpenFileName(
            this,
            QStringLiteral("Open Archive"),
            start,
            QStringLiteral("Archives (*.tar *.tar.gz *.tgz *.tar.xz *.txz *.tar.bz2 *.tbz2 *.zip);;All files (*)"));
        if (!path.isEmpty()) {
            loadArchive(path);
        }
    }

    static ArchiveKind kindForPath(const QString& path)
    {
        const QString lower = path.toLower();
        if (lower.endsWith(QStringLiteral(".zip"))) {
            return ArchiveKind::Zip;
        }
        if (lower.endsWith(QStringLiteral(".tar")) || lower.endsWith(QStringLiteral(".tar.gz"))
            || lower.endsWith(QStringLiteral(".tgz")) || lower.endsWith(QStringLiteral(".tar.xz"))
            || lower.endsWith(QStringLiteral(".txz")) || lower.endsWith(QStringLiteral(".tar.bz2"))
            || lower.endsWith(QStringLiteral(".tbz2"))) {
            return ArchiveKind::Tar;
        }
        return ArchiveKind::None;
    }

    static QString toolForPath(const QString& path)
    {
        const ArchiveKind kind = kindForPath(path);
        if (kind == ArchiveKind::Zip) {
            return QStandardPaths::findExecutable(QStringLiteral("unzip"));
        }
        if (kind == ArchiveKind::Tar) {
            return QStandardPaths::findExecutable(QStringLiteral("tar"));
        }
        return {};
    }

    void loadArchive(const QString& path)
    {
        const QFileInfo info(path);
        if (!info.isFile()) {
            QMessageBox::warning(this, QStringLiteral("Archive Not Found"),
                                 QStringLiteral("“%1” is not a file.").arg(path));
            return;
        }
        const ArchiveKind kind = kindForPath(path);
        const QString tool = toolForPath(path);
        if (kind == ArchiveKind::None) {
            QMessageBox::warning(this, QStringLiteral("Unsupported Archive"),
                                 QStringLiteral("Aero supports .tar, .tar.gz, .tgz, .tar.xz, .tar.bz2 and .zip archives."));
            return;
        }
        if (tool.isEmpty()) {
            QMessageBox::warning(
                this,
                QStringLiteral("Helper Tool Missing"),
                QStringLiteral("The platform does not provide “%1”. Install the corresponding command line tool to use this archive.")
                    .arg(kind == ArchiveKind::Zip ? QStringLiteral("unzip") : QStringLiteral("tar")));
            return;
        }

        m_status->setText(QStringLiteral("Reading archive…"));
        QCoreApplication::processEvents();

        QProcess process;
        process.setProcessChannelMode(QProcess::SeparateChannels);
        if (kind == ArchiveKind::Zip) {
            process.start(tool, {QStringLiteral("-l"), QStringLiteral("-q"), path});
        } else {
            process.start(tool, {QStringLiteral("-tvf"), path});
        }
        if (!process.waitForFinished(10000)) {
            process.kill();
            process.waitForFinished(1000);
            QMessageBox::warning(this, QStringLiteral("Archive Error"),
                                 QStringLiteral("Reading the archive timed out."));
            m_status->clear();
            return;
        }
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            const QString error = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            QMessageBox::warning(this, QStringLiteral("Archive Error"),
                                 error.isEmpty() ? QStringLiteral("The archive could not be read.") : error);
            m_status->clear();
            return;
        }

        const QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
        QList<ArchiveEntry> entries;
        if (kind == ArchiveKind::Zip) {
            entries = parseZipListing(output);
        } else {
            entries = parseTarListing(output);
        }

        m_path = path;
        m_kind = kind;
        m_tool = tool;
        m_pathLabel->setText(info.fileName());
        m_infoLabel->setText(QStringLiteral("%1 · %2 · %3 entries")
                                 .arg(Core::Utils::formatBytes(static_cast<quint64>(info.size())),
                                      kind == ArchiveKind::Zip ? QStringLiteral("ZIP") : QStringLiteral("TAR"),
                                      QString::number(entries.size())));
        m_extractButton->setEnabled(true);
        m_stack->setCurrentIndex(0);
        populateTable(entries);

        qint64 totalSize = 0;
        for (const ArchiveEntry& entry : entries) {
            totalSize += entry.size;
        }
        m_status->setText(QStringLiteral("%1 entries · %2 uncompressed")
                              .arg(entries.size())
                              .arg(Core::Utils::formatBytes(static_cast<quint64>(qMax<qint64>(0, totalSize)))));
        emit titleChanged(windowTitle());
        emit subtitleChanged(subtitle());
    }

    static QList<ArchiveEntry> parseZipListing(const QString& output)
    {
        QList<ArchiveEntry> entries;
        const QRegularExpression expression(
            QStringLiteral("^\\s*(\\d+)\\s+(\\d{4}-\\d{2}-\\d{2})\\s+(\\d{2}:\\d{2})\\s+(.+?)\\s*$"));
        const QStringList lines = output.split(QLatin1Char('\n'));
        for (const QString& line : lines) {
            if (line.trimmed().isEmpty()) {
                continue;
            }
            const QRegularExpressionMatch match = expression.match(line);
            if (!match.hasMatch()) {
                continue;
            }
            ArchiveEntry entry;
            entry.size = match.captured(1).toLongLong();
            entry.modified = QDateTime::fromString(match.captured(2) + QLatin1Char(' ') + match.captured(3),
                                                    QStringLiteral("yyyy-MM-dd HH:mm"));
            entry.name = match.captured(4).trimmed();
            entry.directory = entry.name.endsWith(QLatin1Char('/'));
            entries.append(entry);
        }
        return entries;
    }

    static QList<ArchiveEntry> parseTarListing(const QString& output)
    {
        QList<ArchiveEntry> entries;
        const QRegularExpression verbose(
            QStringLiteral("^\\S+\\s+\\S+\\s+(\\d+)\\s+(\\d{4}-\\d{2}-\\d{2})\\s+(\\d{2}:\\d{2})\\s+(.+?)\\s*$"));
        const QStringList lines = output.split(QLatin1Char('\n'));
        for (const QString& line : lines) {
            if (line.trimmed().isEmpty()) {
                continue;
            }
            ArchiveEntry entry;
            const QRegularExpressionMatch match = verbose.match(line);
            if (match.hasMatch()) {
                entry.size = match.captured(1).toLongLong();
                entry.modified = QDateTime::fromString(match.captured(2) + QLatin1Char(' ') + match.captured(3),
                                                        QStringLiteral("yyyy-MM-dd HH:mm"));
                entry.name = match.captured(4).trimmed();
            } else {
                const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")),
                                                     Qt::SkipEmptyParts);
                if (parts.isEmpty()) {
                    continue;
                }
                entry.name = parts.last();
            }
            entry.directory = entry.name.endsWith(QLatin1Char('/'));
            entries.append(entry);
        }
        return entries;
    }

    void populateTable(const QList<ArchiveEntry>& entries)
    {
        m_table->setSortingEnabled(false);
        m_table->setRowCount(entries.size());
        int row = 0;
        for (const ArchiveEntry& entry : entries) {
            auto* name = new QTableWidgetItem(entry.directory ? entry.name : entry.name);
            if (entry.directory) {
                name->setIcon(Theme::IconFactory::icon(QStringLiteral("folder"), 16,
                                                       Theme::ThemeManager::instance().tokens().color.accent));
            } else {
                name->setIcon(Theme::IconFactory::icon(QStringLiteral("file"), 16,
                                                       Theme::ThemeManager::instance().tokens().color.textMuted));
            }
            auto* size = new QTableWidgetItem(entry.directory ? QStringLiteral("—")
                                                               : Core::Utils::formatBytes(static_cast<quint64>(entry.size)));
            size->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            auto* type = new QTableWidgetItem(entry.directory ? QStringLiteral("Folder")
                                                               : Widgets::fileKindLabel(entry.name, false));
            auto* modified = new QTableWidgetItem(entry.modified.isValid()
                                                       ? entry.modified.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                                                       : QStringLiteral("—"));
            m_table->setItem(row, 0, name);
            m_table->setItem(row, 1, size);
            m_table->setItem(row, 2, type);
            m_table->setItem(row, 3, modified);
            ++row;
        }
        m_table->setSortingEnabled(true);
        m_table->sortItems(0, Qt::AscendingOrder);
    }

    void extract()
    {
        if (m_path.isEmpty()) {
            return;
        }
        const QString destination = QFileDialog::getExistingDirectory(
            this,
            QStringLiteral("Extract Archive"),
            context().fileSystem() != nullptr ? context().fileSystem()->homePath() : QString());
        if (destination.isEmpty()) {
            return;
        }

        if (m_extractProcess == nullptr) {
            m_extractProcess = new QProcess(this);
            m_extractProcess->setProcessChannelMode(QProcess::MergedChannels);
            connect(m_extractProcess, &QProcess::finished, this,
                    [this](int exitCode, QProcess::ExitStatus status) {
                        m_extractButton->setEnabled(true);
                        if (status == QProcess::NormalExit && exitCode == 0) {
                            m_status->setText(QStringLiteral("Extraction complete."));
                            context().notify(QStringLiteral("Archive extracted"),
                                             QStringLiteral("The archive was extracted successfully."),
                                             QStringLiteral("archive"));
                        } else {
                            m_status->setText(QStringLiteral("Extraction failed."));
                            QMessageBox::warning(this, QStringLiteral("Extraction Failed"),
                                                 QString::fromLocal8Bit(m_extractProcess->readAll()).trimmed());
                        }
                    });
        }

        m_extractProcess->setWorkingDirectory(destination);
        if (m_kind == ArchiveKind::Zip) {
            m_extractProcess->start(m_tool, {QStringLiteral("-o"), m_path, QStringLiteral("-d"), destination});
        } else {
            m_extractProcess->start(m_tool, {QStringLiteral("-xf"), m_path, QStringLiteral("-C"), destination});
        }
        m_extractButton->setEnabled(false);
        m_status->setText(QStringLiteral("Extracting to %1…").arg(Core::Paths::elideHome(destination)));
    }

    QStackedWidget* m_stack = nullptr;
    QTableWidget* m_table = nullptr;
    QLabel* m_pathLabel = nullptr;
    QLabel* m_infoLabel = nullptr;
    QLabel* m_status = nullptr;
    Theme::AeroButton* m_extractButton = nullptr;
    QProcess* m_extractProcess = nullptr;
    QString m_path;
    QString m_tool;
    ArchiveKind m_kind = ArchiveKind::None;
};

} // namespace

AppView* createArchiveApp(const AppContext& context, QWidget* parent)
{
    return new ArchiveView(context, parent);
}

} // namespace Aero::Applications
