#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QDateTime>

#include "playlist/PlaylistModel.h"

class PlaylistModelTests : public QObject
{
    Q_OBJECT

private slots:
    void newPlaylistClearsFileUrlAndModified();
    void invalidWhenEndBeforeStart();
    void missingLocalSourceInvalid();
    void relinkMissingFromFolderMatchesByFileName();
    void relinkSourceUpdatesValidity();
    void moveFragmentReordersRows();
    void duplicateFragmentCopiesItemAfterSource();
    void undoRedoRestoresMetadataAndStructure();
    void saveAndLoadRoundTrip();
    void metadataRoundTrips();
    void loadRejectsInvalidPlaylistJson();
    void playlistNameValidationAndSuggestedFileName();
    void updatedAtChangesButCreatedAtStaysStable();

private:
    static QUrl createMediaFile(QTemporaryDir &dir, const QString &name);
};

QUrl PlaylistModelTests::createMediaFile(QTemporaryDir &dir, const QString &name)
{
    const QString path = dir.filePath(name);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    if (file.write("media") <= 0) {
        return {};
    }
    file.close();
    return QUrl::fromLocalFile(path);
}

void PlaylistModelTests::newPlaylistClearsFileUrlAndModified()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PlaylistModel model;
    model.setName(QStringLiteral("playlist_one"));
    model.addFragment(createMediaFile(dir, QStringLiteral("a.mp4")));
    QVERIFY(model.save(QUrl::fromLocalFile(dir.filePath(QStringLiteral("playlist.json")))));
    QVERIFY(!model.fileUrl().isEmpty());
    QVERIFY(!model.modified());

    model.addFragment(createMediaFile(dir, QStringLiteral("b.mp4")));
    QVERIFY(model.modified());

    model.newPlaylist();
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.name(), QStringLiteral("Untitled"));
    QVERIFY(model.nameValid());
    QVERIFY(!model.repeat());
    QVERIFY(!model.createdAt().isValid());
    QVERIFY(!model.updatedAt().isValid());
    QVERIFY(model.fileUrl().isEmpty());
    QVERIFY(!model.modified());
    QVERIFY(model.valid());
}

void PlaylistModelTests::invalidWhenEndBeforeStart()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PlaylistModel model;
    model.addFragment(createMediaFile(dir, QStringLiteral("a.mp4")));
    model.updateFragment(0, {
        {QStringLiteral("start"), 10.0},
        {QStringLiteral("end"), 5.0}
    });

    QVERIFY(!model.valid());
    const QVariantMap item = model.get(0);
    QCOMPARE(item.value(QStringLiteral("valid")).toBool(), false);
    QCOMPARE(item.value(QStringLiteral("validationMessage")).toString(), QStringLiteral("End time must be greater than start time."));
}

void PlaylistModelTests::missingLocalSourceInvalid()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PlaylistModel model;
    model.addFragment(QUrl::fromLocalFile(dir.filePath(QStringLiteral("missing.mp4"))));

    QVERIFY(!model.valid());
    const QVariantMap item = model.get(0);
    QCOMPARE(item.value(QStringLiteral("sourceStatus")).toString(), QStringLiteral("Missing"));
    QCOMPARE(item.value(QStringLiteral("valid")).toBool(), false);
    QCOMPARE(item.value(QStringLiteral("validationMessage")).toString(), QStringLiteral("Source file does not exist."));
}

void PlaylistModelTests::relinkMissingFromFolderMatchesByFileName()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PlaylistModel model;
    model.addFragment(QUrl::fromLocalFile(dir.filePath(QStringLiteral("old/missing.mp4"))));
    QVERIFY(!model.valid());
    QCOMPARE(model.missingSources().size(), 1);

    const QUrl replacement = createMediaFile(dir, QStringLiteral("new/missing.mp4"));
    QVERIFY(replacement.isValid());

    QCOMPARE(model.relinkMissingFromFolder(QUrl::fromLocalFile(dir.filePath(QStringLiteral("new")))), 1);
    QVERIFY(model.valid());
    QCOMPARE(model.missingSources().size(), 0);
    QCOMPARE(model.get(0).value(QStringLiteral("source")).toUrl(), replacement);
}

void PlaylistModelTests::relinkSourceUpdatesValidity()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PlaylistModel model;
    model.addFragment(QUrl::fromLocalFile(dir.filePath(QStringLiteral("missing.mp4"))));
    QVERIFY(!model.valid());

    QSignalSpy validitySpy(&model, &PlaylistModel::validityChanged);
    model.relinkSource(0, createMediaFile(dir, QStringLiteral("replacement.mp4")));

    QVERIFY(model.valid());
    QVERIFY(validitySpy.count() > 0);
    const QVariantMap item = model.get(0);
    QCOMPARE(item.value(QStringLiteral("fileName")).toString(), QStringLiteral("replacement.mp4"));
    QCOMPARE(item.value(QStringLiteral("sourceStatus")).toString(), QStringLiteral("Available"));
}

