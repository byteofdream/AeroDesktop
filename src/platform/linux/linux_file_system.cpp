#include "linux_file_system.h"

#include "aero/core/paths.h"

#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMimeDatabase>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>

namespace Aero::Platform::Linux {
namespace {

QString mimeIcon(const QMimeType& mime, const QFileInfo& info)
{
    if (info.isDir()) {
        return QStringLiteral("folder");
    }
    if (mime.name().startsWith(QStringLiteral("image/"))) {
        return QStringLiteral("image");
    }
    if (mime.name().startsWith(QStringLiteral("audio/"))) {
        return QStringLiteral("audio");
    }
    if (mime.name().startsWith(QStringLiteral("video/"))) {
        return QStringLiteral("video");
    }
    if (mime.name().startsWith(QStringLiteral("text/")) || mime.name() == QStringLiteral("application/json")) {
        return QStringLiteral("file-text");
    }
    if (mime.name() == QStringLiteral("application/zip")
        || mime.name() == QStringLiteral("application/x-tar")
        || mime.name() == QStringLiteral("application/gzip")) {
        return QStringLiteral("archive");
    }
    if (mime.name() == QStringLiteral("application/pdf")) {
        return QStringLiteral("file-pdf");
    }
    return QStringLiteral("file");
}

void setError(QString* error, const QString& message)
{
    if (error != nullptr) {
        *error = message;
    }
}

QString fallbackLocation(StandardLocation location)
{
    const QString home = QDir::homePath();
    switch (location) {
    case StandardLocation::Home: return home;
    case StandardLocation::Desktop: return home + QStringLiteral("/Desktop");
    case StandardLocation::Documents: return home + QStringLiteral("/Documents");
    case StandardLocation::Downloads: return home + QStringLiteral("/Downloads");
    case StandardLocation::Pictures: return home + QStringLiteral("/Pictures");
    case StandardLocation::Music: return home + QStringLiteral("/Music");
    case StandardLocation::Videos: return home + QStringLiteral("/Videos");
    case StandardLocation::Templates: return home + QStringLiteral("/Templates");
    case StandardLocation::PublicShare: return home + QStringLiteral("/Public");
    }
    return home;
}

QStandardPaths::StandardLocation qtLocation(StandardLocation location)
{
    switch (location) {
    case StandardLocation::Home: return QStandardPaths::HomeLocation;
    case StandardLocation::Desktop: return QStandardPaths::DesktopLocation;
    case StandardLocation::Documents: return QStandardPaths::DocumentsLocation;
    case StandardLocation::Downloads: return QStandardPaths::DownloadLocation;
    case StandardLocation::Pictures: return QStandardPaths::PicturesLocation;
    case StandardLocation::Music: return QStandardPaths::MusicLocation;
    case StandardLocation::Videos: return QStandardPaths::MoviesLocation;
    case StandardLocation::Templates: return QStandardPaths::TempLocation;
    case StandardLocation::PublicShare: return QStandardPaths::GenericDataLocation;
    }
    return QStandardPaths::HomeLocation;
}

} // namespace

LinuxFileSystem::LinuxFileSystem(QObject* parent)
    : IFileSystem(parent)
{
}

bool LinuxFileSystem::isAvailable() const
{
    return true;
}

QString LinuxFileSystem::homePath() const
{
    return QDir::homePath();
}

QString LinuxFileSystem::configDirectory() const
{
    return Core::Paths::configDirectory();
}

QString LinuxFileSystem::dataDirectory() const
{
    return Core::Paths::dataDirectory();
}

QString LinuxFileSystem::cacheDirectory() const
{
    return Core::Paths::cacheDirectory();
}

QString LinuxFileSystem::standardLocation(StandardLocation location) const
{
    if (location == StandardLocation::Templates || location == StandardLocation::PublicShare) {
        return fallbackLocation(location);
    }

    const QString path = QStandardPaths::writableLocation(qtLocation(location));
    return path.isEmpty() ? fallbackLocation(location) : path;
}

QString LinuxFileSystem::displayName(const QString& path) const
{
    const QFileInfo info(path);
    if (info.fileName().isEmpty()) {
        return path;
    }
    return info.fileName();
}

QString LinuxFileSystem::mimeTypeForFile(const QString& path) const
{
    static const QMimeDatabase database;
    return database.mimeTypeForFile(path, QMimeDatabase::MatchExtension).name();
}

QString LinuxFileSystem::iconNameForPath(const QString& path) const
{
    static const QMimeDatabase database;
    const QFileInfo info(path);
    return mimeIcon(database.mimeTypeForFile(info, QMimeDatabase::MatchExtension), info);
}

bool LinuxFileSystem::exists(const QString& path) const
{
    return QFileInfo::exists(path);
}

bool LinuxFileSystem::isDirectory(const QString& path) const
{
    return QFileInfo(path).isDir();
}

bool LinuxFileSystem::isFile(const QString& path) const
{
    return QFileInfo(path).isFile();
}

QList<FileEntry> LinuxFileSystem::listDirectory(const QString& path,
                                                bool includeHidden,
                                                QString* error) const
{
    QList<FileEntry> entries;

    QDir directory(path);
    if (!directory.exists()) {
        setError(error, QStringLiteral("Directory does not exist: %1").arg(path));
        return entries;
    }
    if (!directory.isReadable()) {
        setError(error, QStringLiteral("Permission denied: %1").arg(path));
        return entries;
    }

    QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System;
    if (includeHidden) {
        filters |= QDir::Hidden;
    }

    static const QMimeDatabase database;
    const QFileInfoList infos = directory.entryInfoList(filters, QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    entries.reserve(infos.size());

    for (const QFileInfo& info : infos) {
        FileEntry entry;
        entry.name = info.fileName();
        entry.absolutePath = info.absoluteFilePath();
        entry.isDirectory = info.isDir();
        entry.isSymlink = info.isSymLink();
        entry.isHidden = info.isHidden();
        entry.isReadable = info.isReadable();
        entry.size = info.size();
        entry.modified = info.lastModified();
        entry.mimeType = database.mimeTypeForFile(info, QMimeDatabase::MatchExtension).name();
        entry.iconName = mimeIcon(database.mimeTypeForFile(info, QMimeDatabase::MatchExtension), info);
        entries.append(entry);
    }

    return entries;
}

bool LinuxFileSystem::makeDirectory(const QString& path, QString* error)
{
    if (QDir().mkpath(path)) {
        return true;
    }
    setError(error, QStringLiteral("Unable to create directory %1").arg(path));
    return false;
}

bool LinuxFileSystem::remove(const QString& path, bool recursive, QString* error)
{
    const QFileInfo info(path);
    if (!info.exists()) {
        return true;
    }

    bool ok = false;
    if (info.isDir() && !info.isSymLink()) {
        ok = recursive ? QDir(path).removeRecursively() : QDir().rmdir(path);
    } else {
        ok = QFile::remove(path);
    }

    if (!ok) {
        setError(error, QStringLiteral("Unable to delete %1").arg(path));
    }
    return ok;
}

bool LinuxFileSystem::rename(const QString& from, const QString& to, QString* error)
{
    if (QFile::rename(from, to)) {
        return true;
    }
    setError(error, QStringLiteral("Unable to rename %1 to %2").arg(from, to));
    return false;
}

bool LinuxFileSystem::moveToTrash(const QString& path, QString* error)
{
    QString reason;
    if (QFile::moveToTrash(path, &reason)) {
        return true;
    }
    setError(error, reason.isEmpty() ? QStringLiteral("Unable to move %1 to trash").arg(path) : reason);
    return false;
}

QList<FileEntry> LinuxFileSystem::search(const QString& root,
                                          const QString& query,
                                          int maxResults,
                                          int maxDepth) const
{
    QList<FileEntry> results;
    const QString needle = query.trimmed();
    if (needle.isEmpty() || root.isEmpty()) {
        return results;
    }

    static const QMimeDatabase database;
    QDirIterator iterator(root,
                          QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);

    int visited = 0;
    while (iterator.hasNext() && results.size() < maxResults && visited < 20000) {
        iterator.next();
        ++visited;

        const QString path = iterator.filePath();
        if (path.count(QLatin1Char('/')) - root.count(QLatin1Char('/')) > maxDepth) {
            continue;
        }
        const QString name = iterator.fileName();
        if (name.startsWith(QLatin1Char('.'))
            || name == QStringLiteral("node_modules")
            || name == QStringLiteral(".git")
            || name == QStringLiteral(".cache")) {
            continue;
        }
        if (!name.contains(needle, Qt::CaseInsensitive)) {
            continue;
        }

        const QFileInfo info = iterator.fileInfo();
        FileEntry entry;
        entry.name = info.fileName();
        entry.absolutePath = info.absoluteFilePath();
        entry.isDirectory = info.isDir();
        entry.isSymlink = info.isSymLink();
        entry.isHidden = info.isHidden();
        entry.isReadable = info.isReadable();
        entry.size = info.size();
        entry.modified = info.lastModified();
        entry.mimeType = database.mimeTypeForFile(info, QMimeDatabase::MatchExtension).name();
        entry.iconName = mimeIcon(database.mimeTypeForFile(info, QMimeDatabase::MatchExtension), info);
        results.append(entry);
    }

    std::sort(results.begin(), results.end(), [&needle](const FileEntry& a, const FileEntry& b) {
        const bool aStarts = a.name.startsWith(needle, Qt::CaseInsensitive);
        const bool bStarts = b.name.startsWith(needle, Qt::CaseInsensitive);
        if (aStarts != bStarts) {
            return aStarts;
        }
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });

    return results;
}

QByteArray LinuxFileSystem::readFile(const QString& path, qint64 maxBytes, QString* error) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, file.errorString());
        return {};
    }
    if (maxBytes > 0 && file.size() > maxBytes) {
        setError(error, QStringLiteral("File is too large to open (%1 bytes)").arg(file.size()));
        return {};
    }
    return file.readAll();
}

bool LinuxFileSystem::writeFile(const QString& path, const QByteArray& data, QString* error)
{
    QFileInfo info(path);
    if (!info.absoluteDir().exists() && !QDir().mkpath(info.absolutePath())) {
        setError(error, QStringLiteral("Unable to create %1").arg(info.absolutePath()));
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, file.errorString());
        return false;
    }
    if (file.write(data) != data.size()) {
        setError(error, file.errorString());
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        setError(error, file.errorString());
        return false;
    }
    return true;
}

bool LinuxFileSystem::openExternally(const QString& path)
{
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

} // namespace Aero::Platform::Linux
