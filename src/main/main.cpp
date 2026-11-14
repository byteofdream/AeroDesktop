#include "aero/core/logging.h"
#include "aero/core/paths.h"
#include "aero/core/version.h"
#include "aero/platform/platform_types.h"
#include "aero/shell/desktop_window.h"
#include "aero/shell/shell.h"
#include "aero/theme/style.h"
#include "aero/theme/theme.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSize>
#include <QStringList>

#include <cstdlib>

namespace {

constexpr auto kDefaultWindowSize = "1360x860";

QRect parseGeometry(const QString& value, bool* ok)
{
    if (ok != nullptr) {
        *ok = false;
    }
    static const QRegularExpression expression(
        QStringLiteral("^(\\d{2,5})[xX](\\d{2,5})(?:\\+(-?\\d+)\\+(-?\\d+))?$"));
    const QRegularExpressionMatch match = expression.match(value.trimmed());
    if (!match.hasMatch()) {
        return {};
    }
    const int width = match.captured(1).toInt();
    const int height = match.captured(2).toInt();
    if (width < 640 || height < 480) {
        return {};
    }
    int x = 0;
    int y = 0;
    if (!match.captured(3).isEmpty()) {
        x = match.captured(3).toInt();
        y = match.captured(4).toInt();
    }
    if (ok != nullptr) {
        *ok = true;
    }
    return QRect(QPoint(x, y), QSize(width, height));
}

Aero::Platform::Backend parseBackend(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("dreamyos") || normalized == QStringLiteral("dreamy")) {
        return Aero::Platform::Backend::DreamyOS;
    }
    return Aero::Platform::Backend::LinuxDev;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Aero"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("aero.desktop"));
    QCoreApplication::setApplicationName(QStringLiteral("Aero Desktop"));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(Aero::Core::VersionString));
    application.setDesktopFileName(QStringLiteral("aero-desktop"));
    application.setQuitOnLastWindowClosed(true);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Aero Desktop — a liquid-glass desktop environment for DreamyOS."));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption backendOption(
        QStringList {QStringLiteral("b"), QStringLiteral("backend")},
        QStringLiteral("Platform backend to run: linux (development) or dreamyos."),
        QStringLiteral("backend"), QStringLiteral("linux"));
    parser.addOption(backendOption);

    QCommandLineOption windowedOption(
        QStringList {QStringLiteral("w"), QStringLiteral("windowed")},
        QStringLiteral("Run the desktop in a normal window instead of taking over the screen."));
    parser.addOption(windowedOption);

    QCommandLineOption fullscreenOption(
        QStringList {QStringLiteral("f"), QStringLiteral("fullscreen")},
        QStringLiteral("Force fullscreen mode (the default)."));
    parser.addOption(fullscreenOption);

    QCommandLineOption geometryOption(
        QStringList {QStringLiteral("g"), QStringLiteral("geometry")},
        QStringLiteral("Windowed geometry in WIDTHxHEIGHT[+X+Y] form (default 1360x860)."),
        QStringLiteral("geometry"), QString::fromLatin1(kDefaultWindowSize));
    parser.addOption(geometryOption);

    QCommandLineOption openOption(
        QStringList {QStringLiteral("o"), QStringLiteral("open")},
        QStringLiteral("Open a file or directory after the shell starts."),
        QStringLiteral("path"));
    parser.addOption(openOption);

    parser.process(application);

    QString pathError;
    if (!Aero::Core::Paths::ensureDirectories(&pathError)) {
        qWarning() << "Aero could not create its state directories:" << pathError;
    }
    Aero::Core::Log::install(Aero::Core::Paths::logFile());

    const QString backendName = parser.value(backendOption).trimmed().toLower();
    const Aero::Platform::Backend backend = parseBackend(backendName);
    qCInfo(aeroLog) << "Starting Aero Desktop" << Aero::Core::VersionString
                    << "with backend" << Aero::Platform::backendName(backend);

    Aero::Theme::Style::install(application);

    Aero::Shell::Shell shell;
    QString error;
    if (!shell.initialize(backend, &error)) {
        qCritical().noquote() << "Aero Desktop could not start:" << error;
        QMessageBox::critical(nullptr, QStringLiteral("Aero Desktop"),
                              QStringLiteral("Aero Desktop could not start.\n\n%1").arg(error));
        Aero::Core::Log::shutdown();
        return 1;
    }

    const bool windowed = parser.isSet(windowedOption) && !parser.isSet(fullscreenOption);
    if (Aero::Shell::DesktopWindow* desktop = shell.desktopWindow()) {
        if (windowed) {
            bool geometryOk = false;
            QRect geometry = parseGeometry(parser.value(geometryOption), &geometryOk);
            if (!geometryOk) {
                geometry = QRect(QPoint(80, 60), QSize(1360, 860));
            }
            desktop->setGeometry(geometry);
            desktop->show();
        } else {
            desktop->showFullScreen();
        }
    } else {
        shell.show();
    }

    const QString openPath = parser.value(openOption);
    if (!openPath.isEmpty()) {
        shell.openPath(openPath);
    }

    QObject::connect(&application, &QCoreApplication::aboutToQuit,
                     &shell, &Aero::Shell::Shell::shutdown);

    const int result = application.exec();
    shell.shutdown();
    Aero::Core::Log::shutdown();
    return result;
}