void PlaylistModelTests::moveFragmentReordersRows()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PlaylistModel model;
    model.addFragment(createMediaFile(dir, QStringLiteral("a.mp4")));
    model.addFragment(createMediaFile(dir, QStringLiteral("b.mp4")));
    model.addFragment(createMediaFile(dir, QStringLiteral("c.mp4")));

    model.moveFragment(0, 2);

    QCOMPARE(model.get(0).value(QStringLiteral("fileName")).toString(), QStringLiteral("b.mp4"));
    QCOMPARE(model.get(1).value(QStringLiteral("fileName")).toString(), QStringLiteral("c.mp4"));
    QCOMPARE(model.get(2).value(QStringLiteral("fileName")).toString(), QStringLiteral("a.mp4"));
}

void PlaylistModelTests::duplicateFragmentCopiesItemAfterSource()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PlaylistModel model;
    model.setName(QStringLiteral("Duplicate_test"));
    const QUrl media = createMediaFile(dir, QStringLiteral("a.mp4"));
    model.addFragment(media);
    model.updateFragment(0, {
        {QStringLiteral("start"), 1.25},
        {QStringLiteral("end"), 3.5},
        {QStringLiteral("delayBefore"), 0.75},
        {QStringLiteral("audioEnabled"), false},
        {QStringLiteral("volume"), 0.4},
        {QStringLiteral("speed"), 1.5},
        {QStringLiteral("label"), QStringLiteral("Intro sting")},
        {QStringLiteral("notes"), QStringLiteral("Fade lights before playback")},
        {QStringLiteral("delayColor"), QStringLiteral("#1A73E8")}
    });
    QVERIFY(model.save(QUrl::fromLocalFile(dir.filePath(QStringLiteral("playlist.json")))));
    QVERIFY(!model.modified());

    QSignalSpy countSpy(&model, &PlaylistModel::countChanged);
    const int duplicateRow = model.duplicateFragment(0);

    QCOMPARE(duplicateRow, 1);
    QCOMPARE(model.count(), 2);
    QCOMPARE(countSpy.count(), 1);
    QVERIFY(model.modified());

    const QVariantMap original = model.get(0);
    const QVariantMap duplicate = model.get(1);
    QCOMPARE(duplicate.value(QStringLiteral("source")).toUrl(), media);
    QCOMPARE(duplicate.value(QStringLiteral("start")).toDouble(), original.value(QStringLiteral("start")).toDouble());
    QCOMPARE(duplicate.value(QStringLiteral("end")).toDouble(), original.value(QStringLiteral("end")).toDouble());
    QCOMPARE(duplicate.value(QStringLiteral("delayBefore")).toDouble(), original.value(QStringLiteral("delayBefore")).toDouble());
    QCOMPARE(duplicate.value(QStringLiteral("audioEnabled")).toBool(), original.value(QStringLiteral("audioEnabled")).toBool());
    QCOMPARE(duplicate.value(QStringLiteral("volume")).toDouble(), original.value(QStringLiteral("volume")).toDouble());
    QCOMPARE(duplicate.value(QStringLiteral("speed")).toDouble(), original.value(QStringLiteral("speed")).toDouble());
    QCOMPARE(duplicate.value(QStringLiteral("label")).toString(), original.value(QStringLiteral("label")).toString());
    QCOMPARE(duplicate.value(QStringLiteral("notes")).toString(), original.value(QStringLiteral("notes")).toString());
    QCOMPARE(duplicate.value(QStringLiteral("delayColor")).toString(), original.value(QStringLiteral("delayColor")).toString());
}

