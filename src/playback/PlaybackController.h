#pragma once

#include "PlaybackBackend.h"

#include <QMediaPlayer>
#include <QObject>
#include <QTimer>

#include <memory>

struct Fragment;
class PlaylistModel;

class PlaybackController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QMediaPlayer *player READ player CONSTANT)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(double currentStart READ currentStart NOTIFY currentFragmentChanged)
    Q_PROPERTY(double currentEnd READ currentEnd NOTIFY currentFragmentChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(bool playingSource READ playingSource NOTIFY playingSourceChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(bool stopAfterCurrent READ stopAfterCurrent NOTIFY stopAfterCurrentChanged)
    Q_PROPERTY(bool delayActive READ delayActive NOTIFY delayStateChanged)
    Q_PROPERTY(QString currentDelayColor READ currentDelayColor NOTIFY currentFragmentChanged)
    Q_PROPERTY(int delayRemainingMs READ delayRemainingMs NOTIFY delayProgressChanged)
    Q_PROPERTY(double delayProgress READ delayProgress NOTIFY delayProgressChanged)

public:
    explicit PlaybackController(QObject *parent = nullptr);
    explicit PlaybackController(PlaybackBackend *backend, QObject *parent = nullptr);

    QMediaPlayer *player();
    int currentIndex() const;
    double currentStart() const;
    double currentEnd() const;
    bool playing() const;
    bool playingSource() const;
    bool stopAfterCurrent() const;
    bool muted() const;
    Q_INVOKABLE void setMuted(bool muted);
    bool delayActive() const;
    QString currentDelayColor() const;
    int delayRemainingMs() const;
    double delayProgress() const;

    void setPlaylist(PlaylistModel *playlist);

    Q_INVOKABLE void cue(int index);
    Q_INVOKABLE void play(int index = -1);
    Q_INVOKABLE void preview(int index);
    Q_INVOKABLE void previewRange(int index, double start, double end, double delayBefore, const QString &delayColor, bool audioEnabled, double volume, double speed);
    Q_INVOKABLE void playSource(int index, qint64 positionMs);
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void setVideoSink(QObject *sink);

signals:
    void currentIndexChanged();
    void currentFragmentChanged();
    void playingChanged();
    void playingSourceChanged();
    void stopAfterCurrentChanged();
    void mutedChanged();
    void delayStateChanged();
    void delayProgressChanged();
    void playbackError(const QString &message);

private:
    void startCurrentAfterDelay();
    void startCurrentNow();
    void setCurrentIndex(int index);
    void setPlaying(bool playing);
    void setPlayingSource(bool playingSource);
    void setStopAfterCurrent(bool value);
    void clearPreviewOverride();
    double effectiveStart(const Fragment &fragment) const;
    double effectiveEnd(const Fragment &fragment) const;
    double effectiveDelayBefore(const Fragment &fragment) const;
    QString effectiveDelayColor(const Fragment &fragment) const;
    bool effectiveAudioEnabled(const Fragment &fragment) const;
    double effectiveVolume(const Fragment &fragment) const;
    double effectiveSpeed(const Fragment &fragment) const;

    PlaylistModel *m_playlist = nullptr;
    std::unique_ptr<PlaybackBackend> m_backend;
    QTimer m_endTimer;
    QTimer m_delayTimer;
    QTimer m_delayProgressTimer;
    int m_currentIndex = -1;
    bool m_playing = false;
    bool m_playingSource = false;
    bool m_userMuted = false;
    bool m_stopAfterCurrent = false;
    bool m_hasPreviewOverride = false;
    qint64 m_startPositionOverrideMs = -1;
    double m_previewStart = 0.0;
    double m_previewEnd = 0.0;
    double m_previewDelayBefore = 0.0;
    QString m_previewDelayColor = QStringLiteral("#000000");
    bool m_previewAudioEnabled = true;
    double m_previewVolume = 1.0;
    double m_previewSpeed = 1.0;
    int m_delayDurationMs = 0;
    qint64 m_delayEndsAtMs = 0;
};
