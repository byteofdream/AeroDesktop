#include "aero/apps/builtin_apps.h"

#include "aero/apps/common/app_widgets.h"
#include "aero/core/paths.h"
#include "aero/core/utils.h"
#include "aero/platform/i_file_system.h"
#include "aero/theme/controls.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/theme.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QVBoxLayout>

#include <functional>

namespace Aero::Applications {
namespace {

class EditorView final : public AppView {
public:
    explicit EditorView(const AppContext& context, QWidget* parent = nullptr)
        : AppView(context, parent)
    {
        buildUi();
        refreshChrome();
    }

    QString windowTitle() const override
    {
        const QString name = m_path.isEmpty() ? QStringLiteral("Untitled") : QFileInfo(m_path).fileName();
        return m_dirty ? QStringLiteral("• %1").arg(name) : name;
    }

    QString subtitle() const override
    {
        return m_path.isEmpty() ? QStringLiteral("Text Editor") : Core::Paths::elideHome(m_path);
    }

    bool canClose() override
    {
        return maybeSave();
    }

    void setLaunchArgument(const QString& argument) override
    {
        AppView::setLaunchArgument(argument);
        if (!argument.isEmpty()) {
            loadFile(Core::Utils::absolutePath(argument));
        }
    }

    void activate() override
    {
        m_editor->setFocus();
    }

private:
    void buildUi()
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        auto* toolbar = new QWidget(this);
        toolbar->setFixedHeight(48);
        auto* layout = new QHBoxLayout(toolbar);
        layout->setContentsMargins(10, 7, 10, 7);
        layout->setSpacing(5);

        auto addButton = [this, toolbar, layout](const QString& icon, const QString& tooltip,
                                                 const std::function<void()>& action) {
            auto* button = new Theme::IconButton(icon, toolbar);
            button->setToolTip(tooltip);
            connect(button, &QAbstractButton::clicked, this, action);
            layout->addWidget(button);
            return button;
        };

        addButton(QStringLiteral("plus"), QStringLiteral("New"), [this]() { newFile(); });
        addButton(QStringLiteral("folder"), QStringLiteral("Open…"), [this]() { openDialog(); });
        addButton(QStringLiteral("save"), QStringLiteral("Save"), [this]() { save(); });
        addButton(QStringLiteral("external"), QStringLiteral("Save As…"), [this]() { saveAs(); });

        layout->addWidget(Widgets::separator(toolbar));
        m_wrapButton = addButton(QStringLiteral("list"), QStringLiteral("Toggle Word Wrap"),
                                 [this]() { toggleWrap(); });
        m_wrapButton->setCheckable(true);
        m_wrapButton->setChecked(true);
        m_wrapButton->setAccentWhenChecked(true);

        auto* zoomOut = new Theme::IconButton(QStringLiteral("minus"), toolbar);
        zoomOut->setToolTip(QStringLiteral("Zoom Out"));
        connect(zoomOut, &QAbstractButton::clicked, this, [this]() { zoom(-1); });
        layout->addWidget(zoomOut);
        auto* zoomIn = new Theme::IconButton(QStringLiteral("plus"), toolbar);
        zoomIn->setToolTip(QStringLiteral("Zoom In"));
        connect(zoomIn, &QAbstractButton::clicked, this, [this]() { zoom(1); });
        layout->addWidget(zoomIn);

        layout->addStretch(1);

        m_titleLabel = new QLabel(QStringLiteral("Untitled"), toolbar);
        m_titleLabel->setProperty("role", QStringLiteral("muted"));
        m_titleLabel->setFont(Theme::ThemeManager::instance().font(12.0, QFont::Medium));
        layout->addWidget(m_titleLabel);

        m_menuButton = new Theme::IconButton(QStringLiteral("ellipsis"), toolbar);
        m_menuButton->setToolTip(QStringLiteral("More"));
        layout->addWidget(m_menuButton);
        connect(m_menuButton, &QAbstractButton::clicked, this, [this]() { showMenu(); });

        root->addWidget(toolbar);
        root->addWidget(Widgets::separator(this));

        m_editor = new QPlainTextEdit(this);
        m_editor->setFrameShape(QFrame::NoFrame);
        m_editor->setFont(Theme::ThemeManager::instance().monospaceFont(13.0));
        m_editor->setTabStopDistance(4 * QFontMetricsF(m_editor->font()).horizontalAdvance(QLatin1Char(' ')));
        m_editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        m_editor->setPlaceholderText(QStringLiteral("Start typing…"));
        root->addWidget(m_editor, 1);

