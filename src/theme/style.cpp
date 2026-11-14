#include "aero/theme/style.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

namespace Aero::Theme {

QString Style::cssColor(const QColor& color)
{
    return QStringLiteral("rgba(%1,%2,%3,%4%)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(qRound(color.alphaF() * 100.0));
}

void Style::install(QApplication& application)
{
    const ThemeTokens& tokens = ThemeManager::instance().tokens();
    const ColorTokens& c = tokens.color;

    if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
        QApplication::setStyle(fusion);
    }

    QPalette palette;
    palette.setColor(QPalette::Window, c.windowBackground);
    palette.setColor(QPalette::WindowText, c.textPrimary);
    palette.setColor(QPalette::Base, c.surfaceSunken);
    palette.setColor(QPalette::AlternateBase, c.surface);
    palette.setColor(QPalette::ToolTipBase, c.surfaceRaised);
    palette.setColor(QPalette::ToolTipText, c.textPrimary);
    palette.setColor(QPalette::Text, c.textPrimary);
    palette.setColor(QPalette::Button, c.surface);
    palette.setColor(QPalette::ButtonText, c.textPrimary);
    palette.setColor(QPalette::BrightText, c.danger);
    palette.setColor(QPalette::Link, c.accent);
    palette.setColor(QPalette::Highlight, c.accent);
    palette.setColor(QPalette::HighlightedText, c.onAccent);
    palette.setColor(QPalette::PlaceholderText, c.textMuted);
    palette.setColor(QPalette::Disabled, QPalette::Text, c.textMuted);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, c.textMuted);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, c.textMuted);
    QApplication::setPalette(palette);
    QApplication::setFont(ThemeManager::instance().bodyFont());

    application.setStyleSheet(styleSheet(tokens));
}

