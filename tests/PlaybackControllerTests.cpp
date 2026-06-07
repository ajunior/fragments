#include <QSignalSpy>
#include <QTest>

#include <memory>

#include "playback/PlaybackController.h"
#include "playlist/PlaylistModel.h"

class FakePlaybackBackend : public PlaybackBackend
{
public:
    explicit FakePlaybackBackend(QObject *parent = nullptr)
        : PlaybackBackend(parent)
    {
    }

    qint64 position() const override
    {
        return currentPosition;
    }

    void setMuted(bool value) override
    {
        muted = value;
    }

    void setVolume(double value) override
    {
        volume = value;
    }

    void setPlaybackRate(double value) override
    {
        playbackRate = value;
    }

    void setSource(const QUrl &value) override
    {
        source = value;
    }

    void setPosition(qint64 value) override
    {
        currentPosition = value;
    }

    void play() override
    {
        ++playCalls;
        emit playingChanged(true);
    }

    void pause() override
    {
        ++pauseCalls;
        emit playingChanged(false);
    }

    void stop() override
    {
        ++stopCalls;
        emit playingChanged(false);
    }

    void setVideoSink(QObject *value) override
    {
        videoSink = value;
    }

    qint64 currentPosition = 0;
    bool muted = false;
    double volume = 1.0;
    double playbackRate = 1.0;
    QUrl source;
    QObject *videoSink = nullptr;
    int playCalls = 0;
    int pauseCalls = 0;
    int stopCalls = 0;
};

class PlaybackControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void cueLoadsFragmentWithoutPlaying();
    void playStartsSelectedFragment();
    void playIgnoresOutOfRangeIndex();
    void delayedPlaybackMarksControllerPlayingBeforeBackendStarts();
    void delayedPlaybackExposesDelayVisualState();
    void previewStopsInsteadOfAdvancing();
    void previewRangeUsesDraftValuesWithoutChangingPlaylist();
    void nextAdvancesAndStopsAtEnd();
    void timerAdvancesWhenCurrentFragmentEnds();
    void timerStopsWhenLastFragmentEnds();
    void timerLoopsToFirstFragmentWhenRepeatIsEnabled();
    void backendErrorsAreForwarded();
    void previousClampsToFirstFragment();
    void pauseAndResumeForwardToBackend();
};

static void addConfiguredFragment(PlaylistModel &playlist,
                                  const QString &source,
                                  double start,
                                  double end,
                                  double delayBefore = 0.0,
                                  bool audioEnabled = true,
                                  double volume = 1.0,
                                  double speed = 1.0,
                                  const QString &delayColor = QStringLiteral("#000000"))
{
    const int row = playlist.count();
    playlist.addFragment(QUrl(QStringLiteral("file:///") + source));
    playlist.updateFragment(row, {
        {QStringLiteral("start"), start},
        {QStringLiteral("end"), end},
        {QStringLiteral("delayBefore"), delayBefore},
        {QStringLiteral("audioEnabled"), audioEnabled},
        {QStringLiteral("volume"), volume},
        {QStringLiteral("speed"), speed},
        {QStringLiteral("delayColor"), delayColor}
    });
}

static std::unique_ptr<PlaybackController> makeController(PlaylistModel &playlist, FakePlaybackBackend *backend)
{
    auto controller = std::make_unique<PlaybackController>(backend);
    controller->setPlaylist(&playlist);
    return controller;
}

