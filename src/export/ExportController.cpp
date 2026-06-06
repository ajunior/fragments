#include "ExportController.h"

#include "playlist/Fragment.h"
#include "playlist/PlaylistModel.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>

ExportController::ExportController(QObject *parent)
    : QObject(parent)
{
    connect(&m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (!m_running) {
            return;
        }

        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            const QString error = conciseProcessError(QString::fromUtf8(m_process.readAllStandardError()).trimmed());
            const QString stage = currentStageLabel();
            fail(error.isEmpty()
                     ? QStringLiteral("ffmpeg failed while %1.").arg(stage)
                     : QStringLiteral("ffmpeg failed while %1: %2").arg(stage, error));
            return;
        }

        if (m_concatenating) {
            if (m_format == ExportFormat::Gif) {
                startGifPalette();
            } else {
                finishSuccess();
            }
            return;
        }

        if (m_generatingGifPalette) {
            startGifRender();
            return;
        }

        if (m_renderingGif) {
            finishSuccess();
            return;
        }

        ++m_currentItem;
        startNextSegment();
    });

    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (!m_running) {
            return;
        }

        if (error == QProcess::FailedToStart) {
            fail(QStringLiteral("Could not start ffmpeg. Install ffmpeg and make sure it is on PATH."));
        } else if (error == QProcess::Crashed) {
            fail(QStringLiteral("ffmpeg crashed while %1.").arg(currentStageLabel()));
        } else {
            fail(QStringLiteral("ffmpeg failed while %1.").arg(currentStageLabel()));
        }
    });
}

bool ExportController::running() const
{
    return m_running;
}

double ExportController::progress() const
{
    return m_progress;
}

bool ExportController::ffmpegAvailable() const
{
    return mp4ExportAvailable() || gifExportAvailable();
}

QString ExportController::exportReadinessMessage() const
{
    return mp4ExportReadinessMessage();
}

bool ExportController::mp4ExportAvailable() const
{
    return mp4ExportReadinessMessage().isEmpty();
}

bool ExportController::gifExportAvailable() const
{
    return gifExportReadinessMessage().isEmpty();
}

QString ExportController::mp4ExportReadinessMessage() const
{
    ensureExportReadinessChecked();
    return m_mp4ExportReadinessMessage;
}

QString ExportController::gifExportReadinessMessage() const
{
    ensureExportReadinessChecked();
    return m_gifExportReadinessMessage;
}

QString ExportController::status() const
{
    return m_status;
}

void ExportController::setPlaylist(PlaylistModel *playlist)
{
    m_playlist = playlist;
}

bool ExportController::exportTo(const QUrl &outputUrl, int gifFps, int row)
{
    return startExport(outputUrl, ExportFormat::Mp4, gifFps, 1, row);
}

bool ExportController::exportGifTo(const QUrl &outputUrl, int gifFps, int quality, int row)
{
    return startExport(outputUrl, ExportFormat::Gif, gifFps, quality, row);
}

