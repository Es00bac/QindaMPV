// SPDX-License-Identifier: GPL-3.0-or-later
//
// Scene-graph node that renders mpv's frames into a Qt Quick item via the
// libmpv render_gl API. The whole stack runs on the OpenGL RHI backend —
// main() forces QSGRendererInterface::OpenGL before any window exists.
//
// AGENT-GUARD: the mpv_render_context is created, used and freed on the
// render thread only (inside MpvVideoRenderer). The GUI thread owns the
// mpv_handle (PlayerController); mpv documents render-API calls on a
// separate context as safe against concurrent client-API calls.

#pragma once

#include "player_controller.h"

#include <QQuickFramebufferObject>

namespace QindaMpv::Player {

class MpvVideoItem : public QQuickFramebufferObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QindaMpv::Player::PlayerController *controller READ controller
                   WRITE setController NOTIFY controllerChanged FINAL)

public:
    explicit MpvVideoItem(QQuickItem *parent = nullptr);
    ~MpvVideoItem() override;

    [[nodiscard]] PlayerController *controller() const { return m_controller; }
    void setController(PlayerController *controller);

    [[nodiscard]] Renderer *createRenderer() const override;

    // Queued from the render thread's update callback; schedules a new frame.
    Q_INVOKABLE void requestSceneUpdate();

signals:
    void controllerChanged();

private:
    PlayerController *m_controller = nullptr;
};

} // namespace QindaMpv::Player
