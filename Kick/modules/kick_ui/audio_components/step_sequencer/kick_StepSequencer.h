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
    class StepComponent : public juce::Component
        , private juce::AsyncUpdater
    {
    public:
        StepComponent( StepSequencerData& Data, StepSequencerTopControl& stepControl, StepPresets& stepPreset );

        void setMinimumSize( int W, int H );

        void refresh();

        void        toggleStep( int Index, int Velocity = 127 );
        void        updateStepValue( int Index, int Velocity );
        inline void updateStepValue( int Index, float Normalized );

        void nudge( bool direction, int steps = 1 );
        std::function<void()> onNudged;
        std::function<void()> onPageFollowed; // the view followed the play head to another page
        void knobRotated( int knob, int Velocity );
        void setPadSelect( bool State ) { isPadSelected = State; }
        void setPage( int index ) { control.setSelectedPage( index ); }
        int  getPage() { return control.getPage(); }
        void asyncUpdate() { triggerAsyncUpdate(); };
        void setOnFirstLaunch( bool State ) { onFirstLaunch = State; }
        // Shown on screen: only then the engine is read, the pads are updated and the play head followed.
        void setActive( bool Active );
        // Hardware encoder / pad on the page position 'Index': changes only the shown parameter.
        void adjustStep( int AbsoluteStep, int Delta );

    private:
        int   getStepFromX( int X );
        float getNormalizedHeightFromY( int Y );

        void updateValuestoEngine( int Values );
        void updateValuestoEngine( std::vector<int>& Values );
        void mirrorSteps();
        void flipSteps();

        step_model::Param currentParam() const;
        int               stepStart( int Index ) const;
        void              notesOfStep( int Index, std::vector<mt::MTNote>& Out );
        void              writeParam( const mt::MTNote& N, step_model::Param P, int Value );
        void              createStepNote( int Index, step_model::Param P, int Value );
        void              eraseStepNotes( int Index );
        int               computePlayStep();
        void              updatePlayStep();
        const juce::Drawable* stepDrawable( bool Active, int Number );

        void handleAsyncUpdate() override;

        void paint( juce::Graphics& g ) override;
        void resized() override;

        void handleMouse( const juce::MouseEvent& event );
        void mouseDown( const juce::MouseEvent& event ) override;
        void mouseDrag( const juce::MouseEvent& event ) override;
        void mouseUp( const juce::MouseEvent& event ) override;

        void drawSteps( juce::Graphics& g );

        // Resize as per numSteps available...
        inline void updateCanvasSize() { setMinimumSize( minWidth, height ); }

        inline void clearSteps()
        {
            for( auto& it : data.indexValues )
                it = 0;
        }
        inline void clearSelection()
        {
            for( auto it : data.selected )
                it = 0;
        }

    private:
        mt::MTAudioInterface&  ae{ mt::getAudioInterface() };
        const mt::MTSelected&  aeSel{ mt::getAudioInterface().getSelected() };
        const mt::MTBarNBeats& playHeadBnB{ ae.getPlayhead().bnb };

        StepSequencerData&       data;
        GridData&                gridData{ GridData::Instance() };
        StepSequencerTopControl& control;
        StepPresets&             preset;

        std::vector<mt::MTNote> notes;

        mt::NotesInRange range;

        ui::KickImageButton button1{ "Button1", "seq_button1" };

        juce::Rectangle<int> buttonArea;
        juce::Rectangle<int> noteArea;

        bool isPadSelected{ false };
        bool state{ false };
        bool onFirstLaunch{ false };
        std::atomic<bool> active{ false };
        bool              gestureOpen{ false }; // a touch gesture owns the open undo transaction

        struct PlayheadAsync : juce::AsyncUpdater
        {
            std::function<void()> fn;
            void                  handleAsyncUpdate() override
            {
                if( fn )
                    fn();
            }
        } playheadAsync;
        int playStepIndex{ -1 };
        std::map<juce::String, std::unique_ptr<juce::Drawable>> svgCache;

        int width{ 0 };
        int height{ 0 };
        int minWidth{ 0 };

        int lastIndex = -1;

        std::vector<std::unique_ptr<ui::KickImageButton>> buttonNum;
    };
    class StepSequencer : public juce::Component
        , public kick_ui::IScreen
        , private juce::AsyncUpdater
    {
    public:
        StepSequencer();
        ~StepSequencer() = default;

        bool requiresControlBar() override { return false; }
        bool requiresStatusBar() override { return true; }
        bool requiresHandleCommand() override { return true; };

        std::function<void( int )> onBarsChange;

        void handleCommand( CPktMgr::HWMsg_t& ) override;

        // CLASSIC (false) or GRID (true) view.
        void showGridView( bool Grid );

        int getTrackNo( void ) const { return track; };
        int getSeqNo( void ) const noexcept { return seq; };

    private:
        void resized() override;
        void mouseDrag( const juce::MouseEvent& event ) override;
        void mouseUp( const juce::MouseEvent& event ) override;
        void mouseDown( const juce::MouseEvent& event ) override;

        void visibilityChanged() override;
        void handleAsyncUpdate() override;

        void refresh();
        void applyViewMode();

        void clearPadData();
        void clearBarData();
        void setTempo();
        void movePad( int Delta );
        void moveBar( int Delta );
        void updatePadLeds();
        void updatePadName();
        void updateAvailability(); // no track / audio or clip track: controls off + notice
        void externalNotesChanged();
        int  eraseNotesStartingIn( int Pad, int FromTick, int ToTick );

    private:
        mt::MTAudioInterface& ae{ mt::getAudioInterface() };
        const mt::MTSelected& aeSel{ mt::getAudioInterface().getSelected() };
        GridData&             gridData{ GridData::Instance() };

        ui::KickViewport               stepViewport;
        ui::KickListBox                tracks{ ui::KickListBox::withTitleBar, "TRACK", {} };
        ui::KickLabel                  padName{ "" };
        ui::KickNumber                 totalBars{ "TOTAL BARS", BAR::MIN, BAR::MAX, BAR::DEFAULT, BAR::STEP, true };
        ui::KickLabel                  warningLabel{ "Create any Track Before using StepSequencer" };
        ui::KickTempoWithAttachedValue bpmTempo{ "BPM", BPM::MIN, BPM::MAX, BPM::DEFAULT, BPM::STEP };

        mt::NotesInRange range;

        bool isPadSelect{ false };
        int  seq   = 0;
        int  track = 0;

        StepSequencerData       data;
        StepSequencerControl    stepControl;
        StepSequencerTopControl topControl;
        StepPresets             stepPreset{ data };
        StepComponent           stepComponent{ data, topControl, stepPreset };
        DrumGrid                drumGrid{ data, topControl };
        ui::KickToggleButton    viewModeButton{ "View", "GRID", "CLASSIC" };
        static int              lastPressedPad;
        NoteListWatcher         noteWatcher;

        KickModeHeader header{ "Step Sequencer", "Icon_Stepsequencer" };
    };
}
