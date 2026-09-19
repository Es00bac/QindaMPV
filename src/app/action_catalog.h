// SPDX-License-Identifier: GPL-3.0-or-later
//
// The player's complete menu catalog. One list is the single source of truth
// for the exported global menu, the in-window fallback menu, and the
// shortcut hints shown in both — the panel, Tk.MenuBar and Shortcut items
// all dispatch through ApplicationCoordinator by these ids.
//
// AGENT-CONTRACT: ActionRegistry rejects any entry without a shortcut and
// has no separator concept, so every action carries an accelerator and
// grouping relies on menuOrder/order only. Dispatch lives in
// PlayerController::dispatchMenuAction; keep ids there in sync.

#pragma once

#include <qindaqt/app_shell/app_shell_types.h>

#include <QList>

namespace QindaMpv::App {

[[nodiscard]] QList<QindaQt::AppShell::ActionSpec> buildPlayerActions();

} // namespace QindaMpv::App
