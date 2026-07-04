#pragma once

#include <QList>
#include <QString>

struct LyricLine
{
    qint64 timeMs = 0;
    QString text;
};

class LyricsParser
{
public:
    static QList<LyricLine> parseLrcFile(const QString &filePath);
    static QString lrcPathForAudioFile(const QString &audioFilePath);
};
