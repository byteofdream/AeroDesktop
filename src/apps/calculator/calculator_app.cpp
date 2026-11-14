#include "aero/apps/builtin_apps.h"

#include "aero/apps/common/app_widgets.h"
#include "aero/theme/controls.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/theme.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include <cmath>

namespace Aero::Applications {
namespace {

constexpr double kPi = 3.14159265358979323846;

class ExpressionParser {
public:
    explicit ExpressionParser(const QString& expression)
        : m_expression(expression)
    {
    }

    double evaluate(bool* ok)
    {
        m_position = 0;
        m_valid = true;
        m_error.clear();
        skipSpaces();
        if (m_expression.isEmpty()) {
            m_valid = false;
            m_error = QStringLiteral("Empty expression");
            if (ok != nullptr) {
                *ok = false;
            }
            return 0.0;
        }

        const double value = parseExpression();
        skipSpaces();
        if (m_position < m_expression.size()) {
            fail(QStringLiteral("Unexpected “%1”").arg(m_expression.mid(m_position, 1)));
        }
        if (!m_valid || std::isnan(value) || std::isinf(value)) {
            if (m_error.isEmpty()) {
                m_error = QStringLiteral("Invalid result");
            }
            if (ok != nullptr) {
                *ok = false;
            }
            return 0.0;
        }
        if (ok != nullptr) {
            *ok = true;
        }
        return value;
    }

    QString error() const { return m_error; }

private:
    void fail(const QString& message)
    {
        if (m_valid) {
            m_valid = false;
            m_error = message;
        }
    }

    void skipSpaces()
    {
        while (m_position < m_expression.size() && m_expression.at(m_position).isSpace()) {
            ++m_position;
        }
    }

    QChar peek() const
    {
        return m_position < m_expression.size() ? m_expression.at(m_position) : QChar();
    }

    bool consume(QChar character)
    {
        skipSpaces();
        if (peek() == character) {
            ++m_position;
            return true;
        }
        return false;
    }

    double parseExpression()
    {
        double value = parseTerm();
        while (m_valid) {
            skipSpaces();
            if (consume(QLatin1Char('+'))) {
                value += parseTerm();
            } else if (consume(QLatin1Char('-'))) {
                value -= parseTerm();
            } else {
                break;
            }
        }
        return value;
    }

    double parseTerm()
    {
        double value = parsePower();
        while (m_valid) {
            skipSpaces();
            if (consume(QLatin1Char('*'))) {
                value *= parsePower();
            } else if (consume(QLatin1Char('/'))) {
                const double divisor = parsePower();
                if (qFuzzyIsNull(divisor)) {
                    fail(QStringLiteral("Division by zero"));
                    return 0.0;
                }
                value /= divisor;
            } else {
                break;
            }
        }
        return value;
    }

    double parsePower()
    {
        double base = parseUnary();
        skipSpaces();
        if (consume(QLatin1Char('^'))) {
            const double exponentValue = parsePower();
            base = std::pow(base, exponentValue);
        }
        return base;
    }

    double parseUnary()
    {
        skipSpaces();
        if (consume(QLatin1Char('+'))) {
            return parseUnary();
        }
        if (consume(QLatin1Char('-'))) {
            return -parseUnary();
        }
        return parsePrimary();
    }