        root->addWidget(Widgets::separator(this));
        auto* status = new QWidget(this);
        status->setFixedHeight(27);
        auto* statusLayout = new QHBoxLayout(status);
        statusLayout->setContentsMargins(12, 0, 12, 0);
        m_status = Widgets::mutedLabel(QString(), status);
        m_status->setFont(Theme::ThemeManager::instance().captionFont());
        statusLayout->addWidget(m_status, 1);
        m_position = Widgets::mutedLabel(QStringLiteral("Ln 1, Col 1"), status);
        m_position->setFont(Theme::ThemeManager::instance().captionFont());
        statusLayout->addWidget(m_position);
        root->addWidget(status);

        connect(m_editor, &QPlainTextEdit::textChanged, this, [this]() {
            if (m_loading) {
                return;
            }
            setDirty(true);
        });
        connect(m_editor, &QPlainTextEdit::cursorPositionChanged, this, [this]() {
            const QTextCursor cursor = m_editor->textCursor();
            m_position->setText(QStringLiteral("Ln %1, Col %2")
                                    .arg(cursor.blockNumber() + 1)
                                    .arg(cursor.positionInBlock() + 1));
        });
    }

    bool maybeSave()
    {
        if (!m_dirty) {
            return true;
        }
        const auto answer = QMessageBox::question(
            this,
            QStringLiteral("Save Changes"),
            QStringLiteral("Do you want to save the changes to “%1”?")
                .arg(m_path.isEmpty() ? QStringLiteral("Untitled") : QFileInfo(m_path).fileName()),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);
        if (answer == QMessageBox::Cancel) {
            return false;
        }
        if (answer == QMessageBox::Save) {
            return save();
        }
        return true;
    }

    void newFile()
    {
        if (!maybeSave()) {
            return;
        }
        m_loading = true;
        m_editor->clear();
        m_loading = false;
        m_path.clear();
        setDirty(false);
        m_editor->setFocus();
        refreshChrome();
    }

    void openDialog()
    {
        if (!maybeSave()) {
            return;
        }
        const QString start = m_path.isEmpty()
            ? (context().fileSystem() != nullptr ? context().fileSystem()->homePath() : QString())
            : QFileInfo(m_path).absolutePath();
        const QString path = QFileDialog::getOpenFileName(
            this,
            QStringLiteral("Open File"),
            start,
            QStringLiteral("Text files (*.txt *.md *.cpp *.h *.hpp *.json *.ini *.conf *.log *.csv);;All files (*)"));
        if (!path.isEmpty()) {
            loadFile(path);
        }
    }

    void loadFile(const QString& path)
    {
        if (!maybeSave()) {
            return;
        }
        Platform::IFileSystem* fs = context().fileSystem();
        if (fs == nullptr) {
            QMessageBox::warning(this, QStringLiteral("File System Unavailable"),
                                 QStringLiteral("The platform backend does not expose a file system."));
            return;
        }
        if (!fs->isFile(path)) {
            QMessageBox::warning(this, QStringLiteral("Could Not Open File"),
                                 QStringLiteral("“%1” is not a readable file.").arg(path));
            return;
        }

        QString error;
        const QByteArray data = fs->readFile(path, 32 * 1024 * 1024, &error);
        if (!error.isEmpty() && data.isNull()) {
            QMessageBox::warning(this, QStringLiteral("Could Not Open File"), error);
            return;
        }

        m_loading = true;
        m_editor->setPlainText(QString::fromUtf8(data));
        m_loading = false;
        m_path = path;
        setDirty(false);
        m_editor->document()->setModified(false);
        m_editor->moveCursor(QTextCursor::Start);
        if (context().settings != nullptr) {
            context().settings->addRecentFile(path);
        }
        refreshChrome();
        emit titleChanged(windowTitle());
        emit subtitleChanged(subtitle());
    }

    bool save()
    {
        if (m_path.isEmpty()) {
            return saveAs();
        }
        return writeTo(m_path);
    }

    bool saveAs()
    {
        Platform::IFileSystem* fs = context().fileSystem();
        const QString start = m_path.isEmpty()
            ? (fs != nullptr ? fs->standardLocation(Platform::StandardLocation::Documents) : QString())
            : m_path;
        const QString path = QFileDialog::getSaveFileName(
            this,
            QStringLiteral("Save File"),
            start,
            QStringLiteral("Text files (*.txt *.md);;All files (*)"));
        if (path.isEmpty()) {
            return false;
        }
        return writeTo(path);
    }

