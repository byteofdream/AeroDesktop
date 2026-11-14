#pragma once

#include <QColor>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QString>

class QLayout;
class QVBoxLayout;
class QWidget;

namespace Aero::Applications::Widgets {

QLabel* titleLabel(const QString& text, QWidget* parent = nullptr);
QLabel* subtitleLabel(const QString& text, QWidget* parent = nullptr);
QLabel* mutedLabel(const QString& text, QWidget* parent = nullptr);
QLabel* sectionLabel(const QString& text, QWidget* parent = nullptr);
QLabel* valueLabel(const QString& text, QWidget* parent = nullptr);
QLabel* accentLabel(const QString& text, QWidget* parent = nullptr);

QFrame* separator(QWidget* parent = nullptr);
QWidget* pageHeader(const QString& title, const QString& subtitle, QWidget* parent = nullptr);
QLineEdit* searchField(const QString& placeholder, QWidget* parent = nullptr);
QWidget* emptyState(const QString& iconName,
                    const QString& title,
                    const QString& body,
                    QWidget* parent = nullptr);
QWidget* formRow(const QString& label,
                 QWidget* control,
                 const QString& description = {},
                 QWidget* parent = nullptr);

QLayout* clearLayout(QLayout* layout);
void clearWidgetChildren(QWidget* widget);

QString elidePath(const QString& path, int maxCharacters = 64);
QString fileKindLabel(const QString& path, bool isDirectory);

} // namespace Aero::Applications::Widgets
