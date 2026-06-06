#include <QGuiApplication>
#include <QIcon>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QTextStream>

#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "export/ExportController.h"
#include "playback/PlaybackController.h"
#include "playlist/PlaylistModel.h"

namespace {

QMutex logMutex;
QString logFilePath;

QString messageTypeName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("debug");
    case QtInfoMsg:
        return QStringLiteral("info");
    case QtWarningMsg:
        return QStringLiteral("warning");
    case QtCriticalMsg:
        return QStringLiteral("critical");
    case QtFatalMsg:
        return QStringLiteral("fatal");
    }
    return QStringLiteral("unknown");
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    QMutexLocker locker(&logMutex);
    QFile file(logFilePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
               << " [" << messageTypeName(type) << "]";
        if (context.category && context.category[0] != '\0') {
            stream << " " << context.category;
        }
        stream << ": " << message << '\n';
    }

    if (type == QtFatalMsg) {
        abort();
    }
}

void installFileLogger()
{
    const char *home = std::getenv("HOME");
    std::filesystem::path logDirPath = home && home[0] != '\0'
        ? std::filesystem::path(home) / ".local" / "share" / "Fragments" / "logs"
        : std::filesystem::temp_directory_path() / "Fragments" / "logs";
    std::error_code error;
    std::filesystem::create_directories(logDirPath, error);
    if (error) {
        logDirPath = std::filesystem::temp_directory_path() / "Fragments" / "logs";
        error.clear();
        std::filesystem::create_directories(logDirPath, error);
    }
    const std::filesystem::path path = logDirPath / "fragments.log";
    logFilePath = QString::fromStdString(path.string());

    std::freopen(path.c_str(), "a", stdout);
    std::freopen(path.c_str(), "a", stderr);
    qInstallMessageHandler(messageHandler);

    if (FILE *file = std::fopen(path.c_str(), "a")) {
        const QByteArray timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toUtf8();
        std::fprintf(file, "%s [info] fragments: Logging to %s\n", timestamp.constData(), path.c_str());
        std::fclose(file);
    }
}

}

int main(int argc, char *argv[])
{
    installFileLogger();
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Fragments"));
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.ajunior.fragments"));
    QGuiApplication::setOrganizationName(QStringLiteral("Fragments"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/icons/io.github.ajunior.fragments.svg")));
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    PlaylistModel playlist;
    PlaybackController playback;
    ExportController exporter;
    playback.setPlaylist(&playlist);
    exporter.setPlaylist(&playlist);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("playlistModel"), &playlist);
    engine.rootContext()->setContextProperty(QStringLiteral("playback"), &playback);
    engine.rootContext()->setContextProperty(QStringLiteral("exporter"), &exporter);
    engine.loadFromModule(QStringLiteral("Fragments"), QStringLiteral("Main"));

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
