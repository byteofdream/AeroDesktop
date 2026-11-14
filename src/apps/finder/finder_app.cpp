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
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace Aero::Applications {
namespace {

struct DirectoryResult {
    QString path;
    QList<Platform::FileEntry> entries;
    QString error;
};

class FinderView final : public AppView {
public:
    explicit FinderView(const AppContext& context, QWidget* parent = nullptr)
        : AppView(context, parent)
    {
        buildUi();
        populateSidebar();

        const Platform::IFileSystem* fs = context.fileSystem();
        const QString start = fs != nullptr ? fs->standardLocation(Platform::StandardLocation::Home) : QString();
        navigateTo(start.isEmpty() ? QStringLiteral("/") : start, true);
    }

    ~FinderView() override
    {
        if (m_watcher != nullptr && m_watcher->isRunning()) {
            m_watcher->waitForFinished();
        }
    }

    QString windowTitle() const override
    {
        if (m_currentPath.isEmpty()) {
            return QStringLiteral("Finder");
        }
        const QFileInfo info(m_currentPath);
        const QString name = info.fileName();
        return name.isEmpty() ? QStringLiteral("Computer") : name;
    }

    QString subtitle() const override
    {
        return Core::Paths::elideHome(m_currentPath);
    }

    void activate() override
    {
        refresh();
    }

    void setLaunchArgument(const QString& argument) override
    {
        AppView::setLaunchArgument(argument);
        if (argument.isEmpty()) {
            return;
        }
        const Platform::IFileSystem* fs = context().fileSystem();
        if (fs == nullptr) {
            return;
        }
        QString path = Core::Utils::absolutePath(argument);
        if (fs->isDirectory(path)) {
            navigateTo(path, true);
        } else if (fs->isFile(path)) {
            m_selectAfterLoad = path;
            navigateTo(QFileInfo(path).absolutePath(), true);
        }
    }

private:
    void buildUi()
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        auto* toolbar = new QWidget(this);
        toolbar->setFixedHeight(52);
        auto* toolbarLayout = new QHBoxLayout(toolbar);
        toolbarLayout->setContentsMargins(12, 8, 12, 8);
        toolbarLayout->setSpacing(6);

        m_back = new Theme::IconButton(QStringLiteral("back"), toolbar);
        m_back->setToolTip(QStringLiteral("Back"));
        m_forward = new Theme::IconButton(QStringLiteral("forward"), toolbar);
        m_forward->setToolTip(QStringLiteral("Forward"));
        m_up = new Theme::IconButton(QStringLiteral("arrow-up"), toolbar);
        m_up->setToolTip(QStringLiteral("Enclosing Folder"));
        m_refresh = new Theme::IconButton(QStringLiteral("refresh"), toolbar);
        m_refresh->setToolTip(QStringLiteral("Refresh"));

        toolbarLayout->addWidget(m_back);
        toolbarLayout->addWidget(m_forward);
        toolbarLayout->addWidget(m_up);
        toolbarLayout->addWidget(m_refresh);

        m_pathEdit = new QLineEdit(toolbar);
        m_pathEdit->setMinimumHeight(30);
        m_pathEdit->setClearButtonEnabled(false);
        m_pathEdit->setFont(Theme::ThemeManager::instance().monospaceFont(12.0));
        toolbarLayout->addWidget(m_pathEdit, 1);

        m_search = Widgets::searchField(QStringLiteral("Filter"), toolbar);
        m_search->setMaximumWidth(210);
        toolbarLayout->addWidget(m_search, 0);

        m_viewButton = new Theme::IconButton(QStringLiteral("list"), toolbar);
        m_viewButton->setToolTip(QStringLiteral("Switch View"));
        toolbarLayout->addWidget(m_viewButton);

        m_menuButton = new Theme::IconButton(QStringLiteral("ellipsis"), toolbar);
        m_menuButton->setToolTip(QStringLiteral("More"));
        toolbarLayout->addWidget(m_menuButton);

        root->addWidget(toolbar);
        root->addWidget(Widgets::separator(this));

        auto* splitter = new QSplitter(Qt::Horizontal, this);
        splitter->setChildrenCollapsible(false);
        splitter->setHandleWidth(1);

        m_sidebar = new QListWidget(splitter);
        m_sidebar->setFixedWidth(196);
        m_sidebar->setFrameShape(QFrame::NoFrame);
        m_sidebar->setFocusPolicy(Qt::NoFocus);
        m_sidebar->setIconSize(QSize(17, 17));
        m_sidebar->setSpacing(1);
        splitter->addWidget(m_sidebar);

        m_stack = new QStackedWidget(splitter);
        m_files = new QListWidget(m_stack);
        m_files->setFrameShape(QFrame::NoFrame);
        m_files->setSelectionMode(QAbstractItemView::SingleSelection);
        m_files->setViewMode(QListView::IconMode);
        m_files->setResizeMode(QListView::Adjust);
        m_files->setMovement(QListView::Static);
        m_files->setUniformItemSizes(false);
        m_files->setWordWrap(true);
        m_files->setIconSize(QSize(48, 48));
        m_files->setGridSize(QSize(118, 106));
        m_files->setSpacing(6);
        m_files->setContextMenuPolicy(Qt::CustomContextMenu);
        m_stack->addWidget(m_files);
        m_stack->addWidget(Widgets::emptyState(QStringLiteral("folder"),
                                               QStringLiteral("This folder is empty"),
                                               QStringLiteral("Create a folder or drag files here from another application."),
                                               m_stack));
        splitter->addWidget(m_stack);
        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 1);
        root->addWidget(splitter, 1);

        auto* statusBar = new QWidget(this);
        statusBar->setFixedHeight(28);
        auto* statusLayout = new QHBoxLayout(statusBar);
        statusLayout->setContentsMargins(14, 0, 14, 0);
        m_status = Widgets::mutedLabel(QString(), statusBar);
        m_status->setFont(Theme::ThemeManager::instance().captionFont());
        statusLayout->addWidget(m_status, 1);
        auto* hiddenCount = Widgets::mutedLabel(QString(), statusBar);
        hiddenCount->setObjectName(QStringLiteral("hiddenCount"));
        hiddenCount->setFont(Theme::ThemeManager::instance().captionFont());
        statusLayout->addWidget(hiddenCount);
        root->addWidget(Widgets::separator(this));
        root->addWidget(statusBar);

        m_watcher = new QFutureWatcher<DirectoryResult>(this);
        connect(m_watcher, &QFutureWatcher<DirectoryResult>::finished, this, [this]() {
            m_listing = false;
            const DirectoryResult result = m_watcher->result();
            if (result.path == m_currentPath) {
                applyEntries(result);
            }
            if (m_pendingList) {
                m_pendingList = false;
                startListing();
            }
        });

        connect(m_back, &QAbstractButton::clicked, this, [this]() { goHistory(-1); });
        connect(m_forward, &QAbstractButton::clicked, this, [this]() { goHistory(1); });
        connect(m_up, &QAbstractButton::clicked, this, [this]() { goUp(); });
        connect(m_refresh, &QAbstractButton::clicked, this, [this]() { refresh(); });
        connect(m_viewButton, &QAbstractButton::clicked, this, [this]() { toggleView(); });
        connect(m_menuButton, &QAbstractButton::clicked, this, [this]() { showOptionsMenu(); });

        connect(m_pathEdit, &QLineEdit::returnPressed, this, [this]() {
            navigateTo(m_pathEdit->text().trimmed(), true);
        });
        connect(m_search, &QLineEdit::textChanged, this, [this]() { applyFilter(); });

        connect(m_sidebar, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
            if (item == nullptr) {
                return;
            }
            const QString path = item->data(Qt::UserRole).toString();
            if (!path.isEmpty()) {
                navigateTo(path, true);
            }
        });
        connect(m_sidebar, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
            if (item == nullptr) {
                return;
            }
            const QString path = item->data(Qt::UserRole).toString();
            if (!path.isEmpty()) {
                navigateTo(path, true);
            }
        });
        connect(m_files, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
            openItem(item);
        });
        connect(m_files, &QListWidget::customContextMenuRequested, this,
                [this](const QPoint& position) { showItemMenu(position); });
        connect(m_files, &QListWidget::itemSelectionChanged, this, [this]() { updateStatus(); });
    }

    void populateSidebar()
    {
        m_sidebar->clear();
        Platform::IFileSystem* fs = context().fileSystem();
        if (fs == nullptr) {
            return;
        }

        const Theme::ColorTokens& colors = Theme::ThemeManager::instance().tokens().color;

        auto addSection = [this, &colors](const QString& title) {
            auto* item = new QListWidgetItem(title.toUpper());
            item->setFlags(Qt::NoItemFlags);
            item->setForeground(colors.textMuted);
            item->setFont(Theme::ThemeManager::instance().captionFont(QFont::DemiBold));
            item->setSizeHint(QSize(0, 30));
            m_sidebar->addItem(item);
        };

        auto addPlace = [this, fs](const QString& title, const QString& path, const QString& icon) {
            if (path.isEmpty() || !fs->isDirectory(path)) {
                return;
            }
            auto* item = new QListWidgetItem(Theme::IconFactory::icon(icon, 17,
                                                                     Theme::ThemeManager::instance().tokens().color.textSecondary),
                                             title);
            item->setData(Qt::UserRole, path);
            item->setToolTip(path);
            item->setSizeHint(QSize(0, 30));
            m_sidebar->addItem(item);
            if (path == m_currentPath) {
                m_sidebar->setCurrentItem(item);
            }
        };

        addSection(QStringLiteral("Favorites"));
        addPlace(QStringLiteral("Home"), fs->homePath(), QStringLiteral("home"));
        addPlace(QStringLiteral("Desktop"), fs->standardLocation(Platform::StandardLocation::Desktop), QStringLiteral("desktop"));
        addPlace(QStringLiteral("Documents"), fs->standardLocation(Platform::StandardLocation::Documents), QStringLiteral("documents"));
        addPlace(QStringLiteral("Downloads"), fs->standardLocation(Platform::StandardLocation::Downloads), QStringLiteral("downloads"));
        addPlace(QStringLiteral("Pictures"), fs->standardLocation(Platform::StandardLocation::Pictures), QStringLiteral("pictures"));
        addPlace(QStringLiteral("Music"), fs->standardLocation(Platform::StandardLocation::Music), QStringLiteral("music"));
        addPlace(QStringLiteral("Videos"), fs->standardLocation(Platform::StandardLocation::Videos), QStringLiteral("video"));

        addSection(QStringLiteral("Locations"));
        const QString root = QStringLiteral("/");
        addPlace(QStringLiteral("Computer"), root, QStringLiteral("monitor"));
    }

    void navigateTo(const QString& rawPath, bool addToHistory)
    {
        Platform::IFileSystem* fs = context().fileSystem();
        if (fs == nullptr) {
            m_status->setText(QStringLiteral("The platform has no file system service."));
            return;
        }
        QString path = Core::Utils::absolutePath(rawPath);
        if (path.isEmpty()) {
            return;
        }
        if (!fs->isDirectory(path)) {
            m_status->setText(QStringLiteral("Not a folder: %1").arg(path));
            return;
        }
        path = QFileInfo(path).absoluteFilePath();

        if (addToHistory) {
            if (m_historyIndex >= 0 && m_historyIndex + 1 < m_history.size()) {
                m_history = m_history.mid(0, m_historyIndex + 1);
            }
            if (m_history.isEmpty() || m_history.last() != path) {
                m_history.append(path);
            }
            m_historyIndex = m_history.size() - 1;
        }

        m_currentPath = path;
        m_pathEdit->setText(path);
        m_search->clear();
        m_sidebar->clearSelection();
        for (int i = 0; i < m_sidebar->count(); ++i) {
            if (m_sidebar->item(i)->data(Qt::UserRole).toString() == path) {
                m_sidebar->setCurrentItem(m_sidebar->item(i));
                break;
            }
        }

        setSubtitleText(Core::Paths::elideHome(path));
        emit titleChanged(windowTitle());
        updateNavigationButtons();
        startListing();
    }

    void startListing()
    {
        Platform::IFileSystem* fs = context().fileSystem();
        if (fs == nullptr || m_currentPath.isEmpty()) {
            return;
        }
        if (m_listing) {
            m_pendingList = true;
            return;
        }

        m_listing = true;
        const QString path = m_currentPath;
        const bool hidden = m_showHidden;
        m_files->setEnabled(false);
        m_status->setText(QStringLiteral("Loading %1…").arg(Core::Paths::elideHome(path)));

        m_watcher->setFuture(QtConcurrent::run([fs, path, hidden]() {
            DirectoryResult result;
            result.path = path;
            result.entries = fs->listDirectory(path, hidden, &result.error);
            return result;
        }));
    }

    void refresh()
    {
        startListing();
    }

    void applyEntries(const DirectoryResult& result)
    {
        m_files->setEnabled(true);
        m_files->clear();

        if (!result.error.isEmpty()) {
            m_stack->setCurrentIndex(1);
            m_status->setText(result.error);
            return;
        }

        QList<Platform::FileEntry> entries = result.entries;
        std::sort(entries.begin(), entries.end(), [](const Platform::FileEntry& a, const Platform::FileEntry& b) {
            if (a.isDirectory != b.isDirectory) {
                return a.isDirectory;
            }
            return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
        });

        const Theme::ColorTokens& colors = Theme::ThemeManager::instance().tokens().color;
        const int iconSize = m_grid ? 48 : 20;

        for (const Platform::FileEntry& entry : entries) {
            const QString iconName = entry.iconName.isEmpty()
                ? (entry.isDirectory ? QStringLiteral("folder") : QStringLiteral("file"))
                : entry.iconName;
            auto* item = new QListWidgetItem(Theme::IconFactory::icon(iconName, iconSize, colors.textSecondary),
                                             entry.name,
                                             m_files);
            item->setData(Qt::UserRole, entry.absolutePath);
            item->setData(Qt::UserRole + 1, entry.isDirectory);
            item->setData(Qt::UserRole + 2, entry.size);
            item->setData(Qt::UserRole + 3, entry.modified);
            item->setToolTip(QStringLiteral("%1\n%2\n%3")
                                 .arg(entry.name,
                                      entry.isDirectory ? QStringLiteral("Folder") : Core::Utils::formatBytes(entry.size),
                                      entry.modified.toString(QStringLiteral("yyyy-MM-dd HH:mm"))));
            if (!m_grid) {
                item->setSizeHint(QSize(0, 30));
            }
        }

        applyFilter();

        if (!m_selectAfterLoad.isEmpty()) {
            for (int i = 0; i < m_files->count(); ++i) {
                if (m_files->item(i)->data(Qt::UserRole).toString() == m_selectAfterLoad) {
                    m_files->setCurrentRow(i);
                    m_files->scrollToItem(m_files->item(i));
                    break;
                }
            }
            m_selectAfterLoad.clear();
        }

        updateStatus();
    }

    void applyFilter()
    {
        const QString needle = m_search->text().trimmed();
        int visible = 0;
        for (int i = 0; i < m_files->count(); ++i) {
            QListWidgetItem* item = m_files->item(i);
            const bool match = needle.isEmpty() || item->text().contains(needle, Qt::CaseInsensitive);
            item->setHidden(!match);
            if (match) {
                ++visible;
            }
        }
        m_stack->setCurrentIndex((m_files->count() == 0 || visible == 0) ? 1 : 0);
        if (!needle.isEmpty() && visible == 0 && m_files->count() > 0) {
            m_status->setText(QStringLiteral("No items match “%1”").arg(needle));
        } else {
            updateStatus();
        }
    }

    void updateStatus()
    {
        if (!m_status->text().startsWith(QStringLiteral("Loading"))) {
            const int total = m_files->count();
            const int selected = m_files->selectedItems().size();
            QString text = total == 1 ? QStringLiteral("1 item") : QStringLiteral("%1 items").arg(total);
            if (selected > 0) {
                text += QStringLiteral(" · %1 selected").arg(selected);
            }
            m_status->setText(text);
        }
        auto* hidden = findChild<QLabel*>(QStringLiteral("hiddenCount"));
        if (hidden != nullptr) {
            hidden->setText(m_showHidden ? QStringLiteral("Showing hidden files") : QString());
        }
        updateNavigationButtons();
    }

    void updateNavigationButtons()
    {
        m_back->setEnabled(m_historyIndex > 0);
        m_forward->setEnabled(m_historyIndex >= 0 && m_historyIndex + 1 < m_history.size());
        Platform::IFileSystem* fs = context().fileSystem();
        m_up->setEnabled(fs != nullptr && fs->isDirectory(m_currentPath));
    }

    void goHistory(int delta)
    {
        const int next = m_historyIndex + delta;
        if (next < 0 || next >= m_history.size()) {
            return;
        }
        m_historyIndex = next;
        navigateTo(m_history.at(next), false);
    }

    void goUp()
    {
        const QFileInfo info(m_currentPath);
        const QString parent = info.absolutePath();
        if (!parent.isEmpty() && parent != m_currentPath) {
            navigateTo(parent, true);
        }
    }

    void openItem(QListWidgetItem* item)
    {
        if (item == nullptr) {
            return;
        }
        const QString path = item->data(Qt::UserRole).toString();
        const bool isDir = item->data(Qt::UserRole + 1).toBool();
        if (path.isEmpty()) {
            return;
        }

        if (isDir) {
            navigateTo(path, true);
            return;
        }

        if (context().settings != nullptr) {
            context().settings->addRecentFile(path);
        }

        const QString suffix = QFileInfo(path).suffix().toLower();
        if (suffix == QStringLiteral("tar") || suffix == QStringLiteral("zip")
            || suffix == QStringLiteral("gz") || suffix == QStringLiteral("tgz")
            || suffix == QStringLiteral("xz") || suffix == QStringLiteral("bz2")) {
            context().launch(QString::fromLatin1(AppIds::Archive), path);
            return;
        }
        if (Core::Utils::isTextFile(path)) {
            context().launch(QString::fromLatin1(AppIds::Editor), path);
            return;
        }

        Platform::IFileSystem* fs = context().fileSystem();
        if (fs != nullptr && fs->openExternally(path)) {
            return;
        }
        context().notify(QStringLiteral("Could not open file"),
                         QStringLiteral("No application handler is available for %1").arg(QFileInfo(path).fileName()),
                         QStringLiteral("warning"));
    }

    void showItemMenu(const QPoint& position)
    {
        QListWidgetItem* item = m_files->itemAt(position);
        QMenu menu(this);
        if (item != nullptr) {
            const QString path = item->data(Qt::UserRole).toString();
            const bool isDir = item->data(Qt::UserRole + 1).toBool();

            QAction* open = menu.addAction(isDir ? QStringLiteral("Open Folder") : QStringLiteral("Open"));
            QAction* edit = nullptr;
            if (!isDir && (Core::Utils::isTextFile(path) || QFileInfo(path).suffix().isEmpty())) {
                edit = menu.addAction(QStringLiteral("Open in Text Editor"));
            }
            QAction* terminal = nullptr;
            if (isDir) {
                terminal = menu.addAction(QStringLiteral("Open in Terminal"));
            }
            menu.addSeparator();
            QAction* rename = menu.addAction(QStringLiteral("Rename…"));
            QAction* trash = menu.addAction(QStringLiteral("Move to Trash"));
            menu.addSeparator();
            QAction* copyPath = menu.addAction(QStringLiteral("Copy Path"));

            QAction* chosen = menu.exec(m_files->viewport()->mapToGlobal(position));
            if (chosen == nullptr) {
                return;
            }
            if (chosen == open) {
                openItem(item);
            } else if (edit != nullptr && chosen == edit) {
                context().launch(QString::fromLatin1(AppIds::Editor), path);
            } else if (terminal != nullptr && chosen == terminal) {
                context().launch(QString::fromLatin1(AppIds::Terminal), path);
            } else if (chosen == rename) {
                renameItem(path);
            } else if (chosen == trash) {
                trashItem(path);
            } else if (chosen == copyPath) {
                QApplication::clipboard()->setText(path);
            }
            return;
        }

        QAction* newFolder = menu.addAction(QStringLiteral("New Folder…"));
        QAction* refreshAction = menu.addAction(QStringLiteral("Refresh"));
        QAction* showHidden = menu.addAction(QStringLiteral("Show Hidden Files"));
        showHidden->setCheckable(true);
        showHidden->setChecked(m_showHidden);
        QAction* chosen = menu.exec(m_files->viewport()->mapToGlobal(position));
        if (chosen == newFolder) {
            createFolder();
        } else if (chosen == refreshAction) {
            refresh();
        } else if (chosen == showHidden) {
            m_showHidden = showHidden->isChecked();
            refresh();
        }
    }

    void showOptionsMenu()
    {
        QMenu menu(this);
        QAction* newFolder = menu.addAction(QStringLiteral("New Folder…"));
        QAction* refreshAction = menu.addAction(QStringLiteral("Refresh"));
        menu.addSeparator();
        QAction* toggleHidden = menu.addAction(QStringLiteral("Show Hidden Files"));
        toggleHidden->setCheckable(true);
        toggleHidden->setChecked(m_showHidden);
        menu.addSeparator();
        QAction* terminal = menu.addAction(QStringLiteral("Open in Terminal"));
        QAction* chosen = menu.exec(m_menuButton->mapToGlobal(QPoint(0, m_menuButton->height())));
        if (chosen == newFolder) {
            createFolder();
        } else if (chosen == refreshAction) {
            refresh();
        } else if (chosen == toggleHidden) {
            m_showHidden = toggleHidden->isChecked();
            refresh();
        } else if (chosen == terminal) {
            context().launch(QString::fromLatin1(AppIds::Terminal), m_currentPath);
        }
    }

    void createFolder()
    {
        Platform::IFileSystem* fs = context().fileSystem();
        if (fs == nullptr) {
            return;
        }
        bool accepted = false;
        const QString name = QInputDialog::getText(this,
                                                   QStringLiteral("New Folder"),
                                                   QStringLiteral("Folder name:"),
                                                   QLineEdit::Normal,
                                                   QStringLiteral("Untitled Folder"),
                                                   &accepted);
        if (!accepted || name.trimmed().isEmpty()) {
            return;
        }
        QString error;
        const QString path = m_currentPath + QLatin1Char('/') + name.trimmed();
        if (!fs->makeDirectory(path, &error)) {
            QMessageBox::warning(this, QStringLiteral("Could Not Create Folder"), error);
            return;
        }
        refresh();
    }

    void renameItem(const QString& path)
    {
        Platform::IFileSystem* fs = context().fileSystem();
        if (fs == nullptr) {
            return;
        }
        const QFileInfo info(path);
        bool accepted = false;
        const QString name = QInputDialog::getText(this,
                                                   QStringLiteral("Rename"),
                                                   QStringLiteral("New name:"),
                                                   QLineEdit::Normal,
                                                   info.fileName(),
                                                   &accepted);
        if (!accepted || name.trimmed().isEmpty()) {
            return;
        }
        QString error;
        const QString target = info.absolutePath() + QLatin1Char('/') + name.trimmed();
        if (!fs->rename(path, target, &error)) {
            QMessageBox::warning(this, QStringLiteral("Could Not Rename"), error);
            return;
        }
        refresh();
    }

    void trashItem(const QString& path)
    {
        Platform::IFileSystem* fs = context().fileSystem();
        if (fs == nullptr) {
            return;
        }
        const auto answer = QMessageBox::question(
            this,
            QStringLiteral("Move to Trash"),
            QStringLiteral("Move “%1” to the trash?").arg(QFileInfo(path).fileName()),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (answer != QMessageBox::Yes) {
            return;
        }
        QString error;
        if (!fs->moveToTrash(path, &error)) {
            QMessageBox::warning(this, QStringLiteral("Could Not Move to Trash"), error);
            return;
        }
        refresh();
    }

    void toggleView()
    {
        m_grid = !m_grid;
        m_viewButton->setIconName(m_grid ? QStringLiteral("list") : QStringLiteral("grid"));
        if (m_grid) {
            m_files->setViewMode(QListView::IconMode);
            m_files->setIconSize(QSize(48, 48));
            m_files->setGridSize(QSize(118, 106));
            m_files->setWordWrap(true);
        } else {
            m_files->setViewMode(QListView::ListMode);
            m_files->setIconSize(QSize(20, 20));
            m_files->setGridSize(QSize());
            m_files->setWordWrap(false);
        }
        refresh();
    }

    Theme::IconButton* m_back = nullptr;
    Theme::IconButton* m_forward = nullptr;
    Theme::IconButton* m_up = nullptr;
    Theme::IconButton* m_refresh = nullptr;
    Theme::IconButton* m_viewButton = nullptr;
    Theme::IconButton* m_menuButton = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QLineEdit* m_search = nullptr;
    QListWidget* m_sidebar = nullptr;
    QListWidget* m_files = nullptr;
    QStackedWidget* m_stack = nullptr;
    QLabel* m_status = nullptr;
    QFutureWatcher<DirectoryResult>* m_watcher = nullptr;

    QString m_currentPath;
    QString m_selectAfterLoad;
    QStringList m_history;
    int m_historyIndex = -1;
    bool m_grid = true;
    bool m_showHidden = false;
    bool m_listing = false;
    bool m_pendingList = false;
};

} // namespace

AppView* createFinderApp(const AppContext& context, QWidget* parent)
{
    return new FinderView(context, parent);
}

} // namespace Aero::Applications
