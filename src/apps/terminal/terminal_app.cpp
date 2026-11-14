#include "aero/apps/builtin_apps.h"

#include "aero/apps/common/app_widgets.h"
#include "aero/core/paths.h"
#include "aero/core/utils.h"
#include "aero/core/version.h"
#include "aero/platform/i_file_system.h"
#include "aero/platform/i_session_manager.h"
#include "aero/theme/controls.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/theme.h"

#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextCursor>
#include <QVBoxLayout>

namespace Aero::Applications {
namespace {

QString stripAnsi(QString text)
{
    static const QRegularExpression ansi(QStringLiteral("\\x1B\\[[0-9;?]*[ -/]*[@-~]"));
    text.remove(ansi);
    return text;
}

class TerminalView final : public AppView {
public:
    explicit TerminalView(const AppContext& context, QWidget* parent = nullptr)
        : AppView(context, parent)
    {
        Platform::IFileSystem* fs = context.fileSystem();
        m_home = fs != nullptr ? fs->homePath() : QDir::homePath();
        m_cwd = m_home;
        buildUi();
        m_process = new QProcess(this);
        m_process->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_process, &QProcess::finished, this,
                [this](int exitCode, QProcess::ExitStatus status) { finishCommand(exitCode, status); });
        connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart && m_running) {
                m_running = false;
                appendLine(QStringLiteral("aero: could not start the platform shell."), true);
                setBusy(false);
                showPrompt();
            }
        });

        appendLine(QStringLiteral("Aero Terminal %1 “%2”").arg(Core::version(), QString::fromLatin1(Core::CodeName)),
                   false);
        appendLine(QStringLiteral("Backend: %1").arg(context.platform != nullptr
                                                        ? context.platform->backendDisplayName()
                                                        : QStringLiteral("none")),
                   false);
        appendLine(QStringLiteral("Type “help” for built-in commands. External commands run through the system shell."),
                   false);
        appendLine(QString(), false);
        showPrompt();
    }

    ~TerminalView() override
    {
        if (m_process != nullptr && m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            m_process->waitForFinished(1000);
        }
    }

    QString windowTitle() const override
    {
        return m_running ? QStringLiteral("Terminal — running") : QStringLiteral("Terminal");
    }

    QString subtitle() const override
    {
        return Core::Paths::elideHome(m_cwd);
    }

    void activate() override
    {
        m_input->setFocus();
    }

    void setLaunchArgument(const QString& argument) override
    {
        AppView::setLaunchArgument(argument);
        if (argument.isEmpty()) {
            return;
        }
        const QString path = Core::Utils::absolutePath(argument, m_cwd);
        QFileInfo info(path);
        const QString dir = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
        if (QFileInfo::exists(dir)) {
            m_cwd = dir;
            emit subtitleChanged(subtitle());
            m_prompt->setText(promptText());
        }
    }

