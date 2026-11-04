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
    class WarningPopUp : public juce::Component
    {
    public:
        WarningPopUp( juce::String WarningText = "" )
        {
            mt::utils::addAndMakeVisible( this, infoText );
            mt::utils::setValueColour( ui::Theme::textColour( 1 ), infoText );
            mt::utils::setValueFontHeight( 24, infoText );
            mt::utils::setValueJustification( juce::Justification::centred, infoText );
            mt::utils::setValueBackgroundColour( ui::Theme::backgroundColour( 9 ), infoText );
            infoText.setText( WarningText, juce::dontSendNotification );
        }

    private:
        void resized() override
        {
            infoText.setBoundsRelative( .1f, .4f, .8f, .2f );
        }
        ui::KickLabel infoText;
    };

    class CopyPads : public juce::Component
    {
    public:
        CopyPads();

        int          getCopyTypeIndex() { return copyType.getSelectedIndex(); }
        int          getSourcePad() { return fromPads.getSelectedIndex(); }
        int          getDestinationPad() { return toPads.getSelectedIndex(); }
        juce::String getDestinationProgram() { return toProgram.getSelected(); }
        juce::String getSourceProgram() { return fromProgram.getSelected(); }
        int          getSourceBank() { return fromBank.getSelectedIndex(); }
        int          getDestinationBank() { return toBank.getSelectedIndex(); }
        int          getSourceStartPad() { return fromPadsStart.getSelectedIndex(); }
        int          getSourceEndPad() { return fromPadsEnd.getSelectedIndex(); }
        int          getDestinationStartPad() { return toPadsStart.getSelectedIndex(); }

    private:
        void resized() override;
        void visibilityChanged() override;

        ui::KickListBox copyType{ ui::KickListBox::withTitleBar, "COPY TYPE", { "PAD", "BANK", "RANGE" } };
        ui::KickListBox fromProgram{ "FROM PROGRAM" };
        ui::KickListBox toProgram{ "TO PROGRAM" };
        ui::KickListBox fromPads{ "FROM PADS", 4 };
        ui::KickListBox toPads{ "TO PADS", 4 };
        ui::KickListBox fromBank{ "FROM BANK" };
        ui::KickListBox toBank{ "TO BANK" };
        ui::KickListBox fromPadsStart{ "PADS START", 4 };
        ui::KickListBox fromPadsEnd{ "PADS END", 4 };
        ui::KickListBox toPadsStart{ "TO PADS START", 4 };
    };

    class MidiProgramSettings : public juce::Component
    {
    public:
        MidiProgramSettings();

        void setProgram( int progIndex );
        void applySettings();

    private:
        void resized() override;
        void visibilityChanged() override;

        int currentProgIndex{ -1 };

        ui::KickListBox      noteMin{ ui::KickListBox::withTitleBar, "NOTE MIN", ui::KickListBox::generateOptions( 0, 127, false ), 0, 4 };
        ui::KickListBox      noteMax{ ui::KickListBox::withTitleBar, "NOTE MAX", ui::KickListBox::generateOptions( 0, 127, false ), 127, 4 };
        ui::KickToggleButton learnBtn{ "Note Learn", "Learning...", "Note Learn" };

        ui::KickListBox      velMin{ ui::KickListBox::withTitleBar, "VEL MIN", ui::KickListBox::generateOptions( 1, 127, false ), 0, 4 };
        ui::KickListBox      velMax{ ui::KickListBox::withTitleBar, "VEL MAX", ui::KickListBox::generateOptions( 1, 127, false ), 126, 4 };
        ui::KickListBox      transpose{ ui::KickListBox::withTitleBar, "TRANSPOSE", ui::KickListBox::generateOptions( -36, 36, false ), 36, 4 };

        ui::KickToggleButton filterModWheel{ "Filter Mod Wheel", "Filter Mod Wheel", "Filter Mod Wheel" };
        ui::KickToggleButton filterPitchBend{ "Filter Pitch Bend", "Filter Pitch Bend", "Filter Pitch Bend" };
        ui::KickToggleButton filterAftertouch{ "Filter Aftertouch", "Filter Aftertouch", "Filter Aftertouch" };
        ui::KickToggleButton filterProgChange{ "Filter Prog Change", "Filter Prog Change", "Filter Prog Change" };
        ui::KickToggleButton filterSustain{ "Filter Sustain", "Filter Sustain", "Filter Sustain" };
        ui::KickToggleButton filterAllCC{ "Filter All CC", "Filter All CC", "Filter All CC" };
    };

    class AppProgramOptionsMenu : public ui::KickMenuComponent
    {
    public:
        AppProgramOptionsMenu();
        ~AppProgramOptionsMenu();
        int getSelectedId() { return selectedId; }

    private:
        int selectedId = -1;
    };

    class AppProgramOptions : public juce::Component
    {
    public:
        //==============================================================================
        AppProgramOptions();
        ~AppProgramOptions() = default;

        std::function<void()> onprogChanged;
        std::function<void()> onClose;
        void                  handleCommand( CPktMgr::HWMsg_t& HwMsg );

    private:
        ui::KickSerialNumber  srNo{ "SerialNumber", "ID_Program" };
        AppProgramDisplay     prg{ -1 };
        AppProgramOptionsMenu opts;
        ui::KickImageButton   close{ "Close", "Edit_Close" };

        CopyPads            copyPads;
        MidiProgramSettings midiSettings;

        bool isDialogClosed{ true };

        void visibilityChanged() override;
        void paint( juce::Graphics& g ) override;
        void resized() override;

        //CopyPads
        void copyPadDataToPad();
        void copyPadDataToBank();
        void copyPadDataToRange();
        void openCopyPadsPopup( bool isWarning = false, juce::String WarningInfo = "" );
        void openMidiSettingsPopup();
        void duplicateProgramToTrack();
        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( AppProgramOptions )
    };
}
