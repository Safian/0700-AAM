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

        void setBoxInfo( juce::String );

        // A message with an UNDO button in the status bar (destructive actions: Clear Pad / Clear Bar / shortening the
        // sequence ...). Shown for a few seconds, until the next setBoxInfo() or offerUndo(), or until it is pressed;
        // pressing it runs 'UndoAction' once. Message thread only.
        void offerUndo( const juce::String& Text, std::function<void()> UndoAction );
        bool isUndoOffered() const { return undoAction != nullptr; }

    private:
        mt::MTPlayhead&   playHead{ mt::getAudioInterface().getPlayhead() };
        juce::ProgressBar progressBar{ playHead.normized };
        LeftControl       left;
        RightControl      right;
        ui::KickLabel     boxInfo{ "Box Info" };
        ui::KickButton    undoButton{ "UNDO" };
        std::function<void()> undoAction;

        void clearUndoOffer();
        void timerCallback() override;

        // Don't any direct instantiaion of this class.
        AppStatusbar();

        bool init{ false };

        void paint( juce::Graphics& g ) override;
        void resized() override;
        void visibilityChanged() override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( AppStatusbar )
    };

}
