#include "PlaybackController.h"

#include "QtMediaPlaybackBackend.h"
#include "playlist/Fragment.h"
#include "playlist/PlaylistModel.h"

#include <QDateTime>
#include <QRegularExpression>

#include <utility>

namespace {
QString normalizedColor(const QString &value)
{
    QString normalized = value.trimmed();
    static const QRegularExpression hexColor(QStringLiteral("^#[0-9a-fA-F]{6}$"));
    static const QRegularExpression hexColorWithAlpha(QStringLiteral("^#[0-9a-fA-F]{8}$"));
    if (hexColorWithAlpha.match(normalized).hasMatch()) {
        normalized = QStringLiteral("#") + normalized.mid(3);
    } else if (!hexColor.match(normalized).hasMatch()) {
        return QStringLiteral("#000000");
    }

    return normalized.toLower();
}
}

PlaybackController::PlaybackController(QObject *parent)
    : PlaybackController(new QtMediaPlaybackBackend, parent)
{
}

PlaybackController::PlaybackController(PlaybackBackend *backend, QObject *parent)
    : QObject(parent)
    , m_backend(backend ? backend : new QtMediaPlaybackBackend)
{
    m_endTimer.setInterval(40);
    connect(&m_endTimer, &QTimer::timeout, this, [this]() {
        if (!m_playlist || m_currentIndex < 0) {
            return;
        }

        const Fragment *fragment = m_playlist->fragmentAt(m_currentIndex);
        if (!fragment) {
            stop();
            return;
        }

        if (m_backend->position() >= static_cast<qint64>(effectiveEnd(*fragment) * 1000.0)) {
            next();
        }
    });

    connect(&m_delayTimer, &QTimer::timeout, this, &PlaybackController::startCurrentNow);
    m_delayProgressTimer.setInterval(50);
    connect(&m_delayProgressTimer, &QTimer::timeout, this, [this]() {
        emit delayProgressChanged();
    });
    connect(m_backend.get(), &PlaybackBackend::playingChanged, this, [this](bool playing) {
        setPlaying(playing || m_delayTimer.isActive());
    });
    connect(m_backend.get(), &PlaybackBackend::errorOccurred, this, [this](const QString &message) {
        emit playbackError(message);
    });
}

QMediaPlayer *PlaybackController::player()
{
    return m_backend->player();
}

int PlaybackController::currentIndex() const
{
    return m_currentIndex;
}

double PlaybackController::currentStart() const
{
    const Fragment *fragment = m_playlist ? m_playlist->fragmentAt(m_currentIndex) : nullptr;
    return fragment ? effectiveStart(*fragment) : 0.0;
}

double PlaybackController::currentEnd() const
{
    const Fragment *fragment = m_playlist ? m_playlist->fragmentAt(m_currentIndex) : nullptr;
    return fragment ? effectiveEnd(*fragment) : 0.0;
}

bool PlaybackController::playing() const
{
    return m_playing;
}

bool PlaybackController::delayActive() const
{
    return m_delayTimer.isActive();
}

QString PlaybackController::currentDelayColor() const
{
    const Fragment *fragment = m_playlist ? m_playlist->fragmentAt(m_currentIndex) : nullptr;
    return fragment ? effectiveDelayColor(*fragment) : QStringLiteral("#000000");
}

int PlaybackController::delayRemainingMs() const
{
    if (!m_delayTimer.isActive()) {
        return 0;
    }

    return static_cast<int>(qMax<qint64>(0, m_delayEndsAtMs - QDateTime::currentMSecsSinceEpoch()));
}

double PlaybackController::delayProgress() const
{
    if (!m_delayTimer.isActive() || m_delayDurationMs <= 0) {
        return 0.0;
    }

    const int elapsedMs = m_delayDurationMs - delayRemainingMs();
    return qBound(0.0, static_cast<double>(elapsedMs) / m_delayDurationMs, 1.0);
}

void PlaybackController::setPlaylist(PlaylistModel *playlist)
{
    m_playlist = playlist;
}

