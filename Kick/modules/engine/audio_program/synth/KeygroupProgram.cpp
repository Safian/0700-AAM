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

#include "audio_voice.h"
#include "layer_play_select.h"

namespace mt
{
    /// @brief
    KeygroupProgram::KeygroupProgram( AudIO::Design& D, const juce::String& Name, MTIProgram* PKeys )
        : ISynthObject( D, POLYPHONY::MAX )
    {
        setName( Name.toStdString() );
        setProgram( PKeys );

        // Pre-reseve Memory...
        for( auto i = 0; i < getPolyphony(); ++i )
        {
            voices.emplace_back( std::make_unique<AVoice>( master ) );
        }
        ISynthObject::addVoices( voices );
    }

    KeygroupProgram::~KeygroupProgram()
    {
        setBypass( true );
    }

    void KeygroupProgram::setProgram( MTIProgram* PKeys )
    {
        // call base class method...
        ISynthObject::setProgram( PKeys );

        noteOffAll();
        pKeys = dynamic_cast<MTKeygroupProgram*>( PKeys );
        setBypass( ( pKeys == nullptr ) );
    }

    void KeygroupProgram::noteOn( int Note, int Velocity )
    {
        if( !pKeys || !CommonUtils::isValidMidiNote( Note ) )
            return;

        if( pKeys->isNoteEffectivelyMuted( Note ) ) return;

        // A key without an assigned sample still has a (default, empty) clip, so "no clip" alone is not the test: only a clip
        // with a sound source on some layer counts as a sample.
        const auto hasSound = []( MTIKeys* k ) {
            if( k == nullptr ) return false;
            for( auto l = 0; l < MAX_LAYERS; ++l )
                if( k->getSource( l ).isNotEmpty() && k->getSourceSlice( l ) != nullptr ) return true;
            return false;
        };

        std::vector<MTIKeys*> sounding;
        for( auto assignedNote : pKeys->getAssignedNotes() )
            if( auto* candidate = pKeys->getKeysClip( assignedNote ); hasSound( candidate ) ) sounding.push_back( candidate );
        if( sounding.empty() ) return;

        // The sample has no root-note field of its own (keyNote is its pad slot). A keygroup with exactly one sample is a
        // single-sample instrument: it sounds at original pitch on middle C and follows the keybed chromatically, wherever
        // it was assigned. With several samples the key is played by the sample whose zone (minNote..maxNote) holds it,
        // else by the nearest sample; every sample sounds at original pitch on its own keyNote (multisample instrument).
        MTIKeys* pNote = sounding.front();
        int      root  = 60;
        if( sounding.size() > 1 )
        {
            auto rank = [Note]( MTIKeys* k ) {
                const auto inZone = (int)k->minNote.get() <= Note && Note <= (int)k->maxNote.get();
                return ( inZone ? 0 : 1000 ) + std::abs( Note - (int)k->keyNote.get() );
            };
            for( auto* candidate : sounding )
                if( rank( candidate ) < rank( pNote ) ) pNote = candidate;
            root = pNote->keyNote.get();
            if( root < 0 || root > 127 ) root = Note;
        }

        // Layer: by Layer Play among the layers with a sound (layer_play_select.h). By velocity the layers already carry
        // the dynamics, so the voice then plays at full gain, as the sampler does.
        const bool byVelocity = pNote->layerPlay.get() == LayerPlayType::VELOCITY;
        std::vector<layerplay::Candidate> candidates;
        for( auto l = 0; l < MAX_LAYERS; ++l )
            if( pNote->getSource( l ).isNotEmpty() && pNote->getSourceSlice( l ) != nullptr )
                candidates.push_back( { l, (int)pNote->layers[l]->velStart.get(), (int)pNote->layers[l]->velEnd.get() } );
        auto mode = pNote->layerPlay.get() == LayerPlayType::RANDOM ? layerplay::Mode::Random :
                    pNote->layerPlay.get() == LayerPlayType::CYCLE  ? layerplay::Mode::Cycle :
                                                                       layerplay::Mode::Velocity;
        const int layer = layerplay::select( mode, candidates, Velocity, noteLayerIdx.try_emplace( Note, -1 ).first->second,
                                             mode == layerplay::Mode::Random ? (unsigned)AudIO::utils::random<int>( 0, 1 << 20 ) : 0u );
        if( layer < 0 ) return;

        auto notePoly = pNote->polyPhony.get();
        if( auto soundSlice = pNote->getSourceSlice( layer ) )
        {
            if( auto voice = dynamic_cast<AVoice*>( getFreeVoice( Note, notePoly ) ) )
            {
                double pr = std::pow( 2.0, ( Note - root ) / 12.0 );
                voice->setPitchRatio( pr );
                voice->setSlice( soundSlice );
                voice->noteOn( Note, byVelocity ? 127 : Velocity );
                addVoiceForPlayback( voice );
            }
        }
    }
}
