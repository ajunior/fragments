#pragma once

#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QUrl>
#include <memory>

class PlaylistModel;

class ExportController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(bool ffmpegAvailable READ ffmpegAvailable NOTIFY ffmpegAvailableChanged)
    Q_PROPERTY(QString exportReadinessMessage READ exportReadinessMessage NOTIFY ffmpegAvailableChanged)
    Q_PROPERTY(bool mp4ExportAvailable READ mp4ExportAvailable NOTIFY ffmpegAvailableChanged)
    Q_PROPERTY(bool gifExportAvailable READ gifExportAvailable NOTIFY ffmpegAvailableChanged)
    Q_PROPERTY(QString mp4ExportReadinessMessage READ mp4ExportReadinessMessage NOTIFY ffmpegAvailableChanged)
    Q_PROPERTY(QString gifExportReadinessMessage READ gifExportReadinessMessage NOTIFY ffmpegAvailableChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit ExportController(QObject *parent = nullptr);

    bool running() const;
    bool ffmpegAvailable() const;
    QString exportReadinessMessage() const;
    bool mp4ExportAvailable() const;
    bool gifExportAvailable() const;
    QString mp4ExportReadinessMessage() const;
    QString gifExportReadinessMessage() const;
    double progress() const;
    QString status() const;

    void setPlaylist(PlaylistModel *playlist);

    Q_INVOKABLE bool exportTo(const QUrl &outputUrl, int gifFps = 15, int row = -1);
    Q_INVOKABLE bool exportGifTo(const QUrl &outputUrl, int gifFps = 15, int row = -1);
    Q_INVOKABLE void cancel();

signals:
    void runningChanged();
    void ffmpegAvailableChanged();
    void progressChanged();
    void statusChanged();
    void exportFinished(const QUrl &outputUrl);
    void exportFailed(const QString &message);

private:
    struct ExportItem {
        int row = -1;
        QString sourcePath;
        QString segmentPath;
        double start = 0.0;
        double end = 0.0;
        double delayBefore = 0.0;
        QString delayColor = QStringLiteral("#000000");
        bool audioEnabled = true;
        double volume = 1.0;
        double speed = 1.0;
        bool hasVideo = false;
        bool hasAudio = false;
    };

    enum class ExportFormat {
        Mp4,
        Gif
    };

    bool startExport(const QUrl &outputUrl, ExportFormat format, int gifFps, int row);
    void startNextSegment();
    void startConcat();
    void startGifPalette();
    void startGifRender();
    void finishSuccess();
    void fail(const QString &message);
    void setRunning(bool running);
    void setProgress(double progress);
    void setStatus(const QString &status);
    QStringList segmentArguments(const ExportItem &item) const;
    QStringList concatArguments() const;
    QStringList gifPaletteArguments() const;
    QStringList gifRenderArguments() const;
    QString atempoFilter(double speed) const;
    double outputDuration(const ExportItem &item) const;
    static QString ffmpegColor(const QString &color);
    QString ffmpegExecutable() const;
    QString ffprobeExecutable() const;
    QString readinessMessageForFormat(ExportFormat format) const;
    void ensureExportReadinessChecked() const;
    QString commonToolingReadinessMessage() const;
    QString codecPreflightError(ExportFormat format) const;
    QString exportBlockerMessage() const;
    QString currentStageLabel() const;
    bool probeMedia(ExportItem *item, QString *errorMessage) const;
    static QString processOutput(const QString &program, const QStringList &arguments, int timeoutMs = 5000);
    static bool outputContainsLineToken(const QString &output, const QString &token);
    static QString conciseProcessError(const QString &error);
    static QString localPathFromUrl(const QUrl &url);
    static QString escapedConcatPath(const QString &path);

    PlaylistModel *m_playlist = nullptr;
    QProcess m_process;
    std::unique_ptr<QTemporaryDir> m_tempDir;
    QVector<ExportItem> m_items;
    QUrl m_outputUrl;
    QString m_outputPath;
    QString m_concatListPath;
    QString m_intermediateVideoPath;
    QString m_gifPalettePath;
    int m_exportRow = -1;
    int m_currentItem = 0;
    bool m_running = false;
    bool m_concatenating = false;
    bool m_generatingGifPalette = false;
    bool m_renderingGif = false;
    ExportFormat m_format = ExportFormat::Mp4;
    int m_gifFps = 15;
    double m_progress = 0.0;
    QString m_status;
    mutable bool m_exportReadinessChecked = false;
    mutable QString m_mp4ExportReadinessMessage;
    mutable QString m_gifExportReadinessMessage;
};
