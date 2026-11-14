#include "aero/apps/common/app_widgets.h"

#include "aero/core/utils.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/theme.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QPixmap>
#include <QVBoxLayout>

namespace Aero::Applications::Widgets {
namespace {

QLabel* labelWithRole(const QString& text, const char* role, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setProperty("role", QString::fromLatin1(role));
    label->setWordWrap(false);
    return label;
}

} // namespace

QLabel* titleLabel(const QString& text, QWidget* parent)
{
    auto* label = labelWithRole(text, "title", parent);
    label->setFont(Theme::ThemeManager::instance().bodyFont(QFont::DemiBold));
    return label;
}

QLabel* subtitleLabel(const QString& text, QWidget* parent)
{
    auto* label = labelWithRole(text, "subtitle", parent);
    label->setWordWrap(true);
    return label;
}

QLabel* mutedLabel(const QString& text, QWidget* parent)
{
    auto* label = labelWithRole(text, "muted", parent);
    label->setWordWrap(true);
    return label;
}

QLabel* sectionLabel(const QString& text, QWidget* parent)
{
    auto* label = labelWithRole(text.toUpper(), "section", parent);
    label->setFont(Theme::ThemeManager::instance().captionFont(QFont::DemiBold));
    return label;
}

QLabel* valueLabel(const QString& text, QWidget* parent)
{
    auto* label = labelWithRole(text, "value", parent);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return label;
}

QLabel* accentLabel(const QString& text, QWidget* parent)
{
    return labelWithRole(text, "accent", parent);
}

QFrame* separator(QWidget* parent)
{
    auto* line = new QFrame(parent);
    line->setProperty("role", QStringLiteral("separator"));
    line->setFrameShape(QFrame::NoFrame);
    line->setFixedHeight(1);
    line->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return line;
}

QWidget* pageHeader(const QString& title, const QString& subtitle, QWidget* parent)
{
    auto* header = new QWidget(parent);
    auto* layout = new QVBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);

    auto* titleLabelWidget = new QLabel(title, header);
    titleLabelWidget->setProperty("role", QStringLiteral("title"));
    titleLabelWidget->setFont(Theme::ThemeManager::instance().font(23.0, QFont::DemiBold));
    layout->addWidget(titleLabelWidget);

    if (!subtitle.isEmpty()) {
        auto* subtitleLabelWidget = mutedLabel(subtitle, header);
        subtitleLabelWidget->setFont(Theme::ThemeManager::instance().font(12.5));
        layout->addWidget(subtitleLabelWidget);
    }
    return header;
}

QLineEdit* searchField(const QString& placeholder, QWidget* parent)
{
    auto* edit = new QLineEdit(parent);
    edit->setPlaceholderText(placeholder);
    edit->setClearButtonEnabled(true);
    edit->setMinimumHeight(30);
    edit->addAction(Theme::IconFactory::icon(QStringLiteral("search"), 15,
                                             Theme::ThemeManager::instance().tokens().color.textMuted),
                    QLineEdit::LeadingPosition);
    return edit;
}

QWidget* emptyState(const QString& iconName,
                    const QString& title,
                    const QString& body,
                    QWidget* parent)
{
    auto* state = new QWidget(parent);
    auto* layout = new QVBoxLayout(state);
    layout->setContentsMargins(24, 36, 24, 36);
    layout->setSpacing(10);
    layout->setAlignment(Qt::AlignCenter);

    const Theme::ColorTokens& colors = Theme::ThemeManager::instance().tokens().color;
    auto* iconLabel = new QLabel(state);
    iconLabel->setPixmap(Theme::IconFactory::pixmap(iconName, 52, colors.textMuted, 2.0));
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);

    auto* titleWidget = new QLabel(title, state);
    titleWidget->setProperty("role", QStringLiteral("title"));
    titleWidget->setFont(Theme::ThemeManager::instance().font(16.0, QFont::DemiBold));
    titleWidget->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleWidget);

    auto* bodyWidget = mutedLabel(body, state);
    bodyWidget->setAlignment(Qt::AlignCenter);
    bodyWidget->setMaximumWidth(420);
    layout->addWidget(bodyWidget, 0, Qt::AlignHCenter);

    return state;
}

QWidget* formRow(const QString& label,
                 QWidget* control,
                 const QString& description,
                 QWidget* parent)
{
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 7, 0, 7);
    layout->setSpacing(18);

    auto* textColumn = new QVBoxLayout;
    textColumn->setContentsMargins(0, 0, 0, 0);
    textColumn->setSpacing(2);

    auto* name = new QLabel(label, row);
    name->setProperty("role", QStringLiteral("title"));
    name->setFont(Theme::ThemeManager::instance().bodyFont(QFont::Medium));
    textColumn->addWidget(name);

    if (!description.isEmpty()) {
        auto* hint = mutedLabel(description, row);
        hint->setFont(Theme::ThemeManager::instance().font(11.5));
        textColumn->addWidget(hint);
    }

    layout->addLayout(textColumn, 1);
    if (control != nullptr) {
        control->setParent(row);
        control->setMinimumWidth(qMin(320, control->sizeHint().width()));
        layout->addWidget(control, 0, Qt::AlignVCenter | Qt::AlignRight);
    }
    return row;
}

QLayout* clearLayout(QLayout* layout)
{
    if (layout == nullptr) {
        return nullptr;
    }
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        if (QLayout* child = item->layout()) {
            clearLayout(child);
            delete child;
        }
        delete item;
    }
    return layout;
}

void clearWidgetChildren(QWidget* widget)
{
    if (widget == nullptr) {
        return;
    }
    if (QLayout* layout = widget->layout()) {
        clearLayout(layout);
    }
}

QString elidePath(const QString& path, int maxCharacters)
{
    return Core::Utils::elideMiddle(path, maxCharacters);
}

QString fileKindLabel(const QString& path, bool isDirectory)
{
    if (isDirectory) {
        return QStringLiteral("Folder");
    }
    const QString suffix = path.section(QLatin1Char('.'), -1).toLower();
    if (suffix.isEmpty()) {
        return QStringLiteral("File");
    }
    if (suffix == QStringLiteral("png") || suffix == QStringLiteral("jpg")
        || suffix == QStringLiteral("jpeg") || suffix == QStringLiteral("webp")
        || suffix == QStringLiteral("svg")) {
        return QStringLiteral("Image");
    }
    if (suffix == QStringLiteral("txt") || suffix == QStringLiteral("md")
        || suffix == QStringLiteral("cpp") || suffix == QStringLiteral("h")
        || suffix == QStringLiteral("json") || suffix == QStringLiteral("ini")) {
        return QStringLiteral("Text document");
    }
    if (suffix == QStringLiteral("zip") || suffix == QStringLiteral("tar")
        || suffix == QStringLiteral("gz") || suffix == QStringLiteral("xz")
        || suffix == QStringLiteral("bz2")) {
        return QStringLiteral("Archive");
    }
    if (suffix == QStringLiteral("pdf")) {
        return QStringLiteral("PDF document");
    }
    if (suffix == QStringLiteral("mp3") || suffix == QStringLiteral("flac")
        || suffix == QStringLiteral("wav") || suffix == QStringLiteral("ogg")) {
        return QStringLiteral("Audio");
    }
    if (suffix == QStringLiteral("mp4") || suffix == QStringLiteral("mkv")
        || suffix == QStringLiteral("webm")) {
        return QStringLiteral("Video");
    }
    return suffix.toUpper() + QStringLiteral(" file");
}

} // namespace Aero::Applications::Widgets
