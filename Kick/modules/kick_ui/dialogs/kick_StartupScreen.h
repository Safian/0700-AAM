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

namespace kick_ui
{
    // Text of one startup problem (cause + what to do) for the standard dialog (mt::utils::showDialog).
    class ProblemMessage : public juce::Component
    {
    public:
        explicit ProblemMessage( const kick_startup::Problem& p )
            : cause( p.cause ), action( p.action ) {}

        void paint( juce::Graphics& g ) override
        {
            auto b = getLocalBounds().reduced( 24, 56 );
            g.setFont( juce::Font( 19.0f ) );
            g.setColour( ui::Theme::textColour( 2 ) );
            const auto causeArea = b.removeFromTop( b.getHeight() * 6 / 10 );
            g.drawFittedText( cause, causeArea, juce::Justification::topLeft, 6 );
            g.setColour( ui::Theme::textColour( 1 ) );
            g.drawFittedText( action, b, juce::Justification::topLeft, 4 );
        }

    private:
        juce::String cause, action;
    };

    // Full-screen start-up screen: brand, version + build id, one line per start-up step from the shared state
    // (kick_startup::state()), an indeterminate progress bar. It leaves when every step has a final state (or after
    // kMaxShowMs as a safety net), and from then on it shows the start-up problems one by one in the standard dialog.
    class StartupScreen : public juce::Component
        , private juce::Timer
    {
    public:
        StartupScreen();
        ~StartupScreen() override = default;

        bool hitTest( int, int ) override { return isVisible(); } // swallows touches while it is up

        void paint( juce::Graphics& g ) override;
        void resized() override;

        static constexpr int kMaxShowMs = 20000;

    private:
        void timerCallback() override;
        void dismiss( const char* why );
        void showNextProblem();

        ui::KickImage          logo{ "", "LOGO" }; // the Kick logo of the active theme (Sim KICK logo.png)
        juce::String           buildText;
        juce::int64            startMs{ 0 };
        uint32_t               lastRevision{ 0xffffffffu };
        bool                   dismissed{ false };
        bool                   dialogOpen{ false };
        float                  barPhase{ 0.0f };
        kick_startup::Snapshot snap;
    };

    // One-line strip: connected MIDI devices, audio device, /data free space, pendrive install state. Read only; whatever
    // cannot be read shows "n/a". Refreshes itself while visible.
    class StatusStrip : public juce::Component
        , private juce::Timer
    {
    public:
        StatusStrip();
        void paint( juce::Graphics& g ) override;
        void visibilityChanged() override;
        void refresh();
        const juce::String& getText() const { return text; }

    private:
        void timerCallback() override { refresh(); }
        juce::String text;
    };
}
