// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Dialogs as Dialogs
import QindaTK as Tk
import QindaMPV

// The QindaMPV player window: QindaTK chrome around the mpv render surface.
//
// Menu authority: the ApplicationCoordinator's action catalog. It is
// exported as the desktop's global menu (see main.cpp); the in-window
// PlayerMenuBar below is the fail-closed twin, shown exactly while the
// export is not live (`inWindowMenuVisible` is driven by the export's
// setLocalMenuVisible callback). Every activation path — panel, menu,
// Shortcut, island button — calls coordinator.activateAction with the same
// stable ids (src/app/action_catalog.cpp).
Tk.AppWindow {
    id: root

    // Set from C++ by the menu export's fail-closed callback; starts true
    // because the export announces only after the window exists.
    property bool inWindowMenuVisible: true
    // Root-scope alias for the context property: inside child objects whose
    // own property is also named `player`, the bare name self-shadows.
    readonly property var playerRef: player

    width: 960
    height: 640
    minimumWidth: 480
    minimumHeight: 320
    title: player.mediaTitle.length > 0
           ? qsTr("%1 — QindaMPV").arg(player.mediaTitle)
           : qsTr("QindaMPV")

    menuBar: PlayerMenuBar {
        visible: root.inWindowMenuVisible && !player.fullscreen
        menusModel: coordinator.menus
        onActivated: function(actionId) { coordinator.activateAction(actionId); }
    }

    statusBar: Tk.StatusBar {
        visible: !player.fullscreen
        Tk.StatusField {
            iconName: "film"
            text: player.loaded ? player.mediaTitle : qsTr("No file loaded")
            tooltip: player.mediaTitle
        }
        Tk.Spacer {}
        Tk.StatusField {
            visible: player.chapterCount > 0
            iconName: "list"
            text: qsTr("Chapter %1/%2").arg(player.chapter + 1).arg(player.chapterCount)
        }
        Tk.StatusField {
            visible: player.playlistCount > 1
            iconName: "layout-list"
            text: qsTr("%1/%2").arg(player.playlistPos + 1).arg(player.playlistCount)
            tooltip: qsTr("Playlist position")
        }
        Tk.StatusField {
            iconName: "gauge"
            text: player.speed % 1 === 0 ? player.speed.toFixed(1) + "×"
                                         : player.speed.toFixed(2) + "×"
            tooltip: qsTr("Playback speed")
        }
    }

    Item {
        id: stage
        focus: true

        // Video letterbox backdrop; the canvas role is the toolkit's
        // work-surface colour.
        Rectangle {
            anchors.fill: parent
            color: Tk.Theme.color.canvas
        }

        MpvVideoItem {
            id: video
            anchors.fill: parent
            controller: player
            visible: player.available
        }

        // ---- Engine-missing state -------------------------------------
        Item {
            anchors.centerIn: parent
            width: noticeBox.width
            height: noticeBox.height
            visible: !player.available

            Tk.Notice {
                id: noticeBox
                width: 420
                variant: "warning"
                title: qsTr("Playback engine unavailable")
                text: qsTr("%1. Install mpv (libmpv.so.2, media-video/mpv) and restart QindaMPV.")
                          .arg(player.unavailableReason)
            }
        }

        // ---- Idle state ------------------------------------------------
        Tk.EmptyState {
            anchors.centerIn: parent
            visible: player.available && !player.loaded
            iconName: "monitor-play"
            title: qsTr("Nothing playing")
            text: qsTr("Open a file or URL to start playback.")

            Tk.Button {
                text: qsTr("Open File…")
                iconName: "folder-open"
                variant: "accent"
                onClicked: coordinator.activateAction("file.open")
            }
            Tk.Button {
                text: qsTr("Open URL…")
                iconName: "link"
                onClicked: coordinator.activateAction("file.open-url")
            }
        }

        // ---- Pointer chrome -------------------------------------------
        MouseArea {
            id: videoMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
            cursorShape: chromeHidden ? Qt.BlankCursor : Qt.ArrowCursor

            readonly property bool chromeHidden: chromeTimer.expired && !islandHover.hovered
                                                 && !player.paused && player.fullscreen

            onPositionChanged: chromeTimer.reveal()
            onClicked: {
                if (player.loaded) {
                    coordinator.activateAction("playback.play-pause");
                }
            }
            onDoubleClicked: coordinator.activateAction("video.fullscreen")
        }

        Timer {
            id: chromeTimer
            property bool expired: false
            interval: 2600
            onTriggered: expired = true
            function reveal() {
                expired = false;
                restart();
            }
        }

        ControlIsland {
            id: island
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Tk.Theme.space.xl
            visible: player.loaded && !videoMouse.chromeHidden
            player: root.playerRef
            onActivated: function(actionId) { coordinator.activateAction(actionId); }

            HoverHandler { id: islandHover }
        }
    }

    // ---- Fullscreen mirror (window state follows the player property) ---
    Connections {
        target: player
        function onFullscreenChanged() {
            root.visibility = player.fullscreen ? Window.FullScreen
                                                : Window.AutomaticVisibility;
            chromeTimer.reveal();
        }
    }

    // ---- UI-side service requests from the controller ------------------
    Connections {
        target: player
        function onOpenFileRequested() { openDialog.open(); }
        function onOpenUrlRequested() { urlPrompt.open(); }
        function onAboutRequested() { aboutDialog.open(); }
    }

    Dialogs.FileDialog {
        id: openDialog
        title: qsTr("Open media file")
        fileMode: Dialogs.FileDialog.OpenFile
        nameFilters: [
            qsTr("Media files (*.mkv *.mp4 *.webm *.avi *.mov *.m4v *.mpg *.mpeg *.mp3 *.flac *.ogg *.opus *.m4a *.wav)"),
            qsTr("All files (*)"),
        ]
        onAccepted: player.openUrl(selectedFile)
    }

    Tk.PromptDialog {
        id: urlPrompt
        title: qsTr("Open URL")
        label: qsTr("URL")
        placeholderText: "https://…"
        primaryText: qsTr("Open")
        onAccepted: {
            player.openUrl(urlPrompt.text);
            urlPrompt.text = "";
        }
    }

    Tk.MessageDialog {
        id: aboutDialog
        title: qsTr("About QindaMPV")
        variant: "info"
        iconName: "info"
        primaryText: qsTr("Close")
        text: qsTr("QindaMPV %1 — an mpv media player for the QindaQt desktop.\nEngine: %2")
              .arg("0.1.0")
              .arg(player.available ? player.mpvVersion : qsTr("unavailable (libmpv not loaded)"))
    }

    // ---- Shortcuts: sole key authority (menus carry display strings) ----
    // Every one mirrors an action id from src/app/action_catalog.cpp.
    Shortcut { sequence: "Ctrl+O"; onActivated: coordinator.activateAction("file.open") }
    Shortcut { sequence: "Ctrl+U"; onActivated: coordinator.activateAction("file.open-url") }
    Shortcut { sequence: "Ctrl+Q"; onActivated: coordinator.activateAction("file.quit") }
    Shortcut { sequence: "Space"; onActivated: coordinator.activateAction("playback.play-pause") }
    Shortcut { sequence: "s"; onActivated: coordinator.activateAction("playback.stop") }
    Shortcut { sequence: "Left"; onActivated: coordinator.activateAction("playback.seek-back") }
    Shortcut { sequence: "Right"; onActivated: coordinator.activateAction("playback.seek-forward") }
    Shortcut { sequence: "Ctrl+Left"; onActivated: coordinator.activateAction("playback.seek-back-large") }
    Shortcut { sequence: "Ctrl+Right"; onActivated: coordinator.activateAction("playback.seek-forward-large") }
    Shortcut { sequence: "."; onActivated: coordinator.activateAction("playback.frame-step") }
    Shortcut { sequence: ","; onActivated: coordinator.activateAction("playback.frame-back-step") }
    Shortcut { sequence: "PgUp"; onActivated: coordinator.activateAction("playback.chapter-prev") }
    Shortcut { sequence: "PgDown"; onActivated: coordinator.activateAction("playback.chapter-next") }
    Shortcut { sequence: "<"; onActivated: coordinator.activateAction("playback.playlist-prev") }
    Shortcut { sequence: ">"; onActivated: coordinator.activateAction("playback.playlist-next") }
    Shortcut { sequence: "["; onActivated: coordinator.activateAction("playback.speed-slower") }
    Shortcut { sequence: "]"; onActivated: coordinator.activateAction("playback.speed-faster") }
    Shortcut { sequence: "Backspace"; onActivated: coordinator.activateAction("playback.speed-normal") }
    Shortcut { sequence: "0"; onActivated: coordinator.activateAction("audio.volume-up") }
    Shortcut { sequence: "9"; onActivated: coordinator.activateAction("audio.volume-down") }
    Shortcut { sequence: "m"; onActivated: coordinator.activateAction("audio.mute") }
    Shortcut { sequence: "#"; onActivated: coordinator.activateAction("audio.track-cycle") }
    Shortcut { sequence: "f"; onActivated: coordinator.activateAction("video.fullscreen") }
    Shortcut { sequence: "Shift+S"; onActivated: coordinator.activateAction("video.screenshot") }
    Shortcut { sequence: "j"; onActivated: coordinator.activateAction("video.sub-cycle") }
    Shortcut { sequence: "v"; onActivated: coordinator.activateAction("video.sub-toggle") }
    Shortcut { sequence: "F1"; onActivated: coordinator.activateAction("help.about") }
    Shortcut {
        sequence: "Escape"
        enabled: player.fullscreen
        onActivated: coordinator.activateAction("video.fullscreen")
    }

    Component.onCompleted: {
        stage.forceActiveFocus();
        chromeTimer.reveal();
    }
}