private:
    void buildUi()
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        auto* header = new QWidget(this);
        header->setFixedHeight(42);
        auto* headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(14, 0, 10, 0);
        auto* title = Widgets::titleLabel(QStringLiteral("Terminal"), header);
        title->setFont(Theme::ThemeManager::instance().font(13.0, QFont::DemiBold));
        headerLayout->addWidget(title);
        headerLayout->addStretch(1);
        auto* clearButton = new Theme::IconButton(QStringLiteral("trash"), header);
        clearButton->setToolTip(QStringLiteral("Clear Scrollback"));
        connect(clearButton, &QAbstractButton::clicked, this, [this]() { m_output->clear(); });
        headerLayout->addWidget(clearButton);
        root->addWidget(header);
        root->addWidget(Widgets::separator(this));

        m_output = new QPlainTextEdit(this);
        m_output->setReadOnly(true);
        m_output->setFrameShape(QFrame::NoFrame);
        m_output->setFont(Theme::ThemeManager::instance().monospaceFont(12.5));
        m_output->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_output->setUndoRedoEnabled(false);
        m_output->setMaximumBlockCount(6000);
        root->addWidget(m_output, 1);

        auto* inputRow = new QWidget(this);
        inputRow->setFixedHeight(38);
        auto* inputLayout = new QHBoxLayout(inputRow);
        inputLayout->setContentsMargins(14, 3, 14, 6);
        inputLayout->setSpacing(8);

        m_prompt = new QLabel(inputRow);
        m_prompt->setFont(Theme::ThemeManager::instance().monospaceFont(12.5));
        m_prompt->setProperty("role", QStringLiteral("accent"));
        m_prompt->setText(promptText());
        inputLayout->addWidget(m_prompt);

        m_input = new QLineEdit(inputRow);
        m_input->setFrame(false);
        m_input->setFont(Theme::ThemeManager::instance().monospaceFont(12.5));
        m_input->setStyleSheet(QStringLiteral("background: transparent; border: none; padding: 0;"));
        m_input->setPlaceholderText(QStringLiteral("Enter a command"));
        inputLayout->addWidget(m_input, 1);

        root->addWidget(Widgets::separator(this));
        root->addWidget(inputRow);

        connect(m_input, &QLineEdit::returnPressed, this, [this]() { submit(); });
        m_input->installEventFilter(this);
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == m_input && event->type() == QEvent::KeyPress) {
            auto* key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_Up) {
                historyMove(-1);
                return true;
            }
            if (key->key() == Qt::Key_Down) {
                historyMove(1);
                return true;
            }
            if (key->key() == Qt::Key_L && (key->modifiers() & Qt::ControlModifier)) {
                m_output->clear();
                return true;
            }
            if (key->key() == Qt::Key_C && (key->modifiers() & Qt::ControlModifier)) {
                if (m_running) {
                    m_process->kill();
                    appendLine(QStringLiteral("^C"), true);
                } else {
                    m_input->clear();
                }
                return true;
            }
        }
        return AppView::eventFilter(watched, event);
    }

    QString promptText() const
    {
        QString user = QStringLiteral("aero");
        QString host = QStringLiteral("dreamy");
        if (context().sessionManager() != nullptr) {
            const Platform::UserInfo info = context().sessionManager()->currentUser();
            if (!info.userName.isEmpty()) {
                user = info.userName;
            }
            const Platform::SessionInfo session = context().sessionManager()->session();
            if (!session.hostName.isEmpty()) {
                host = session.hostName;
            }
        }
        QString path = m_cwd;
        if (!m_home.isEmpty() && path.startsWith(m_home)) {
            path = QLatin1Char('~') + path.mid(m_home.size());
        }
        return QStringLiteral("%1@%2 %3 %").arg(user, host, path);
    }

    void appendLine(const QString& text, bool dim)
    {
        if (dim) {
            m_output->appendPlainText(text);
            return;
        }
        m_output->appendPlainText(text);
        QScrollBar* bar = m_output->verticalScrollBar();
        if (bar != nullptr) {
            bar->setValue(bar->maximum());
        }
    }

    void appendOutput(const QString& text)
    {
        if (text.isEmpty()) {
            return;
        }
        m_output->moveCursor(QTextCursor::End);
        m_output->insertPlainText(text);
        m_output->moveCursor(QTextCursor::End);
        QScrollBar* bar = m_output->verticalScrollBar();
        if (bar != nullptr) {
            bar->setValue(bar->maximum());
        }
    }

    void showPrompt()
    {
        m_prompt->setText(promptText());
        emit subtitleChanged(subtitle());
        m_input->clear();
        m_input->setFocus();
    }

    void setBusy(bool busy)
    {
        m_running = busy;
        m_input->setEnabled(!busy);
        if (!busy) {
            m_input->setFocus();
        }
        emit titleChanged(windowTitle());
    }

    void submit()
    {
        if (m_running) {
            return;
        }
        const QString command = m_input->text().trimmed();
        if (command.isEmpty()) {
            showPrompt();
            return;
        }

        if (m_history.isEmpty() || m_history.last() != command) {
            m_history.append(command);
            while (m_history.size() > 200) {
                m_history.removeFirst();
            }
        }
        m_historyIndex = m_history.size();

        appendLine(QStringLiteral("%1 %2").arg(promptText(), command), true);
        m_input->clear();

        if (runBuiltIn(command)) {
            showPrompt();
            return;
        }

        const QString shell = QProcessEnvironment::systemEnvironment().value(QStringLiteral("SHELL"),
                                                                              QStringLiteral("/bin/sh"));
        setBusy(true);
        m_process->setWorkingDirectory(m_cwd);
        m_process->start(shell, {QStringLiteral("-c"), command});
    }

    void finishCommand(int exitCode, QProcess::ExitStatus status)
    {
        if (!m_running) {
            return;
        }
        const QString output = stripAnsi(QString::fromLocal8Bit(m_process->readAll()));
        if (!output.isEmpty()) {
            appendOutput(output);
            if (!output.endsWith(QLatin1Char('\n'))) {
                appendOutput(QStringLiteral("\n"));
            }
        }
        if (status == QProcess::CrashExit) {
            appendLine(QStringLiteral("aero: command terminated by signal."), true);
        } else if (exitCode != 0) {
            appendLine(QStringLiteral("aero: exit status %1").arg(exitCode), true);
        }
        setBusy(false);
        showPrompt();
    }

    bool runBuiltIn(const QString& line)
    {
        QString command = line;
        QString argument;
        const int space = line.indexOf(QLatin1Char(' '));
        if (space >= 0) {
            command = line.left(space);
            argument = line.mid(space + 1).trimmed();
        }

        if (command == QStringLiteral("help")) {
            appendLine(QStringLiteral("Built-in commands:"), false);
            appendLine(QStringLiteral("  help              show this help"), false);
            appendLine(QStringLiteral("  clear             clear the scrollback"), false);
            appendLine(QStringLiteral("  pwd               print the working directory"), false);
            appendLine(QStringLiteral("  cd [path]         change directory (~ and .. supported)"), false);
            appendLine(QStringLiteral("  ls [path]         list a directory"), false);
            appendLine(QStringLiteral("  cat <file>        print a text file"), false);
            appendLine(QStringLiteral("  open <path>       open a file or folder in Aero"), false);
            appendLine(QStringLiteral("  edit <file>       open a file in Text Editor"), false);
            appendLine(QStringLiteral("  version           show Aero version"), false);
            appendLine(QStringLiteral("  whoami            show the current user"), false);
            appendLine(QStringLiteral("  date              show the current date and time"), false);
            appendLine(QStringLiteral("  exit              close this terminal"), false);
            appendLine(QStringLiteral("Anything else is executed by the platform shell in the working directory."), false);
            return true;
        }
        if (command == QStringLiteral("clear") || command == QStringLiteral("cls")) {
            m_output->clear();
            return true;
        }
        if (command == QStringLiteral("pwd")) {
            appendLine(m_cwd, false);
            return true;
        }
        if (command == QStringLiteral("cd")) {
            QString target = argument;
            if (target.isEmpty() || target == QStringLiteral("~")) {
                target = m_home;
            } else if (target == QStringLiteral("-")) {
                target = m_previousCwd;
            }
            const QString path = Core::Utils::absolutePath(target, m_cwd);
            if (!QFileInfo(path).isDir()) {
                appendLine(QStringLiteral("cd: no such directory: %1").arg(target), true);
                return true;
            }
            m_previousCwd = m_cwd;
            m_cwd = QFileInfo(path).absoluteFilePath();
            return true;
        }
        if (command == QStringLiteral("ls") || command == QStringLiteral("dir")) {
            Platform::IFileSystem* fs = context().fileSystem();
            if (fs == nullptr) {
                appendLine(QStringLiteral("ls: file system service unavailable"), true);
                return true;
            }
            const QString path = argument.isEmpty() ? m_cwd : Core::Utils::absolutePath(argument, m_cwd);
            QString error;
            const QList<Platform::FileEntry> entries = fs->listDirectory(path, true, &error);
            if (!error.isEmpty()) {
                appendLine(QStringLiteral("ls: %1").arg(error), true);
                return true;
            }
            QStringList names;
            for (const Platform::FileEntry& entry : entries) {
                names.append(entry.isDirectory ? entry.name + QLatin1Char('/') : entry.name);
            }
            names.sort(Qt::CaseInsensitive);
            appendLine(names.join(QStringLiteral("   ")), false);
            return true;
        }
        if (command == QStringLiteral("cat")) {
            Platform::IFileSystem* fs = context().fileSystem();
            if (fs == nullptr || argument.isEmpty()) {
                appendLine(QStringLiteral("cat: supply a file path"), true);
                return true;
            }
            QString error;
            const QByteArray data = fs->readFile(Core::Utils::absolutePath(argument, m_cwd), 2 * 1024 * 1024, &error);
            if (!error.isEmpty()) {
                appendLine(QStringLiteral("cat: %1").arg(error), true);
                return true;
            }
            appendOutput(QString::fromUtf8(data));
            if (!data.endsWith('\n')) {
                appendOutput(QStringLiteral("\n"));
            }
            return true;
        }
        if (command == QStringLiteral("open") || command == QStringLiteral("edit")) {
            if (argument.isEmpty()) {
                appendLine(QStringLiteral("%1: supply a path").arg(command), true);
                return true;
            }
            const QString path = Core::Utils::absolutePath(argument, m_cwd);
            if (command == QStringLiteral("edit") || Core::Utils::isTextFile(path)) {
                context().launch(QString::fromLatin1(AppIds::Editor), path);
            } else {
                Platform::IFileSystem* fs = context().fileSystem();
                if (fs == nullptr || !fs->openExternally(path)) {
                    appendLine(QStringLiteral("open: no handler for %1").arg(path), true);
                }
            }
            return true;
        }
        if (command == QStringLiteral("version")) {
            appendLine(QStringLiteral("Aero Desktop %1 (%2)").arg(Core::version(), QString::fromLatin1(Core::CodeName)),
                       false);
            return true;
        }
        if (command == QStringLiteral("whoami")) {
            Platform::ISessionManager* session = context().sessionManager();
            appendLine(session != nullptr ? session->currentUser().userName : QStringLiteral("aero"), false);
            return true;
        }
        if (command == QStringLiteral("date")) {
            appendLine(QDateTime::currentDateTime().toString(Qt::TextDate), false);
            return true;
        }
        if (command == QStringLiteral("exit") || command == QStringLiteral("quit") || command == QStringLiteral("logout")) {
            emit closeRequested();
            return true;
        }

        return false;
    }

    void historyMove(int delta)
    {
        if (m_history.isEmpty()) {
            return;
        }
        m_historyIndex = Core::Utils::clamp(m_historyIndex + delta, 0, int(m_history.size()));
        if (m_historyIndex >= m_history.size()) {
            m_input->clear();
        } else {
            m_input->setText(m_history.at(m_historyIndex));
        }
    }

    QPlainTextEdit* m_output = nullptr;
    QLineEdit* m_input = nullptr;
    QLabel* m_prompt = nullptr;
    QProcess* m_process = nullptr;
    QString m_home;
    QString m_cwd;
    QString m_previousCwd;
    QStringList m_history;
    int m_historyIndex = 0;
    bool m_running = false;
};

} // namespace

AppView* createTerminalApp(const AppContext& context, QWidget* parent)
{
    return new TerminalView(context, parent);
}

} // namespace Aero::Applications
