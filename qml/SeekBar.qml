// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// Seek slider with mono time readouts. The slider is write-only while the
// user scrubs and follows playback position otherwise; `valueModified` is
// the toolkit's user-only signal, so position updates never re-seek.
Tk.Flex {
    id: bar

    property real position: 0
    property real duration: 0

    signal seekRequested(real seconds)

    direction: Tk.Flex.Row
    align: Tk.Flex.Center
    gap: Tk.Theme.space.sm
    implicitHeight: Tk.Theme.size.control

    function stamp(seconds) {
        const total = Math.max(0, Math.floor(seconds));
        const h = Math.floor(total / 3600);
        const m = Math.floor((total % 3600) / 60);
        const s = total % 60;
        const mm = (h > 0 && m < 10 ? "0" : "") + m;
        const ss = (s < 10 ? "0" : "") + s;
        return h > 0 ? h + ":" + mm + ":" + ss : mm + ":" + ss;
    }

    Tk.Mono {
        text: bar.stamp(bar.position)
    }

    Tk.Slider {
        id: seek
        Tk.Flex.grow: 1
        from: 0
        to: Math.max(1, bar.duration)
        stepSize: 1
        enabled: bar.duration > 0
        tooltip: qsTr("Seek")
        onValueModified: function(value) { bar.seekRequested(value); }
        Binding on value {
            value: bar.position
            when: !seek.pressed
            restoreMode: Binding.RestoreNone
        }
    }

    Tk.Mono {
        text: bar.stamp(bar.duration)
    }
}
