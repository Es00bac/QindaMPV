// SPDX-License-Identifier: GPL-3.0-or-later
//
// Minimal ABI declarations for the subset of the libmpv client and render_gl
// APIs QindaMPV uses. AGENT-NOTE: libmpv is loaded at run time (see
// mpv_library.cpp) so the player builds and its UI runs on systems without
// mpv headers or the library installed; playback activates when libmpv.so.2
// is present. These prototypes mirror <mpv/client.h> and <mpv/render_gl.h>
// (LGPL-2.1-or-later); the enum values and struct layouts are part of mpv's
// stable ABI and must not be reordered or extended speculatively — add new
// entries only when resolving a new symbol, with the upstream header as the
// source of truth.

#pragma once

#include <cstdint>

#define QQMPV_MPV_MAKE_VERSION(major, minor) ((static_cast<unsigned long>(major) << 16) | (minor))

// The render_gl API exists since client API 1.105 (mpv 0.29); loading
// libmpv.so.2 already implies it, and MpvLibrary still checks.
#define QQMPV_MPV_MIN_CLIENT_API QQMPV_MPV_MAKE_VERSION(1, 105)

extern "C" {

typedef struct mpv_handle mpv_handle;
typedef struct mpv_render_context mpv_render_context;

typedef enum mpv_format {
    MPV_FORMAT_NONE       = 0,
    MPV_FORMAT_STRING     = 1,
    MPV_FORMAT_OSD_STRING = 2,
    MPV_FORMAT_FLAG       = 3,
    MPV_FORMAT_INT64      = 4,
    MPV_FORMAT_DOUBLE     = 5,
    MPV_FORMAT_NODE       = 6,
    MPV_FORMAT_NODE_ARRAY = 7,
    MPV_FORMAT_NODE_MAP   = 8,
    MPV_FORMAT_BYTE_ARRAY = 9
} mpv_format;

// Only the events QindaMPV consumes; numeric values are ABI-stable and match
// <mpv/client.h> exactly (deprecated ids omitted, gaps intentional).
typedef enum qqmpv_mpv_event_id {
    QQMPV_MPV_EVENT_NONE             = 0,
    QQMPV_MPV_EVENT_SHUTDOWN         = 1,
    QQMPV_MPV_EVENT_START_FILE       = 6,
    QQMPV_MPV_EVENT_END_FILE         = 7,
    QQMPV_MPV_EVENT_FILE_LOADED      = 8,
    QQMPV_MPV_EVENT_PROPERTY_CHANGE  = 22
} qqmpv_mpv_event_id;

typedef enum qqmpv_mpv_end_file_reason {
    QQMPV_MPV_END_FILE_REASON_EOF       = 0,
    QQMPV_MPV_END_FILE_REASON_RESTARTED = 1,
    QQMPV_MPV_END_FILE_REASON_ABORTED   = 2,
    QQMPV_MPV_END_FILE_REASON_QUIT      = 3,
    QQMPV_MPV_END_FILE_REASON_ERROR     = 4,
    QQMPV_MPV_END_FILE_REASON_REDIRECT  = 5
} qqmpv_mpv_end_file_reason;

typedef struct mpv_event {
    int event_id;
    int error;
    uint64_t reply_userdata;
    void *data;
} mpv_event;

typedef struct mpv_event_property {
    const char *name;
    mpv_format format;
    void *data;
} mpv_event_property;

typedef struct mpv_event_end_file {
    int reason;
    int error;
} mpv_event_end_file;

typedef enum mpv_render_param_type {
    MPV_RENDER_PARAM_INVALID              = 0,
    MPV_RENDER_PARAM_API_TYPE             = 1,
    MPV_RENDER_PARAM_OPENGL_INIT_PARAMS   = 2,
    MPV_RENDER_PARAM_OPENGL_FBO           = 3,
    MPV_RENDER_PARAM_FLIP_Y               = 4,
    MPV_RENDER_PARAM_ADVANCED_CONTROL     = 10,
    MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME = 12,
    MPV_RENDER_PARAM_SKIP_RENDERING       = 13
} mpv_render_param_type;

#define QQMPV_MPV_RENDER_API_TYPE_OPENGL "opengl"

typedef struct mpv_render_param {
    mpv_render_param_type type;
    void *data;
} mpv_render_param;

typedef struct mpv_opengl_init_params {
    void *(*get_proc_address)(void *ctx, const char *name);
    void *get_proc_address_ctx;
} mpv_opengl_init_params;

typedef struct mpv_opengl_fbo {
    int fbo;
    int w;
    int h;
    int internal_format;
} mpv_opengl_fbo;

// Function pointer typedefs for every symbol MpvLibrary resolves. Keep in
// sync with mpv_library.cpp's resolution table; names carry the qqmpv prefix
// so a future vendored <mpv/client.h> can never collide.
using qqmpv_mpv_client_api_version_fn    = unsigned long (*)(void);
using qqmpv_mpv_error_string_fn          = const char *(*)(int error);
using qqmpv_mpv_event_name_fn            = const char *(*)(int event_id);
using qqmpv_mpv_create_fn                = mpv_handle *(*)(void);
using qqmpv_mpv_initialize_fn            = int (*)(mpv_handle *ctx);
using qqmpv_mpv_terminate_destroy_fn     = void (*)(mpv_handle *ctx);
using qqmpv_mpv_set_option_string_fn     = int (*)(mpv_handle *ctx, const char *name, const char *data);
using qqmpv_mpv_command_fn               = int (*)(mpv_handle *ctx, const char **args);
using qqmpv_mpv_command_async_fn         = int (*)(mpv_handle *ctx, uint64_t reply_userdata, const char **args);
using qqmpv_mpv_get_property_fn          = int (*)(mpv_handle *ctx, const char *name, mpv_format format, void *data);
using qqmpv_mpv_get_property_string_fn   = char *(*)(mpv_handle *ctx, const char *name);
using qqmpv_mpv_set_property_fn          = int (*)(mpv_handle *ctx, const char *name, mpv_format format, void *data);
using qqmpv_mpv_set_property_string_fn   = int (*)(mpv_handle *ctx, const char *name, const char *data);
using qqmpv_mpv_observe_property_fn      = int (*)(mpv_handle *ctx, uint64_t reply_userdata, const char *name, mpv_format format);
using qqmpv_mpv_unobserve_property_fn    = int (*)(mpv_handle *ctx, uint64_t registered_reply_userdata);
using qqmpv_mpv_wait_event_fn            = mpv_event *(*)(mpv_handle *ctx, double timeout);
using qqmpv_mpv_set_wakeup_callback_fn   = void (*)(mpv_handle *ctx, void (*callback)(void *callback_ctx), void *callback_ctx);
using qqmpv_mpv_free_fn                  = void (*)(void *data);

using qqmpv_mpv_render_context_create_fn = int (*)(mpv_render_context **res, mpv_handle *mpv, mpv_render_param *params);
using qqmpv_mpv_render_context_free_fn   = void (*)(mpv_render_context *render_ctx);
using qqmpv_mpv_render_context_render_fn = int (*)(mpv_render_context *render_ctx, mpv_render_param *params);
using qqmpv_mpv_render_context_set_update_callback_fn = void (*)(mpv_render_context *render_ctx, void (*callback)(void *callback_ctx), void *callback_ctx);
using qqmpv_mpv_render_context_report_swap_fn = void (*)(mpv_render_context *render_ctx);

} // extern "C"
