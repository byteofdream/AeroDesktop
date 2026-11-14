#include "aero/shell/overlay.h"

#include "aero/theme/theme.h"

#include <QGraphicsOpacityEffect>
#include <QHideEvent>
#include <QKeyEvent>
#include <QPropertyAnimation>
#include <QShowEvent>

namespace Aero::Shell {

Overlay::Overlay(QWidget* parent)
    : Graphics::GlassPanel(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_DeleteOnClose, false);
    hide();
}

Overlay::~Overlay() = default;

bool Overlay::isOpen() const
{
    return m_open;
}

bool Overlay::isDismissing() const
{
    return m_dismissing;
}

void Overlay::setSlideOffset(int pixels)
{
    m_slideOffset = qMax(0, pixels);
}

int Overlay::slideOffset() const
{
    return m_slideOffset;
}

void Overlay::present()
{
    if (m_open && isVisible()) {
        raise();
        finishShow();
        return;
    }

    stopAnimation();
    m_open = true;
    m_dismissing = false;
    prepareShow();

    if (m_opacity == nullptr) {
        m_opacity = new QGraphicsOpacityEffect(this);
        m_opacity->setOpacity(0.0);
        setGraphicsEffect(m_opacity);
    }

    m_opacity->setOpacity(0.0);
    show();
    raise();
    finishShow();

    const int duration = Theme::ThemeManager::instance().duration(170);
    if (duration <= 0) {
        m_opacity->setOpacity(1.0);
        return;
    }

    m_fade = new QPropertyAnimation(m_opacity, "opacity", this);
    m_fade->setDuration(duration);
    m_fade->setStartValue(0.0);
    m_fade->setEndValue(1.0);
    m_fade->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_fade, &QPropertyAnimation::finished, this, [this]() {
        if (m_opacity != nullptr) {
            m_opacity->setOpacity(1.0);
        }
    });
    m_fade->start();
}

void Overlay::dismiss()
{
    if (!isVisible()) {
        m_open = false;
        return;
    }
    if (m_dismissing) {
        return;
    }

    m_open = false;
    m_dismissing = true;
    emit closing();
    prepareHide();
    stopAnimation();

    const int duration = Theme::ThemeManager::instance().duration(130);
    if (duration <= 0 || m_opacity == nullptr) {
        finishDismiss();
        return;
    }

    m_fade = new QPropertyAnimation(m_opacity, "opacity", this);
    m_fade->setDuration(duration);
    m_fade->setStartValue(m_opacity->opacity());
    m_fade->setEndValue(0.0);
    m_fade->setEasingCurve(QEasingCurve::InCubic);
    connect(m_fade, &QPropertyAnimation::finished, this, &Overlay::finishDismiss);
    m_fade->start();
}

void Overlay::stopAnimation()
{
    if (m_fade != nullptr) {
        m_fade->stop();
        m_fade->deleteLater();
        m_fade = nullptr;
    }
}

void Overlay::finishDismiss()
{
    m_dismissing = false;
    hide();
    if (m_opacity != nullptr) {
        m_opacity->setOpacity(0.0);
    }
    emit dismissed();
}

void Overlay::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        event->accept();
        dismiss();
        return;
    }
    Graphics::GlassPanel::keyPressEvent(event);
}

void Overlay::showEvent(QShowEvent* event)
{
    Graphics::GlassPanel::showEvent(event);
}

void Overlay::hideEvent(QHideEvent* event)
{
    Graphics::GlassPanel::hideEvent(event);
    if (!m_dismissing && m_open) {
        m_open = false;
        stopAnimation();
        emit dismissed();
    }
}

void Overlay::prepareShow()
{
}

void Overlay::prepareHide()
{
}

void Overlay::finishShow()
{
    setFocus(Qt::PopupFocusReason);
}

} // namespace Aero::Shell
