#pragma once

#include "aero/shell/overlay.h"

#include <QList>

namespace Aero::Compositor {
class AeroWindow;
class WindowManager;
}

namespace Aero::Shell {

/// Alt+Tab window switcher.  The shortcut manager drives begin/cycle/commit so
/// holding Alt and tapping Tab walks the list; releasing Alt activates the
/// highlighted window.
class AppSwitcher : public Overlay {
    Q_OBJECT
public:
    explicit AppSwitcher(Compositor::WindowManager* windowManager, QWidget* parent = nullptr);

    void begin(int direction = 1);
    void cycle(int direction);
    void commit();
    void cancel();

    int selectedWindowId() const;

signals:
    void windowChosen(int windowId);
    void selectionCancelled();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void refreshWindows();
    void clampSelection();

    Compositor::WindowManager* m_windowManager = nullptr;
    QList<Compositor::AeroWindow*> m_windows;
    int m_selected = 0;
    bool m_committed = false;
};

} // namespace Aero::Shell
