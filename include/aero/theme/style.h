#pragma once

#include "aero/theme/theme.h"

class QApplication;

namespace Aero::Theme {

/// Installs the shared application palette, font and minimal control styling.
/// Custom widgets paint themselves; this exists so standard dialogs, menus,
/// tooltips and text fields feel native to Aero rather than to a generic Qt
/// application.
class Style {
public:
    static void install(QApplication& application);
    static QString styleSheet(const ThemeTokens& tokens);
    static QString cssColor(const QColor& color);
};

} // namespace Aero::Theme