void PlaylistModelTests::undoRedoRestoresMetadataAndStructure()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PlaylistModel model;
    model.addFragment(createMediaFile(dir, QStringLiteral("a.mp4")));
    model.addFragment(createMediaFile(dir, QStringLiteral("b.mp4")));
    QVERIFY(model.canUndo());
    QVERIFY(!model.canRedo());

    model.updateFragment(0, {
        {QStringLiteral("start"), 2.0},
        {QStringLiteral("end"), 5.0},
        {QStringLiteral("label"), QStringLiteral("Edited")}
    });
    QCOMPARE(model.get(0).value(QStringLiteral("start")).toDouble(), 2.0);
    QCOMPARE(model.get(0).value(QStringLiteral("label")).toString(), QStringLiteral("Edited"));

    model.undo();
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.get(0).value(QStringLiteral("start")).toDouble(), 0.0);
    QCOMPARE(model.get(0).value(QStringLiteral("label")).toString(), QString());
    QVERIFY(model.canRedo());

    model.redo();
    QCOMPARE(model.get(0).value(QStringLiteral("start")).toDouble(), 2.0);
    QCOMPARE(model.get(0).value(QStringLiteral("label")).toString(), QStringLiteral("Edited"));

    model.removeFragment(1);
    QCOMPARE(model.count(), 1);

    model.undo();
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.get(1).value(QStringLiteral("fileName")).toString(), QStringLiteral("b.mp4"));

    model.redo();
    QCOMPARE(model.count(), 1);
}

void PlaylistModelTests::saveAndLoadRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QUrl media = createMediaFile(dir, QStringLiteral("a.mp4"));
    const QUrl playlist = QUrl::fromLocalFile(dir.filePath(QStringLiteral("playlist.json")));

    PlaylistModel saved;
    saved.setName(QStringLiteral("Show_opener"));
    saved.setRepeat(true);
    saved.addFragment(media);
    saved.updateFragment(0, {
        {QStringLiteral("start"), 1.25},
        {QStringLiteral("end"), 3.5},
        {QStringLiteral("delayBefore"), 0.75},
        {QStringLiteral("audioEnabled"), false},
        {QStringLiteral("volume"), 0.4},
        {QStringLiteral("speed"), 1.5},
        {QStringLiteral("label"), QStringLiteral("Opening")},
        {QStringLiteral("notes"), QStringLiteral("Start after presenter cue")},
        {QStringLiteral("delayColor"), QStringLiteral("#E53935")}
    });
    QVERIFY(saved.save(playlist));

    PlaylistModel loaded;
    QVERIFY(loaded.load(playlist));
    QCOMPARE(loaded.count(), 1);
    QVERIFY(loaded.valid());
    QVERIFY(!loaded.modified());
    QCOMPARE(loaded.fileUrl(), playlist);
    QCOMPARE(loaded.name(), QStringLiteral("Show_opener"));
    QCOMPARE(loaded.repeat(), true);
    QVERIFY(loaded.createdAt().isValid());
    QVERIFY(loaded.updatedAt().isValid());

    const QVariantMap item = loaded.get(0);
    QCOMPARE(item.value(QStringLiteral("source")).toUrl(), media);
    QCOMPARE(item.value(QStringLiteral("start")).toDouble(), 1.25);
    QCOMPARE(item.value(QStringLiteral("end")).toDouble(), 3.5);
    QCOMPARE(item.value(QStringLiteral("delayBefore")).toDouble(), 0.75);
    QCOMPARE(item.value(QStringLiteral("audioEnabled")).toBool(), false);
    QCOMPARE(item.value(QStringLiteral("volume")).toDouble(), 0.4);
    QCOMPARE(item.value(QStringLiteral("speed")).toDouble(), 1.5);
    QCOMPARE(item.value(QStringLiteral("label")).toString(), QStringLiteral("Opening"));
    QCOMPARE(item.value(QStringLiteral("notes")).toString(), QStringLiteral("Start after presenter cue"));
    QCOMPARE(item.value(QStringLiteral("delayColor")).toString(), QStringLiteral("#e53935"));
}

void PlaylistModelTests::metadataRoundTrips()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QUrl playlistUrl = QUrl::fromLocalFile(dir.filePath(QStringLiteral("playlist.json")));

    PlaylistModel saved;
    saved.setName(QStringLiteral("Late_night-loop"));
    saved.setRepeat(true);
    QVERIFY(saved.modified());
    QVERIFY(saved.save(playlistUrl));

    PlaylistModel loaded;
    QVERIFY(loaded.load(playlistUrl));
    QCOMPARE(loaded.name(), QStringLiteral("Late_night-loop"));
    QCOMPARE(loaded.repeat(), true);
    QCOMPARE(loaded.count(), 0);
    QVERIFY(loaded.createdAt().isValid());
    QVERIFY(loaded.updatedAt().isValid());
    QVERIFY(!loaded.modified());
}