QString Style::styleSheet(const ThemeTokens& tokens)
{
    const ColorTokens& c = tokens.color;

    QString sheet = QStringLiteral(R"QSS(
QToolTip {
    background-color: @surface;
    color: @text;
    border: 1px solid @border;
    border-radius: @radiusSmall;
    padding: 5px 8px;
}
QMenu {
    background-color: @surface;
    color: @text;
    border: 1px solid @border;
    border-radius: @radiusMedium;
    padding: 6px;
}
QMenu::item {
    padding: 6px 22px 6px 12px;
    border-radius: @radiusSmall;
}
QMenu::item:selected {
    background-color: @hover;
    color: @text;
}
QMenu::separator {
    height: 1px;
    background: @separator;
    margin: 5px 8px;
}
QMenu::icon {
    padding-left: 8px;
}
QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 2px;
}
QScrollBar::handle:vertical {
    background: @muted;
    min-height: 28px;
    border-radius: 3px;
}
QScrollBar::handle:vertical:hover {
    background: @text;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0px;
}
QScrollBar:horizontal {
    background: transparent;
    height: 10px;
    margin: 2px;
}
QScrollBar::handle:horizontal {
    background: @muted;
    min-width: 28px;
    border-radius: 3px;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0px;
}
QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox, QDoubleSpinBox {
    background-color: @base;
    color: @text;
    border: 1px solid @border;
    border-radius: @radiusSmall;
    padding: 5px 8px;
    selection-background-color: @accent;
    selection-color: @onAccent;
}
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus {
    border: 1px solid @accent;
}
QAbstractItemView {
    background-color: transparent;
    color: @text;
    border: none;
    outline: none;
    selection-background-color: @hover;
    selection-color: @text;
}
QHeaderView::section {
    background: transparent;
    color: @muted;
    border: none;
    border-bottom: 1px solid @separator;
    padding: 6px;
}
QPushButton {
    background-color: @surface;
    color: @text;
    border: 1px solid @border;
    border-radius: @radiusSmall;
    padding: 6px 14px;
}
QPushButton:hover {
    background-color: @hover;
}
QPushButton:pressed {
    background-color: @accent;
}
QPushButton:default {
    background-color: @accent;
    color: @onAccent;
    border: none;
}
QPushButton[variant="function"] {
    background-color: transparent;
    color: @muted;
    border: none;
}
QPushButton[variant="function"]:hover {
    background-color: @hover;
    color: @text;
}
QPushButton[variant="operator"] {
    background-color: @hover;
    color: @accent;
    border: none;
}
QPushButton[variant="operator"]:hover {
    background-color: @accent;
    color: @onAccent;
}
QPushButton[variant="equals"] {
    background-color: @accent;
    color: @onAccent;
    border: none;
}
QPushButton[variant="equals"]:hover {
    background-color: @accent;
    color: @onAccent;
}
QPushButton[variant="digit"] {
    background-color: @surface;
    color: @text;
    border: 1px solid @border;
}
QCheckBox, QRadioButton {
    color: @text;
    spacing: 8px;
}
QComboBox {
    background-color: @base;
    color: @text;
    border: 1px solid @border;
    border-radius: @radiusSmall;
    padding: 5px 10px;
}
QComboBox QAbstractItemView {
    background-color: @surface;
    border: 1px solid @border;
    border-radius: @radiusSmall;
    padding: 4px;
}
QSlider::groove:horizontal {
    height: 4px;
    background: @separator;
    border-radius: 2px;
}
QSlider::handle:horizontal {
    width: 16px;
    height: 16px;
    margin: -6px 0;
    border-radius: 8px;
    background: @accent;
}
QTabWidget::pane {
    border: none;
}
QTabBar::tab {
    background: transparent;
    color: @muted;
    padding: 7px 14px;
    border-radius: @radiusSmall;
}
QTabBar::tab:selected {
    color: @text;
    background: @hover;
}
QSplitter::handle {
    background: transparent;
}
QToolButton {
    background: transparent;
    border: none;
    border-radius: @radiusSmall;
    padding: 4px;
}
QToolButton:hover {
    background: @hover;
}
QLabel[role="title"] {
    color: @text;
    background: transparent;
}
QLabel[role="subtitle"], QLabel[role="value"] {
    color: @textSecondary;
    background: transparent;
}
QLabel[role="muted"], QLabel[role="section"] {
    color: @muted;
    background: transparent;
}
QLabel[role="accent"] {
    color: @accent;
    background: transparent;
}
QLabel[role="danger"] {
    color: @danger;
    background: transparent;
}
QFrame[role="separator"] {
    background-color: @separator;
    border: none;
    max-height: 1px;
}
QTableWidget, QTreeWidget, QListWidget {
    background: transparent;
    alternate-background-color: transparent;
    border: none;
    outline: none;
}
QTableWidget::item, QTreeWidget::item {
    padding: 5px 6px;
    border-bottom: 1px solid @separator;
}
QTableWidget::item:selected, QTreeWidget::item:selected, QListWidget::item:selected {
    background: @hover;
    color: @text;
}
QHeaderView::section {
    background: transparent;
    color: @muted;
    border: none;
    border-bottom: 1px solid @separator;
    padding: 7px 6px;
    font-weight: 600;
}
QTextEdit, QPlainTextEdit {
    background-color: @base;
}
)QSS");

    sheet.replace(QStringLiteral("@surface"), cssColor(c.surfaceRaised));
    sheet.replace(QStringLiteral("@hover"), cssColor(c.accentSoft));
    sheet.replace(QStringLiteral("@border"), cssColor(c.glassBorder));
    sheet.replace(QStringLiteral("@text"), cssColor(c.textPrimary));
    sheet.replace(QStringLiteral("@muted"), cssColor(c.textMuted));
    sheet.replace(QStringLiteral("@accent"), cssColor(c.accent));
    sheet.replace(QStringLiteral("@onAccent"), cssColor(c.onAccent));
    sheet.replace(QStringLiteral("@danger"), cssColor(c.danger));
    sheet.replace(QStringLiteral("@base"), cssColor(c.surfaceSunken));
    sheet.replace(QStringLiteral("@separator"), cssColor(c.separator));
    sheet.replace(QStringLiteral("@radiusSmall"), QString::number(tokens.radiusSmall) + QStringLiteral("px"));
    sheet.replace(QStringLiteral("@radiusMedium"), QString::number(tokens.radiusMedium) + QStringLiteral("px"));
    return sheet;
}

} // namespace Aero::Theme
