#pragma once

#include "PlaybackBackend.h"

#include <QAudioOutput>
#include <QMediaPlayer>

class QtMediaPlaybackBackend : public PlaybackBackend
{
    Q_OBJECT

public:
    explicit QtMediaPlaybackBackend(QObject *parent = nullptr);

    QMediaPlayer *player() override;
    qint64 position() const override;
    void setMuted(bool muted) override;
    void setVolume(double volume) override;
    void setPlaybackRate(double rate) override;
    void setSource(const QUrl &source) override;
    void setPosition(qint64 position) override;
    void play() override;
    void pause() override;
    void stop() override;
    void setVideoSink(QObject *sink) override;

private:
    QMediaPlayer m_player;
    QAudioOutput m_audioOutput;
};
