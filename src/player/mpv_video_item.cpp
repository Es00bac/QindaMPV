// SPDX-License-Identifier: GPL-3.0-or-later

#include "mpv_video_item.h"

#include "player_controller.h"

#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QPointer>
#include <QQuickWindow>

namespace QindaMpv::Player {

namespace {

// Runs on the render thread. Owns the mpv_render_context for exactly one
// MpvVideoItem; never touches the mpv_handle directly.
class MpvVideoRenderer final : public QQuickFramebufferObject::Renderer {
public:
    MpvVideoRenderer(const MpvVideoItem *item, PlayerController *controller)
        : m_item(item),
          m_controller(controller)
    {
    }

    ~MpvVideoRenderer() override
    {
        destroyRenderContext();
    }

    [[nodiscard]] QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        return new QOpenGLFramebufferObject(size, format);
    }

    void render() override
    {
        if (m_controller == nullptr || !m_controller->available()) {
            return;
        }
        if (m_renderContext == nullptr) {
            createRenderContext();
        }
        if (m_renderContext == nullptr) {
            return;
        }

        QOpenGLFramebufferObject *fbo = framebufferObject();
        const MpvLibrary &mpv = m_controller->mpv();
        mpv_opengl_fbo fboSettings{
            static_cast<int>(fbo->handle()),
            fbo->size().width(),
            fbo->size().height(),
            0, // internal_format: 0 lets mpv pick (upstream qtquick example)
        };
        int flipY = 1; // GL FBO origin is bottom-left, Qt textures top-left
        int blockForTargetTime = 0; // never stall the render thread on vsync
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &fboSettings},
            {MPV_RENDER_PARAM_FLIP_Y, &flipY},
            {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &blockForTargetTime},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        mpv.renderContextRender(m_renderContext, params);
        mpv.renderContextReportSwap(m_renderContext);
    }

    void synchronize(QQuickFramebufferObject *item) override
    {
        m_item = static_cast<const MpvVideoItem *>(item);
        m_controller = m_item->controller();
    }

private:
    static void *getProcAddress(void *, const char *name)
    {
        QOpenGLContext *context = QOpenGLContext::currentContext();
        if (context == nullptr) {
            return nullptr;
        }
        return reinterpret_cast<void *>(context->getProcAddress(name));
    }

    // mpv calls this from its own threads when a new frame is ready. The
    // item lives on the GUI thread, so the frame request must cross threads.
    static void updateCallback(void *ctx)
    {
        auto *self = static_cast<MpvVideoRenderer *>(ctx);
        if (QPointer<MpvVideoItem> item = self->m_itemGuard) {
            QMetaObject::invokeMethod(item.data(), &MpvVideoItem::requestSceneUpdate,
                                      Qt::QueuedConnection);
        }
    }

    void createRenderContext()
    {
        const MpvLibrary &mpv = m_controller->mpv();
        mpv_opengl_init_params initParams{&getProcAddress, nullptr};
        // AGENT-NOTE: mpv writes back into this array (API type negotiation),
        // so it must be mutable and outlive the call, which the stack does.
        char apiType[] = QQMPV_MPV_RENDER_API_TYPE_OPENGL;
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE, apiType},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &initParams},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        if (mpv.renderContextCreate(&m_renderContext, m_controller->handle(), params) < 0) {
            m_renderContext = nullptr;
            qWarning("qqmpv: mpv_render_context_create failed");
            return;
        }
        qInfo("qqmpv: mpv render context created (api=%s)", apiType);
        // The controller queues loadfile calls behind this point (mpv drops
        // the video track of any file loaded earlier). Posted queued: the
        // controller lives on the GUI thread.
        QMetaObject::invokeMethod(m_controller, &PlayerController::noteRenderContextReady,
                                  Qt::QueuedConnection);
        // createRenderer() hands us the item as const; the object itself is
        // never const, and update() below needs the mutable pointer.
        m_itemGuard = const_cast<MpvVideoItem *>(m_item);
        mpv.renderContextSetUpdateCallback(m_renderContext, &updateCallback, this);
    }

    void destroyRenderContext()
    {
        if (m_renderContext != nullptr) {
            const MpvLibrary &mpv = m_controller->mpv();
            // Stop callbacks before freeing so a late mpv thread can never
            // dereference this renderer.
            mpv.renderContextSetUpdateCallback(m_renderContext, nullptr, nullptr);
            mpv.renderContextFree(m_renderContext);
            m_renderContext = nullptr;
        }
    }

    const MpvVideoItem *m_item = nullptr;
    QPointer<MpvVideoItem> m_itemGuard;
    PlayerController *m_controller = nullptr;
    mpv_render_context *m_renderContext = nullptr;
};

} // namespace

MpvVideoItem::MpvVideoItem(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
{
    setMirrorVertically(false); // the renderer flips Y itself
}

MpvVideoItem::~MpvVideoItem() = default;

void MpvVideoItem::setController(PlayerController *controller)
{
    if (m_controller != controller) {
        m_controller = controller;
        emit controllerChanged();
        update();
    }
}

QQuickFramebufferObject::Renderer *MpvVideoItem::createRenderer() const
{
    return new MpvVideoRenderer(this, m_controller);
}

void MpvVideoItem::requestSceneUpdate()
{
    update();
}

} // namespace QindaMpv::Player
