#include "providers/LocalProvider.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

LocalProvider::LocalProvider(QObject *parent)
    : MusicProvider(parent)
{
}

QString LocalProvider::id() const
{
    return "local";
}

QString LocalProvider::displayName() const
{
    return tr("Local Files");
}

ProviderInfo LocalProvider::info() const
{
    return ProviderInfo{
        id(),
        displayName(),
        ProviderAccess::Playable,
        tr("Ready"),
        tr("Scans and plays local audio files stored on this Windows device.")
    };
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

QStringList LocalProvider::supportedSuffixes()
{
    return {"mp3", "flac", "wav", "m4a", "aac", "ogg", "wma", "opus"};
}
