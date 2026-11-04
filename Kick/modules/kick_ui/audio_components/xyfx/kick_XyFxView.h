#pragma once
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

namespace kick_ui
{
    class XyFxSetupScreen : public juce::Component
    {
    public:
        std::function<void( void )> onPresetChange; // the preset changed (axes change)
        std::function<void( void )> onChanged;      // anything changed (preset, attack, release, wet / dry)

        XyFxSetupScreen();
        void paint( juce::Graphics& g );
        void resized() override;

        int   getPreset() { return presets.getSelectedIndex() < 0 ? 0 : presets.getSelectedIndex(); }
        float getAttackPercent() { return (float)attack.getValue(); }
        float getReleasePercent() { return (float)release.getValue(); }
        float getWetPercent() { return (float)wetDry.getValue(); }
        void  setValues( int Preset, float AttackPct, float ReleasePct, float WetPct ); // restore, no change callbacks
        void  stepPreset( int Delta );                                                  // Data Dial / +-
        void  knobCommand( int knob, int value );
        void  setXYCoordinateValues( const float xCoord, const float yCoord );
        // "LPF Manual | Attack 12 ms | Release 80 ms | Wet 100 %" for the status line (the panel may be closed)
        juce::String summary();

    private:
        void refreshAxes();

        ui::KickListBox                 presets{ "PRESETS", 2 };
        ui::KickSliderWithAttachedValue attack{ "Attack" };
        ui::KickSliderWithAttachedValue release{ "Release" };
        ui::KickSlider                  wetDry{ "Wet/Dry" };
        ui::KickLabel                   xAxisLabel{ "X AXIS", "Beat Division", true };
        ui::KickLabel                   yAxisLabel{ "Y AXIS", "Cutoff", true };
        ui::KickLabel                   xCoordinateValue;
        ui::KickLabel                   yCoordinateValue;
        ui::KickListBox                 xCoordinates{ "X AXIS" };
        ui::KickListBox                 yCoordinates{ "Y AXIS" };
    };

    class XyFxView : public juce::Component
        , public juce::Button::Listener
        , public kick_ui::IScreen
    {
    public:
        XyFxView();
        ~XyFxView() override;

        bool requiresControlBar() override { return false; }
        bool requiresStatusBar() override { return true; }
        bool requiresHandleCommand() override { return true; }

        void handleCommand( CPktMgr::HWMsg_t& ) override;

        std::function<void( void )> onchange;

    private:
        // Where the effect sits: the selected track's program, or a bus (return / submix / output).
        struct Target
        {
            juce::String label;
            bool         isTrack{ true };
            juce::String bus;
            bool         isMidiCc{ false };
        };

        mt::MTIInsertsInterface* interfaceFor( const Target& T );
        void                     rebuildTargets();
        const Target*            selectedTarget();
        void                     push();            // current state -> the engine (lock-free)
        void                     closeEffect();     // gate shut on the applied destination, grid dark
        void                     applyGate();
        void                     saveState();
        void                     loadState();
        void                     showSummary();
        double                   bpm();

        ui::KickToggleButton latch{ "Latch", "Latch", "Latch" };
        ui::KickToggleButton setup{ "Setup", "Setup", "Setup" };
        ui::KickToggleButton on_off{ "Off", "On", "Off" };
        ui::KickLabel        qlinkModeBadge{ "Q-LINK" };
        ui::KickListBox      output{ ui::KickListBox::withTitleBar, "XY Location", {} };
        ui::XYFXGrid         xyFx;

        XyFxSetupScreen                setupScreen;
        KickModeHeader                 header{ "XY Effects", "Icon_XY" };
        KickProgramSelectorWithDisplay hdrComp{ true };

        xyfx::Gate          gate;
        std::vector<Target> targets;
        Target              applied;          // the destination the engine currently holds the effect on
        bool                hasApplied{ false };
        bool                restoring{ false };

        void resized() override;
        void visibilityChanged() override;
        void buttonClicked( juce::Button* ) override;
    };

}
