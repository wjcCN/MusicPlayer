#pragma once

#include "core/MusicTrack.h"

#include <QAudioOutput>
#include <QList>
#include <QMediaPlayer>
#include <QObject>

enum class PlaybackMode
{
    Sequential,
    LoopAll,
    LoopOne,
    Shuffle
};

class PlaybackController final : public QObject
{
    Q_OBJECT

public:
    explicit PlaybackController(QObject *parent = nullptr);

    QList<MusicTrack> queue() const;
    MusicTrack currentTrack() const;
    int currentIndex() const;
    PlaybackMode playbackMode() const;
    QMediaPlayer::PlaybackState playbackState() const;

    void setQueue(const QList<MusicTrack> &tracks, int startIndex = -1);
    void playAt(int index);
    void playTrack(const MusicTrack &track);
    void togglePlayPause();
    void pause();
    void stop();
    void next();
    void previous();
    void seek(qint64 positionMs);
    void setVolume(int volume);
    void setPlaybackMode(PlaybackMode mode);

signals:
    void queueChanged(const QList<MusicTrack> &queue);
    void currentTrackChanged(const MusicTrack &track);
    void playbackStateChanged(QMediaPlayer::PlaybackState state);
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void durationLearned(const MusicTrack &track, qint64 durationMs);
    void volumeChanged(int volume);
    void playbackModeChanged(PlaybackMode mode);
    void errorOccurred(const QString &message);

private:
    void playCurrent();
    int nextIndex() const;
    int previousIndex() const;

    QMediaPlayer m_player;
    QAudioOutput m_audioOutput;
    QList<MusicTrack> m_queue;
    int m_currentIndex = -1;
    PlaybackMode m_mode = PlaybackMode::Sequential;
};