void PlaybackControllerTests::cueLoadsFragmentWithoutPlaying()
{
    PlaylistModel playlist;
    addConfiguredFragment(playlist, QStringLiteral("clip-a.mp4"), 1.25, 3.0, 0.0, false, 0.35, 1.5);
    auto *backend = new FakePlaybackBackend;
    auto controller = makeController(playlist, backend);

    controller->cue(0);

    QCOMPARE(controller->currentIndex(), 0);
    QCOMPARE(controller->currentStart(), 1.25);
    QCOMPARE(controller->currentEnd(), 3.0);
    QVERIFY(!controller->playing());
    QCOMPARE(backend->stopCalls, 1);
    QCOMPARE(backend->playCalls, 0);
    QCOMPARE(backend->source, QUrl(QStringLiteral("file:///clip-a.mp4")));
    QCOMPARE(backend->currentPosition, 1250);
    QCOMPARE(backend->muted, true);
    QCOMPARE(backend->volume, 0.35);
    QCOMPARE(backend->playbackRate, 1.5);
}

void PlaybackControllerTests::playStartsSelectedFragment()
{
    PlaylistModel playlist;
    addConfiguredFragment(playlist, QStringLiteral("clip-a.mp4"), 0.0, 2.0);
    addConfiguredFragment(playlist, QStringLiteral("clip-b.mp4"), 2.5, 8.0, 0.0, true, 0.8, 1.25);
    auto *backend = new FakePlaybackBackend;
    auto controller = makeController(playlist, backend);
    QSignalSpy playingSpy(controller.get(), &PlaybackController::playingChanged);

    controller->play(1);

    QCOMPARE(controller->currentIndex(), 1);
    QCOMPARE(controller->currentStart(), 2.5);
    QCOMPARE(controller->currentEnd(), 8.0);
    QVERIFY(controller->playing());
    QCOMPARE(playingSpy.count(), 1);
    QCOMPARE(backend->stopCalls, 1);
    QCOMPARE(backend->playCalls, 1);
    QCOMPARE(backend->source, QUrl(QStringLiteral("file:///clip-b.mp4")));
    QCOMPARE(backend->currentPosition, 2500);
    QCOMPARE(backend->volume, 0.8);
    QCOMPARE(backend->playbackRate, 1.25);
}

void PlaybackControllerTests::playIgnoresOutOfRangeIndex()
{
    PlaylistModel playlist;
    addConfiguredFragment(playlist, QStringLiteral("clip-a.mp4"), 0.0, 2.0);
    auto *backend = new FakePlaybackBackend;
    auto controller = makeController(playlist, backend);

    controller->play(4);
    controller->preview(4);

    QCOMPARE(controller->currentIndex(), -1);
    QVERIFY(!controller->playing());
    QCOMPARE(backend->stopCalls, 0);
    QCOMPARE(backend->playCalls, 0);
}

void PlaybackControllerTests::delayedPlaybackMarksControllerPlayingBeforeBackendStarts()
{
    PlaylistModel playlist;
    addConfiguredFragment(playlist, QStringLiteral("clip-a.mp4"), 0.0, 2.0, 10.0);
    auto *backend = new FakePlaybackBackend;
    auto controller = makeController(playlist, backend);

    controller->play();

    QCOMPARE(controller->currentIndex(), 0);
    QVERIFY(controller->playing());
    QVERIFY(controller->delayActive());
    QVERIFY(controller->delayProgress() >= 0.0);
    QVERIFY(controller->delayProgress() <= 1.0);
    QVERIFY(controller->delayRemainingMs() > 0);
    QCOMPARE(backend->stopCalls, 1);
    QCOMPARE(backend->playCalls, 0);
}

void PlaybackControllerTests::delayedPlaybackExposesDelayVisualState()
{
    PlaylistModel playlist;
    addConfiguredFragment(playlist, QStringLiteral("clip-a.mp4"), 0.0, 2.0, 10.0, true, 1.0, 1.0, QStringLiteral("#1A73E8"));
    auto *backend = new FakePlaybackBackend;
    auto controller = makeController(playlist, backend);
    QSignalSpy delayStateSpy(controller.get(), &PlaybackController::delayStateChanged);

    controller->play();

    QVERIFY(controller->delayActive());
    QCOMPARE(controller->currentDelayColor(), QStringLiteral("#1a73e8"));
    QVERIFY(controller->delayRemainingMs() > 0);
    QVERIFY(controller->delayProgress() >= 0.0);
    QCOMPARE(delayStateSpy.count(), 1);

    controller->stop();

    QVERIFY(!controller->delayActive());
    QCOMPARE(controller->delayRemainingMs(), 0);
    QCOMPARE(controller->delayProgress(), 0.0);
    QCOMPARE(delayStateSpy.count(), 2);
}

