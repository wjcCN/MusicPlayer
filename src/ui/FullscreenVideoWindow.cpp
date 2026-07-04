#include "ui/FullscreenVideoWindow.h"

#include "ui/VideoStateOverlay.h"

#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QVideoWidget>

FullscreenVideoWindow::FullscreenVideoWindow(PlaybackController *player, bool english, QWidget *parent)
    : QDialog(parent)
    , m_player(player)
    , m_english(english)
{
    setObjectName("fullscreenVideoWindow");
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setWindowState(Qt::WindowFullScreen);
    setFocusPolicy(Qt::StrongFocus);

    m_videoFrame = new QFrame(this);
    m_videoFrame->setObjectName("fullscreenVideoFrame");

    m_videoWidget = new QVideoWidget(m_videoFrame);
    m_videoWidget->setObjectName("fullscreenVideoSurface");
    m_videoWidget->setFocusPolicy(Qt::StrongFocus);
    m_videoWidget->installEventFilter(this);
    m_videoStateOverlay = new VideoStateOverlay(m_videoWidget);

    auto *videoFrameLayout = new QVBoxLayout(m_videoFrame);
    videoFrameLayout->setContentsMargins(6, 6, 6, 6);
    videoFrameLayout->setSpacing(0);
    videoFrameLayout->addWidget(m_videoWidget);

    m_positionSlider = new QSlider(Qt::Horizontal, this);
    m_positionSlider->setObjectName("fullscreenPositionSlider");
    m_positionSlider->setRange(0, 0);

    m_timeLabel = new QLabel(QStringLiteral("00:00 / 00:00"), this);
    m_timeLabel->setObjectName("fullscreenTimeLabel");

    m_playPauseButton = new QPushButton(this);
    m_playPauseButton->setObjectName("playButton");

    m_exitButton = new QPushButton(QStringLiteral("Esc"), this);
    m_exitButton->setObjectName("ghostButton");

    auto *controlLayout = new QHBoxLayout;
    controlLayout->setContentsMargins(18, 10, 18, 14);
    controlLayout->setSpacing(12);
    controlLayout->addWidget(m_playPauseButton);
    controlLayout->addWidget(m_positionSlider, 1);
    controlLayout->addWidget(m_timeLabel);
    controlLayout->addWidget(m_exitButton);

    auto *controlFrame = new QWidget(this);
    controlFrame->setObjectName("fullscreenControlBar");
    controlFrame->setLayout(controlLayout);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 0);
    layout->setSpacing(0);
    layout->addWidget(m_videoFrame, 1);
    layout->addWidget(controlFrame);
    setLayout(layout);

    m_rightHoldTimer = new QTimer(this);
    m_rightHoldTimer->setSingleShot(true);
    connect(m_rightHoldTimer, &QTimer::timeout, this, [this] {
        if (!m_rightKeyPressed || !m_player) {
            return;
        }
        m_rightLongPressActive = true;
        m_player->setPlaybackRate(2.0);
    });

    connect(m_playPauseButton, &QPushButton::clicked, m_player, &PlaybackController::togglePlayPause);
    connect(m_exitButton, &QPushButton::clicked, this, &QDialog::close);
    connect(m_positionSlider, &QSlider::sliderPressed, this, [this] {
        m_seeking = true;
    });
    connect(m_positionSlider, &QSlider::sliderReleased, this, [this] {
        m_seeking = false;
        if (m_player) {
            m_player->seek(m_positionSlider->value());
        }
    });

    if (m_player) {
        m_durationMs = m_player->duration();
        m_positionSlider->setRange(0, static_cast<int>(m_durationMs));
        m_positionSlider->setValue(static_cast<int>(m_player->position()));
        updateTimeLabel(m_player->position(), m_durationMs);
        applyPlaybackState(m_player->playbackState());

        connect(m_player, &PlaybackController::positionChanged, this, &FullscreenVideoWindow::updatePosition);
        connect(m_player, &PlaybackController::durationChanged, this, &FullscreenVideoWindow::updateDuration);
        connect(m_player, &PlaybackController::playbackStateChanged, this, &FullscreenVideoWindow::applyPlaybackState);
    }

    installEventFilter(this);
    qApp->installEventFilter(this);
}

QVideoWidget *FullscreenVideoWindow::videoWidget() const
{
    return m_videoWidget;
}

void FullscreenVideoWindow::setAspectRatioMode(Qt::AspectRatioMode mode)
{
    m_videoWidget->setAspectRatioMode(mode);
}

