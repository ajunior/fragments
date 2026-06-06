#include "PlaybackBackend.h"

PlaybackBackend::PlaybackBackend(QObject *parent)
    : QObject(parent)
{
}

PlaybackBackend::~PlaybackBackend() = default;

QMediaPlayer *PlaybackBackend::player()
{
    return nullptr;
}
