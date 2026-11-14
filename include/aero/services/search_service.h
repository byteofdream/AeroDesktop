#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace Aero::Platform {
class IFileSystem;
}

namespace Aero::Services {

enum class SearchKind {
    File,
    Application,
    Action,
    Setting,
};

struct SearchResult {
    SearchKind kind = SearchKind::File;
    QString title;
    QString subtitle;
    QString path;
    QString appId;
    QString iconName;
    int score = 0;
};

/// Debounced, asynchronous file search used by Spotlight.  Scanning happens on
/// a worker thread through IFileSystem; results are delivered on the GUI
/// thread and stale queries are discarded.
class SearchService : public QObject {
    Q_OBJECT
public:
    explicit SearchService(Platform::IFileSystem* fileSystem, QObject* parent = nullptr);
    ~SearchService() override;

    void setSearchRoots(const QStringList& roots);
    QStringList searchRoots() const;

    void search(const QString& query);
    void cancel();

signals:
    void resultsReady(const QString& query, const QList<Aero::Services::SearchResult>& results);

private:
    void startPending();

    Platform::IFileSystem* m_fileSystem = nullptr;
    QStringList m_roots;
    QTimer m_debounce;
    QFutureWatcher<QList<SearchResult>>* m_watcher = nullptr;
    QString m_pendingQuery;
    QString m_currentQuery;
    bool m_running = false;
};

} // namespace Aero::Services

Q_DECLARE_METATYPE(Aero::Services::SearchResult)
