#include "providers/LocalProvider.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

QString LocalProvider::id() const
{
    return "local";
}

QString LocalProvider::displayName() const
{
    return QStringLiteral("Local Files");
}

QList<MusicTrack> LocalProvider::scanFolder(const QString &folderPath) const
{
    QList<MusicTrack> tracks;
    QDirIterator iterator(folderPath, QDir::Files, QDirIterator::Subdirectories);

    while (iterator.hasNext()) {
        const QString filePath = QDir::toNativeSeparators(iterator.next());
        if (!isSupportedFile(filePath)) {
            continue;
        }

        const QFileInfo info(filePath);
        MusicTrack track;
        track.providerId = id();
        track.title = info.completeBaseName();
        track.filePath = QDir::toNativeSeparators(info.absoluteFilePath());
        track.isVideo = isVideoFile(track.filePath);
        track.addedAt = QDateTime::currentDateTimeUtc();
        tracks.append(track);
    }

    return tracks;
}

bool LocalProvider::isSupportedFile(const QString &filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return supportedSuffixes().contains(suffix);
}

bool LocalProvider::isVideoFile(const QString &filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return supportedVideoSuffixes().contains(suffix);
}

QStringList LocalProvider::supportedAudioSuffixes()
{
    return {"mp3", "flac", "wav", "m4a", "aac", "ogg", "wma", "opus"};
}

QStringList LocalProvider::supportedVideoSuffixes()
{
    return {"mp4", "mkv", "avi", "mov", "wmv", "webm", "m4v", "flv"};
}

QStringList LocalProvider::supportedSuffixes()
{
    return supportedAudioSuffixes() + supportedVideoSuffixes();
}
