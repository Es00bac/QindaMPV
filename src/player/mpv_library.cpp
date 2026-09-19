// SPDX-License-Identifier: GPL-3.0-or-later

#include "mpv_library.h"

#include <QLibrary>
#include <QStringList>

namespace QindaMpv::Player {

namespace {

// AGENT-GUARD: every entry must resolve before the library is accepted. A
// partially resolved client API is an ABI trap — calls would jump through
// null pointers. When adding a symbol to MpvLibrary, add it here in the same
// order and keep the typedef in mpv_api.h alongside.
bool resolveAll(QLibrary &library, MpvLibrary &out, QString &missingSymbol)
{
    struct Entry {
        const char *symbol;
        void **slot;
    };
    const Entry entries[] = {
        {"mpv_client_api_version", reinterpret_cast<void **>(&out.clientApiVersion)},
        {"mpv_error_string", reinterpret_cast<void **>(&out.errorString)},
        {"mpv_event_name", reinterpret_cast<void **>(&out.eventName)},
        {"mpv_create", reinterpret_cast<void **>(&out.create)},
        {"mpv_initialize", reinterpret_cast<void **>(&out.initialize)},
        {"mpv_terminate_destroy", reinterpret_cast<void **>(&out.terminateDestroy)},
        {"mpv_set_option_string", reinterpret_cast<void **>(&out.setOptionString)},
        {"mpv_command", reinterpret_cast<void **>(&out.command)},
        {"mpv_command_async", reinterpret_cast<void **>(&out.commandAsync)},
        {"mpv_get_property", reinterpret_cast<void **>(&out.getProperty)},
        {"mpv_get_property_string", reinterpret_cast<void **>(&out.getPropertyString)},
        {"mpv_set_property", reinterpret_cast<void **>(&out.setProperty)},
        {"mpv_set_property_string", reinterpret_cast<void **>(&out.setPropertyString)},
        {"mpv_observe_property", reinterpret_cast<void **>(&out.observeProperty)},
        {"mpv_unobserve_property", reinterpret_cast<void **>(&out.unobserveProperty)},
        {"mpv_wait_event", reinterpret_cast<void **>(&out.waitEvent)},
        {"mpv_set_wakeup_callback", reinterpret_cast<void **>(&out.setWakeupCallback)},
        {"mpv_request_log_messages", reinterpret_cast<void **>(&out.requestLogMessages)},
        {"mpv_free", reinterpret_cast<void **>(&out.free)},
        {"mpv_render_context_create", reinterpret_cast<void **>(&out.renderContextCreate)},
        {"mpv_render_context_free", reinterpret_cast<void **>(&out.renderContextFree)},
        {"mpv_render_context_render", reinterpret_cast<void **>(&out.renderContextRender)},
        {"mpv_render_context_set_update_callback",
         reinterpret_cast<void **>(&out.renderContextSetUpdateCallback)},
        {"mpv_render_context_report_swap",
         reinterpret_cast<void **>(&out.renderContextReportSwap)},
    };
    for (const Entry &entry : entries) {
        // QLibrary::resolve yields QFunctionPointer; converting to void* is
        // the POSIX-safe way to store it in the typed table below.
        *entry.slot = reinterpret_cast<void *>(library.resolve(entry.symbol));
        if (*entry.slot == nullptr) {
            missingSymbol = QString::fromLatin1(entry.symbol);
            return false;
        }
    }
    return true;
}

} // namespace

MpvLibrary MpvLibrary::load()
{
    MpvLibrary out;
    // libmpv.so.2 is the stable soname on every mainstream distribution; the
    // bare names cover source builds installed without versioned links.
    const QStringList candidates = {
        QStringLiteral("mpv.so.2"),
        QStringLiteral("mpv.2"),
        QStringLiteral("mpv"),
    };
    QLibrary library;
    bool found = false;
    for (const QString &candidate : candidates) {
        library.setFileName(candidate);
        if (library.load()) {
            found = true;
            break;
        }
    }
    if (!found) {
        out.failureReason = QStringLiteral("libmpv.so.2 not found");
        return out;
    }

    QString missingSymbol;
    if (!resolveAll(library, out, missingSymbol)) {
        out = {};
        out.failureReason = QStringLiteral("libmpv lacks symbol %1 (too old?)").arg(missingSymbol);
        return out;
    }

    out.clientApi = out.clientApiVersion();
    if (out.clientApi < QQMPV_MPV_MIN_CLIENT_API) {
        out = {};
        out.failureReason = QStringLiteral("libmpv client API too old for the render API");
        return out;
    }

    out.loaded = true;
    // The human-readable version comes from the handle once it exists; the
    // controller fills mpvVersion after mpv_create (property "mpv-version").
    return out;
}

} // namespace QindaMpv::Player
