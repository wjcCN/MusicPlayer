#include "ui/VideoStateOverlay.h"

#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

namespace
{
constexpr int OverlaySize = 112;
}

VideoStateOverlay::VideoStateOverlay(QWidget *videoHost)
    : QWidget(videoHost)
    , m_videoHost(videoHost)
    , m_hideTimer(new QTimer(this))
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFixedSize(OverlaySize, OverlaySize);
    hide();

    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this, &VideoStateOverlay::hideIndicator);

    if (m_videoHost) {
        m_videoHost->installEventFilter(this);
        centerInHost();
    }
}

void VideoStateOverlay::showPausedIndicator()
{
    m_hideTimer->stop();
    m_icon = Icon::Play;
    centerInHost();
    show();
    raise();
    update();
}

void VideoStateOverlay::showPlayingIndicator()
{
    m_hideTimer->stop();
    m_icon = Icon::Pause;
    centerInHost();
    show();
    raise();
    update();
    m_hideTimer->start(500);
}

void VideoStateOverlay::hideIndicator()
{
    m_hideTimer->stop();
    hide();
}

bool VideoStateOverlay::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_videoHost
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show || event->type() == QEvent::Move)) {
        centerInHost();
    }

    return QWidget::eventFilter(watched, event);
}

void VideoStateOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF circle = rect().adjusted(6, 6, -6, -6);
    painter.setPen(QPen(QColor(255, 255, 255, 185), 2));
    painter.setBrush(QColor(8, 14, 22, 176));
    painter.drawEllipse(circle);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 235));

    if (m_icon == Icon::Play) {
        QPainterPath path;
        path.moveTo(width() / 2.0 - 14, height() / 2.0 - 22);
        path.lineTo(width() / 2.0 - 14, height() / 2.0 + 22);
        path.lineTo(width() / 2.0 + 24, height() / 2.0);
        path.closeSubpath();
        painter.drawPath(path);
        return;
    }

    const qreal barWidth = 12;
    const qreal barHeight = 42;
    const qreal y = (height() - barHeight) / 2.0;
    painter.drawRoundedRect(QRectF(width() / 2.0 - 20, y, barWidth, barHeight), 4, 4);
    painter.drawRoundedRect(QRectF(width() / 2.0 + 8, y, barWidth, barHeight), 4, 4);
}

void VideoStateOverlay::centerInHost()
{
    if (!m_videoHost) {
        return;
    }

    const int x = (m_videoHost->width() - width()) / 2;
    const int y = (m_videoHost->height() - height()) / 2;
    move(qMax(0, x), qMax(0, y));
}
