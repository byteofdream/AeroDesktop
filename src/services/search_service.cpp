#include "aero/services/search_service.h"

#include "aero/platform/i_file_system.h"
#include "aero/core/paths.h"
#include "aero/core/utils.h"

#include <QFileInfo>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace Aero::Services {

SearchService::SearchService(Platform::IFileSystem* fileSystem, QObject* parent)
    : QObject(parent)
    , m_fileSystem(fileSystem)
    , m_watcher(new QFutureWatcher<QList<SearchResult>>(this))
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(180);
    connect(&m_debounce, &QTimer::timeout, this, &SearchService::startPending);

    if (m_fileSystem != nullptr) {
        const QStringList locations = {
            m_fileSystem->standardLocation(Platform::StandardLocation::Home),
            m_fileSystem->standardLocation(Platform::StandardLocation::Desktop),
            m_fileSystem->standardLocation(Platform::StandardLocation::Documents),
            m_fileSystem->standardLocation(Platform::StandardLocation::Downloads),
            m_fileSystem->standardLocation(Platform::StandardLocation::Pictures),
            m_fileSystem->standardLocation(Platform::StandardLocation::Music),
            m_fileSystem->standardLocation(Platform::StandardLocation::Videos),
        };
        for (const QString& location : locations) {
            if (!location.isEmpty() && !m_roots.contains(location)) {
                m_roots.append(location);
            }
        }
    }

    connect(m_watcher, &QFutureWatcher<QList<SearchResult>>::finished, this, [this]() {
        m_running = false;
        const QString query = m_currentQuery;
        const QList<SearchResult> results = m_watcher->result();

        if (!query.isEmpty() && query == m_pendingQuery) {
            emit resultsReady(query, results);
        }

        if (m_pendingQuery != m_currentQuery && !m_pendingQuery.isEmpty()) {
            startPending();
        }
    });
}

SearchService::~SearchService()
{
    if (m_watcher->isRunning()) {
        m_watcher->waitForFinished();
    }
}

void SearchService::setSearchRoots(const QStringList& roots)
{
    m_roots = roots;
}

QStringList SearchService::searchRoots() const
{
    return m_roots;
}

void SearchService::search(const QString& query)
{
    m_pendingQuery = query.trimmed();

    if (m_pendingQuery.isEmpty()) {
        m_debounce.stop();
        emit resultsReady(QString(), {});
        return;
    }

    m_debounce.start();
}

void SearchService::startPending()
{
    if (m_running || m_pendingQuery.isEmpty()) {
        return;
    }

    m_currentQuery = m_pendingQuery;
    m_running = true;

    const QString query = m_currentQuery;
    const QStringList roots = m_roots;
    Platform::IFileSystem* fileSystem = m_fileSystem;

    m_watcher->setFuture(QtConcurrent::run([query, roots, fileSystem]() {
        QList<SearchResult> results;
        if (fileSystem == nullptr) {
            return results;
        }

        QSet<QString> seen;
        for (const QString& root : roots) {
            if (!fileSystem->isDirectory(root)) {
                continue;
            }

            const QList<Platform::FileEntry> entries = fileSystem->search(root, query, 40, 4);
            for (const Platform::FileEntry& entry : entries) {
                if (seen.contains(entry.absolutePath)) {
                    continue;
                }
                seen.insert(entry.absolutePath);

                SearchResult result;
                result.kind = SearchKind::File;
                result.title = entry.name;
                result.path = entry.absolutePath;
                result.subtitle = Core::Paths::elideHome(QFileInfo(entry.absolutePath).absolutePath());
                result.iconName = entry.iconName;
                result.score = entry.name.startsWith(query, Qt::CaseInsensitive) ? 100 : 50;
                if (entry.isDirectory) {
                    result.score += 2;
                }
                results.append(result);

                if (results.size() >= 60) {
                    break;
                }
            }
            if (results.size() >= 60) {
                break;
            }
        }

        std::sort(results.begin(), results.end(), [](const SearchResult& a, const SearchResult& b) {
            if (a.score != b.score) {
                return a.score > b.score;
            }
            return a.title.compare(b.title, Qt::CaseInsensitive) < 0;
        });

        return results;
    }));
}

void SearchService::cancel()
{
    m_debounce.stop();
    m_pendingQuery.clear();
    m_currentQuery.clear();
}

} // namespace Aero::Services
