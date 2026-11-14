#include "aero/core/logging.h"

#include "aero/core/paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>
#include <QDateTime>

#include <cstdio>

Q_LOGGING_CATEGORY(aeroLog, "aero")

namespace Aero::Core {
namespace {

QMutex g_logMutex;
QString g_logFilePath;
QFile* g_logFile = nullptr;
QtMessageHandler g_previousHandler = nullptr;
bool g_installed = false;

const char* levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return "DEBUG";
    case QtInfoMsg: return "INFO ";
    case QtWarningMsg: return "WARN ";
    case QtCriticalMsg: return "ERROR";
    case QtFatalMsg: return "FATAL";
    }
    return "?????";
}

void aeroMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    const QString category = context.category ? QString::fromLatin1(context.category) : QStringLiteral("default");
    const QString line = QStringLiteral("%1 [%2] %3: %4\n")
                             .arg(timestamp, QString::fromLatin1(levelName(type)), category, message);

    {
        QMutexLocker locker(&g_logMutex);
        if (g_logFile != nullptr && g_logFile->isOpen()) {
            g_logFile->write(line.toUtf8());
            g_logFile->flush();
        }
    }

    std::fputs(line.toLocal8Bit().constData(), stderr);
    std::fflush(stderr);

    if (type == QtFatalMsg) {
        std::abort();
    }
}

void rotateIfNeeded(const QString& path)
{
    constexpr qint64 maxSize = 4 * 1024 * 1024;
    QFileInfo info(path);
    if (!info.exists() || info.size() < maxSize) {
        return;
    }

    const QString backup = path + QStringLiteral(".1");
    QFile::remove(backup);
    QFile::rename(path, backup);
}

} // namespace

void Log::install(const QString& filePath)
{
    if (g_installed) {
        return;
    }

    g_installed = true;
    g_previousHandler = qInstallMessageHandler(aeroMessageHandler);

    if (!filePath.isEmpty()) {
        QDir().mkpath(QFileInfo(filePath).absolutePath());
        rotateIfNeeded(filePath);

        auto* file = new QFile(filePath);
        if (file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QMutexLocker locker(&g_logMutex);
            g_logFile = file;
            g_logFilePath = filePath;
        } else {
            delete file;
            qCWarning(aeroLog) << "Unable to open log file" << filePath << ":"
                               << file->errorString();
        }
    }

    qCInfo(aeroLog) << "Aero log installed";
}

void Log::shutdown()
{
    if (!g_installed) {
        return;
    }

    QMutexLocker locker(&g_logMutex);
    if (g_logFile != nullptr) {
        g_logFile->flush();
        g_logFile->close();
        delete g_logFile;
        g_logFile = nullptr;
    }
    g_logFilePath.clear();
    locker.unlock();

    qInstallMessageHandler(g_previousHandler);
    g_previousHandler = nullptr;
    g_installed = false;
}

QString Log::filePath()
{
    QMutexLocker locker(&g_logMutex);
    return g_logFilePath;
}

} // namespace Aero::Core
