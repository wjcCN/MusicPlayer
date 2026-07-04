#pragma once

#include "core/PlaybackController.h"

#include <QDialog>

class QLabel;
class QCloseEvent;
class QEvent;
class QFrame;
class QKeyEvent;
class QPushButton;
class QSlider;
class QTimer;
class QVideoWidget;
class VideoStateOverlay;

class FullscreenVideoWindow final : public QDialog
{
    Q_OBJECT

public:
    explicit FullscreenVideoWindow(PlaybackController *player, bool english, QWidget *parent = nullptr);

    QVideoWidget *videoWidget() const;
    void setAspectRatioMode(Qt::AspectRatioMode mode);
    void setEnglish(bool english);

signals:
    void closed();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applyPlaybackState(QMediaPlayer::PlaybackState state);
    void updatePosition(qint64 positionMs);
    void updateDuration(qint64 durationMs);
    void updateTimeLabel(qint64 positionMs, qint64 durationMs);
    void seekRelative(qint64 deltaMs);
    bool handleKeyEvent(QKeyEvent *event, bool pressed);
    QString formatDuration(qint64 durationMs) const;
    QString playText() const;
    QString pauseText() const;

    PlaybackController *m_player = nullptr;
    QFrame *m_videoFrame = nullptr;
    QVideoWidget *m_videoWidget = nullptr;
    VideoStateOverlay *m_videoStateOverlay = nullptr;
    QSlider *m_positionSlider = nullptr;
    QLabel *m_timeLabel = nullptr;
    QPushButton *m_playPauseButton = nullptr;
    QPushButton *m_exitButton = nullptr;
    QTimer *m_rightHoldTimer = nullptr;
    qint64 m_durationMs = 0;
    bool m_seeking = false;
    bool m_english = false;
    bool m_rightKeyPressed = false;
    bool m_rightLongPressActive = false;
};