void PlaybackControllerTests::previewStopsInsteadOfAdvancing()
{
    PlaylistModel playlist;
    addConfiguredFragment(playlist, QStringLiteral("clip-a.mp4"), 0.0, 2.0);
    addConfiguredFragment(playlist, QStringLiteral("clip-b.mp4"), 2.0, 4.0);
    auto *backend = new FakePlaybackBackend;
    auto controller = makeController(playlist, backend);

    controller->preview(0);
    controller->next();

    QCOMPARE(controller->currentIndex(), 0);
    QVERIFY(!controller->playing());
    QCOMPARE(backend->playCalls, 1);
    QCOMPARE(backend->stopCalls, 2);
}

void PlaybackControllerTests::previewRangeUsesDraftValuesWithoutChangingPlaylist()
{
    PlaylistModel playlist;
    addConfiguredFragment(playlist, QStringLiteral("clip-a.mp4"), 1.0, 9.0, 0.0, true, 1.0, 1.0);
    auto *backend = new FakePlaybackBackend;
    auto controller = makeController(playlist, backend);

    controller->previewRange(0, 2.25, 4.5, 0.0, QStringLiteral("#E53935"), false, 0.4, 1.5);

    QCOMPARE(controller->currentIndex(), 0);
    QCOMPARE(controller->currentStart(), 2.25);
    QCOMPARE(controller->currentEnd(), 4.5);
    QCOMPARE(controller->currentDelayColor(), QStringLiteral("#e53935"));
    QVERIFY(controller->playing());
    QCOMPARE(backend->source, QUrl(QStringLiteral("file:///clip-a.mp4")));
    QCOMPARE(backend->currentPosition, 2250);
    QCOMPARE(backend->muted, true);
    QCOMPARE(backend->volume, 0.4);
    QCOMPARE(backend->playbackRate, 1.5);

    const QVariantMap savedFragment = playlist.get(0);
    QCOMPARE(savedFragment.value(QStringLiteral("start")).toDouble(), 1.0);
    QCOMPARE(savedFragment.value(QStringLiteral("end")).toDouble(), 9.0);

    controller->cue(0);
    QCOMPARE(controller->currentStart(), 1.0);
    QCOMPARE(controller->currentEnd(), 9.0);
    QCOMPARE(backend->currentPosition, 1000);
}

void PlaybackControllerTests::nextAdvancesAndStopsAtEnd()
{
    PlaylistModel playlist;
    addConfiguredFragment(playlist, QStringLiteral("clip-a.mp4"), 0.0, 2.0);
    addConfiguredFragment(playlist, QStringLiteral("clip-b.mp4"), 2.0, 4.0);
    auto *backend = new FakePlaybackBackend;
    auto controller = makeController(playlist, backend);

    controller->play(0);
    controller->next();
    QCOMPARE(controller->currentIndex(), 1);
    QCOMPARE(backend->playCalls, 2);
    QCOMPARE(backend->source, QUrl(QStringLiteral("file:///clip-b.mp4")));

    controller->next();
    QCOMPARE(controller->currentIndex(), 1);
    QVERIFY(!controller->playing());
    QCOMPARE(backend->stopCalls, 3);
}

