#pragma once

#include "aero/platform/platform_service.h"

#include <QDateTime>
#include <QString>
#include <QStringList>

namespace Aero::Platform {

struct FileEntry {
    QString name;
    QString absolutePath;
    QString mimeType;
    QString iconName;
    qint64 size = 0;
    QDateTime modified;
    bool isDirectory = false;
    bool isSymlink = false;
    bool isHidden = false;
    bool isReadable = true;
};

enum class StandardLocation {
    Home,
    Desktop,
    Documents,
    Downloads,
    Pictures,
    Music,
    Videos,
    Templates,
    PublicShare,
};

class IFileSystem : public PlatformService {
    Q_OBJECT
public:
    explicit IFileSystem(QObject* parent = nullptr)
        : PlatformService(parent)
    {
        setCapability(Capability::Filesystem);
    }

    virtual QString homePath() const = 0;
    virtual QString configDirectory() const = 0;
    virtual QString dataDirectory() const = 0;
    virtual QString cacheDirectory() const = 0;

    virtual QString standardLocation(StandardLocation location) const = 0;
    virtual QString displayName(const QString& path) const = 0;
    virtual QString mimeTypeForFile(const QString& path) const = 0;
    virtual QString iconNameForPath(const QString& path) const = 0;

    virtual bool exists(const QString& path) const = 0;
    virtual bool isDirectory(const QString& path) const = 0;
    virtual bool isFile(const QString& path) const = 0;

    virtual QList<FileEntry> listDirectory(const QString& path,
                                           bool includeHidden,
                                           QString* error = nullptr) const = 0;

    virtual bool makeDirectory(const QString& path, QString* error = nullptr) = 0;
    virtual bool remove(const QString& path, bool recursive, QString* error = nullptr) = 0;
    virtual bool rename(const QString& from, const QString& to, QString* error = nullptr) = 0;
    virtual bool moveToTrash(const QString& path, QString* error = nullptr) = 0;

    /// Recursive, bounded filename search.  Implementations must avoid following
    /// symlinked directories and should return quickly for large trees.
    virtual QList<FileEntry> search(const QString& root,
                                    const QString& query,
                                    int maxResults,
                                    int maxDepth) const = 0;

    virtual QByteArray readFile(const QString& path,
                                qint64 maxBytes,
                                QString* error = nullptr) const = 0;
    virtual bool writeFile(const QString& path,
                           const QByteArray& data,
                           QString* error = nullptr) = 0;

    /// Launches the platform's default handler for a file or URL.  Returns
    /// false if no handler could be started.
    virtual bool openExternally(const QString& path) = 0;
};

} // namespace Aero::Platform
