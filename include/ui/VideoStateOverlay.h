#pragma once

#include <QWidget>

class QTimer;

class VideoStateOverlay final : public QWidget
{
public:
    explicit VideoStateOverlay(QWidget *videoHost);

    void showPausedIndicator();
    void showPlayingIndicator();
    void hideIndicator();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    enum class Icon
    {
        Play,
        Pause
    };

    void centerInHost();

    QWidget *m_videoHost = nullptr;
    QTimer *m_hideTimer = nullptr;
    Icon m_icon = Icon::Play;
};
