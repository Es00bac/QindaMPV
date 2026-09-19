// SPDX-License-Identifier: GPL-3.0-or-later

#include "player_controller.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStandardPaths>

namespace QindaMpv::Player {

void PlayerController::wakeupCallback(void *ctx)
{
    auto *self = static_cast<PlayerController *>(ctx);
    // mpv threads fire this for every queued event; post one drain per
    // event-loop pass instead of one invocation per event.
    bool expected = false;
    if (self->m_drainQueued.compare_exchange_strong(expected, true)) {
        QMetaObject::invokeMethod(self, &PlayerController::drainMpvEvents,
                                  Qt::QueuedConnection);
    }
}

PlayerController::PlayerController(QObject *parent)
    : QObject(parent)
{
    m_mpv = MpvLibrary::load();
    if (!m_mpv.loaded) {
        m_unavailableReason = m_mpv.failureReason;
        return;
    }
    initializeMpv();
}

PlayerController::~PlayerController()
{
    if (m_handle != nullptr) {
        // The renderer's mpv_render_context must already be gone (scene
        // graph invalidates before QML context teardown); terminating while
        // a render context lives is documented as safe but would kill the
        // client id it is bound to.
        m_mpv.unobserveProperty(m_handle, 0);
        m_mpv.terminateDestroy(m_handle);
        m_handle = nullptr;
    }
}

void PlayerController::initializeMpv()
{
    m_handle = m_mpv.create();
    if (m_handle == nullptr) {
        m_unavailableReason = QStringLiteral("mpv_create failed");
        return;
    }

    // Options must be set before mpv_initialize. idle keeps the handle alive
    // with nothing loaded; keep-open holds the last frame at EOF; the render
    // API requires the libmpv video output.
    m_mpv.setOptionString(m_handle, "vo", "libmpv");
    m_mpv.setOptionString(m_handle, "idle", "yes");
    m_mpv.setOptionString(m_handle, "keep-open", "yes");
    m_mpv.setOptionString(m_handle, "osd-level", "1");
    m_mpv.setOptionString(m_handle, "input-default-bindings", "no");
    m_mpv.setOptionString(m_handle, "input-vo-keyboard", "no");

    if (const int error = m_mpv.initialize(m_handle); error < 0) {
        m_unavailableReason = QStringLiteral("mpv_initialize: %1")
                                  .arg(QString::fromUtf8(m_mpv.errorString(error)));
        m_mpv.terminateDestroy(m_handle);
        m_handle = nullptr;
        return;
    }

    if (char *version = m_mpv.getPropertyString(m_handle, "mpv-version")) {
        m_mpvVersion = QString::fromUtf8(version);
        m_mpv.free(version);
    }

    m_mpv.observeProperty(m_handle, ObserveTimePos, "time-pos", MPV_FORMAT_DOUBLE);
    m_mpv.observeProperty(m_handle, ObserveDuration, "duration", MPV_FORMAT_DOUBLE);
    m_mpv.observeProperty(m_handle, ObservePause, "pause", MPV_FORMAT_FLAG);
    m_mpv.observeProperty(m_handle, ObserveVolume, "volume", MPV_FORMAT_DOUBLE);
    m_mpv.observeProperty(m_handle, ObserveMute, "mute", MPV_FORMAT_FLAG);
    m_mpv.observeProperty(m_handle, ObserveSpeed, "speed", MPV_FORMAT_DOUBLE);
    m_mpv.observeProperty(m_handle, ObserveMediaTitle, "media-title", MPV_FORMAT_STRING);
    m_mpv.observeProperty(m_handle, ObserveChapter, "chapter", MPV_FORMAT_INT64);
    m_mpv.observeProperty(m_handle, ObserveChapterListCount, "chapter-list/count", MPV_FORMAT_INT64);
    m_mpv.observeProperty(m_handle, ObservePlaylistPos, "playlist-pos", MPV_FORMAT_INT64);
    m_mpv.observeProperty(m_handle, ObservePlaylistCount, "playlist-count", MPV_FORMAT_INT64);
    m_mpv.observeProperty(m_handle, ObserveSubVisibility, "sub-visibility", MPV_FORMAT_FLAG);

    m_mpv.setWakeupCallback(m_handle, &wakeupCallback, this);
    m_available = true;
    emit availabilityChanged();
}

void PlayerController::drainMpvEvents()
{
    m_drainQueued.store(false);
    if (m_handle == nullptr) {
        return;
    }
    for (;;) {
        mpv_event *event = m_mpv.waitEvent(m_handle, 0);
        if (event == nullptr || event->event_id == QQMPV_MPV_EVENT_NONE) {
            break;
        }
        switch (event->event_id) {
        case QQMPV_MPV_EVENT_PROPERTY_CHANGE:
            if (event->data != nullptr) {
                handlePropertyChange(event->reply_userdata,
                                     *static_cast<mpv_event_property *>(event->data));
            }
            break;
        case QQMPV_MPV_EVENT_START_FILE:
            if (!m_loaded) {
                m_loaded = true;
                emit loadedChanged();
            }
            break;
        case QQMPV_MPV_EVENT_END_FILE: {
            const auto *endFile = static_cast<mpv_event_end_file *>(event->data);
            const bool reachedEof = endFile != nullptr
                && endFile->reason == QQMPV_MPV_END_FILE_REASON_EOF;
            // keep-open=yes leaves the last frame on screen at EOF; only an
            // aborted/replaced file returns the window to the empty state.
            if (!reachedEof && m_loaded) {
                m_loaded = false;
                emit loadedChanged();
            }
            break;
        }
        case QQMPV_MPV_EVENT_SHUTDOWN:
            m_available = false;
            m_loaded = false;
            emit availabilityChanged();
            emit loadedChanged();
            return;
        default:
            break;
        }
    }
}

void PlayerController::handlePropertyChange(uint64_t replyUserdata,
                                            const mpv_event_property &property)
{
    if (property.format == MPV_FORMAT_NONE || property.data == nullptr) {
        return; // property became unavailable (e.g. no file loaded)
    }
    switch (replyUserdata) {
    case ObserveTimePos: {
        const double value = *static_cast<double *>(property.data);
        if (m_timePos != value) {
            m_timePos = value;
            emit timePosChanged();
        }
        break;
    }
    case ObserveDuration: {
        const double value = *static_cast<double *>(property.data);
        if (m_duration != value) {
            m_duration = value;
            emit durationChanged();
        }
        break;
    }
    case ObservePause: {
        const bool value = *static_cast<int *>(property.data) != 0;
        if (m_paused != value) {
            m_paused = value;
            emit pausedChanged();
        }
        break;
    }
    case ObserveVolume: {
        const double value = *static_cast<double *>(property.data);
        if (m_volume != value) {
            m_volume = value;
            emit volumeChanged();
        }
        break;
    }
    case ObserveMute: {
        const bool value = *static_cast<int *>(property.data) != 0;
        if (m_muted != value) {
            m_muted = value;
            emit mutedChanged();
        }
        break;
    }
    case ObserveSpeed: {
        const double value = *static_cast<double *>(property.data);
        if (m_speed != value) {
            m_speed = value;
            emit speedChanged();
        }
        break;
    }
    case ObserveMediaTitle: {
        const char *text = static_cast<const char *>(property.data);
        const QString value = QString::fromUtf8(text != nullptr ? text : "");
        if (m_mediaTitle != value) {
            m_mediaTitle = value;
            emit mediaTitleChanged();
        }
        break;
    }
    case ObserveChapter: {
        const int value = static_cast<int>(*static_cast<int64_t *>(property.data));
        if (m_chapter != value) {
            m_chapter = value;
            emit chapterChanged();
        }
        break;
    }
    case ObserveChapterListCount: {
        const int value = static_cast<int>(*static_cast<int64_t *>(property.data));
        if (m_chapterCount != value) {
            m_chapterCount = value;
            emit chapterChanged();
        }
        break;
    }
    case ObservePlaylistPos: {
        const int value = static_cast<int>(*static_cast<int64_t *>(property.data));
        if (m_playlistPos != value) {
            m_playlistPos = value;
            emit playlistChanged();
        }
        break;
    }
    case ObservePlaylistCount: {
        const int value = static_cast<int>(*static_cast<int64_t *>(property.data));
        if (m_playlistCount != value) {
            m_playlistCount = value;
            emit playlistChanged();
        }
        break;
    }
    case ObserveSubVisibility: {
        const bool value = *static_cast<int *>(property.data) != 0;
        if (m_subtitlesVisible != value) {
            m_subtitlesVisible = value;
            emit subtitlesVisibleChanged();
        }
        break;
    }
    default:
        break;
    }
}

void PlayerController::command(const char **args)
{
    if (m_handle != nullptr) {
        m_mpv.commandAsync(m_handle, 0, args);
    }
}

void PlayerController::setFlagProperty(const char *name, bool value)
{
    if (m_handle != nullptr) {
        int flag = value ? 1 : 0;
        m_mpv.setProperty(m_handle, name, MPV_FORMAT_FLAG, &flag);
    }
}

void PlayerController::setDoubleProperty(const char *name, double value)
{
    if (m_handle != nullptr) {
        m_mpv.setProperty(m_handle, name, MPV_FORMAT_DOUBLE, &value);
    }
}

void PlayerController::openUrl(const QUrl &url)
{
    if (m_handle == nullptr || url.isEmpty()) {
        return;
    }
    const QByteArray target = url.isLocalFile()
        ? QDir::toNativeSeparators(url.toLocalFile()).toUtf8()
        : url.toString(QUrl::FullyEncoded).toUtf8();
    const char *args[] = {"loadfile", target.constData(), nullptr};
    command(args);
}

void PlayerController::openPlaylist(const QList<QUrl> &urls)
{
    bool first = true;
    for (const QUrl &url : urls) {
        if (url.isEmpty()) {
            continue;
        }
        const QByteArray target = url.isLocalFile()
            ? QDir::toNativeSeparators(url.toLocalFile()).toUtf8()
            : url.toString(QUrl::FullyEncoded).toUtf8();
        const char *args[] = {"loadfile", target.constData(),
                              first ? "replace" : "append", nullptr};
        command(args);
        first = false;
    }
}

void PlayerController::togglePause()
{
    const char *args[] = {"cycle", "pause", nullptr};
    command(args);
}

void PlayerController::stop()
{
    const char *args[] = {"stop", nullptr};
    command(args);
}

void PlayerController::seekBy(double seconds)
{
    const QByteArray amount = QByteArray::number(seconds);
    const char *args[] = {"seek", amount.constData(), "relative", nullptr};
    command(args);
}

void PlayerController::seekTo(double seconds)
{
    const QByteArray position = QByteArray::number(seconds, 'f', 3);
    const char *args[] = {"seek", position.constData(), "absolute", nullptr};
    command(args);
}

void PlayerController::frameStep(bool backwards)
{
    const char *args[] = {backwards ? "frame-back-step" : "frame-step", nullptr};
    command(args);
}

void PlayerController::chapterSkip(int delta)
{
    const QByteArray amount = QByteArray::number(delta);
    const char *args[] = {"add", "chapter", amount.constData(), nullptr};
    command(args);
}

void PlayerController::playlistSkip(int delta)
{
    const char *args[] = {delta < 0 ? "playlist-prev" : "playlist-next", nullptr};
    command(args);
}

void PlayerController::adjustVolume(double delta)
{
    setVolume(m_volume + delta);
}

void PlayerController::setVolume(double volume)
{
    setDoubleProperty("volume", std::clamp(volume, 0.0, 100.0));
}

void PlayerController::toggleMute()
{
    const char *args[] = {"cycle", "mute", nullptr};
    command(args);
}

void PlayerController::cycleAudioTrack()
{
    const char *args[] = {"cycle", "aid", nullptr};
    command(args);
}

void PlayerController::cycleSubtitleTrack()
{
    const char *args[] = {"cycle", "sid", nullptr};
    command(args);
}

void PlayerController::toggleSubtitles()
{
    const char *args[] = {"cycle", "sub-visibility", nullptr};
    command(args);
}

void PlayerController::adjustSpeed(double factor)
{
    setSpeed(m_speed * factor);
}

void PlayerController::setSpeed(double speed)
{
    setDoubleProperty("speed", std::clamp(speed, 0.01, 100.0));
}

void PlayerController::setFullscreen(bool fullscreen)
{
    if (m_fullscreen != fullscreen) {
        m_fullscreen = fullscreen;
        emit fullscreenChanged();
    }
}

void PlayerController::toggleFullscreen()
{
    setFullscreen(!m_fullscreen);
}

void PlayerController::screenshotToFile()
{
    if (m_handle == nullptr) {
        return;
    }
    const QString picturesLocation =
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QDir targetDir(picturesLocation.isEmpty() ? QDir::homePath() : picturesLocation);
    targetDir.mkpath(QStringLiteral("QindaMPV"));
    targetDir.cd(QStringLiteral("QindaMPV"));
    const QByteArray path = targetDir
        .filePath(QStringLiteral("qqmpv-%1.png")
                      .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"))))
        .toUtf8();
    const char *args[] = {"screenshot-to-file", path.constData(), "video", nullptr};
    command(args);
}

void PlayerController::dispatchMenuAction(const QString &actionId)
{
    // AGENT-CONTRACT: ids are the stable strings declared in
    // src/app/action_catalog.cpp; the panel, the in-window menu and the
    // Shortcut items all resolve to this chain. Unknown ids are ignored so a
    // stale export can never crash dispatch.
    if (actionId == QLatin1String("file.open")) {
        emit openFileRequested();
    } else if (actionId == QLatin1String("file.open-url")) {
        emit openUrlRequested();
    } else if (actionId == QLatin1String("file.quit")) {
        emit quitRequested();
    } else if (actionId == QLatin1String("playback.play-pause")) {
        togglePause();
    } else if (actionId == QLatin1String("playback.stop")) {
        stop();
    } else if (actionId == QLatin1String("playback.seek-back")) {
        seekBy(-5);
    } else if (actionId == QLatin1String("playback.seek-forward")) {
        seekBy(5);
    } else if (actionId == QLatin1String("playback.seek-back-large")) {
        seekBy(-60);
    } else if (actionId == QLatin1String("playback.seek-forward-large")) {
        seekBy(60);
    } else if (actionId == QLatin1String("playback.frame-step")) {
        frameStep(false);
    } else if (actionId == QLatin1String("playback.frame-back-step")) {
        frameStep(true);
    } else if (actionId == QLatin1String("playback.chapter-prev")) {
        chapterSkip(-1);
    } else if (actionId == QLatin1String("playback.chapter-next")) {
        chapterSkip(1);
    } else if (actionId == QLatin1String("playback.playlist-prev")) {
        playlistSkip(-1);
    } else if (actionId == QLatin1String("playback.playlist-next")) {
        playlistSkip(1);
    } else if (actionId == QLatin1String("playback.speed-faster")) {
        adjustSpeed(1.1);
    } else if (actionId == QLatin1String("playback.speed-slower")) {
        adjustSpeed(1.0 / 1.1);
    } else if (actionId == QLatin1String("playback.speed-normal")) {
        setSpeed(1.0);
    } else if (actionId == QLatin1String("audio.volume-up")) {
        adjustVolume(2);
    } else if (actionId == QLatin1String("audio.volume-down")) {
        adjustVolume(-2);
    } else if (actionId == QLatin1String("audio.mute")) {
        toggleMute();
    } else if (actionId == QLatin1String("audio.track-cycle")) {
        cycleAudioTrack();
    } else if (actionId == QLatin1String("video.fullscreen")) {
        toggleFullscreen();
    } else if (actionId == QLatin1String("video.screenshot")) {
        screenshotToFile();
    } else if (actionId == QLatin1String("video.sub-cycle")) {
        cycleSubtitleTrack();
    } else if (actionId == QLatin1String("video.sub-toggle")) {
        toggleSubtitles();
    } else if (actionId == QLatin1String("help.about")) {
        emit aboutRequested();
    }
}

} // namespace QindaMpv::Player
