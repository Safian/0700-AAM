/*
 * Copyright (c) 2024 MUSIC Tribe
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

#include "kick_ui_settings_midi_sync.h"

namespace kick_ui
{

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////  SettingsMidiSync
    ///////////////////////////////////////////////////////////////////////////////////////////////////////

    SettingsMidiSync::SettingsMidiSync()
        : SettingsInterface( 6, 2, true )
    {
        ui::KickGrid::setBackgroundColour( juce::Colours::transparentBlack );
        setRowTitles( { "MIDI Input", "MIDI Output" } );

        addComponent( inputList, 0, 0, 2 );
        addComponent( outputList, 1, 0, 2 );

        m_lastInputs  = mt::getAudioInterface().getMidiDeviceNames( true );
        m_lastOutputs = mt::getAudioInterface().getMidiDeviceNames( false );

        inputList.setListOptions( m_lastInputs );
        outputList.setListOptions( m_lastOutputs );

        juce::String savedInput = settings.midiControl.ipPortName();
        if( !savedInput.isEmpty() )
            inputList.setSelected( savedInput, juce::dontSendNotification );

        juce::String savedOutput = settings.midiControl.opPortName();
        if( !savedOutput.isEmpty() )
            outputList.setSelected( savedOutput, juce::dontSendNotification );

        inputList.onSelectionChanged = [this]( int index, juce::String name ) {
            juce::ignoreUnused( index );
            if( !m_isRefreshing && !name.isEmpty() )
            {
                settings.midiControl.ipPortName = name;
            }
        };

        outputList.onSelectionChanged = [this]( int index, juce::String name ) {
            juce::ignoreUnused( index );
            if( !m_isRefreshing && !name.isEmpty() )
            {
                settings.midiControl.opPortName = name;
            }
        };
    }

    void SettingsMidiSync::refresh()
    {
        m_isRefreshing = true;

        const auto input  = mt::getAudioInterface().getMidiDeviceNames( true );
        const auto output = mt::getAudioInterface().getMidiDeviceNames( false );

        if( input != m_lastInputs )
        {
            m_lastInputs = input;
            inputList.setListOptions( input );
        }

        if( output != m_lastOutputs )
        {
            m_lastOutputs = output;
            outputList.setListOptions( output );
        }

        juce::String savedInput = settings.midiControl.ipPortName();
        if( !savedInput.isEmpty() )
            inputList.setSelected( savedInput, juce::dontSendNotification );

        juce::String savedOutput = settings.midiControl.opPortName();
        if( !savedOutput.isEmpty() )
            outputList.setSelected( savedOutput, juce::dontSendNotification );

        m_isRefreshing = false;
    }

    void SettingsMidiSync::setupHeader( kick_ui::KickModeHeader& Hdr )
    {
        Hdr.setTitle( "Preferences - MIDI & Sync" );
    }

    void SettingsMidiSync::handleCommand( CPktMgr::HWMsg_t& ) {}
}