void PlaybackController::cue(int index)
{
    if (!m_playlist || index < 0 || index >= m_playlist->rowCount()) {
        return;
    }

    m_stopAfterCurrent = true;
    clearPreviewOverride();
    m_delayTimer.stop();
    m_endTimer.stop();
    m_backend->stop();
    setPlaying(false);
    setCurrentIndex(index);

    const Fragment *fragment = m_playlist->fragmentAt(m_currentIndex);
    if (!fragment) {
        return;
    }

    m_backend->setMuted(!fragment->audioEnabled);
    m_backend->setVolume(fragment->volume);
    m_backend->setPlaybackRate(fragment->speed);
    m_backend->setSource(fragment->source);
    m_backend->setPosition(static_cast<qint64>(fragment->start * 1000.0));
}

bool PlaybackController::playingSource() const
{
    return m_playingSource;
}

void PlaybackController::setPlayingSource(bool playingSource)
{
    if (m_playingSource == playingSource) {
        return;
    }
    m_playingSource = playingSource;
    emit playingSourceChanged();
}

void PlaybackController::play(int index)
{
    if (!m_playlist || m_playlist->rowCount() == 0) {
        return;
    }

    m_stopAfterCurrent = false;
    setPlayingSource(false);
    clearPreviewOverride();

    if (index >= m_playlist->rowCount()) {
        return;
    }

    if (index >= 0) {
        setCurrentIndex(index);
    } else if (m_currentIndex < 0) {
        setCurrentIndex(0);
    }

    startCurrentAfterDelay();
}

void PlaybackController::preview(int index)
{
    if (!m_playlist || m_playlist->rowCount() == 0 || index < 0 || index >= m_playlist->rowCount()) {
        return;
    }

    m_stopAfterCurrent = true;
    setPlayingSource(false);
    clearPreviewOverride();
    setCurrentIndex(index);
    startCurrentAfterDelay();
}

void PlaybackController::previewRange(int index, double start, double end, double delayBefore, const QString &delayColor, bool audioEnabled, double volume, double speed)
{
    if (!m_playlist || m_playlist->rowCount() == 0 || index < 0 || index >= m_playlist->rowCount() || end <= start) {
        return;
    }

    m_stopAfterCurrent = true;
    setPlayingSource(false);
    m_hasPreviewOverride = true;
    m_previewStart = qMax(0.0, start);
    m_previewEnd = qMax(m_previewStart, end);
    m_previewDelayBefore = qMax(0.0, delayBefore);
    m_previewDelayColor = normalizedColor(delayColor);
    m_previewAudioEnabled = audioEnabled;
    m_previewVolume = qBound(0.0, volume, 1.0);
    m_previewSpeed = qBound(0.25, speed, 3.0);
    setCurrentIndex(index);
    emit currentFragmentChanged();
    startCurrentAfterDelay();
}

void PlaybackController::playSource(int index, qint64 positionMs)
{
    if (!m_playlist || m_playlist->rowCount() == 0 || index < 0 || index >= m_playlist->rowCount()) {
        return;
    }

    m_stopAfterCurrent = true;
    clearPreviewOverride();
    setPlayingSource(true);
    m_startPositionOverrideMs = positionMs;
    setCurrentIndex(index);
    startCurrentNow();
}

void PlaybackController::pause()
{
    m_delayTimer.stop();
    m_delayProgressTimer.stop();
    m_delayDurationMs = 0;
    m_delayEndsAtMs = 0;
    emit delayStateChanged();
    emit delayProgressChanged();
    m_backend->pause();
    m_endTimer.stop();
    setPlaying(false);
}

void PlaybackController::resume()
{
    if (m_currentIndex < 0) {
        play(0);
        return;
    }

    m_backend->play();
    if (!m_playingSource) {
        m_endTimer.start();
    }
}

void PlaybackController::stop()
{
    m_stopAfterCurrent = false;
    setPlayingSource(false);
    clearPreviewOverride();
    m_delayTimer.stop();
    m_delayProgressTimer.stop();
    m_delayDurationMs = 0;
    m_delayEndsAtMs = 0;
    emit delayStateChanged();
    emit delayProgressChanged();
    m_endTimer.stop();
    m_backend->stop();
    setPlaying(false);
}

void PlaybackController::next()
{
    if (!m_playlist) {
        return;
    }

    if (m_stopAfterCurrent) {
        stop();
        return;
    }

    clearPreviewOverride();

    const int nextIndex = m_currentIndex + 1;
    if (nextIndex >= m_playlist->rowCount()) {
        if (m_playlist->repeat() && m_playlist->rowCount() > 0) {
            setCurrentIndex(0);
            startCurrentAfterDelay();
            return;
        }

        stop();
        return;
    }

    setCurrentIndex(nextIndex);
    startCurrentAfterDelay();
}

