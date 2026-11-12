// SPDX-License-Identifier: AGPL-3.0-or-later
// Kick - hooks through which the visible plug-in panel takes hardware buttons the bridge would otherwise send to the DAW.
#pragma once
#include <functional>

namespace kick::clap {
/** Undo / redo (redo = true with Shift): set by the visible plug-in panel, returns true when the plug-in took it. Empty or false: the project's undo. */
inline std::function<bool(bool redo)>& hardwareUndoHook() { static std::function<bool(bool)> hook; return hook; }
} // namespace kick::clap