bool ExportController::startExport(const QUrl &outputUrl, ExportFormat format, int gifFps, int gifQuality, int row)
{
    if (m_running) {
        return false;
    }

    if (!m_playlist || m_playlist->rowCount() == 0) {
        fail(QStringLiteral("There are no fragments to export."));
        return false;
    }

    const QString readinessMessage = readinessMessageForFormat(format);
    if (!readinessMessage.isEmpty()) {
        fail(readinessMessage);
        return false;
    }

    const QString blockerMessage = exportBlockerMessage();
    if (!blockerMessage.isEmpty()) {
        fail(blockerMessage);
        return false;
    }

    m_outputPath = localPathFromUrl(outputUrl);
    if (m_outputPath.isEmpty()) {
        fail(QStringLiteral("Choose an output file."));
        return false;
    }

    m_tempDir = std::make_unique<QTemporaryDir>();
    if (!m_tempDir->isValid()) {
        fail(QStringLiteral("Could not create a temporary export folder."));
        return false;
    }

    if (row >= 0 && row >= m_playlist->rowCount()) {
        fail(QStringLiteral("Select a fragment to export."));
        return false;
    }

    m_exportRow = row;

    m_items.clear();
    for (int playlistRow = 0; playlistRow < m_playlist->rowCount(); ++playlistRow) {
        if (m_exportRow >= 0 && playlistRow != m_exportRow) {
            continue;
        }

        const Fragment *fragment = m_playlist->fragmentAt(playlistRow);
        if (!fragment) {
            continue;
        }

        if (!fragment->source.isLocalFile()) {
            fail(QStringLiteral("Export currently supports local media files only."));
            return false;
        }

        ExportItem item;
        item.row = playlistRow;
        item.sourcePath = fragment->source.toLocalFile();
        item.segmentPath = m_tempDir->filePath(QStringLiteral("segment_%1.mp4").arg(playlistRow, 4, 10, QLatin1Char('0')));
        item.start = fragment->start;
        item.end = fragment->end;
        item.delayBefore = fragment->delayBefore;
        item.delayColor = fragment->delayColor;
        item.audioEnabled = fragment->audioEnabled;
        item.volume = fragment->volume;
        item.speed = fragment->speed;

        QString probeError;
        if (!probeMedia(&item, &probeError)) {
            fail(probeError);
            return false;
        }

        m_items.append(item);
    }

    if (m_items.isEmpty()) {
        fail(QStringLiteral("There are no exportable fragments."));
        return false;
    }

    QFileInfo outputInfo(m_outputPath);
    if (!outputInfo.dir().exists()) {
        fail(QStringLiteral("The output folder does not exist."));
        return false;
    }
    if (!QFileInfo(outputInfo.dir().absolutePath()).isWritable()) {
        fail(QStringLiteral("The output folder is not writable."));
        return false;
    }

    m_outputUrl = outputUrl;
    m_format = format;
    m_gifFps = qBound(1, gifFps, 60);
    m_gifQuality = qBound(0, gifQuality, 2);
    m_currentItem = 0;
    m_concatenating = false;
    m_generatingGifPalette = false;
    m_renderingGif = false;
    m_intermediateVideoPath.clear();
    m_gifPalettePath.clear();
    setProgress(0.0);
    setRunning(true);
    startNextSegment();
    return true;
}

void ExportController::cancel()
{
    if (!m_running) {
        return;
    }

    m_process.kill();
    m_process.waitForFinished(1000);
    fail(QStringLiteral("Export canceled."));
}

void ExportController::startNextSegment()
{
    if (m_currentItem >= m_items.size()) {
        startConcat();
        return;
    }

    const ExportItem &item = m_items.at(m_currentItem);
    const int totalSteps = m_items.size() + (m_format == ExportFormat::Gif ? 3 : 1);
    setProgress(static_cast<double>(m_currentItem) / totalSteps);
    setStatus(QStringLiteral("Rendering fragment %1 of %2").arg(m_currentItem + 1).arg(m_items.size()));
    m_process.start(ffmpegExecutable(), segmentArguments(item));
}

void ExportController::startConcat()
{
    m_concatenating = true;
    const int totalSteps = m_items.size() + (m_format == ExportFormat::Gif ? 3 : 1);
    setProgress(static_cast<double>(m_items.size()) / totalSteps);
    setStatus(QStringLiteral("Combining fragments"));
    if (m_format == ExportFormat::Gif) {
        m_intermediateVideoPath = m_tempDir->filePath(QStringLiteral("gif_source.mp4"));
        m_gifPalettePath = m_tempDir->filePath(QStringLiteral("gif_palette.png"));
    }

    m_concatListPath = m_tempDir->filePath(QStringLiteral("concat.txt"));
    QFile listFile(m_concatListPath);
    if (!listFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        fail(listFile.errorString());
        return;
    }

    QTextStream stream(&listFile);
    for (const ExportItem &item : std::as_const(m_items)) {
        stream << "file '" << escapedConcatPath(item.segmentPath) << "'\n";
    }
    listFile.close();

    m_process.start(ffmpegExecutable(), concatArguments());
}

void ExportController::startGifPalette()
{
    m_concatenating = false;
    m_generatingGifPalette = true;
    const int totalSteps = m_items.size() + 3;
    setProgress(static_cast<double>(m_items.size() + 1) / totalSteps);
    setStatus(QStringLiteral("Generating GIF palette"));
    m_process.start(ffmpegExecutable(), gifPaletteArguments());
}

