#pragma once

#include "core/MusicTrack.h"

#include <QList>
#include <QStringList>

class LocalProvider final
{
public:
    QString id() const;
    QString displayName() const;

    QList<MusicTrack> scanFolder(const QString &folderPath) const;
    static bool isSupportedFile(const QString &filePath);
    static bool isVideoFile(const QString &filePath);
    static QStringList supportedAudioSuffixes();
    static QStringList supportedVideoSuffixes();
    static QStringList supportedSuffixes();
};
