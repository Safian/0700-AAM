// SPDX-License-Identifier: AGPL-3.0-or-later
// Native Apple helpers (compiled via kick_ui_apple.mm on iOS & macOS):
// 1. In-app browser OAuth PKCE session via AuthenticationServices (ASWebAuthenticationSession).
// 2. Native iOS Files / Google Drive / iCloud Drive document picker (UIDocumentPickerViewController).
#pragma once

#include <functional>

namespace kick_apple_cloud
{
    // Launches ASWebAuthenticationSession on iOS / macOS (and reports the callback URL or error asynchronously).
    // On unsupported platforms invokes onDone(false, "Unsupported platform").
    void startWebAuthSession( const juce::String& authUrl,
                              const juce::String& callbackScheme,
                              std::function<void( bool ok, juce::String callbackUrlOrError )> onDone );

    // On iOS, opens UIDocumentPickerViewController allowing multi-selection of audio/MIDI/Kick files
    // from Files, Google Drive app, iCloud Drive, OneDrive, etc., staged into a temporary directory ready to import.
    void pickFilesFromIosDocumentProvider( std::function<void( juce::StringArray stagedPaths, juce::String error )> onDone );
}