    double parsePrimary()
    {
        skipSpaces();
        if (!m_valid) {
            return 0.0;
        }

        if (consume(QLatin1Char('('))) {
            const double value = parseExpression();
            if (!consume(QLatin1Char(')'))) {
                fail(QStringLiteral("Missing closing parenthesis"));
            }
            return value;
        }

        if (peek().isLetter()) {
            QString identifier;
            while (m_position < m_expression.size()
                   && (m_expression.at(m_position).isLetterOrNumber())) {
                identifier.append(m_expression.at(m_position));
                ++m_position;
            }
            const QString lower = identifier.toLower();
            if (lower == QStringLiteral("pi")) {
                return kPi;
            }
            if (lower == QStringLiteral("e")) {
                return std::exp(1.0);
            }

            if (!consume(QLatin1Char('('))) {
                fail(QStringLiteral("Expected ( after %1").arg(identifier));
                return 0.0;
            }
            const double argument = parseExpression();
            if (!consume(QLatin1Char(')'))) {
                fail(QStringLiteral("Missing closing parenthesis"));
                return 0.0;
            }

            if (lower == QStringLiteral("sqrt")) {
                if (argument < 0.0) {
                    fail(QStringLiteral("Square root of a negative number"));
                    return 0.0;
                }
                return std::sqrt(argument);
            }
            if (lower == QStringLiteral("sin")) {
                return std::sin(argument);
            }
            if (lower == QStringLiteral("cos")) {
                return std::cos(argument);
            }
            if (lower == QStringLiteral("tan")) {
                return std::tan(argument);
            }
            if (lower == QStringLiteral("ln")) {
                if (argument <= 0.0) {
                    fail(QStringLiteral("Logarithm requires a positive value"));
                    return 0.0;
                }
                return std::log(argument);
            }
            if (lower == QStringLiteral("log")) {
                if (argument <= 0.0) {
                    fail(QStringLiteral("Logarithm requires a positive value"));
                    return 0.0;
                }
                return std::log10(argument);
            }
            if (lower == QStringLiteral("abs")) {
                return std::abs(argument);
            }
            fail(QStringLiteral("Unknown function %1").arg(identifier));
            return 0.0;
        }

        const int start = m_position;
        bool hasDigit = false;
        while (m_position < m_expression.size()) {
            const QChar character = m_expression.at(m_position);
            if (character.isDigit()) {
                hasDigit = true;
                ++m_position;
            } else if (character == QLatin1Char('.')) {
                ++m_position;
            } else {
                break;
            }
        }
        if (!hasDigit) {
            fail(QStringLiteral("Expected a number"));
            return 0.0;
        }
        bool ok = false;
        const double value = m_expression.mid(start, m_position - start).toDouble(&ok);
        if (!ok) {
            fail(QStringLiteral("Malformed number"));
            return 0.0;
        }
        if (consume(QLatin1Char('%'))) {
            return value / 100.0;
        }
        return value;
    }

    QString m_expression;
    int m_position = 0;
    bool m_valid = true;
    QString m_error;
};

class CalculatorView final : public AppView {
public:
    explicit CalculatorView(const AppContext& context, QWidget* parent = nullptr)
        : AppView(context, parent)
    {
        buildUi();
        setFocusPolicy(Qt::StrongFocus);
    }

    QString windowTitle() const override { return QStringLiteral("Calculator"); }
    QString subtitle() const override { return QStringLiteral("Expression calculator"); }

private:
    void buildUi()
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(18, 16, 18, 18);
        root->setSpacing(12);

        auto* header = new QHBoxLayout;
        header->setContentsMargins(0, 0, 0, 0);
        auto* title = Widgets::titleLabel(QStringLiteral("Calculator"), this);
        title->setFont(Theme::ThemeManager::instance().font(20.0, QFont::DemiBold));
        header->addWidget(title);
        header->addStretch(1);
        m_historyButton = new Theme::IconButton(QStringLiteral("clock"), this);
        m_historyButton->setToolTip(QStringLiteral("Show History"));
        m_historyButton->setCheckable(true);
        m_historyButton->setAccentWhenChecked(true);
        header->addWidget(m_historyButton);
        root->addLayout(header);

        auto* displayCard = new Theme::Card(this);
        displayCard->setRadius(18);
        auto* displayLayout = new QVBoxLayout(displayCard);
        displayLayout->setContentsMargins(20, 16, 20, 16);
        displayLayout->setSpacing(4);

        m_expression = new QLabel(displayCard);
        m_expression->setProperty("role", QStringLiteral("muted"));
        m_expression->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_expression->setFont(Theme::ThemeManager::instance().monospaceFont(13.0));
        m_expression->setMinimumHeight(22);
        m_expression->setText(QString());
        displayLayout->addWidget(m_expression);

