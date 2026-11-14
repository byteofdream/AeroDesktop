#pragma once

#include <QObject>
#include <QJsonObject>
#include <QTimer>
#include <QVariant>

namespace Aero::Core {

/// Small JSON document store with dotted-key access and debounced persistence.
/// Writes are atomic (QSaveFile) so a crash cannot corrupt user settings.
///
/// Keys use dots as separators: "appearance.theme" maps to
/// {"appearance": {"theme": ...}}.  Values are stored as JSON and exposed as
/// QVariant for convenient use by settings pages.
class JsonConfig : public QObject {
    Q_OBJECT
public:
    explicit JsonConfig(QString filePath, QObject* parent = nullptr);
    ~JsonConfig() override;

    QString filePath() const;

    bool load(QString* error = nullptr);
    bool save(QString* error = nullptr);

    QVariant value(const QString& key, const QVariant& fallback = {}) const;
    QString stringValue(const QString& key, const QString& fallback = {}) const;
    int intValue(const QString& key, int fallback = 0) const;
    bool boolValue(const QString& key, bool fallback = false) const;
    double doubleValue(const QString& key, double fallback = 0.0) const;
    QStringList stringListValue(const QString& key, const QStringList& fallback = {}) const;

    void setValue(const QString& key, const QVariant& value, bool persistImmediately = false);
    void remove(const QString& key, bool persistImmediately = false);

    bool contains(const QString& key) const;
    QJsonObject root() const;
    void replaceRoot(const QJsonObject& object);

    /// Flushes pending debounced writes.  Called on destruction as well.
    void flush();

signals:
    void changed(const QString& key, const QVariant& value);
    void reloaded();
    void saveFailed(const QString& error);

private slots:
    void persistNow();

private:
    static QStringList splitKey(const QString& key);
    QJsonValue readValue(const QStringList& parts) const;
    bool writeValue(const QStringList& parts, const QJsonValue& value, bool remove);

    QString m_filePath;
    QJsonObject m_root;
    QTimer m_saveTimer;
    bool m_dirty = false;
    bool m_loading = false;
};

} // namespace Aero::Core
