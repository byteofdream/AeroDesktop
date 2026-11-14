#pragma once

#include "aero/apps/app_registry.h"

namespace Aero::Applications {

AppView* createFinderApp(const AppContext& context, QWidget* parent = nullptr);
AppView* createSettingsApp(const AppContext& context, QWidget* parent = nullptr);
AppView* createTerminalApp(const AppContext& context, QWidget* parent = nullptr);
AppView* createMonitorApp(const AppContext& context, QWidget* parent = nullptr);
AppView* createCalculatorApp(const AppContext& context, QWidget* parent = nullptr);
AppView* createCalendarApp(const AppContext& context, QWidget* parent = nullptr);
AppView* createEditorApp(const AppContext& context, QWidget* parent = nullptr);
AppView* createTaskManagerApp(const AppContext& context, QWidget* parent = nullptr);
AppView* createArchiveApp(const AppContext& context, QWidget* parent = nullptr);
AppView* createAboutApp(const AppContext& context, QWidget* parent = nullptr);

} // namespace Aero::Applications
