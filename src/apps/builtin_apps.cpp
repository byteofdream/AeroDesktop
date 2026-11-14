#include "aero/apps/builtin_apps.h"

namespace Aero::Applications {

void AppRegistry::registerBuiltInApplications()
{
    AppRegistry& registry = instance();

    registry.registerApp({
        QString::fromLatin1(AppIds::Finder),
        QStringLiteral("Finder"),
        QStringLiteral("Browse files and folders"),
        QStringLiteral("System"),
        {QStringLiteral("files"), QStringLiteral("browse"), QStringLiteral("home"), QStringLiteral("folder")},
        QSize(1020, 680),
        QSize(680, 420),
        true,
        false,
        &createFinderApp,
    });

    registry.registerApp({
        QString::fromLatin1(AppIds::Settings),
        QStringLiteral("Settings"),
        QStringLiteral("Configure Aero and system services"),
        QStringLiteral("System"),
        {QStringLiteral("preferences"), QStringLiteral("appearance"), QStringLiteral("network")},
        QSize(980, 700),
        QSize(760, 480),
        true,
        false,
        &createSettingsApp,
    });

    registry.registerApp({
        QString::fromLatin1(AppIds::Terminal),
        QStringLiteral("Terminal"),
        QStringLiteral("Run commands in the platform shell"),
        QStringLiteral("Utilities"),
        {QStringLiteral("shell"), QStringLiteral("command"), QStringLiteral("console")},
        QSize(820, 520),
        QSize(520, 320),
        false,
        false,
        &createTerminalApp,
    });

    registry.registerApp({
        QString::fromLatin1(AppIds::Monitor),
        QStringLiteral("Monitor"),
        QStringLiteral("Live system activity and resources"),
        QStringLiteral("Utilities"),
        {QStringLiteral("activity"), QStringLiteral("cpu"), QStringLiteral("memory")},
        QSize(940, 640),
        QSize(700, 460),
        true,
        false,
        &createMonitorApp,
    });

    registry.registerApp({
        QString::fromLatin1(AppIds::Calculator),
        QStringLiteral("Calculator"),
        QStringLiteral("Arithmetic with a full expression parser"),
        QStringLiteral("Utilities"),
        {QStringLiteral("math"), QStringLiteral("numbers")},
        QSize(520, 560),
        QSize(420, 500),
        true,
        false,
        &createCalculatorApp,
    });

    registry.registerApp({
        QString::fromLatin1(AppIds::Calendar),
        QStringLiteral("Calendar"),
        QStringLiteral("Events, months and daily agenda"),
        QStringLiteral("Productivity"),
        {QStringLiteral("events"), QStringLiteral("schedule"), QStringLiteral("agenda")},
        QSize(960, 640),
        QSize(720, 460),
        true,
        false,
        &createCalendarApp,
    });

    registry.registerApp({
        QString::fromLatin1(AppIds::Editor),
        QStringLiteral("Text Editor"),
        QStringLiteral("Edit plain text files"),
        QStringLiteral("Productivity"),
        {QStringLiteral("text"), QStringLiteral("notes"), QStringLiteral("code")},
        QSize(920, 640),
        QSize(560, 380),
        false,
        false,
        &createEditorApp,
    });

    registry.registerApp({
        QString::fromLatin1(AppIds::TaskManager),
        QStringLiteral("Task Manager"),
        QStringLiteral("Inspect and control running processes"),
        QStringLiteral("System"),
        {QStringLiteral("processes"), QStringLiteral("cpu"), QStringLiteral("kill")},
        QSize(980, 620),
        QSize(720, 440),
        true,
        false,
        &createTaskManagerApp,
    });

    registry.registerApp({
        QString::fromLatin1(AppIds::Archive),
        QStringLiteral("Archive Manager"),
        QStringLiteral("Inspect and extract tar and zip archives"),
        QStringLiteral("Utilities"),
        {QStringLiteral("zip"), QStringLiteral("tar"), QStringLiteral("extract")},
        QSize(880, 580),
        QSize(600, 400),
        true,
        false,
        &createArchiveApp,
    });

    registry.registerApp({
        QString::fromLatin1(AppIds::About),
        QStringLiteral("About Aero"),
        QStringLiteral("Version, system and platform information"),
        QStringLiteral("System"),
        {QStringLiteral("version"), QStringLiteral("system"), QStringLiteral("credits")},
        QSize(620, 600),
        QSize(520, 520),
        true,
        false,
        &createAboutApp,
    });
}

} // namespace Aero::Applications
