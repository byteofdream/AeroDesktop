#pragma once

#include <QKeySequence>
#include <QList>
#include <QObject>

class QShortcut;
class QWidget;

namespace Aero::Settings {
class SettingsManager;
}

namespace Aero::Shell {

/// Translates persisted shortcuts into Qt shortcuts and implements the
/// press-and-hold Alt+Tab gesture that QShortcut cannot express.
class ShortcutManager : public QObject {
    Q_OBJECT
public:
    ShortcutManager(Settings::SettingsManager* settings,
                    QWidget* target,
                    QObject* parent = nullptr);
    ~ShortcutManager() override;

    void reload();
    void setEnabled(bool enabled);
    bool isEnabled() const;

signals:
    void actionTriggered(const QString& action);
    void appSwitcherStarted(bool reverse);
    void appSwitcherCycled(bool reverse);
    void appSwitcherCommitted();
    void appSwitcherCancelled();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void clearShortcuts();
    bool matchesSwitchShortcut(const QKeySequence& sequence) const;

    Settings::SettingsManager* m_settings = nullptr;
    QWidget* m_target = nullptr;
    QList<QShortcut*> m_shortcuts;
    QKeySequence m_switchSequence;
    bool m_enabled = true;
    bool m_switchActive = false;
};

} // namespace Aero::Shell
