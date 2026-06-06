#pragma once

#include <QMediaPlayer>
#include <QObject>
#include <QUrl>

class PlaybackBackend : public QObject
{
    Q_OBJECT

public:
    explicit PlaybackBackend(QObject *parent = nullptr);
    ~PlaybackBackend() override;

    virtual QMediaPlayer *player();
    virtual qint64 position() const = 0;
    virtual void setMuted(bool muted) = 0;
    virtual void setVolume(double volume) = 0;
    virtual void setPlaybackRate(double rate) = 0;
    virtual void setSource(const QUrl &source) = 0;
    virtual void setPosition(qint64 position) = 0;
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual void setVideoSink(QObject *sink) = 0;

signals:
    void playingChanged(bool playing);
    void errorOccurred(const QString &message);
};
