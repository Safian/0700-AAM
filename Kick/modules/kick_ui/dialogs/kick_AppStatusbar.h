/*
 * Copyright (c) 2021 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once
#include "kick_ui/audio_components//gridview/kick_Locate.h"
#include "kick_statusbar_logic.h"
namespace kick_ui
{

    class LeftControl : public juce::Component
    {
    public:
        LeftControl();
        ~LeftControl() = default;
        mt::ac::LocateSTP stp;

    private:
        void resized() override;
        void mouseDown( const juce::MouseEvent& e ) override;
    };

    class RightControl : public juce::Component
    {
    public:
        RightControl();
        ~RightControl() = default;
        mt::ac::Locate bbt;

    private:
        void resized() override;
        void mouseDown( const juce::MouseEvent& e ) override;
    };

    class AppStatusbar : public juce::Component, public juce::DeletedAtShutdown, private juce::Timer
    {
    public:
        ~AppStatusbar() = default;

        static AppStatusbar& Instance();

        // A message in the Box Info line: it shows for a few seconds, then the standing state comes back.
        void setBoxInfo( juce::String );
        // The standing state of the line (e.g. "Unsaved project changes"; empty = nothing). A device fault of the startup
        // report takes its place.
        void setStanding( juce::String );

        // The popups of the three fields (also used by the smoke test): playhead control (STP), BBT locate, tempo editor.
        void showStpPopup() { left.stp.displaySTPPopup(); }
        void showBbtPopup() { right.bbt.displayPopup(); }
        void showTempoEditor() { editTempo(); }

    private:
        mt::MTPlayhead&   playHead{ mt::getAudioInterface().getPlayhead() };
        juce::ProgressBar progressBar{ playHead.normized };
        LeftControl       left;
        RightControl      right;
        ui::KickLabel     boxInfo{ "Box Info" };
        // Tempo and time signature of the selected sequence; a tap opens the Tempo editor / the Time Signature dialog.
        ui::KickImageButton tempoBtn{ "Tempo", "" }, timeSigBtn{ "Time signature", "" };

        kick_ui::statusbar_logic::BoxInfoState boxState;
        juce::String                           projectStanding, shownBox, shownTempo, shownTimeSig, lowStorage, lowMemory;
        int                                    resourceTicks{ 0 };

        void timerCallback() override;
        void refreshBox();
        void refreshTempo();
        void refreshResources(); // low storage / memory (every 5 s)
        void editTempo();

        // Don't any direct instantiaion of this class.
        AppStatusbar();

        bool init{ false };

        void paint( juce::Graphics& g ) override;
        void resized() override;
        void visibilityChanged() override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( AppStatusbar )
    };

}
