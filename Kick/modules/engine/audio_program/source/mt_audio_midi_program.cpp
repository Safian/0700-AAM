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

#include "mt_audio_midi_program.h"
#include "mt_midi_pan.h"

namespace mt
{
    MTMidiProgram::MTMidiProgram( AudIO::DawEngine& Engine, juce::ValueTree& VT, juce::UndoManager* UM )
        : MTIProgram( VT, UM )
        , MTIMidiProgramData( VT, UM )
    {
        sendPanCC10();
    }

    void MTMidiProgram::sendPanCC10()
    {
        auto port = getPort();
        if( port.isNotEmpty() && port != "None" )
        {
            const int cc = midipan::panToCC10( (double)vTree.getProperty( IDs::pan, PAN::DEFAULT ) );
            lastPanCC = cc;
            ae.sendMidiMessage( port, juce::MidiMessage::controllerEvent( getMidiChannel(), 10, cc ) );

            reloadMidiMeter( 1.f );
            outMidiMeter.reload( ( MIDI::SCALER * cc ) );
        }
    }

    void MTMidiProgram::processNoteLearn( int noteNumber )
    {
        if( !learningNotes.load() ) return;
        if( learnStep == 0 )
        {
            noteMin = noteNumber;
            noteMax = noteNumber;
            learnStep = 1;
        }
        else
        {
            if( noteNumber < noteMin.get() )
                noteMin = noteNumber;
            else
                noteMax = noteNumber;
            learningNotes = false;
            learnStep = 0;
        }
    }

    void MTMidiProgram::valueTreePropertyChanged( juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& Property )
    {
        auto port = getPort();

        if( vTree == treeWhosePropertyHasChanged )
        {
            // Strictly "CC<number>" (what the Q-Link Edit and the program page write): any other property that merely
            // contains "cc" must never become a controller message.
            const auto propName     = Property.toString();
            const bool isCcProperty = propName.startsWith( "CC" ) && propName.length() > 2 && propName.substring( 2 ).containsOnly( "0123456789" ) && propName.substring( 2 ).getIntValue() <= 127;
            if( isCcProperty )
            { // CC messages...
                const auto controllerType = propName.substring( 2 ).getIntValue();
                const auto value          = (int)vTree.getProperty( Property, 0 );
                auto       msg            = juce::MidiMessage::controllerEvent( getMidiChannel(), controllerType, value );

                ae.sendMidiMessage( port, msg );

                reloadMidiMeter( 1.f );
                outMidiMeter.reload( ( MIDI::SCALER * value ) );
            }
            else if( Property.toString().containsIgnoreCase( "BankLSB" ) || Property.toString().containsIgnoreCase( "BankMSB" ) )
            {
                juce::MidiMessage CCmsg;
                const auto        ch = getMidiChannel();

                // Send the Bank Select MSB message
                CCmsg = juce::MidiMessage::controllerEvent(
                    ch,
                    0, // MSB controller number
                    getBankMSB() );
                ae.sendMidiMessage( port, CCmsg );

                // Send the Bank Select LSB message
                CCmsg = juce::MidiMessage::controllerEvent(
                    ch,
                    32, // LSB controller number
                    getBankLSB() );
                ae.sendMidiMessage( port, CCmsg );

                // Send the program change message
                CCmsg = juce::MidiMessage::programChange( ch, getProgramChannel() );
                ae.sendMidiMessage( port, CCmsg );

                reloadMidiMeter( 1.f );
                outMidiMeter.reload( ( MIDI::SCALER * getProgramChannel() ) );
            }
            else if( Property.toString().containsIgnoreCase( "ProgCH" ) )
            {
                juce::MidiMessage CCmsg;

                // Send the program change message
                CCmsg = juce::MidiMessage::programChange( getMidiChannel(), getProgramChannel() );
                ae.sendMidiMessage( port, CCmsg );

                reloadMidiMeter( 1.f );
                outMidiMeter.reload( ( MIDI::SCALER * getProgramChannel() ) );
            }
            else if( Property == IDs::pan )
            {
                sendPanCC10();
            }
            else if( Property == IDs::MIDIPort )
            {
                sendPanCC10();
            }
        }
        else
            MTIProgram::valueTreePropertyChanged( treeWhosePropertyHasChanged, Property );
    }

