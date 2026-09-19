// SPDX-License-Identifier: GPL-3.0-or-later
//
// Run-time loader for libmpv. The player never links libmpv at build time:
// the UI (window, menus, dialogs) is fully functional without it, and the
// controller reports available=false so QML can show the install hint.
// Resolution is all-or-nothing per table below — a partial client API would
// be an ABI trap, so one missing symbol marks the library unusable.

#pragma once

#include "mpv_api.h"

#include <QString>

namespace QindaMpv::Player {

struct MpvLibrary {
    // Client API.
    qqmpv_mpv_client_api_version_fn clientApiVersion = nullptr;
    qqmpv_mpv_error_string_fn errorString = nullptr;
    qqmpv_mpv_event_name_fn eventName = nullptr;
    qqmpv_mpv_create_fn create = nullptr;
    qqmpv_mpv_initialize_fn initialize = nullptr;
    qqmpv_mpv_terminate_destroy_fn terminateDestroy = nullptr;
    qqmpv_mpv_set_option_string_fn setOptionString = nullptr;
    qqmpv_mpv_command_fn command = nullptr;
    qqmpv_mpv_command_async_fn commandAsync = nullptr;
    qqmpv_mpv_get_property_fn getProperty = nullptr;
    qqmpv_mpv_get_property_string_fn getPropertyString = nullptr;
    qqmpv_mpv_set_property_fn setProperty = nullptr;
    qqmpv_mpv_set_property_string_fn setPropertyString = nullptr;
    qqmpv_mpv_observe_property_fn observeProperty = nullptr;
    qqmpv_mpv_unobserve_property_fn unobserveProperty = nullptr;
    qqmpv_mpv_wait_event_fn waitEvent = nullptr;
    qqmpv_mpv_set_wakeup_callback_fn setWakeupCallback = nullptr;
    qqmpv_mpv_free_fn free = nullptr;

    // render_gl API.
    qqmpv_mpv_render_context_create_fn renderContextCreate = nullptr;
    qqmpv_mpv_render_context_free_fn renderContextFree = nullptr;
    qqmpv_mpv_render_context_render_fn renderContextRender = nullptr;
    qqmpv_mpv_render_context_set_update_callback_fn renderContextSetUpdateCallback = nullptr;
    qqmpv_mpv_render_context_report_swap_fn renderContextReportSwap = nullptr;

    bool loaded = false;
    unsigned long clientApi = 0;
    QString mpvVersion;      // human readable, e.g. "mpv 0.40.0"
    QString failureReason;   // why loaded == false, for the UI hint

    // Tries libmpv.so.2 (then plain names for unusual distros). Idempotent;
    // on failure every field stays null/false and failureReason is set.
    static MpvLibrary load();
};

} // namespace QindaMpv::Player