void ExportController::startGifRender()
{
    m_generatingGifPalette = false;
    m_renderingGif = true;
    const int totalSteps = m_items.size() + 3;
    setProgress(static_cast<double>(m_items.size() + 2) / totalSteps);
    setStatus(QStringLiteral("Rendering GIF"));
    m_process.start(ffmpegExecutable(), gifRenderArguments());
}

void ExportController::finishSuccess()
{
    setProgress(1.0);
    setStatus(m_format == ExportFormat::Gif ? QStringLiteral("GIF export complete") : QStringLiteral("Export complete"));
    setRunning(false);
    m_tempDir.reset();
    emit exportFinished(m_outputUrl);
}

void ExportController::fail(const QString &message)
{
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
    }

    setRunning(false);
    setStatus(message);
    m_concatenating = false;
    m_generatingGifPalette = false;
    m_renderingGif = false;
    m_tempDir.reset();
    emit exportFailed(message);
}

void ExportController::setRunning(bool running)
{
    if (m_running == running) {
        return;
    }

    m_running = running;
    emit runningChanged();
}

void ExportController::setProgress(double progress)
{
    const double normalized = qBound(0.0, progress, 1.0);
    if (qFuzzyCompare(m_progress, normalized)) {
        return;
    }

    m_progress = normalized;
    emit progressChanged();
}

void ExportController::setStatus(const QString &status)
{
    if (m_status == status) {
        return;
    }

    m_status = status;
    emit statusChanged();
}

QStringList ExportController::segmentArguments(const ExportItem &item) const
{
    QString videoFilter;
    if (item.hasVideo) {
        videoFilter = QStringLiteral("setpts=PTS/%1").arg(item.speed, 0, 'f', 4);
        if (item.delayBefore > 0.0) {
            videoFilter += QStringLiteral(",tpad=start_duration=%1:start_mode=add:color=%2")
                               .arg(item.delayBefore, 0, 'f', 3)
                               .arg(ffmpegColor(item.delayColor));
        }
        videoFilter += QStringLiteral(",scale=1280:720:force_original_aspect_ratio=decrease,pad=1280:720:(ow-iw)/2:(oh-ih)/2,fps=30,format=yuv420p,setsar=1");
    } else {
        videoFilter = QStringLiteral("format=yuv420p,setsar=1");
    }

    QStringList args = {
        QStringLiteral("-y"),
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"),
        QStringLiteral("error"),
        QStringLiteral("-ss"),
        QString::number(item.start, 'f', 3),
        QStringLiteral("-to"),
        QString::number(item.end, 'f', 3),
        QStringLiteral("-i"),
        item.sourcePath
    };

    int nextInputIndex = 1;
    int videoInputIndex = 0;
    if (!item.hasVideo) {
        videoInputIndex = nextInputIndex++;
        args << QStringLiteral("-f") << QStringLiteral("lavfi")
             << QStringLiteral("-t") << QString::number(outputDuration(item), 'f', 3)
             << QStringLiteral("-i") << QStringLiteral("color=c=%1:s=1280x720:r=30").arg(ffmpegColor(item.delayColor));
    }

    const bool useSourceAudio = item.audioEnabled && item.hasAudio;
    int audioInputIndex = 0;
    if (!useSourceAudio) {
        audioInputIndex = nextInputIndex++;
        args << QStringLiteral("-f") << QStringLiteral("lavfi")
             << QStringLiteral("-t") << QString::number(outputDuration(item), 'f', 3)
             << QStringLiteral("-i") << QStringLiteral("anullsrc=channel_layout=stereo:sample_rate=48000");
    }

    args << QStringLiteral("-map") << QStringLiteral("%1:v:0").arg(videoInputIndex)
         << QStringLiteral("-vf") << videoFilter
         << QStringLiteral("-c:v") << QStringLiteral("libx264")
         << QStringLiteral("-preset") << QStringLiteral("veryfast")
         << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p");

    if (useSourceAudio) {
        QString audioFilter = QStringLiteral("volume=%1").arg(item.volume, 0, 'f', 4);
        const QString tempo = atempoFilter(item.speed);
        if (!tempo.isEmpty()) {
            audioFilter += QStringLiteral(",") + tempo;
        }
        if (item.delayBefore > 0.0) {
            audioFilter += QStringLiteral(",adelay=%1:all=1").arg(qRound(item.delayBefore * 1000.0));
        }
        audioFilter += QStringLiteral(",aformat=sample_rates=48000:channel_layouts=stereo");

        args << QStringLiteral("-map") << QStringLiteral("0:a:0")
             << QStringLiteral("-af") << audioFilter
             << QStringLiteral("-c:a") << QStringLiteral("aac")
             << QStringLiteral("-b:a") << QStringLiteral("192k");
    } else {
        args << QStringLiteral("-map") << QStringLiteral("%1:a:0").arg(audioInputIndex)
             << QStringLiteral("-c:a") << QStringLiteral("aac")
             << QStringLiteral("-b:a") << QStringLiteral("192k");
    }

    args << QStringLiteral("-movflags") << QStringLiteral("+faststart") << item.segmentPath;
    return args;
}

