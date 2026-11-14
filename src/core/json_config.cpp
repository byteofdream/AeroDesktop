#include "aero/core/json_config.h"

#include "aero/core/logging.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include <vector>

namespace Aero::Core {
namespace {
constexpr int kSaveDebounceMs = 350;
}

JsonConfig::JsonConfig(QString filePath, QObject* parent)
    : QObject(parent)
    , m_filePath(std::move(filePath))
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(kSaveDebounceMs);
    connect(&m_saveTimer, &QTimer::timeout, this, &JsonConfig::persistNow);
}

JsonConfig::~JsonConfig()
{
    if (m_dirty) {
        save();
    }
}

QString JsonConfig::filePath() const
{
    return m_filePath;
}

bool JsonConfig::load(QString* error)
{
    QFile file(m_filePath);
    if (!file.exists()) {
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = file.errorString();
        }
        return false;
    }

    QJsonParseError parseError {};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error != nullptr) {
            *error = QStringLiteral("%1 at offset %2")
                         .arg(parseError.errorString())
                         .arg(parseError.offset);
        }
        qCWarning(aeroLog) << "Invalid JSON in" << m_filePath << ":" << parseError.errorString();
        return false;
    }

    m_loading = true;
    if (document.isObject()) {
        m_root = document.object();
    } else {
        m_root = {};
    }
    m_loading = false;
    m_dirty = false;
    emit reloaded();
    return true;
}

bool JsonConfig::save(QString* error)
{
    (void)error;
    m_saveTimer.stop();
    if (!m_dirty) {
        return true;
    }
    persistNow();
    return !m_dirty;
}

void JsonConfig::persistNow()
{
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString message = file.errorString();
        qCWarning(aeroLog) << "Unable to write settings" << m_filePath << ":" << message;
        emit saveFailed(message);
        return;
    }

    file.write(QJsonDocument(m_root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        const QString message = file.errorString();
        qCWarning(aeroLog) << "Unable to commit settings" << m_filePath << ":" << message;
        emit saveFailed(message);
        return;
    }

    m_dirty = false;
}

QStringList JsonConfig::splitKey(const QString& key)
{
    QStringList parts = key.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        parts.append(key);
    }
    return parts;
}

QJsonValue JsonConfig::readValue(const QStringList& parts) const
{
    if (parts.isEmpty()) {
        return {};
    }

    QJsonValue current = m_root.value(parts.first());
    for (int i = 1; i < parts.size(); ++i) {
        if (!current.isObject()) {
            return {};
        }
        current = current.toObject().value(parts.at(i));
    }
    return current;
}

bool JsonConfig::writeValue(const QStringList& parts, const QJsonValue& value, bool remove)
{
    if (parts.isEmpty()) {
        return false;
    }

    // Walk / create the object chain.  Because QJsonObject is implicitly
    // shared, assigning a child object back into its parent is required at
    // every level.
    std::vector<QJsonObject> parents;
    parents.reserve(static_cast<size_t>(parts.size()));
    parents.push_back(m_root);

    for (int i = 0; i < parts.size() - 1; ++i) {
        QJsonObject child = parents.back().value(parts.at(i)).toObject();
        parents.push_back(child);
    }

    const QString leaf = parts.last();
    QJsonObject& deepest = parents.back();
    if (remove) {
        deepest.remove(leaf);
    } else {
        deepest.insert(leaf, value);
    }

    for (int i = parts.size() - 2; i >= 0; --i) {
        QJsonObject parent = parents.at(static_cast<size_t>(i));
        parent.insert(parts.at(i), parents.at(static_cast<size_t>(i + 1)));
        parents[static_cast<size_t>(i)] = parent;
    }

    m_root = parents.front();
    return true;
}

QVariant JsonConfig::value(const QString& key, const QVariant& fallback) const
{
    const QJsonValue json = readValue(splitKey(key));
    return json.isUndefined() ? fallback : json.toVariant();
}

QString JsonConfig::stringValue(const QString& key, const QString& fallback) const
{
    return value(key, fallback).toString();
}

int JsonConfig::intValue(const QString& key, int fallback) const
{
    bool ok = false;
    const int result = value(key, fallback).toInt(&ok);
    return ok ? result : fallback;
}

bool JsonConfig::boolValue(const QString& key, bool fallback) const
{
    const QVariant variant = value(key, fallback);
    if (variant.typeId() == QMetaType::Bool) {
        return variant.toBool();
    }
    const QString text = variant.toString().toLower();
    if (text == QStringLiteral("true") || text == QStringLiteral("1")) {
        return true;
    }
    if (text == QStringLiteral("false") || text == QStringLiteral("0")) {
        return false;
    }
    return fallback;
}

double JsonConfig::doubleValue(const QString& key, double fallback) const
{
    bool ok = false;
    const double result = value(key, fallback).toDouble(&ok);
    return ok ? result : fallback;
}

QStringList JsonConfig::stringListValue(const QString& key, const QStringList& fallback) const
{
    const QVariant variant = value(key);
    if (!variant.isValid()) {
        return fallback;
    }
    const QStringList list = variant.toStringList();
    return list.isEmpty() ? fallback : list;
}

void JsonConfig::setValue(const QString& key, const QVariant& value, bool persistImmediately)
{
    const QStringList parts = splitKey(key);
    const QJsonValue existing = readValue(parts);
    const QJsonValue json = QJsonValue::fromVariant(value);

    if (existing == json) {
        return;
    }

    writeValue(parts, json, false);
    m_dirty = true;

    if (!m_loading) {
        emit changed(key, value);
    }

    if (persistImmediately) {
        save();
    } else {
        m_saveTimer.start();
    }
}

void JsonConfig::remove(const QString& key, bool persistImmediately)
{
    if (!contains(key)) {
        return;
    }

    writeValue(splitKey(key), {}, true);
    m_dirty = true;
    emit changed(key, {});

    if (persistImmediately) {
        save();
    } else {
        m_saveTimer.start();
    }
}

bool JsonConfig::contains(const QString& key) const
{
    return !readValue(splitKey(key)).isUndefined();
}

QJsonObject JsonConfig::root() const
{
    return m_root;
}

void JsonConfig::replaceRoot(const QJsonObject& object)
{
    m_root = object;
    m_dirty = true;
    m_saveTimer.start();
    emit reloaded();
}

void JsonConfig::flush()
{
    if (m_dirty) {
        persistNow();
    }
}

} // namespace Aero::Core