void PlaybackController::previous()
{
    if (!m_playlist || m_playlist->rowCount() == 0) {
        return;
    }

    clearPreviewOverride();
    setCurrentIndex(qMax(0, m_currentIndex - 1));
    startCurrentAfterDelay();
}

void PlaybackController::setVideoSink(QObject *sink)
{
    m_backend->setVideoSink(sink);
}

void PlaybackController::startCurrentAfterDelay()
{
    if (!m_playlist) {
        return;
    }

    const Fragment *fragment = m_playlist->fragmentAt(m_currentIndex);
    if (!fragment) {
        return;
    }

    m_backend->stop();
    m_endTimer.stop();

    const int delayMs = static_cast<int>(effectiveDelayBefore(*fragment) * 1000.0);
    if (delayMs > 0) {
        m_delayDurationMs = delayMs;
        m_delayEndsAtMs = QDateTime::currentMSecsSinceEpoch() + delayMs;
        setPlaying(true);
        m_delayTimer.start(delayMs);
        m_delayProgressTimer.start();
        emit delayStateChanged();
        emit delayProgressChanged();
        return;
    }

    startCurrentNow();
}

void PlaybackController::startCurrentNow()
{
    const bool wasDelaying = m_delayTimer.isActive();
    m_delayTimer.stop();
    m_delayProgressTimer.stop();
    m_delayDurationMs = 0;
    m_delayEndsAtMs = 0;
    if (wasDelaying) {
        emit delayStateChanged();
        emit delayProgressChanged();
    }

    if (!m_playlist) {
        return;
    }

    const Fragment *fragment = m_playlist->fragmentAt(m_currentIndex);
    if (!fragment) {
        return;
    }

    m_backend->setMuted(!effectiveAudioEnabled(*fragment));
    m_backend->setVolume(effectiveVolume(*fragment));
    m_backend->setPlaybackRate(effectiveSpeed(*fragment));
    m_backend->setSource(fragment->source);
    const qint64 startPos = m_startPositionOverrideMs >= 0
        ? std::exchange(m_startPositionOverrideMs, -1)
        : static_cast<qint64>(effectiveStart(*fragment) * 1000.0);
    m_backend->setPosition(startPos);
    m_backend->play();
    if (!m_playingSource) {
        m_endTimer.start();
    }
}

void PlaybackController::setCurrentIndex(int index)
{
    if (m_currentIndex == index) {
        return;
    }

    m_currentIndex = index;
    emit currentIndexChanged();
    emit currentFragmentChanged();
}

void PlaybackController::setPlaying(bool playing)
{
    if (m_playing == playing) {
        return;
    }

    m_playing = playing;
    emit playingChanged();
}

void PlaybackController::clearPreviewOverride()
{
    if (!m_hasPreviewOverride) {
        return;
    }

    m_hasPreviewOverride = false;
    emit currentFragmentChanged();
}

double PlaybackController::effectiveStart(const Fragment &fragment) const
{
    return m_hasPreviewOverride ? m_previewStart : fragment.start;
}

double PlaybackController::effectiveEnd(const Fragment &fragment) const
{
    return m_hasPreviewOverride ? m_previewEnd : fragment.end;
}

double PlaybackController::effectiveDelayBefore(const Fragment &fragment) const
{
    return m_hasPreviewOverride ? m_previewDelayBefore : fragment.delayBefore;
}

QString PlaybackController::effectiveDelayColor(const Fragment &fragment) const
{
    return m_hasPreviewOverride ? m_previewDelayColor : fragment.delayColor;
}

bool PlaybackController::effectiveAudioEnabled(const Fragment &fragment) const
{
    return m_hasPreviewOverride ? m_previewAudioEnabled : fragment.audioEnabled;
}

double PlaybackController::effectiveVolume(const Fragment &fragment) const
{
    return m_hasPreviewOverride ? m_previewVolume : fragment.volume;
}

double PlaybackController::effectiveSpeed(const Fragment &fragment) const
{
    return m_hasPreviewOverride ? m_previewSpeed : fragment.speed;
}
