#include "aero/shell/shortcut_manager.h"

#include "aero/settings/settings_manager.h"

#include <QApplication>
#include <QKeyEvent>
#include <QSet>
#include <QShortcut>
#include <QWidget>

namespace Aero::Shell {

ShortcutManager::ShortcutManager(Settings::SettingsManager* settings,
                                 QWidget* target,
                                 QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_target(target)
{
    if (QApplication::instance() != nullptr) {
        QApplication::instance()->installEventFilter(this);
    }
    if (m_settings != nullptr) {
        connect(m_settings, &Settings::SettingsManager::shortcutsChanged,
                this, &ShortcutManager::reload);
    }
    reload();
}

ShortcutManager::~ShortcutManager()
{
    if (QApplication::instance() != nullptr) {
        QApplication::instance()->removeEventFilter(this);
    }
    clearShortcuts();
}

void ShortcutManager::clearShortcuts()
{
    for (QShortcut* shortcut : m_shortcuts) {
        if (shortcut != nullptr) {
            shortcut->setEnabled(false);
            shortcut->deleteLater();
        }
    }
    m_shortcuts.clear();
}

void ShortcutManager::reload()
{
    clearShortcuts();
    if (m_settings == nullptr) {
        m_switchSequence = {};
        return;
    }

    m_switchSequence = m_settings->shortcut(QStringLiteral("window-switch"));
    const QHash<QString, QKeySequence> sequences = m_settings->shortcuts();
    QSet<QString> used;

    for (auto it = sequences.cbegin(); it != sequences.cend(); ++it) {
        const QString action = it.key();
        const QKeySequence sequence = it.value();
        if (sequence.isEmpty() || action == QStringLiteral("window-switch")) {
            continue;
        }

        const QString portable = sequence.toString(QKeySequence::PortableText);
        if (used.contains(portable)) {
            continue;
        }
        used.insert(portable);

        auto* shortcut = new QShortcut(m_target);
        shortcut->setKey(sequence);
        shortcut->setContext(Qt::ApplicationShortcut);
        connect(shortcut, &QShortcut::activated, this, [this, action]() {
            if (m_enabled) {
                emit actionTriggered(action);
            }
        });
        m_shortcuts.append(shortcut);
    }
}

void ShortcutManager::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    if (!enabled && m_switchActive) {
        m_switchActive = false;
        emit appSwitcherCancelled();
    }
    for (QShortcut* shortcut : m_shortcuts) {
        if (shortcut != nullptr) {
            shortcut->setEnabled(enabled);
        }
    }
}

bool ShortcutManager::isEnabled() const
{
    return m_enabled;
}

bool ShortcutManager::matchesSwitchShortcut(const QKeySequence& sequence) const
{
    return !m_switchSequence.isEmpty() && sequence == m_switchSequence;
}

bool ShortcutManager::eventFilter(QObject* watched, QEvent* event)
{
    Q_UNUSED(watched);
    if (!m_enabled || m_switchSequence.isEmpty()) {
        return QObject::eventFilter(watched, event);
    }

    if (event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->isAutoRepeat() && !m_switchActive) {
            return QObject::eventFilter(watched, event);
        }

        const bool altTab = keyEvent->key() == Qt::Key_Tab
            && keyEvent->modifiers().testFlag(Qt::AltModifier);
        const QKeySequence pressed(keyEvent->modifiers().toInt() | keyEvent->key());

        if (altTab || matchesSwitchShortcut(pressed)) {
            const bool reverse = keyEvent->modifiers().testFlag(Qt::ShiftModifier);
            if (!m_switchActive) {
                m_switchActive = true;
                emit appSwitcherStarted(reverse);
            } else {
                emit appSwitcherCycled(reverse);
            }
            keyEvent->accept();
            return true;
        }

        if (m_switchActive && keyEvent->key() == Qt::Key_Escape) {
            m_switchActive = false;
            emit appSwitcherCancelled();
            keyEvent->accept();
            return true;
        }
    } else if (event->type() == QEvent::KeyRelease) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (m_switchActive
            && (keyEvent->key() == Qt::Key_Alt
                || !keyEvent->modifiers().testFlag(Qt::AltModifier))) {
            m_switchActive = false;
            emit appSwitcherCommitted();
            keyEvent->accept();
            return true;
        }
    }

    return QObject::eventFilter(watched, event);
}

} // namespace Aero::Shell
