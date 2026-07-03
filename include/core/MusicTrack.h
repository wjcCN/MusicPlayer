#pragma once

#include <QDateTime>
#include <QString>

struct MusicTrack
{
    qint64 id = 0;
    QString providerId = "local";
    QString title;
    QString artist;
    QString album;
    QString filePath;
    qint64 durationMs = 0;
    bool favorite = false;
    QDateTime addedAt;

    QString displayTitle() const
    {
        return title.isEmpty() ? filePath : title;
    }
};
