// SPDX-License-Identifier: GPL-3.0-or-later
//
// QObject facade over one libmpv handle, exposed to QML as the `player`
// context property. Owns handle lifetime, observes playback properties, and
// turns menu action ids (src/app/action_catalog.cpp) into mpv commands.
//
// AGENT-NOTE (threading): every mpv_handle call happens on the GUI thread.
// mpv_set_wakeup_callback fires from arbitrary mpv threads; it only posts a
// queued drain of mpv_wait_event(0) back here. The render thread touches the
// same handle solely through its own mpv_render_context (mpv_video_item.cpp),
// which mpv documents as safe against concurrent client-API use.

#pragma once

#include "mpv_library.h"

#include <QObject>
#include <QUrl>

#include <atomic>

class QQmlEngine;
class QJSEngine;

namespace QindaMpv::Player {

class PlayerController : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged FINAL)
    Q_PROPERTY(QString unavailableReason READ unavailableReason NOTIFY availabilityChanged FINAL)
    Q_PROPERTY(QString mpvVersion READ mpvVersion NOTIFY availabilityChanged FINAL)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged FINAL)
    Q_PROPERTY(QString mediaTitle READ mediaTitle NOTIFY mediaTitleChanged FINAL)
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged FINAL)
    Q_PROPERTY(double timePos READ timePos NOTIFY timePosChanged FINAL)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged FINAL)
    Q_PROPERTY(double volume READ volume NOTIFY volumeChanged FINAL)
    Q_PROPERTY(bool muted READ muted NOTIFY mutedChanged FINAL)
    Q_PROPERTY(double speed READ speed NOTIFY speedChanged FINAL)
    Q_PROPERTY(int chapter READ chapter NOTIFY chapterChanged FINAL)
    Q_PROPERTY(int chapterCount READ chapterCount NOTIFY chapterChanged FINAL)
    Q_PROPERTY(int playlistPos READ playlistPos NOTIFY playlistChanged FINAL)
    Q_PROPERTY(int playlistCount READ playlistCount NOTIFY playlistChanged FINAL)
    Q_PROPERTY(bool subtitlesVisible READ subtitlesVisible NOTIFY subtitlesVisibleChanged FINAL)
    // Window-state mirror: QML owns the actual visibility switch; the
    // property only keeps the exported checkable menu action truthful.
    Q_PROPERTY(bool fullscreen READ fullscreen WRITE setFullscreen NOTIFY fullscreenChanged FINAL)

public:
    explicit PlayerController(QObject *parent = nullptr);
    ~PlayerController() override;

    [[nodiscard]] bool available() const { return m_available; }
    [[nodiscard]] QString unavailableReason() const { return m_unavailableReason; }
    [[nodiscard]] QString mpvVersion() const { return m_mpvVersion; }
    [[nodiscard]] bool loaded() const { return m_loaded; }
    [[nodiscard]] QString mediaTitle() const { return m_mediaTitle; }
    [[nodiscard]] bool paused() const { return m_paused; }
    [[nodiscard]] double timePos() const { return m_timePos; }
    [[nodiscard]] double duration() const { return m_duration; }
    [[nodiscard]] double volume() const { return m_volume; }
    [[nodiscard]] bool muted() const { return m_muted; }
    [[nodiscard]] double speed() const { return m_speed; }
    [[nodiscard]] int chapter() const { return m_chapter; }
    [[nodiscard]] int chapterCount() const { return m_chapterCount; }
    [[nodiscard]] int playlistPos() const { return m_playlistPos; }
    [[nodiscard]] int playlistCount() const { return m_playlistCount; }
    [[nodiscard]] bool subtitlesVisible() const { return m_subtitlesVisible; }
    [[nodiscard]] bool fullscreen() const { return m_fullscreen; }

    void setFullscreen(bool fullscreen);

    // The mpv handle and resolved library for MpvVideoItem's renderer. Both
    // stay valid for the controller's lifetime; the renderer must be torn
    // down (scene-graph invalidate) before the controller is destroyed.
    [[nodiscard]] mpv_handle *handle() const { return m_handle; }
    [[nodiscard]] const MpvLibrary &mpv() const { return m_mpv; }

public slots:
    void openUrl(const QUrl &url);
    void openPlaylist(const QList<QUrl> &urls);
    void togglePause();
    void stop();
    void seekBy(double seconds);
    void seekTo(double seconds);
    void frameStep(bool backwards);
    void chapterSkip(int delta);
    void playlistSkip(int delta);
    void adjustVolume(double delta);
    void setVolume(double volume);
    void toggleMute();
    void cycleAudioTrack();
    void cycleSubtitleTrack();
    void toggleSubtitles();
    void adjustSpeed(double factor);
    void setSpeed(double speed);
    void toggleFullscreen();
    void screenshotToFile();

    // Single dispatch path for menu accelerators, in-window menus and the
    // exported global menu: every entry point funnels through
    // ApplicationCoordinator::activateAction into here.
    void dispatchMenuAction(const QString &actionId);

signals:
    void availabilityChanged();
    void loadedChanged();
    void mediaTitleChanged();
    void pausedChanged();
    void timePosChanged();
    void durationChanged();
    void volumeChanged();
    void mutedChanged();
    void speedChanged();
    void chapterChanged();
    void playlistChanged();
    void subtitlesVisibleChanged();
    void fullscreenChanged();

    // UI requests the controller cannot honor itself (no window knowledge).
    void openFileRequested();
    void openUrlRequested();
    void aboutRequested();
    void quitRequested();

private:
    enum ObserveId : uint64_t {
        ObserveTimePos = 1,
        ObserveDuration,
        ObservePause,
        ObserveVolume,
        ObserveMute,
        ObserveSpeed,
        ObserveMediaTitle,
        ObserveChapter,
        ObserveChapterListCount,
        ObservePlaylistPos,
        ObservePlaylistCount,
        ObserveSubVisibility,
    };

    // mpv_wakeup_fn entry point (arbitrary mpv threads); posts one queued
    // drain per event-loop pass. Static member so it can touch
    // m_drainQueued; the trampoline target is always `this`.
    static void wakeupCallback(void *ctx);
    void initializeMpv();
    void drainMpvEvents();
    void handlePropertyChange(uint64_t replyUserdata, const mpv_event_property &property);
    void command(const char **args);
    void setFlagProperty(const char *name, bool value);
    void setDoubleProperty(const char *name, double value);

    MpvLibrary m_mpv;
    mpv_handle *m_handle = nullptr;
    bool m_available = false;
    QString m_unavailableReason;
    QString m_mpvVersion;
    bool m_loaded = false;
    QString m_mediaTitle;
    bool m_paused = true;
    double m_timePos = 0.0;
    double m_duration = 0.0;
    double m_volume = 100.0;
    bool m_muted = false;
    double m_speed = 1.0;
    int m_chapter = 0;
    int m_chapterCount = 0;
    int m_playlistPos = 0;
    int m_playlistCount = 0;
    bool m_subtitlesVisible = true;
    bool m_fullscreen = false;

    // Coalesces wakeup-callback floods (any mpv thread) into one queued
    // event drain on the GUI thread.
    std::atomic<bool> m_drainQueued{false};
};

} // namespace QindaMpv::Player
