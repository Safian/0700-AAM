// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

namespace kick_ui
{
    // The one Time Signature dialog of the app: the Sequence tab option, the Metronome popup, the BBT (Locate) popup and the status
    // bar tempo / time signature field all open it for a sequence. Numerator 1..16, denominator 4 / 8 / 16 / 32 (mt::timesig::isValid);
    // the engine (setTimeSignature) keeps the events on their ticks and rounds the length up to whole bars, in one undo step.
    class TimeSignatureDialog
    {
    public:
        // Opens the dialog for `sequence`. onApplied runs after a successful change (UI refresh). Does nothing when the
        // sequence does not exist.
        static void show( int sequence, std::function<void()> onApplied = {} );

        // "4/4" for the sequence ("" when it does not exist); the label the status bar and the popups show.
        static juce::String describe( int sequence );
    };
}
