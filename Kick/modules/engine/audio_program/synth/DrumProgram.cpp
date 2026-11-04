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

// module include files...

#include "drum_voice.h"
#include "layer_play_select.h"

namespace mt
{
    /// @brief
    DrumProgram::DrumProgram( AudIO::Design& D, const juce::String& Name, MTIProgram* PDrum )
        : ISynthObject( D, POLYPHONY::MAX )
    {
        setName( Name.toStdString() );
        setProgram( PDrum );

        // Pre-reseve Memory...
        for( auto i = 0; i < getPolyphony(); ++i )
        {
            voices.emplace_back( std::make_unique<DrumVoice>( master ) );
        }
        ISynthObject::addVoices( voices );
    }

    DrumProgram::~DrumProgram()
    {
        setBypass( true );
    }

    void DrumProgram::setProgram( MTIProgram* PDrum )
    {
        // call base class method...
        ISynthObject::setProgram( PDrum );

        noteOffAll();
        pDrum = dynamic_cast<MTDrumProgram*>( PDrum );
        setBypass( ( pDrum == nullptr ) );
    }

    void DrumProgram::noteOn( int Note, int Velocity )
    {
        // Return if no audio synth available or invalid midi length
        if( !pDrum || !CommonUtils::isValidMidiNote( Note ) ) return;

        if( auto pNote = pDrum->getDrumClip( Note ) )
        {
            // Stop clips in same mute group.
            {
                auto notes = pDrum->getNotesInSameMuteGroup( Note );
                for( auto it : notes )
                    if( CommonUtils::isValidMidiNote( it ) )
                        noteOff( it );
            }

            // Stop playback of clips simultaneously.
            {
                auto notes = pNote->sMute.getActions();
                for( auto it : notes )
                    noteOff( it );
            }

            if( !pDrum->isNoteEffectivelyMuted( Note ) && pNote->getSource( 0 ).length() || pDrum->isNoteSolo( Note ) && pNote->getSource( 0 ).length() )
            {
                auto notePoly = pNote->polyPhony.get();

                // Layers that have a sound, then the layer by Layer Play (layer_play_select.h).
                std::vector<layerplay::Candidate> candidates;
                auto                              sources = pNote->getSources();
                for( auto i = 0; i < (int)sources.size(); ++i )
                    if( sources[i].isNotEmpty() )
                        candidates.push_back( { i, (int)pNote->layers[(size_t)i]->velStart.get(), (int)pNote->layers[(size_t)i]->velEnd.get() } );

                auto cycle = noteLayerIdx.try_emplace( Note, -1 ).first;
                auto mode  = pNote->layerPlay.get() == LayerPlayType::RANDOM ? layerplay::Mode::Random :
                             pNote->layerPlay.get() == LayerPlayType::CYCLE  ? layerplay::Mode::Cycle :
                                                                                layerplay::Mode::Velocity;
                auto layerToPlay = layerplay::select( mode, candidates, Velocity, cycle->second,
                                                      mode == layerplay::Mode::Random ? (unsigned)AudIO::utils::random<int>( 0, 1 << 20 ) : 0u );
                if( layerToPlay < 0 ) layerToPlay = 0;

                if( auto soundSlice = pNote->layers[layerToPlay]->src )
                {
                    // Start playback.
                    if( auto voice = dynamic_cast<DrumVoice*>( getFreeVoice( Note, notePoly ) ) )
                    {
                        voice->setPadGains( pNote->gL.load(), pNote->gR.load() );
                        voice->setSlice( soundSlice );
                        voice->noteOn( Note, Velocity );
                        addVoiceForPlayback( voice );
                    }
                }
            }

            // Trigger playback of clips simultaneously.
            {
                auto notes = pNote->sPlay.getActions();
                for( auto it : notes )
                    noteOn( it, Velocity );
            }
        }
    }

    void DrumProgram::noteOff( int Note )
    {
        if( !pDrum || !CommonUtils::isValidMidiNote( Note ) ) return;
        if( auto pNote = pDrum->getDrumClip( Note ) )
        {
            if( pNote->samplePlay.get() == SamplePlayType::ONE_SHOT )
                return;
        }
        ISynthObject::noteOff( Note );
    }
};