    // this is not a reentrant function, calling from multiple threads simultaneously may result in incorrect results.
    void MTMidiProgram::noteProcess( juce::Array<juce::MidiMessage>& Msgs, bool /*live*/ )
    {
        const bool muted = isProgramEffectivelyMuted();

        auto progGain = MIDI::SCALER * level.get();
        const auto port = getPort();
        const auto ch   = getMidiChannel();
        const int  tr   = transpose.get();
        const int  minN = noteMin.get();
        const int  maxN = noteMax.get();
        const int  minV = velMin.get();
        const int  maxV = velMax.get();

        for( auto& it : Msgs )
        {
            if( muted )
            {
                // Muted: only release-type messages reach the external port (no stuck notes / bend / pedal), no meters.
                const bool release = it.isNoteOff() || it.isPitchWheel()
                                     || ( it.isController() && ( ( it.getControllerNumber() == 64 && it.getControllerValue() < 64 )
                                                                 || it.getControllerNumber() == 123 ) );
                if( release )
                {
                    it.setChannel( ch );
                    ae.sendMidiMessage( port, it );
                }
                continue;
            }

            if( it.isNoteOnOrOff() )
            {
                if( learningNotes.load() && it.isNoteOn() )
                    processNoteLearn( it.getNoteNumber() );

                // Program-level transposition
                int effNote = juce::jlimit( 0, 127, it.getNoteNumber() + tr );

                // Program-level note range filtering
                if( effNote < minN || effNote > maxN )
                    continue;

                // Program-level velocity range filtering (for note-on)
                if( it.isNoteOn() )
                {
                    int vel = it.getVelocity();
                    if( vel < minV || vel > maxV )
                        continue;
                }

                // Pad mute filtering: applies ONLY to note messages!
                if( isNoteEffectivelyMuted( effNote ) )
                    continue;

                it.setNoteNumber( effNote );

                auto noteLevel = MIDI::SCALER * ( it.isNoteOnOrOff() ? it.getVelocity() : 127 );
                if( it.isNoteOn() )
                    reloadMidiMeter( noteLevel );

                auto gainLevel = progGain * noteLevel;
                if( it.isNoteOnOrOff() )
                    it.setVelocity( gainLevel );

                outMidiMeter.reload( gainLevel );

                it.setChannel( ch );
                ae.sendMidiMessage( port, it );
                continue;
            }

            // Pitch Wheel filtering
            if( it.isPitchWheel() )
            {
                if( filterPitchBend.get() )
                    continue;

                it.setChannel( ch );
                ae.sendMidiMessage( port, it );
                continue;
            }

            // Channel Pressure / Aftertouch filtering
            if( it.isChannelPressure() || it.isAftertouch() )
            {
                if( filterAftertouch.get() )
                    continue;

                it.setChannel( ch );
                ae.sendMidiMessage( port, it );
                continue;
            }

            // Program Change filtering
            if( it.isProgramChange() )
            {
                if( filterProgramChange.get() )
                    continue;

                it.setChannel( ch );
                ae.sendMidiMessage( port, it );
                continue;
            }

            // Controller messages (CC)
            if( it.isController() )
            {
                int ccNum = it.getControllerNumber();
                if( filterAllCC.get() )
                    continue;
                if( ccNum == 1 && filterModWheel.get() )
                    continue;
                if( ccNum == 64 && filterSustain.get() )
                    continue;

                it.setChannel( ch );
                ae.sendMidiMessage( port, it );
                continue;
            }

            // Other MIDI messages
            it.setChannel( ch );
            ae.sendMidiMessage( port, it );
        }
    }

    void MTMidiProgram::processMidiMeter()
    {
        MTIProgram::processMidiMeter();
        outMidiMeter.process();
    }

} // namespace MT
