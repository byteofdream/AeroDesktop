#pragma once

#include "aero/graphics/glass_panel.h"

#include <QPointer>

class QGraphicsOpacityEffect;
class QPropertyAnimation;

namespace Aero::Shell {

/// Base class for shell overlays such as Spotlight, Control Center and the
/// notification sidebar.
///
/// Overlays are children of the desktop window rather than separate top-level
/// windows: that keeps focus handling deterministic inside the shell and lets
/// every overlay sample the same blurred backdrop.  The class owns a small
/// fade/slide entrance animation and closes on Escape.
class Overlay : public Graphics::GlassPanel {
    Q_OBJECT
public:
    explicit Overlay(QWidget* parent = nullptr);
    ~Overlay() override;

    virtual void present();
    virtual void dismiss();

    bool isOpen() const;
    bool isDismissing() const;

signals:
    /// Emitted after the close animation has finished (or immediately when
    /// motion is disabled), and once when a visible overlay is hidden directly.
    void dismissed();

    /// Emitted as soon as dismissal starts, before the fade completes.
    void closing();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

    /// Called once after the overlay becomes visible, before the animation
    /// starts.  Subclasses refresh their content here.
    virtual void prepareShow();

    /// Called when the overlay is about to disappear.
    virtual void prepareHide();

    /// Called after the overlay has become visible.  Default implementation
    /// gives the overlay keyboard focus so Escape is handled.
    virtual void finishShow();

    void setSlideOffset(int pixels);
    int slideOffset() const;

private:
    void finishDismiss();
    void stopAnimation();

    QPointer<QGraphicsOpacityEffect> m_opacity;
    QPointer<QPropertyAnimation> m_fade;
    int m_slideOffset = 10;
    bool m_open = false;
    bool m_dismissing = false;
};

} // namespace Aero::Shell