        m_display = new QLabel(QStringLiteral("0"), displayCard);
        m_display->setProperty("role", QStringLiteral("title"));
        m_display->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_display->setFont(Theme::ThemeManager::instance().font(40.0, QFont::DemiBold));
        m_display->setMinimumHeight(58);
        m_display->setTextInteractionFlags(Qt::TextSelectableByMouse);
        displayLayout->addWidget(m_display);

        root->addWidget(displayCard);

        auto* keypad = new QWidget(this);
        auto* grid = new QGridLayout(keypad);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(8);

        struct Key {
            const char* label;
            const char* token;
            const char* variant;
        };
        const Key keys[] = {
            {"AC", "AC", "function"}, {"(", "(", "function"}, {")", ")", "function"}, {"÷", "/", "operator"},
            {"7", "7", "digit"}, {"8", "8", "digit"}, {"9", "9", "digit"}, {"×", "*", "operator"},
            {"4", "4", "digit"}, {"5", "5", "digit"}, {"6", "6", "digit"}, {"−", "-", "operator"},
            {"1", "1", "digit"}, {"2", "2", "digit"}, {"3", "3", "digit"}, {"+", "+", "operator"},
            {"±", "NEG", "function"}, {"0", "0", "digit"}, {".", ".", "digit"}, {"=", "=", "equals"},
        };

        int row = 0;
        int column = 0;
        for (const Key& key : keys) {
            auto* button = new QPushButton(QString::fromUtf8(key.label), keypad);
            button->setProperty("variant", QString::fromLatin1(key.variant));
            button->setMinimumHeight(48);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            button->setFocusPolicy(Qt::NoFocus);
            button->setFont(Theme::ThemeManager::instance().font(16.0, QFont::DemiBold));
            const QString token = QString::fromLatin1(key.token);
            connect(button, &QPushButton::clicked, this, [this, token]() { handleKey(token); });
            grid->addWidget(button, row, column);
            ++column;
            if (column == 4) {
                column = 0;
                ++row;
            }
        }
        // Backspace shares the last row by replacing the percent slot in the
        // function row: rebuild the bottom row with a dedicated erase key.
        auto* erase = new QPushButton(QStringLiteral("⌫"), keypad);
        erase->setProperty("variant", QStringLiteral("function"));
        erase->setMinimumHeight(42);
        erase->setFocusPolicy(Qt::NoFocus);
        erase->setFont(Theme::ThemeManager::instance().font(15.0, QFont::DemiBold));
        connect(erase, &QPushButton::clicked, this, [this]() {
            if (!m_input.isEmpty()) {
                m_input.chop(1);
                if (m_input.isEmpty()) {
                    m_input = QStringLiteral("0");
                }
                updateDisplay();
            }
        });
        grid->addWidget(erase, row, 0, 1, 4);

        root->addWidget(keypad, 1);

        m_history = new QListWidget(this);
        m_history->setFixedHeight(116);
        m_history->setVisible(false);
        m_history->setFrameShape(QFrame::NoFrame);
        m_history->setFont(Theme::ThemeManager::instance().monospaceFont(12.0));
        root->addWidget(m_history);

        connect(m_historyButton, &QAbstractButton::toggled, this, [this](bool shown) {
            m_history->setVisible(shown);
        });
        connect(m_history, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
            if (item == nullptr) {
                return;
            }
            m_input = item->data(Qt::UserRole).toString();
            m_justEvaluated = true;
            updateDisplay();
        });

        updateDisplay();
    }