    bool writeTo(const QString& path)
    {
        Platform::IFileSystem* fs = context().fileSystem();
        if (fs == nullptr) {
            QMessageBox::warning(this, QStringLiteral("File System Unavailable"),
                                 QStringLiteral("The platform backend does not expose a file system."));
            return false;
        }
        QString error;
        const QByteArray data = m_editor->toPlainText().toUtf8();
        if (!fs->writeFile(path, data, &error)) {
            QMessageBox::warning(this, QStringLiteral("Could Not Save File"), error);
            return false;
        }
        m_path = path;
        setDirty(false);
        if (context().settings != nullptr) {
            context().settings->addRecentFile(path);
        }
        refreshChrome();
        emit titleChanged(windowTitle());
        emit subtitleChanged(subtitle());
        emit statusMessage(QStringLiteral("Saved %1").arg(QFileInfo(path).fileName()));
        return true;
    }

    void setDirty(bool dirty)
    {
        if (m_dirty == dirty) {
            return;
        }
        m_dirty = dirty;
        refreshChrome();
        emit titleChanged(windowTitle());
    }

    void refreshChrome()
    {
        m_titleLabel->setText(m_dirty
                                  ? QStringLiteral("%1 — Edited").arg(m_path.isEmpty() ? QStringLiteral("Untitled")
                                                                                      : QFileInfo(m_path).fileName())
                                  : (m_path.isEmpty() ? QStringLiteral("Untitled") : QFileInfo(m_path).fileName()));
        m_status->setText(m_path.isEmpty() ? QStringLiteral("Not saved yet") : Core::Paths::elideHome(m_path));
    }

    void toggleWrap()
    {
        const bool enabled = m_wrapButton->isChecked();
        m_editor->setLineWrapMode(enabled ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    }

    void zoom(int direction)
    {
        QFont font = m_editor->font();
        const int size = qBound(9, font.pixelSize() + direction, 28);
        if (size == font.pixelSize()) {
            return;
        }
        font.setPixelSize(size);
        m_editor->setFont(font);
        m_editor->setTabStopDistance(4 * QFontMetricsF(font).horizontalAdvance(QLatin1Char(' ')));
    }

    void showMenu()
    {
        QMenu menu(this);
        QAction* reload = menu.addAction(QStringLiteral("Reload from Disk"));
        reload->setEnabled(!m_path.isEmpty());
        QAction* reveal = menu.addAction(QStringLiteral("Open Containing Folder"));
        reveal->setEnabled(!m_path.isEmpty());
        menu.addSeparator();

        QMenu* recentMenu = menu.addMenu(QStringLiteral("Open Recent"));
        QStringList recent = context().settings != nullptr ? context().settings->recentFiles() : QStringList();
        if (recent.isEmpty()) {
            QAction* empty = recentMenu->addAction(QStringLiteral("No Recent Files"));
            empty->setEnabled(false);
        }
        QList<QAction*> recentActions;
        for (const QString& path : recent) {
            QAction* action = recentMenu->addAction(QFileInfo(path).fileName());
            action->setToolTip(path);
            recentActions.append(action);
        }
        QAction* chosen = menu.exec(m_menuButton->mapToGlobal(QPoint(0, m_menuButton->height())));
        if (chosen == nullptr) {
            return;
        }
        if (chosen == reload && !m_path.isEmpty()) {
            if (m_dirty) {
                const auto answer = QMessageBox::question(this, QStringLiteral("Reload File"),
                                                          QStringLiteral("Discard unsaved changes and reload from disk?"),
                                                          QMessageBox::Yes | QMessageBox::Cancel,
                                                          QMessageBox::Cancel);
                if (answer != QMessageBox::Yes) {
                    return;
                }
                m_dirty = false;
            }
            loadFile(m_path);
        } else if (chosen == reveal && !m_path.isEmpty()) {
            context().launch(QString::fromLatin1(AppIds::Finder), QFileInfo(m_path).absolutePath());
        } else {
            const int index = recentActions.indexOf(chosen);
            if (index >= 0 && index < recent.size()) {
                loadFile(recent.at(index));
            }
        }
    }

    QPlainTextEdit* m_editor = nullptr;
    Theme::IconButton* m_wrapButton = nullptr;
    Theme::IconButton* m_menuButton = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_position = nullptr;
    QString m_path;
    bool m_dirty = false;
    bool m_loading = false;
};

} // namespace

AppView* createEditorApp(const AppContext& context, QWidget* parent)
{
    return new EditorView(context, parent);
}

} // namespace Aero::Applications