QStringList ExportController::concatArguments() const
{
    return {
        QStringLiteral("-y"),
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"),
        QStringLiteral("error"),
        QStringLiteral("-f"),
        QStringLiteral("concat"),
        QStringLiteral("-safe"),
        QStringLiteral("0"),
        QStringLiteral("-i"),
        m_concatListPath,
        QStringLiteral("-c"),
        QStringLiteral("copy"),
        m_format == ExportFormat::Gif ? m_intermediateVideoPath : m_outputPath
    };
}

QStringList ExportController::gifPaletteArguments() const
{
    QString vf;
    if (m_gifQuality == 0)
        vf = QStringLiteral("fps=%1,scale=320:-1:flags=lanczos,palettegen=stats_mode=diff").arg(m_gifFps);
    else if (m_gifQuality == 2)
        vf = QStringLiteral("fps=%1,palettegen=stats_mode=full").arg(m_gifFps);
    else
        vf = QStringLiteral("fps=%1,scale=640:-1:flags=lanczos,palettegen=stats_mode=diff").arg(m_gifFps);
    return {
        QStringLiteral("-y"),
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"),
        QStringLiteral("error"),
        QStringLiteral("-i"),
        m_intermediateVideoPath,
        QStringLiteral("-vf"),
        vf,
        m_gifPalettePath
    };
}

QStringList ExportController::gifRenderArguments() const
{
    QString lavfi;
    if (m_gifQuality == 0)
        lavfi = QStringLiteral("fps=%1,scale=320:-1:flags=lanczos[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=5").arg(m_gifFps);
    else if (m_gifQuality == 2)
        lavfi = QStringLiteral("fps=%1[x];[x][1:v]paletteuse=dither=sierra2_4a").arg(m_gifFps);
    else
        lavfi = QStringLiteral("fps=%1,scale=640:-1:flags=lanczos[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=5").arg(m_gifFps);
    return {
        QStringLiteral("-y"),
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"),
        QStringLiteral("error"),
        QStringLiteral("-i"),
        m_intermediateVideoPath,
        QStringLiteral("-i"),
        m_gifPalettePath,
        QStringLiteral("-lavfi"),
        lavfi,
        QStringLiteral("-loop"),
        QStringLiteral("0"),
        m_outputPath
    };
}

QString ExportController::atempoFilter(double speed) const
{
    QStringList filters;
    double remaining = qBound(0.25, speed, 4.0);
    while (remaining < 0.5) {
        filters << QStringLiteral("atempo=0.5");
        remaining /= 0.5;
    }
    while (remaining > 2.0) {
        filters << QStringLiteral("atempo=2.0");
        remaining /= 2.0;
    }
    filters << QStringLiteral("atempo=%1").arg(remaining, 0, 'f', 4);
    return filters.join(QLatin1Char(','));
}

double ExportController::outputDuration(const ExportItem &item) const
{
    const double sourceDuration = qMax(0.001, item.end - item.start);
    return item.delayBefore + sourceDuration / qMax(0.001, item.speed);
}

QString ExportController::ffmpegColor(const QString &color)
{
    QString normalized = color.trimmed();
    if (normalized.startsWith(QLatin1Char('#'))) {
        normalized.remove(0, 1);
    }

    static const QRegularExpression hexColor(QStringLiteral("^[0-9a-fA-F]{6}$"));
    if (!hexColor.match(normalized).hasMatch()) {
        normalized = QStringLiteral("000000");
    }

    return QStringLiteral("0x") + normalized.toLower();
}

QString ExportController::ffmpegExecutable() const
{
    return QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
}

