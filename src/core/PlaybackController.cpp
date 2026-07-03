#include "core/PlaybackController.h"

#include <QFileInfo>
#include <QRandomGenerator>
#include <QUrl>

#include <algorithm>

PlaybackController::PlaybackController(QObject *parent)
    : QObject(parent)
{
    m_player.setAudioOutput(&m_audioOutput);
    m_audioOutput.setVolume(0.7);

    connect(&m_player, &QMediaPlayer::playbackStateChanged, this, &PlaybackController::playbackStateChanged);
    connect(&m_player, &QMediaPlayer::positionChanged, this, &PlaybackController::positionChanged);
    connect(&m_player, &QMediaPlayer::durationChanged, this, [this](qint64 duration) {
        emit durationChanged(duration);
        if (m_currentIndex >= 0 && m_currentIndex < m_queue.size() && duration > 0) {
            emit durationLearned(m_queue.at(m_currentIndex), duration);
        }
    });
    connect(&m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia) {
            next();
        }
    });
    connect(&m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &errorString) {
        emit errorOccurred(errorString.isEmpty() ? tr("Playback failed.") : errorString);
    });
}

QList<MusicTrack> PlaybackController::queue() const
{
    return m_queue;
}

MusicTrack PlaybackController::currentTrack() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_queue.size()) {
        return {};
    }
    return m_queue.at(m_currentIndex);
}

int PlaybackController::currentIndex() const
{
    return m_currentIndex;
}

PlaybackMode PlaybackController::playbackMode() const
{
    return m_mode;
}

QMediaPlayer::PlaybackState PlaybackController::playbackState() const
{
    return m_player.playbackState();
}

void PlaybackController::setQueue(const QList<MusicTrack> &tracks, int startIndex)
{
    m_queue = tracks;
    m_currentIndex = (startIndex >= 0 && startIndex < m_queue.size()) ? startIndex : -1;
    emit queueChanged(m_queue);
}

void PlaybackController::playAt(int index)
{
    if (index < 0 || index >= m_queue.size()) {
        return;
    }

    m_currentIndex = index;
    playCurrent();
}

void PlaybackController::playTrack(const MusicTrack &track)
{
    const auto it = std::find_if(m_queue.cbegin(), m_queue.cend(), [&track](const MusicTrack &candidate) {
        return candidate.filePath == track.filePath;
    });

    if (it != m_queue.cend()) {
        playAt(static_cast<int>(std::distance(m_queue.cbegin(), it)));
        return;
    }

    m_queue.append(track);
    m_currentIndex = m_queue.size() - 1;
    emit queueChanged(m_queue);
    playCurrent();
}

void PlaybackController::togglePlayPause()
{
    if (m_player.playbackState() == QMediaPlayer::PlayingState) {
        m_player.pause();
        return;
    }

    if (m_currentIndex < 0 && !m_queue.isEmpty()) {
        m_currentIndex = 0;
    }

    if (m_player.source().isEmpty()) {
        playCurrent();
    } else {
        m_player.play();
    }
}

void PlaybackController::pause()
{
    m_player.pause();
}

void PlaybackController::stop()
{
    m_player.stop();
}

void PlaybackController::next()
{
    const int index = nextIndex();
    if (index < 0) {
        m_player.stop();
        return;
    }

    m_currentIndex = index;
    playCurrent();
}

void PlaybackController::previous()
{
    const int index = previousIndex();
    if (index < 0) {
        return;
    }

    m_currentIndex = index;
    playCurrent();
}

void PlaybackController::seek(qint64 positionMs)
{
    m_player.setPosition(positionMs);
}

void PlaybackController::setVolume(int volume)
{
    const int boundedVolume = qBound(0, volume, 100);
    m_audioOutput.setVolume(static_cast<float>(boundedVolume) / 100.0f);
    emit volumeChanged(boundedVolume);
}

void PlaybackController::setPlaybackMode(PlaybackMode mode)
{
    if (m_mode == mode) {
        return;
    }

    m_mode = mode;
    emit playbackModeChanged(m_mode);
}

void PlaybackController::playCurrent()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_queue.size()) {
        return;
    }

    const MusicTrack track = m_queue.at(m_currentIndex);
    const QFileInfo fileInfo(track.filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        emit errorOccurred(tr("File is missing: %1").arg(track.filePath));
        return;
    }

    emit currentTrackChanged(track);
    m_player.setSource(QUrl::fromLocalFile(fileInfo.absoluteFilePath()));
    m_player.play();
}

int PlaybackController::nextIndex() const
{
    if (m_queue.isEmpty()) {
        return -1;
    }

    if (m_mode == PlaybackMode::LoopOne && m_currentIndex >= 0) {
        return m_currentIndex;
    }

    if (m_mode == PlaybackMode::Shuffle && m_queue.size() > 1) {
        int randomIndex = m_currentIndex;
        while (randomIndex == m_currentIndex) {
            randomIndex = QRandomGenerator::global()->bounded(m_queue.size());
        }
        return randomIndex;
    }

    const int candidate = m_currentIndex + 1;
    if (candidate < m_queue.size()) {
        return candidate;
    }

    return m_mode == PlaybackMode::LoopAll ? 0 : -1;
}

int PlaybackController::previousIndex() const
{
    if (m_queue.isEmpty()) {
        return -1;
    }

    const int candidate = m_currentIndex - 1;
    if (candidate >= 0) {
        return candidate;
    }

    return m_mode == PlaybackMode::LoopAll ? m_queue.size() - 1 : 0;
}