void PlaylistModelTests::loadRejectsInvalidPlaylistJson()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    auto writePlaylist = [&dir](const QString &fileName, const QByteArray &content) {
        const QString path = dir.filePath(fileName);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return QUrl();
        }
        file.write(content);
        file.close();
        return QUrl::fromLocalFile(path);
    };

    const QList<QPair<QByteArray, QString>> invalidPlaylists {
        {
            R"({"version":1,"repeat":false,"createdAt":"2026-06-05T18:23:00.000Z","updatedAt":"2026-06-05T18:23:00.000Z","items":[]})",
            QStringLiteral("Playlist is missing required property 'name'.")
        },
        {
            R"({"version":1,"name":"playlist_one","repeat":false,"createdAt":"2026-06-05T18:23:00.000Z","updatedAt":"2026-06-05T18:23:00.000Z","items":{}})",
            QStringLiteral("Playlist.items must be an array, got object.")
        },
        {
            R"({"version":2,"name":"playlist_one","repeat":false,"createdAt":"2026-06-05T18:23:00.000Z","updatedAt":"2026-06-05T18:23:00.000Z","items":[]})",
            QStringLiteral("Playlist.version must be 1.")
        },
        {
            R"({"version":1,"name":"playlist_one","repeat":false,"createdAt":"2026-06-05T18:23:00.000Z","updatedAt":"2026-06-05T18:23:00.000Z","items":[{"file":"file:///tmp/a.mp4","start":0,"end":10,"delayBefore":0,"audio":true,"volume":1,"speed":1,"label":"","notes":"","delayColor":"#E53935"}]})",
            QStringLiteral("Playlist.items[0].delayColor must use lowercase #rrggbb format.")
        },
        {
            R"({"version":1,"name":"playlist_one","repeat":false,"createdAt":"2026-06-05T18:23:00.000Z","updatedAt":"2026-06-05T18:23:00.000Z","items":[{"file":"file:///tmp/a.mp4","start":0,"end":10,"delayBefore":0,"audio":true,"volume":1,"speed":1,"label":"","notes":"","delayColor":"#e53935","extra":true}]})",
            QStringLiteral("Playlist.items[0] has unknown property 'extra'.")
        }
    };

    for (int i = 0; i < invalidPlaylists.size(); ++i) {
        PlaylistModel model;
        model.setName(QStringLiteral("Existing"));
        model.addFragment(createMediaFile(dir, QStringLiteral("existing_%1.mp4").arg(i)));
        const int previousCount = model.count();

        QSignalSpy failedSpy(&model, &PlaylistModel::loadFailed);
        const QUrl playlistUrl = writePlaylist(QStringLiteral("invalid_%1.json").arg(i), invalidPlaylists.at(i).first);

        QVERIFY(!model.load(playlistUrl));
        QCOMPARE(model.count(), previousCount);
        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(failedSpy.takeFirst().at(0).toString(), invalidPlaylists.at(i).second);
    }
}

void PlaylistModelTests::playlistNameValidationAndSuggestedFileName()
{
    PlaylistModel model;
    QVERIFY(!model.nameValid());
    QVERIFY(!model.isValidName(QString()));
    QVERIFY(!model.isValidName(QStringLiteral("1_playlist")));
    QVERIFY(!model.isValidName(QStringLiteral("-playlist")));
    QVERIFY(!model.isValidName(QStringLiteral("show opener")));
    QVERIFY(!model.isValidName(QStringLiteral("show.opener")));
    QVERIFY(model.isValidName(QStringLiteral("_playlist")));
    QVERIFY(model.isValidName(QStringLiteral("Show_Opener-01")));

    model.setName(QStringLiteral("Show_Opener-01"));
    QVERIFY(model.nameValid());
    QCOMPARE(model.name(), QStringLiteral("Show_Opener-01"));
    QCOMPARE(model.suggestedFileName(), QStringLiteral("show_opener_01.json"));

    model.setName(QStringLiteral("invalid name"));
    QCOMPARE(model.name(), QStringLiteral("Show_Opener-01"));
}

void PlaylistModelTests::updatedAtChangesButCreatedAtStaysStable()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PlaylistModel model;
    model.setName(QStringLiteral("Updated_playlist"));
    const QDateTime createdAt = model.createdAt();
    const QDateTime initialUpdatedAt = model.updatedAt();
    QVERIFY(!createdAt.isValid());
    QVERIFY(!initialUpdatedAt.isValid());

    QTest::qWait(20);
    model.setName(QStringLiteral("Updated_playlist_2"));

    QCOMPARE(model.createdAt(), createdAt);
    QCOMPARE(model.updatedAt(), initialUpdatedAt);
    QVERIFY(model.modified());

    QVERIFY(model.save(QUrl::fromLocalFile(dir.filePath(QStringLiteral("playlist.json")))));
    QVERIFY(model.createdAt().isValid());
    QVERIFY(model.updatedAt().isValid());
    QCOMPARE(model.createdAt(), model.updatedAt());
    QVERIFY(!model.modified());
}

QTEST_MAIN(PlaylistModelTests)

#include "PlaylistModelTests.moc"