QString ExportController::ffprobeExecutable() const
{
    return QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
}

QString ExportController::readinessMessageForFormat(ExportFormat format) const
{
    ensureExportReadinessChecked();
    return format == ExportFormat::Gif ? m_gifExportReadinessMessage : m_mp4ExportReadinessMessage;
}

void ExportController::ensureExportReadinessChecked() const
{
    if (m_exportReadinessChecked) {
        return;
    }

    const QString toolingMessage = commonToolingReadinessMessage();
    if (!toolingMessage.isEmpty()) {
        m_mp4ExportReadinessMessage = toolingMessage;
        m_gifExportReadinessMessage = toolingMessage;
        m_exportReadinessChecked = true;
        return;
    }

    m_mp4ExportReadinessMessage = codecPreflightError(ExportFormat::Mp4);
    m_gifExportReadinessMessage = codecPreflightError(ExportFormat::Gif);
    m_exportReadinessChecked = true;
}

QString ExportController::commonToolingReadinessMessage() const
{
    if (ffmpegExecutable().isEmpty()) {
        return QStringLiteral("Could not find ffmpeg. Install FFmpeg and make sure it is on PATH.");
    }

    if (ffprobeExecutable().isEmpty()) {
        return QStringLiteral("Could not find ffprobe. Install FFmpeg tools and make sure ffprobe is on PATH.");
    }

    return {};
}

QString ExportController::codecPreflightError(ExportFormat format) const
{
    const QString encoders = processOutput(ffmpegExecutable(), {QStringLiteral("-hide_banner"), QStringLiteral("-encoders")});
    if (encoders.isEmpty()) {
        return QStringLiteral("Could not inspect FFmpeg encoders. Run ffmpeg -encoders in a terminal to check the installation.");
    }

    if (!outputContainsLineToken(encoders, QStringLiteral("libx264"))) {
        return QStringLiteral("This FFmpeg build does not include the libx264 H.264 encoder required for MP4 export.");
    }

    if (!outputContainsLineToken(encoders, QStringLiteral("aac"))) {
        return QStringLiteral("This FFmpeg build does not include the AAC encoder required for MP4 export.");
    }

    const QString filters = processOutput(ffmpegExecutable(), {QStringLiteral("-hide_banner"), QStringLiteral("-filters")});
    if (filters.isEmpty()) {
        return QStringLiteral("Could not inspect FFmpeg filters. Run ffmpeg -filters in a terminal to check the installation.");
    }

    const QStringList requiredFilters = {
        QStringLiteral("scale"),
        QStringLiteral("pad"),
        QStringLiteral("fps"),
        QStringLiteral("format"),
        QStringLiteral("setsar"),
        QStringLiteral("tpad"),
        QStringLiteral("atempo"),
        QStringLiteral("adelay"),
        QStringLiteral("aformat")
    };
    QStringList filtersToCheck = requiredFilters;
    if (format == ExportFormat::Gif) {
        filtersToCheck << QStringLiteral("palettegen")
                       << QStringLiteral("paletteuse");
    }

    for (const QString &filter : std::as_const(filtersToCheck)) {
        if (!outputContainsLineToken(filters, filter)) {
            return QStringLiteral("This FFmpeg build is missing the %1 filter required for %2 export.")
                .arg(filter, format == ExportFormat::Gif ? QStringLiteral("GIF") : QStringLiteral("MP4"));
        }
    }

    return {};
}

QString ExportController::exportBlockerMessage() const
{
    if (!m_playlist || m_playlist->rowCount() == 0) {
        return QStringLiteral("There are no fragments to export.");
    }

    for (int row = 0; row < m_playlist->rowCount(); ++row) {
        const Fragment *fragment = m_playlist->fragmentAt(row);
        if (!fragment) {
            return QStringLiteral("Fragment %1 could not be read.").arg(row + 1);
        }

        const QString fragmentLabel = fragment->label.trimmed().isEmpty()
            ? QStringLiteral("Fragment %1").arg(row + 1)
            : QStringLiteral("Fragment %1 (%2)").arg(row + 1).arg(fragment->label.trimmed());

        if (!fragment->source.isValid()) {
            return QStringLiteral("%1 has no source file.").arg(fragmentLabel);
        }

        if (!fragment->source.isLocalFile()) {
            return QStringLiteral("%1 uses a non-local source. Export currently supports local files only.").arg(fragmentLabel);
        }

        const QFileInfo sourceInfo(fragment->source.toLocalFile());
        if (!sourceInfo.exists()) {
            return QStringLiteral("%1 source is missing: %2").arg(fragmentLabel, sourceInfo.fileName());
        }

        if (!sourceInfo.isFile() || !sourceInfo.isReadable()) {
            return QStringLiteral("%1 source is not readable: %2").arg(fragmentLabel, sourceInfo.fileName());
        }

        if (fragment->end <= fragment->start) {
            return QStringLiteral("%1 end time must be greater than start time.").arg(fragmentLabel);
        }
    }

    return {};
}

