#pragma once

#include "aero/services/search_service.h"
#include "aero/shell/overlay.h"

#include <QList>

class QLineEdit;
class QListWidget;
class QListWidgetItem;

namespace Aero::Services {
class SearchService;
}

namespace Aero::Shell {

/// Spotlight-style launcher and file search.
///
/// Application, action and setting matches are computed synchronously from the
/// registry, while the filesystem query runs through SearchService on a worker
/// thread.  Stale file results are discarded by comparing the query string.
class Spotlight : public Overlay {
    Q_OBJECT
public:
    explicit Spotlight(Services::SearchService* searchService, QWidget* parent = nullptr);

signals:
    void applicationRequested(const QString& appId);
    void fileRequested(const QString& path);
    void actionRequested(const QString& action);

protected:
    void prepareShow() override;
    void prepareHide() override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void build();
    void setQuery(const QString& query);
    void rebuildResults();
    void addStaticResults(const QString& query);
    void addFileResults(const QString& query, const QList<Services::SearchResult>& results);
    void activateCurrent();
    void moveSelection(int delta);
    void clearRows();

    Services::SearchService* m_searchService = nullptr;
    QLineEdit* m_field = nullptr;
    QListWidget* m_results = nullptr;
    QString m_currentQuery;
};

} // namespace Aero::Shell
