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
#include "kick_ui/audio_components/gridview/kick_GridData.h"
#include "kick_ui/audio_components/step_sequencer/kick_NoteListWatcher.h"

namespace kick_ui
{
    class ListEntry;

    class ListEditTimingCorrect : public juce::Component
    {
    public:
        ListEditTimingCorrect();
        ~ListEditTimingCorrect();

        // Explicit "dialog is open" state for the hardware bridge (the dialog is modal and has touch-only buttons).
        // Message thread only. closeFromHardware() ends the modal state with APPLY / CANCEL (same as the buttons);
        // false (and nothing closed) when no dialog is open or a nested popup is the foremost modal component.
        static bool isOpen();
        static bool closeFromHardware( bool apply );

        // Current dialog values as engine params (Apply -> MTAudioInterface::timingCorrect).
        mt::track::timingCorrectInfo getData();
        // Global TC Off/On choice; the caller commits it to the engine on Apply only.
        bool getGlobalEnabled();

    private:
        ui::KickToggleOptions globalTC{ "GLOBAL TIMING CORRECT", { "Off", "On" } };
        ui::KickLabel         timeDivisionLabel{ "TIME DIVISION" };
        ui::KickListBox       timeDivList{ ui::KickListBox::withoutTitleBar, "", CommonUtils::getDefaultTimeDivisions(), 3 };
        ui::KickImageButton   triplet{ "TRIPLET" };
        ui::KickListBox       type{ "TYPE", { "Start", "End", "Length", "Legato" } };
        // "Range" is not supported by the engine (it always answered "range not supported"): not offered.
        ui::KickListBox       events{ "EVENTS", { "All", "Selected" } };
        ui::KickSlider        swing{ "SWING", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
        ui::KickSlider        shiftTiming{ "SHIFT  TIMING", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
        ui::KickSlider        window{ "WINDOW", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
        ui::KickSlider        strength{ "STRENGTH", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };

        void resized() override;
    };

    class ListEditMode : public IScreen
        , public juce::Component
    {
    public:
        ListEditMode();
        ~ListEditMode();

        bool canClose() override { return true; }
        bool requiresControlBar() override { return false; }
        bool requiresStatusBar() override { return true; }
        bool requiresHandleCommand() override { return true; }

        void handleCommand( CPktMgr::HWMsg_t& ) override;
        void knobCommand( int knob, int value );

    private:
        mt::MTAudioInterface& ae{ mt::getAudioInterface() };
        const mt::MTSelected& sel{ mt::getAudioInterface().getSelected() };
        AppStatusbar&         statusBar{ AppStatusbar::Instance() };

        KickModeHeader         header{ "List Edit", "Icon_listEdit" };
        HeaderBarTrackSelector trackSelector;
        GridSettings&          gridSettings{ GridSettings::Instance() };
        GridData&              gridData{ GridData::Instance() };

        std::unique_ptr<ui::KickTableView> tableView;
        ui::KickTableViewProvider          provider;

        ui::KickListBox      filterListBox{ ui::KickListBox::withTitleBar, "FILTER", {} };
        ui::KickToggleButton snapButton{ "Snap:Off", "Snap:On", "Snap:Off" };
        ui::KickToggleButton deleteButton{ "Delete", "Delete", "Delete" };
        ui::KickToggleButton insertButton{ "Insert", "Insert", "Insert" };
        ui::KickButton       tcButton{ "TC", "TC" };

        // Mod Type / Value columns have no events behind them (only notes exist): hidden.
        std::vector<juce::String>                   headerTitles{ "Event", "Time", "Pad/Note", "Length", "Velocity", "Mod Type", "Value", "Prob", "Rat" };
        std::vector<std::unique_ptr<ui::KickLabel>> tableHeaderData;
        int                                         numNotes{ 0 };
        juce::ValueTree                             noteList;

        NoteListWatcher noteWatcher;
        unsigned        lastTouchMs{ 0 };

        void refreshData();
        void reloadNotes( int SelectRow = -2 ); // -2 keep the selection
        void externalNotesChanged();
        void updateNoteNaming();
        void editTime( ListEntry* Entry );
        void editPad( ListEntry* Entry );
        void stepRowBy( int Delta );
        void mouseDown( const juce::MouseEvent& ) override;
        void mouseDrag( const juce::MouseEvent& ) override;
        void mouseWheelMove( const juce::MouseEvent&, const juce::MouseWheelDetails& ) override;
        void paint( juce::Graphics& g ) override;
        void resized() override;
        void visibilityChanged() override;
        void markSelectedRows();
        void updateViewPosition();
    };

    class ListEntry : public juce::Component
    {
    public:
        ListEntry( int Row, juce::ValueTree Vt );

        void                 setValues( int Row, juce::ValueTree Vt );
        void                 refreshData();
        void                 setSelected( bool Select );
        void                 linkRotated( int ID, int value );
        void                 adjust( list_edit_logic::Cell C, int Delta ); // relative edit, straight on the note
        juce::Rectangle<int> getElementBounds( int state );
        juce::ValueTree      getValueTree() const { return vt; }

        std::function<void( ListEntry* )> onPadTapped;
        std::function<void( ListEntry* )> onTimeTapped;
        std::function<void()>             onEdited;

        // Keygroup / MIDI programs show note names (C3), drum programs pad names (A01).
        static bool useNoteNames;

    private:
        juce::ValueTree    vt;
        bool               isSelected{ false };
        ui::KickTitleLabel time{ "" };
        ui::KickLabel      noteLabel{ "" };
        ui::KickNumber     length{ "Length", 1, 9999, 1, 1, false }, velocity{ "Velocity", 1, MIDI::MAX, 1, 1, false }, rat{ "Rat", 1, 8, 1, 1, false }, prob{ "Probability", 0, 100, 100, 1, false };
        ui::KickLabel      row{ "" };

        void writeProperty( const juce::Identifier& Id, int Value );
        void mouseUp( const juce::MouseEvent& ) override;
        void resized() override;
    };

    // Edit the time of one note: BAR / BEAT / TICK (same fields as Insert).
    class ListEditTimeDialog : public juce::Component
    {
    public:
        ListEditTimeDialog( int Bar1, int Beat1, int Tick0, int Bars )
        {
            mt::utils::addAndMakeVisible( this, bar, beat, tick, barLabel, beatLabel, tickLabel );
            mt::utils::setValueColour( ui::Theme::textColour( 1 ), barLabel, beatLabel, tickLabel );
            mt::utils::setValueBackgroundColour( ui::Theme::backgroundColour( 9 ), barLabel, beatLabel, tickLabel );
            mt::utils::setValueFontHeight( 16, barLabel, beatLabel, tickLabel, bar, beat, tick );
            const auto beats = juce::jmax( 1, gridData.getBeatsPerBar( Bar1 - 1 ) );
            const auto tpb   = juce::jmax( 1, gridData.getTicksPerBar( Bar1 - 1 ) / beats );
            bar.setRange( 1, juce::jmax( 1, Bars ), 1 );
            beat.setRange( 1, beats, 1 );
            tick.setRange( 0, list_edit_logic::maxTick( tpb ), 1 );
            bar.setValue( Bar1, juce::dontSendNotification );
            beat.setValue( Beat1, juce::dontSendNotification );
            tick.setValue( Tick0, juce::dontSendNotification );
            setName( "EDIT TIME" );
        }
        int getBar() const { return bar.getValue(); }
        int getBeat() const { return beat.getValue(); }
        int getTick() const { return tick.getValue(); }

    private:
        GridData&      gridData{ GridData::Instance() };
        ui::KickNumber bar{ "", BAR::MIN, BAR::MAX, 1 }, beat{ "", BAR::MIN, BAR::MAX, 1 }, tick{ "", BAR::MIN, BAR::MAX, 0 };
        ui::KickLabel  barLabel{ "BAR" }, beatLabel{ "BEAT" }, tickLabel{ "TICK" };

        void resized() override
        {
            barLabel.setBoundsRelative( .083f, .1f, .167f, .2f );
            beatLabel.setBoundsRelative( .083f, .4f, .167f, .2f );
            tickLabel.setBoundsRelative( .083f, .7f, .167f, .2f );
            bar.setBoundsRelative( .334f, .1f, .582f, .22f );
            beat.setBoundsRelative( .334f, .4f, .582f, .22f );
            tick.setBoundsRelative( .334f, .7f, .582f, .22f );
        }
    };

    // Pick the pad / note of one note.
    class ListEditPadDialog : public juce::Component
    {
    public:
        ListEditPadDialog( const std::vector<juce::String>& Names, int Current )
        {
            addAndMakeVisible( list );
            list.setJustificationType( juce::Justification::centred );
            list.setListOptions( Names, Current );
            list.onSelectionChanged = [this]( int Index, const juce::String& ) { selected = Index; };
            selected                = Current;
            setName( "EDIT PAD / NOTE" );
        }
        int getSelected() const { return selected; }

    private:
        ui::KickListBox list{ ui::KickListBox::withoutTitleBar, "", {} };
        int             selected{ 0 };
        void            resized() override { list.setBoundsRelative( .2f, .4f, .6f, .2f ); }
    };

    class ListEditInsertDialog : public juce::Component
    {
    public:
        struct ListInsertOptions
        {
            int          bar;
            int          beats;
            int          tick;
            juce::String sample;
            int          padIndex;
        };

        int getStartBar() const { return bar.getValue(); }
        int getStartBeat() const { return beat.getValue(); }
        int getStartTick() const { return tick.getValue(); } // 0-based, like the list

        ListEditInsertDialog()
        {
            mt::utils::addAndMakeVisible( this, bar, beat, tick, barLabel, beatLabel, tickLabel, typeLabel, padLabel, typeList, padsList );

            mt::utils::setValueColour( ui::Theme::textColour( 1 ), barLabel, beatLabel, tickLabel, typeLabel, padLabel );
            mt::utils::setValueBackgroundColour( ui::Theme::backgroundColour( 9 ), barLabel, beatLabel, tickLabel, typeLabel, padLabel );
            mt::utils::setValueFontHeight( 16, barLabel, beatLabel, tickLabel, typeLabel, padLabel, bar, beat, tick );
            mt::utils::setJustificationType( juce::Justification::centred, padsList, typeList );

            setName( "INSERT EVENT" );

            auto numBars = ae.getSequenceLength( ae.getSelectedSequence() );
            bar.setRange( 1, numBars, 1 );
            updateRanges( 0 );

            bar.onValueChanged          = [&]( int Value ) { info.bar = Value; updateRanges( Value - 1 ); };
            beat.onValueChanged         = [&]( int Value ) { info.beats = Value; };
            tick.onValueChanged         = [&]( int Value ) { info.tick = Value; };
            padsList.onSelectionChanged = [&]( int Index, const juce::String& value ) { info.padIndex = Index; };
            typeList.onSelectionChanged = [&]( int Index, const juce::String& Value ) {};
            // Automation events have no model yet (the list only holds notes): only "Notes" is offered.

            padsList.setValueBackgroundColour( ui::Theme::backgroundColour( 13 ) );

            int letterAsciiCode = 65;
            padData.clear();
            for( int i = 0; i <= kick_Constants::totalPianoKeys; i += 16 )
            {
                for( int j = 0; j < 16; j++ )
                {
                    juce::String noteLabel;
                    noteLabel = noteLabel.formatted( "%c%02d", letterAsciiCode, ( j + 1 ) );
                    padData.push_back( noteLabel );
                }
                letterAsciiCode++;
            }
            padsList.setListOptions( padData );
            padsList.setSelected( 0, juce::sendNotification );
        }

        // BEAT 1..beats of the bar, TICK 0..ticksPerBeat-1 (the list shows ticks 0-based).
        void updateRanges( int Bar0 )
        {
            const auto beats = juce::jmax( 1, gridData.getBeatsPerBar( Bar0 ) );
            const auto tpb   = juce::jmax( 1, gridData.getTicksPerBar( Bar0 ) / beats );
            beat.setRange( 1, beats, 1 );
            tick.setRange( 0, list_edit_logic::maxTick( tpb ), 1 );
        }

        ~ListEditInsertDialog() {}
        ListInsertOptions& getData() { return info; }

    private:
        ListInsertOptions         info{ 1, 1, 0, {}, 0 };
        mt::MTAudioInterface&     ae{ mt::getAudioInterface() };
        GridData&                 gridData{ GridData::Instance() };
        std::vector<juce::String> padData;
        ui::KickListBox           padsList{ ui::KickListBox::withoutTitleBar, "", {} };
        ui::KickNumber            bar{ "", BAR::MIN, BAR::MAX, 1 };
        ui::KickNumber            beat{ "", BAR::MIN, BAR::MAX, 1 };
        ui::KickNumber            tick{ "", BAR::MIN, BAR::MAX, 0 };
        ui::KickListBox           typeList{ ui::KickListBox::withoutTitleBar, "", { "Notes" } };
        ui::KickLabel             barLabel{ "BAR" }, beatLabel{ "BEAT" }, tickLabel{ "TICK" }, typeLabel{ "TYPE" }, padLabel{ "PAD" };
        int                       barValue, beatValue, tickValue;

        void resized() override
        {
            barLabel.setBoundsRelative( .083f, .052f, .167f, .088f );
            beatLabel.setBoundsRelative( .083f, .224f, .167f, .088f );
            tickLabel.setBoundsRelative( .083f, .395f, .167f, .088f );
            typeLabel.setBoundsRelative( .083f, .656f, .167f, .12f );
            padLabel.setBoundsRelative( .083f, .828f, .167f, .12f );

            bar.setBoundsRelative( .334f, .052f, .582f, .12f );
            beat.setBoundsRelative( .334f, .224f, .582f, .12f );
            tick.setBoundsRelative( .334f, .395f, .582f, .12f );
            typeList.setBoundsRelative( .334f, .656f, .582f, .12f );
            padsList.setBoundsRelative( .334f, .828f, .582f, .12f );
        }
    };

}
