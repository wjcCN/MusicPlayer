#include "core/LyricsParser.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>

QList<LyricLine> LyricsParser::parseLrcFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    const QString content = QString::fromUtf8(file.readAll());
    const QStringList rows = content.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
    const QRegularExpression timestampPattern(R"(\[(\d{1,2}):(\d{2})(?:[\.:](\d{1,3}))?\])");

    QList<LyricLine> lines;
    for (const QString &row : rows) {
        QRegularExpressionMatchIterator matches = timestampPattern.globalMatch(row);
        QList<qint64> timestamps;
        int lastTimestampEnd = 0;

        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            const qint64 minutes = match.captured(1).toLongLong();
            const qint64 seconds = match.captured(2).toLongLong();
            const QString fractionText = match.captured(3);

            qint64 fractionMs = 0;
            if (!fractionText.isEmpty()) {
                if (fractionText.size() == 1) {
                    fractionMs = fractionText.toLongLong() * 100;
                } else if (fractionText.size() == 2) {
                    fractionMs = fractionText.toLongLong() * 10;
                } else {
                    fractionMs = fractionText.left(3).toLongLong();
                }
            }

            timestamps.append(minutes * 60 * 1000 + seconds * 1000 + fractionMs);
            lastTimestampEnd = qMax(lastTimestampEnd, match.capturedEnd());
        }

        if (timestamps.isEmpty()) {
            continue;
        }

        const QString lyricText = row.mid(lastTimestampEnd).trimmed();
        for (qint64 timeMs : timestamps) {
            lines.append({timeMs, lyricText.isEmpty() ? QStringLiteral("...") : lyricText});
        }
    }

    std::sort(lines.begin(), lines.end(), [](const LyricLine &left, const LyricLine &right) {
        return left.timeMs < right.timeMs;
    });

    return lines;
}

QString LyricsParser::lrcPathForAudioFile(const QString &audioFilePath)
{
    const QFileInfo info(audioFilePath);
    return info.absolutePath() + "/" + info.completeBaseName() + ".lrc";
}
