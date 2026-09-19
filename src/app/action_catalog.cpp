// SPDX-License-Identifier: GPL-3.0-or-later

#include "action_catalog.h"

namespace QindaMpv::App {

namespace {

using QindaQt::AppShell::ActionSpec;

ActionSpec action(const char *id, const char *menuId, const char *menuLabel,
                  int menuOrder, const char *label, const char *shortcut, int order,
                  bool checkable = false, bool destructive = false)
{
    ActionSpec spec;
    spec.id = QString::fromLatin1(id);
    spec.menuId = QString::fromLatin1(menuId);
    spec.menuLabel = QString::fromLatin1(menuLabel);
    spec.menuOrder = menuOrder;
    spec.label = QString::fromLatin1(label);
    spec.shortcut = QKeySequence(QString::fromLatin1(shortcut));
    spec.order = order;
    spec.checkable = checkable;
    spec.destructive = destructive;
    return spec;
}

} // namespace

QList<ActionSpec> buildPlayerActions()
{
    return {
        // File
        action("file.open", "file", "File", 0, "Open File…", "Ctrl+O", 0),
        action("file.open-url", "file", "File", 0, "Open URL…", "Ctrl+U", 1),
        action("file.quit", "file", "File", 0, "Quit", "Ctrl+Q", 2),

        // Playback
        action("playback.play-pause", "playback", "Playback", 10, "Play/Pause", "Space", 0),
        action("playback.stop", "playback", "Playback", 10, "Stop", "s", 1),
        action("playback.seek-back", "playback", "Playback", 10, "Seek Back 5 Seconds", "Left", 2),
        action("playback.seek-forward", "playback", "Playback", 10, "Seek Forward 5 Seconds", "Right", 3),
        action("playback.seek-back-large", "playback", "Playback", 10, "Seek Back 1 Minute", "Ctrl+Left", 4),
        action("playback.seek-forward-large", "playback", "Playback", 10, "Seek Forward 1 Minute", "Ctrl+Right", 5),
        action("playback.frame-step", "playback", "Playback", 10, "Next Frame", ".", 6),
        action("playback.frame-back-step", "playback", "Playback", 10, "Previous Frame", ",", 7),
        action("playback.chapter-prev", "playback", "Playback", 10, "Previous Chapter", "PgUp", 8),
        action("playback.chapter-next", "playback", "Playback", 10, "Next Chapter", "PgDown", 9),
        action("playback.playlist-prev", "playback", "Playback", 10, "Previous in Playlist", "<", 10),
        action("playback.playlist-next", "playback", "Playback", 10, "Next in Playlist", ">", 11),
        action("playback.speed-slower", "playback", "Playback", 10, "Slower", "[", 12),
        action("playback.speed-faster", "playback", "Playback", 10, "Faster", "]", 13),
        action("playback.speed-normal", "playback", "Playback", 10, "Normal Speed", "Backspace", 14),

        // Audio
        action("audio.volume-up", "audio", "Audio", 20, "Volume Up", "0", 0),
        action("audio.volume-down", "audio", "Audio", 20, "Volume Down", "9", 1),
        action("audio.mute", "audio", "Audio", 20, "Mute", "m", 2, true),
        action("audio.track-cycle", "audio", "Audio", 20, "Cycle Audio Track", "#", 3),

        // Video
        action("video.fullscreen", "video", "Video", 30, "Fullscreen", "f", 0, true),
        action("video.screenshot", "video", "Video", 30, "Screenshot", "Shift+S", 1),
        action("video.sub-cycle", "video", "Video", 30, "Cycle Subtitles", "j", 2),
        action("video.sub-toggle", "video", "Video", 30, "Subtitles Visible", "v", 3, true),

        // Help
        action("help.about", "help", "Help", 40, "About QindaMPV", "F1", 0),
    };
}

} // namespace QindaMpv::App