void FullscreenVideoWindow::setEnglish(bool english)
{
    if (m_english == english) {
        applyPlaybackState(m_player ? m_player->playbackState() : QMediaPlayer::StoppedState);
        return;
    }

    m_english = english;
    applyPlaybackState(m_player ? m_player->playbackState() : QMediaPlayer::StoppedState);
}

void FullscreenVideoWindow::closeEvent(QCloseEvent *event)
{
    qApp->removeEventFilter(this);
    if (m_player) {
        m_player->setPlaybackRate(1.0);
    }
    emit closed();
    QDialog::closeEvent(event);
}

bool FullscreenVideoWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        if (handleKeyEvent(static_cast<QKeyEvent *>(event), event->type() == QEvent::KeyPress)) {
            return true;
        }
    }

    if (watched == m_videoWidget && event->type() == QEvent::MouseButtonRelease) {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && m_player) {
            m_player->togglePlayPause();
            return true;
        }
    }

    if (watched == m_videoWidget && event->type() == QEvent::MouseButtonDblClick) {
        close();
        return true;
    }

    return QDialog::eventFilter(watched, event);
}

void FullscreenVideoWindow::applyPlaybackState(QMediaPlayer::PlaybackState state)
{
    m_playPauseButton->setText(state == QMediaPlayer::PlayingState ? pauseText() : playText());

    if (!m_videoStateOverlay) {
        return;
    }

    if (state == QMediaPlayer::PlayingState) {
        m_videoStateOverlay->showPlayingIndicator();
    } else if (state == QMediaPlayer::PausedState) {
        m_videoStateOverlay->showPausedIndicator();
    } else {
        m_videoStateOverlay->hideIndicator();
    }
}

void FullscreenVideoWindow::updatePosition(qint64 positionMs)
{
    if (!m_seeking) {
        m_positionSlider->setValue(static_cast<int>(positionMs));
    }
    updateTimeLabel(positionMs, m_durationMs);
}

void FullscreenVideoWindow::updateDuration(qint64 durationMs)
{
    m_durationMs = durationMs;
    m_positionSlider->setRange(0, static_cast<int>(durationMs));
    updateTimeLabel(m_positionSlider->value(), durationMs);
}

void FullscreenVideoWindow::updateTimeLabel(qint64 positionMs, qint64 durationMs)
{
    m_timeLabel->setText(QStringLiteral("%1 / %2").arg(formatDuration(positionMs), formatDuration(durationMs)));
}

void FullscreenVideoWindow::seekRelative(qint64 deltaMs)
{
    if (!m_player) {
        return;
    }

    const qint64 current = m_player->position();
    const qint64 maximum = m_durationMs > 0 ? m_durationMs : m_player->duration();
    const qint64 target = qBound<qint64>(0, current + deltaMs, maximum);
    m_player->seek(target);
    m_positionSlider->setValue(static_cast<int>(target));
    updateTimeLabel(target, maximum);
}

bool FullscreenVideoWindow::handleKeyEvent(QKeyEvent *event, bool pressed)
{
    if (!event || !m_player) {
        return false;
    }

    const int key = event->key();
    if (key == Qt::Key_Escape && pressed) {
        close();
        return true;
    }

    if (key == Qt::Key_Space && pressed && !event->isAutoRepeat()) {
        m_player->togglePlayPause();
        return true;
    }

    if (key == Qt::Key_Left && pressed && !event->isAutoRepeat()) {
        seekRelative(-5000);
        return true;
    }

    if (key == Qt::Key_Right) {
        if (pressed) {
            if (!event->isAutoRepeat() && !m_rightKeyPressed) {
                m_rightKeyPressed = true;
                m_rightLongPressActive = false;
                m_rightHoldTimer->start(450);
            }
            return true;
        }

        if (event->isAutoRepeat()) {
            return true;
        }

        m_rightKeyPressed = false;
        if (m_rightLongPressActive) {
            m_rightLongPressActive = false;
            m_player->setPlaybackRate(1.0);
        } else {
            m_rightHoldTimer->stop();
            seekRelative(5000);
        }
        return true;
    }

    return false;
}

QString FullscreenVideoWindow::formatDuration(qint64 durationMs) const
{
    if (durationMs <= 0) {
        return QStringLiteral("00:00");
    }

    const qint64 totalSeconds = durationMs / 1000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;

    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }

    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

QString FullscreenVideoWindow::playText() const
{
    return m_english ? QStringLiteral("Play") : QStringLiteral("播放");
}

QString FullscreenVideoWindow::pauseText() const
{
    return m_english ? QStringLiteral("Pause") : QStringLiteral("暂停");
}
