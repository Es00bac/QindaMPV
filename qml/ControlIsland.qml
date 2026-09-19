// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// The floating transport island over the video. All commands leave through
// the `activated` signal; Main.qml routes them to the ApplicationCoordinator
// so the island, the in-window menu and the exported global menu share one
// dispatch path. `player` is the PlayerController context object, injected
// as a property so the island stays instantiable in previews.
Tk.Island {
    id: island

    required property var player
    signal activated(string actionId)

    width: parent ? parent.width - 2 * Tk.Theme.space.xl : implicitWidth
    paddingH: Tk.Theme.space.md
    spacing: Tk.Theme.space.xs

    Tk.IconButton {
        iconName: island.player.paused ? "play" : "pause"
        tooltip: island.player.paused ? qsTr("Play (Space)") : qsTr("Pause (Space)")
        Accessible.name: island.player.paused ? qsTr("Play") : qsTr("Pause")
        onClicked: island.activated("playback.play-pause")
    }

    Tk.IconButton {
        iconName: "square"
        tooltip: qsTr("Stop (S)")
        Accessible.name: qsTr("Stop")
        onClicked: island.activated("playback.stop")
    }

    Tk.IconButton {
        iconName: "skip-back"
        tooltip: qsTr("Previous chapter (PgUp)")
        Accessible.name: qsTr("Previous chapter")
        enabled: island.player.chapterCount > 0
        onClicked: island.activated("playback.chapter-prev")
    }

    Tk.IconButton {
        iconName: "skip-forward"
        tooltip: qsTr("Next chapter (PgDown)")
        Accessible.name: qsTr("Next chapter")
        enabled: island.player.chapterCount > 0
        onClicked: island.activated("playback.chapter-next")
    }

    SeekBar {
        Tk.Flex.grow: 1
        position: island.player.timePos
        duration: island.player.duration
        onSeekRequested: function(seconds) { island.player.seekTo(seconds); }
    }

    Tk.IconButton {
        iconName: island.player.muted || island.player.volume === 0 ? "volume-x" : "volume-2"
        tooltip: qsTr("Mute (M)")
        Accessible.name: qsTr("Mute")
        onClicked: island.activated("audio.mute")
    }

    Tk.Slider {
        id: volumeSlider
        width: 72
        small: true
        from: 0
        to: 100
        stepSize: 1
        tooltip: qsTr("Volume")
        onValueModified: function(value) { island.player.setVolume(value); }
        Binding on value {
            value: island.player.volume
            when: !volumeSlider.pressed
            restoreMode: Binding.RestoreNone
        }
    }

    Tk.Chip {
        small: true
        text: island.player.speed % 1 === 0 ? island.player.speed.toFixed(1) + "×"
                                            : island.player.speed.toFixed(2) + "×"
        tooltip: qsTr("Playback speed — click for normal (Backspace)")
        onClicked: island.activated("playback.speed-normal")
    }

    Tk.IconButton {
        iconName: island.player.fullscreen ? "minimize-2" : "maximize-2"
        tooltip: island.player.fullscreen ? qsTr("Leave fullscreen (F)") : qsTr("Fullscreen (F)")
        Accessible.name: qsTr("Fullscreen")
        onClicked: island.activated("video.fullscreen")
    }
}
