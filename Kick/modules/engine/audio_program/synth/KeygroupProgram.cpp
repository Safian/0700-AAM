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

        // A key without an assigned sample still has a (default, empty) clip, so "no clip" alone is not the test: a clip
        // without a sound source counts as unassigned and falls back to the first assigned sample (chromatic keybed playback).
        const auto hasSound = []( MTIKeys* k ) { return k != nullptr && k->getSource( 0 ).isNotEmpty() && k->getSourceSlice( 0 ) != nullptr; };
        MTIKeys*   pNote    = pKeys->getKeysClip( Note );
        int        root     = Note;
        if( hasSound( pNote ) )
        {
            root = pNote->keyNote.get();
            if( root < 0 || root > 127 ) root = Note;
        }
        else
        {
            pNote = nullptr;
            // Fallback to the first assigned keygroup sample for chromatic keybed playback
            for( auto assignedNote : pKeys->getAssignedNotes() )
            {
                auto* candidate = pKeys->getKeysClip( assignedNote );
                if( hasSound( candidate ) )
                {
                    pNote = candidate;
                    // The sample has no root-note field of its own (keyNote is its pad slot), so a sample played from a key
                    // it is not assigned to sounds at original pitch on middle C and follows the keybed chromatically.
                    root = 60;
                    break;
                }
            }
        }

        if( pNote )
        {
            auto notePoly = pNote->polyPhony.get();
            if( auto soundSlice = pNote->getSourceSlice( 0 ) )
            {
                if( auto voice = dynamic_cast<AVoice*>( getFreeVoice( Note, notePoly ) ) )
                {
                    double pr = std::pow( 2.0, ( Note - root ) / 12.0 );
                    voice->setPitchRatio( pr );
                    voice->setSlice( soundSlice );
                    voice->noteOn( Note, Velocity );
                    addVoiceForPlayback( voice );
                }
            }
        }
    }
}