QString ExportController::currentStageLabel() const
{
    if (m_renderingGif) {
        return QStringLiteral("rendering the GIF");
    }

    if (m_generatingGifPalette) {
        return QStringLiteral("generating the GIF palette");
    }

    if (m_concatenating) {
        return m_format == ExportFormat::Gif
            ? QStringLiteral("preparing the GIF source video")
            : QStringLiteral("combining fragments");
    }

    if (m_currentItem >= 0 && m_currentItem < m_items.size()) {
        const ExportItem &item = m_items.at(m_currentItem);
        return QStringLiteral("rendering fragment %1 (%2)")
            .arg(item.row + 1)
            .arg(QFileInfo(item.sourcePath).fileName());
    }

    return QStringLiteral("exporting");
}

bool ExportController::probeMedia(ExportItem *item, QString *errorMessage) const
{
    if (!item) {
        return false;
    }

    QProcess probe;
    probe.start(ffprobeExecutable(), {
        QStringLiteral("-v"),
        QStringLiteral("error"),
        QStringLiteral("-show_entries"),
        QStringLiteral("stream=codec_type"),
        QStringLiteral("-of"),
        QStringLiteral("csv=p=0"),
        item->sourcePath
    });

    if (!probe.waitForStarted(3000) || !probe.waitForFinished(10000) || probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) {
        if (errorMessage) {
            const QString error = conciseProcessError(QString::fromUtf8(probe.readAllStandardError()).trimmed());
            const QString fileName = QFileInfo(item->sourcePath).fileName();
            *errorMessage = error.isEmpty()
                ? QStringLiteral("Could not inspect media streams for fragment %1 (%2).").arg(item->row + 1).arg(fileName)
                : QStringLiteral("Could not inspect media streams for fragment %1 (%2): %3").arg(item->row + 1).arg(fileName, error);
        }
        return false;
    }

    const QString output = QString::fromUtf8(probe.readAllStandardOutput());
    const QStringList streams = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    item->hasVideo = streams.contains(QStringLiteral("video"));
    item->hasAudio = streams.contains(QStringLiteral("audio"));
    if (!item->hasVideo && !item->hasAudio) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1 has no audio or video streams.").arg(QFileInfo(item->sourcePath).fileName());
        }
        return false;
    }

    return true;
}

QString ExportController::processOutput(const QString &program, const QStringList &arguments, int timeoutMs)
{
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForStarted(3000) || !process.waitForFinished(timeoutMs) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return {};
    }

    return QString::fromUtf8(process.readAllStandardOutput()) + QString::fromUtf8(process.readAllStandardError());
}

bool ExportController::outputContainsLineToken(const QString &output, const QString &token)
{
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.split(QLatin1Char(' '), Qt::SkipEmptyParts).contains(token)) {
            return true;
        }
    }
    return false;
}

QString ExportController::conciseProcessError(const QString &error)
{
    QStringList lines;
    for (const QString &line : error.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) {
            lines << trimmed;
        }
    }

    if (lines.isEmpty()) {
        return {};
    }

    const int maxLines = 6;
    if (lines.size() > maxLines) {
        lines = lines.mid(lines.size() - maxLines);
    }

    QString message = lines.join(QStringLiteral(" "));
    const int maxLength = 900;
    if (message.size() > maxLength) {
        message = message.left(maxLength - 3) + QStringLiteral("...");
    }

    return message;
}

QString ExportController::localPathFromUrl(const QUrl &url)
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

QString ExportController::escapedConcatPath(const QString &path)
{
    QString escaped = path;
    escaped.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return escaped;
}
