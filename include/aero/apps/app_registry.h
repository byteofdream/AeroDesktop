#pragma once

#include "aero/apps/app_context.h"
#include "aero/apps/app_view.h"

#include <QList>
#include <QSize>
#include <QString>
#include <QStringList>

#include <functional>

namespace Aero::Applications {

struct AppDefinition {
    QString id;
    QString name;
    QString description;
    QString category;
    QStringList keywords;
    QSize defaultSize {960, 640};
    QSize minimumSize {560, 380};
    bool singleInstance = true;
    bool startMaximized = false;
    std::function<AppView*(const AppContext&, QWidget*)> factory;
};

/// Registry of installed applications.  The shell, Spotlight and the dock all
/// query the same catalog so metadata never drifts.
class AppRegistry {
public:
    static AppRegistry& instance();

    bool registerApp(const AppDefinition& definition);
    bool contains(const QString& appId) const;
    const AppDefinition* find(const QString& appId) const;

    QList<AppDefinition> apps() const;
    QStringList ids() const;

    AppView* createView(const QString& appId, const AppContext& context, QWidget* parent = nullptr) const;

    /// Registers the ten applications shipped with Aero Desktop.  Safe to call
    /// more than once.
    static void registerBuiltInApplications();

private:
    AppRegistry() = default;

    QList<AppDefinition> m_apps;
};

namespace AppIds {
inline constexpr auto Finder = "aero-finder";
inline constexpr auto Settings = "aero-settings";
inline constexpr auto Terminal = "aero-terminal";
inline constexpr auto Monitor = "aero-monitor";
inline constexpr auto Calculator = "aero-calculator";
inline constexpr auto Calendar = "aero-calendar";
inline constexpr auto Editor = "aero-editor";
inline constexpr auto TaskManager = "aero-tasks";
inline constexpr auto Archive = "aero-archive";
inline constexpr auto About = "aero-about";
} // namespace AppIds

} // namespace Aero::Applications
