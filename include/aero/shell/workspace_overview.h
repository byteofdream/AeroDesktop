#pragma once

#include "aero/shell/overlay.h"

#include <QList>

class QGridLayout;

namespace Aero::Compositor {
class WindowManager;
}
namespace Aero::Settings {
class SettingsManager;
}

namespace Aero::Shell {

/// Mission-Control style overview of the virtual workspaces.  Each card paints
/// the wallpaper and a miniature of the windows on that desktop.
class WorkspaceOverview : public Overlay {
    Q_OBJECT
public:
    WorkspaceOverview(Compositor::WindowManager* windowManager,
                      Settings::SettingsManager* settings,
                      QWidget* parent = nullptr);

    void refresh();

signals:
    void workspaceActivated(int index);

protected:
    void prepareShow() override;
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void build();

    Compositor::WindowManager* m_windowManager = nullptr;
    Settings::SettingsManager* m_settings = nullptr;
    QGridLayout* m_grid = nullptr;

    class WorkspaceCard;
    QList<WorkspaceCard*> m_cards;
};

} // namespace Aero::Shell