void PlaybackControllerTests::timerAdvancesWhenCurrentFragmentEnds()
{
    PlaylistModel playlist;
    addConfiguredFragment(playlist, QStringLiteral("clip-a.mp4"), 0.0, 2.0);
    addConfiguredFragment(playlist, QStringLiteral("clip-b.mp4"), 2.0, 4.0);
    auto *backend = new FakePlaybackBackend;
    auto controller = makeController(playlist, backend);

    controller->play(0);
    backend->currentPosition = 2000;

    QTRY_COMPARE_WITH_TIMEOUT(controller->currentIndex(), 1, 250);
    QCOMPARE(backend->source, QUrl(QStringLiteral("file:///clip-b.mp4")));
    QCOMPARE(backend->playCalls, 2);
}

void PlaybackControllerTests::timerStopsWhenLastFragmentEnds()
{
    PlaylistModel playlist;
    addConfiguredFragment(playlist, QStringLiteral("clip-a.mp4"), 0.0, 2.0);
    auto *backend = new FakePlaybackBackend;
    auto controller = makeController(playlist, backend);

    controller->play(0);
    backend->currentPosition = 2000;

    QTRY_VERIFY_WITH_TIMEOUT(!controller->playing(), 250);
    QCOMPARE(controller->currentIndex(), 0);
    QCOMPARE(backend->stopCalls, 2);
}

void PlaybackControllerTests::timerLoopsToFirstFragmentWhenRepeatIsEnabled()
{
    PlaylistModel playlist;
    playlist.setRepeat(true);
    addConfiguredFragment(playlist, QStringLiteral("clip-a.mp4"), 0.0, 2.0);
    addConfiguredFragment(playlist, QStringLiteral("clip-b.mp4"), 2.0, 4.0);
    auto *backend = new FakePlaybackBackend;
    auto controller = makeController(playlist, backend);

    controller->play(1);
    backend->currentPosition = 4000;

    QTRY_COMPARE_WITH_TIMEOUT(controller->currentIndex(), 0, 250);
    QVERIFY(controller->playing());
    QCOMPARE(backend->source, QUrl(QStringLiteral("file:///clip-a.mp4")));
    QCOMPARE(backend->playCalls, 2);
}

void PlaybackControllerTests::backendErrorsAreForwarded()
{
    PlaylistModel playlist;
    auto *backend = new FakePlaybackBackend;
    auto controller = makeController(playlist, backend);
    QSignalSpy errorSpy(controller.get(), &PlaybackController::playbackError);

    emit backend->errorOccurred(QStringLiteral("Unsupported media format"));

    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorSpy.takeFirst().at(0).toString(), QStringLiteral("Unsupported media format"));
}

void PlaybackControllerTests::previousClampsToFirstFragment()
{
    PlaylistModel playlist;
    addConfiguredFragment(playlist, QStringLiteral("clip-a.mp4"), 0.0, 2.0);
    addConfiguredFragment(playlist, QStringLiteral("clip-b.mp4"), 2.0, 4.0);
    auto *backend = new FakePlaybackBackend;
    auto controller = makeController(playlist, backend);

    controller->play(1);
    controller->previous();
    QCOMPARE(controller->currentIndex(), 0);
    QCOMPARE(backend->source, QUrl(QStringLiteral("file:///clip-a.mp4")));

    controller->previous();
    QCOMPARE(controller->currentIndex(), 0);
    QCOMPARE(backend->playCalls, 3);
}

void PlaybackControllerTests::pauseAndResumeForwardToBackend()
{
    PlaylistModel playlist;
    addConfiguredFragment(playlist, QStringLiteral("clip-a.mp4"), 0.0, 2.0);
    auto *backend = new FakePlaybackBackend;
    auto controller = makeController(playlist, backend);

    controller->play();
    QVERIFY(controller->playing());

    controller->pause();
    QVERIFY(!controller->playing());
    QCOMPARE(backend->pauseCalls, 1);

    controller->resume();
    QVERIFY(controller->playing());
    QCOMPARE(backend->playCalls, 2);
}

QTEST_GUILESS_MAIN(PlaybackControllerTests)

#include "PlaybackControllerTests.moc"
