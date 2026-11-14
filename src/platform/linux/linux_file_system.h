#pragma once

#include "aero/platform/i_file_system.h"

namespace Aero::Platform::Linux {

class LinuxFileSystem final : public IFileSystem {
public:
    explicit LinuxFileSystem(QObject* parent = nullptr);

    bool isAvailable() const override;

    QString homePath() const override;
    QString configDirectory() const override;
    QString dataDirectory() const override;
    QString cacheDirectory() const override;

    QString standardLocation(StandardLocation location) const override;
    QString displayName(const QString& path) const override;
    QString mimeTypeForFile(const QString& path) const override;
    QString iconNameForPath(const QString& path) const override;

    bool exists(const QString& path) const override;
    bool isDirectory(const QString& path) const override;
    bool isFile(const QString& path) const override;

    QList<FileEntry> listDirectory(const QString& path,
                                   bool includeHidden,
                                   QString* error) const override;

    bool makeDirectory(const QString& path, QString* error) override;
    bool remove(const QString& path, bool recursive, QString* error) override;
    bool rename(const QString& from, const QString& to, QString* error) override;
    bool moveToTrash(const QString& path, QString* error) override;

    QList<FileEntry> search(const QString& root,
                            const QString& query,
                            int maxResults,
                            int maxDepth) const override;

    QByteArray readFile(const QString& path, qint64 maxBytes, QString* error) const override;
    bool writeFile(const QString& path, const QByteArray& data, QString* error) override;

    bool openExternally(const QString& path) override;
};

} // namespace Aero::Platform::Linux
