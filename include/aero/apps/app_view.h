#pragma once

#include "aero/apps/app_context.h"

#include <QWidget>

namespace Aero::Applications {

/// Base class for every Aero application surface.
///
/// A view is a plain widget hosted by a compositor window.  It receives the
/// shared application context and reports title changes so the window chrome
/// stays in sync.  `canClose()` lets editors veto a close after prompting.
class AppView : public QWidget {
    Q_OBJECT
public:
    explicit AppView(const AppContext& context, QWidget* parent = nullptr);
    ~AppView() override;

    const AppContext& context() const;

    virtual QString windowTitle() const;
    virtual QString subtitle() const;

    /// Returns false when the view handled the close request itself (for
    /// example by showing a save prompt) and does not want the window closed.
    virtual bool canClose();

    /// Called when the containing window is activated.  Views may refresh
    /// expensive state here instead of polling.
    virtual void activate();

    /// Optional launch argument such as a file path or directory.
    virtual void setLaunchArgument(const QString& argument);
    QString launchArgument() const;

signals:
    void titleChanged(const QString& title);
    void subtitleChanged(const QString& subtitle);
    void closeRequested();
    void statusMessage(const QString& message);

protected:
    void setWindowTitleText(const QString& title);
    void setSubtitleText(const QString& subtitle);

private:
    AppContext m_context;
    QString m_launchArgument;
    QString m_title;
    QString m_subtitle;
};

} // namespace Aero::Applications