    void handleKey(const QString& token)
    {
        if (token == QStringLiteral("AC")) {
            m_input = QStringLiteral("0");
            m_lastExpression.clear();
            m_justEvaluated = false;
            m_error.clear();
            updateDisplay();
            return;
        }
        if (token == QStringLiteral("=")) {
            evaluate();
            return;
        }
        if (token == QStringLiteral("NEG")) {
            if (m_input.startsWith(QLatin1Char('-'))) {
                m_input.remove(0, 1);
            } else if (m_input != QStringLiteral("0")) {
                m_input.prepend(QLatin1Char('-'));
            }
            updateDisplay();
            return;
        }

        const bool isOperator = token == QStringLiteral("+") || token == QStringLiteral("-")
            || token == QStringLiteral("*") || token == QStringLiteral("/");
        const bool isDigit = token.size() == 1 && token.at(0).isDigit();

        if (m_justEvaluated) {
            if (isDigit || token == QStringLiteral("(") || token == QStringLiteral(".")) {
                m_input = QStringLiteral("0");
            }
            m_justEvaluated = false;
            m_lastExpression.clear();
        }

        if (isDigit || token == QStringLiteral(".")) {
            if (m_input == QStringLiteral("0") && token != QStringLiteral(".")) {
                m_input = token;
            } else {
                m_input.append(token);
            }
        } else if (isOperator) {
            if (m_input.endsWith(QLatin1Char('+')) || m_input.endsWith(QLatin1Char('-'))
                || m_input.endsWith(QLatin1Char('*')) || m_input.endsWith(QLatin1Char('/'))) {
                m_input.chop(1);
            }
            m_input.append(token);
        } else {
            m_input.append(token);
        }
        m_error.clear();
        updateDisplay();
    }

    void evaluate()
    {
        bool ok = false;
        ExpressionParser parser(m_input);
        const double value = parser.evaluate(&ok);
        if (!ok) {
            m_error = parser.error();
            m_expression->setText(m_error);
            m_display->setText(QStringLiteral("Error"));
            return;
        }

        const QString result = QLocale::c().toString(value, 'g', 12);
        const QString expression = m_input;
        m_lastExpression = expression + QStringLiteral(" =");
        m_input = result;
        m_justEvaluated = true;

        if (m_history != nullptr) {
            auto* item = new QListWidgetItem(QStringLiteral("%1  →  %2").arg(expression, result));
            item->setData(Qt::UserRole, result);
            item->setToolTip(QStringLiteral("Double-click to reuse the result"));
            m_history->addItem(item);
            while (m_history->count() > 80) {
                delete m_history->takeItem(0);
            }
            m_history->scrollToBottom();
        }
        updateDisplay();
    }

    void updateDisplay()
    {
        m_display->setText(m_input.isEmpty() ? QStringLiteral("0") : m_input);
        if (!m_error.isEmpty()) {
            m_expression->setText(m_error);
            m_expression->setProperty("role", QStringLiteral("danger"));
        } else {
            m_expression->setText(m_lastExpression);
            m_expression->setProperty("role", QStringLiteral("muted"));
        }
        m_expression->style()->unpolish(m_expression);
        m_expression->style()->polish(m_expression);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        const QString text = event->text();
        if (event->key() == Qt::Key_Escape) {
            handleKey(QStringLiteral("AC"));
            return;
        }
        if (event->key() == Qt::Key_Backspace) {
            if (!m_input.isEmpty()) {
                m_input.chop(1);
                if (m_input.isEmpty()) {
                    m_input = QStringLiteral("0");
                }
                updateDisplay();
            }
            return;
        }
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            evaluate();
            return;
        }
        if (text == QStringLiteral("+") || text == QStringLiteral("-") || text == QStringLiteral("*")
            || text == QStringLiteral("/") || text == QStringLiteral("^")
            || text == QStringLiteral("(") || text == QStringLiteral(")")) {
            handleKey(text);
            return;
        }
        if (text.size() == 1 && (text.at(0).isDigit() || text.at(0) == QLatin1Char('.'))) {
            handleKey(text);
            return;
        }
        if (text.compare(QStringLiteral("x"), Qt::CaseInsensitive) == 0) {
            handleKey(QStringLiteral("*"));
            return;
        }
        AppView::keyPressEvent(event);
    }

    QLabel* m_expression = nullptr;
    QLabel* m_display = nullptr;
    Theme::IconButton* m_historyButton = nullptr;
    QListWidget* m_history = nullptr;
    QString m_input = QStringLiteral("0");
    QString m_lastExpression;
    QString m_error;
    bool m_justEvaluated = false;
};

} // namespace

AppView* createCalculatorApp(const AppContext& context, QWidget* parent)
{
    return new CalculatorView(context, parent);
}

} // namespace Aero::Applications
