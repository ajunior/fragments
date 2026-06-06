#include "QtMediaPlaybackBackend.h"

#include <QVideoSink>

QtMediaPlaybackBackend::QtMediaPlaybackBackend(QObject *parent)
    : PlaybackBackend(parent)
{
    m_player.setAudioOutput(&m_audioOutput);

    connect(&m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        emit playingChanged(state == QMediaPlayer::PlayingState);
    });
    connect(&m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &errorString) {
        if (!errorString.isEmpty()) {
            emit errorOccurred(errorString);
        }
    });
}

QMediaPlayer *QtMediaPlaybackBackend::player()
{
    return &m_player;
}

qint64 QtMediaPlaybackBackend::position() const
{
    return m_player.position();
}

void QtMediaPlaybackBackend::setMuted(bool muted)
{
    m_audioOutput.setMuted(muted);
}

void QtMediaPlaybackBackend::setVolume(double volume)
{
    m_audioOutput.setVolume(volume);
}

void QtMediaPlaybackBackend::setPlaybackRate(double rate)
{
    m_player.setPlaybackRate(rate);
}

void QtMediaPlaybackBackend::setSource(const QUrl &source)
{
    m_player.setSource(source);
}

void QtMediaPlaybackBackend::setPosition(qint64 position)
{
    m_player.setPosition(position);
}

void QtMediaPlaybackBackend::play()
{
    m_player.play();
}

void QtMediaPlaybackBackend::pause()
{
    m_player.pause();
}

void QtMediaPlaybackBackend::stop()
{
    m_player.stop();
}

void QtMediaPlaybackBackend::setVideoSink(QObject *sink)
{
    m_player.setVideoSink(qobject_cast<QVideoSink *>(sink));
}
