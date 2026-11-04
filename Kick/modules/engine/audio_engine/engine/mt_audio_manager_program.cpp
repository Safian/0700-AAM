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

namespace mt
{

    MTAudioProgramManager::MTAudioProgramManager( MTSelected& Selected, AudIO::DawEngine& Engine )
        : selected( Selected ), engine( Engine )
    {
        // Reserve default storage size.
        programs.reserve( 64 );

        // Connect callbacks...
        programNameChanged.connect( [&]( MTIProgram* ) { updateProgramList(); } );
    }

    MTAudioProgramManager::~MTAudioProgramManager()
    {
        vTree.removeListener( this );
        programs.clear();
    }

    void MTAudioProgramManager::close()
    {
        vTree.removeListener( this );
        vTree       = juce::ValueTree{ IDs::PROGRAMS };
        undoManager = nullptr;

        MTIProgram::resetSoloingPrograms();
        programs.clear();
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
        updateProgramList();
    }

    void MTAudioProgramManager::setup( juce::ValueTree VT, juce::UndoManager* UM )
    {
        vTree.removeListener( this );
        vTree       = VT;
        undoManager = UM;

        // Initialize programs....
        for( auto it : vTree )
        {
            if( it.hasType( IDs::PROGRAM ) )
            {
                createProgram( it, false );
            }
        }
        vTree.addListener( this );
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
        updateProgramList();
    }

    ////////////////////////////
    //// Selection
    ////////////////////////////
    void MTAudioProgramManager::setSelectedProgram( int num )
    {
        auto selection = AudIO::utils::limit<int>( -1, (int)programs.size() - 1, num );
        auto prev      = selected.program;
        sprog          = "--";

        if( juce::isPositiveAndBelow( selection, (int)programs.size() ) )
        {
            selected.program = selection;
            sprog            = programs[selection]->getName();
        }

        if( ( prev != selected.program ) && progSelectionChanged )
        {
            progSelectionChanged();
        }
    }

    void MTAudioProgramManager::setSelectedProgram( const juce::String& Program )
    {
        setSelectedProgram( getProgramIndexByName( Program ) );
    }

    ////////////////////////////
    //// Program
    ////////////////////////////

    juce::String MTAudioProgramManager::getDefaultProgramName( MTProgramType type )
    {
        juce::String prefix = "Program ";
        switch( type )
        {
            case MTProgramType::Drum:     prefix = "Drum "; break;
            case MTProgramType::Keygroup: prefix = "Keygroup "; break;
            case MTProgramType::Clip:     prefix = "Clip "; break;
            case MTProgramType::Midi:     prefix = "MIDI "; break;
            case MTProgramType::Cv:       prefix = "CV "; break;
            case MTProgramType::Audio:    prefix = "Audio "; break;
            case MTProgramType::Plugin:   prefix = "Plugin "; break;
            default:                      prefix = "Program "; break;
        }

        int count = 1;
        while( true )
        {
            auto candidate = prefix + juce::String( count );
            if( getProgramByName( candidate ) == nullptr )
                return candidate;
            ++count;
        }
    }

    juce::String MTAudioProgramManager::getUniqueProgramName( const juce::String& baseName )
    {
        if( getProgramByName( baseName ) == nullptr )
            return baseName;

        int count = 2;
        while( true )
        {
            auto candidate = baseName + " " + juce::String( count );
            if( getProgramByName( candidate ) == nullptr )
                return candidate;
            ++count;
        }
    }

    std::unique_ptr<MTIProgram> MTAudioProgramManager::makeProgram( juce::ValueTree& VT )
    {
        MTIProgram* prog = nullptr;
        auto        type = toEnum<MTProgramType>( (int)VT.getProperty( IDs::type ) );

        if( type == MTProgramType::Drum ) prog = new MTDrumProgram( engine, VT, undoManager );
        if( type == MTProgramType::Keygroup ) prog = new MTKeygroupProgram( engine, VT, undoManager );
        if( type == MTProgramType::Clip ) prog = new MTClipProgram( engine, VT, undoManager );
        if( type == MTProgramType::Midi ) prog = new MTMidiProgram( engine, VT, undoManager );
        if( type == MTProgramType::Cv ) prog = new MTCVProgram( engine, VT, undoManager );
        if( type == MTProgramType::Audio ) prog = new MTAudioProgram( engine, VT, undoManager );
        if( type == MTProgramType::Plugin ) prog = new MTClapProgram( engine, VT, undoManager );
        return std::unique_ptr<MTIProgram>( prog );
    }

    void MTAudioProgramManager::reconcileWithTree()
    {
        std::vector<juce::ValueTree> nodes;
        for( auto it : vTree )
            if( it.hasType( IDs::PROGRAM ) )
                nodes.push_back( it );

        const auto prevName = ( juce::isPositiveAndBelow( selected.program, (int)programs.size() ) ) ? programs[(size_t)selected.program].get() : nullptr;
        const int  prevSel  = selected.program;

        const bool changed = treesync::reconcile(
            programs, nodes,
            []( MTIProgram& P, const juce::ValueTree& N ) { return P.getState() == N; },
            [&]( const juce::ValueTree& N ) -> std::unique_ptr<MTIProgram> {
                juce::ValueTree vt = N;
                kick_log( "info", "[INFO] [PROJ] Program rebuilt from the project tree (undo/redo): '%s'\n", vt.getProperty( IDs::name, "" ).toString().toRawUTF8() );
                return makeProgram( vt );
            },
            []( MTIProgram& P ) { programBeingDeleted( &P ); } );

        if( !changed )
            return;

        // Selection: the same program if it survived, else the nearest index.
        int sel = -1;
        for( int i = 0; i < (int)programs.size(); ++i )
            if( programs[(size_t)i].get() == prevName ) sel = i;
        if( sel < 0 && !programs.empty() )
            sel = juce::jlimit<int>( 0, (int)programs.size() - 1, prevSel );
        selected.program = sel;
        sprog            = sel >= 0 ? programs[(size_t)sel]->getName() : juce::String( "--" );

        updateProgramList();
        if( progSelectionChanged )
            progSelectionChanged();
    }

    MTIProgram* MTAudioProgramManager::createProgram( juce::ValueTree& VT, bool addToTree )
    {
        MTIProgram* prog = nullptr;
        auto        type = toEnum<MTProgramType>( (int)VT.getProperty( IDs::type ) );
        const auto  progName = VT.getProperty( IDs::name, "" ).toString();
        const auto  start_ms = kick_logger::get_uptime_ms();
        kick_log( "info", "[INFO] [PROJ] Program load begin: '%s' (type: %d)\n", progName.toRawUTF8(), (int)type );

        prog = makeProgram( VT ).release();

        if( prog )
        {
            programs.emplace_back( prog );

            // Add this program to project file.
            if( addToTree ) vTree.appendChild( VT, undoManager );
            // Update list and call listener...
            updateProgramList();
        }
        else
        {
            kick_log( "error", "[ERROR] [PROJ] Unknown or unsupported program type %d for program '%s'\n",
                      (int)type, progName.toRawUTF8() );
        }
        kick_log( "info", "[INFO] [PROJ] Program load end: '%s' (took %llu ms, success=%d)\n",
                  progName.toRawUTF8(), static_cast<unsigned long long>( kick_logger::get_uptime_ms() - start_ms ),
                  prog != nullptr );
        return prog;
    }

    MTIProgram* MTAudioProgramManager::createProgram( MTProgramType Type, const juce::String& Name )
    {
        juce::String progName = Name.trim();
        if( progName.isEmpty() )
            progName = getDefaultProgramName( Type );
        else
            progName = getUniqueProgramName( progName );

        // Create new program... (one undo step)
        if( undoManager ) undoManager->beginNewTransaction( "Create program" );
        juce::ValueTree vt = createVTree( IDs::PROGRAM,
                                          IDs::name, progName,
                                          IDs::type, toInt( Type ) );

        auto prog = createProgram( vt, true );
        if( 1 == programs.size() )
            setSelectedProgram( 0 );

        return prog;
    }

    int MTAudioProgramManager::deleteProgram( int Index )
    {
        if( juce::isPositiveAndBelow( Index, programs.size() ) )
        {
            auto selectedProgram = getSelectedProgram();

            if( undoManager ) undoManager->beginNewTransaction( "Delete program" );
            auto& ptr = programs[(size_t)Index];
            programBeingDeleted( ptr.get() );
            auto vt = ptr->getState();
            programs.erase( programs.begin() + Index );
            vTree.removeChild( vt, undoManager );

            // If we are deleting currently selected program or an earlier program.
            if( selectedProgram == Index )
                setSelectedProgram( juce::jlimit( -1, (int)programs.size() - 1, selectedProgram > 0 ? selectedProgram - 1 : 0 ) );
            else if( Index < selectedProgram )
                setSelectedProgram( selectedProgram - 1 );

            // Update list and call listener...
            updateProgramList();
            return 0;
        }
        return -1;
    }

    int MTAudioProgramManager::deleteProgram( const juce::String& Name )
    {
        return deleteProgram( getProgramIndexByName( Name ) );
    }

    int MTAudioProgramManager::duplicateProgram( int program )
    {
        if( auto prog = getProgram( program ) )
        {
            if (!prog->prepareToSave()) return -1;
            if( undoManager ) undoManager->beginNewTransaction( "Duplicate program" );
            auto name = getUniqueProgramName( prog->getName() + "_copy" );
            auto vt   = prog->getState().createCopy();

            vt.setProperty( IDs::name, name, nullptr );
            vt.setProperty( IDs::id, "", nullptr ); // this will be replaced with unique id.

            createProgram( vt, true );
            setSelectedProgram( static_cast<int>(programs.size()) - 1 );
            return 0;
        }
        return -1;
    }

    void MTAudioProgramManager::valueTreeChildAdded( juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenAdded )
    {
        if( parentTree == vTree && childWhichHasBeenAdded.hasType( IDs::PROGRAM ) )
        {
            auto id = childWhichHasBeenAdded.getProperty( IDs::id ).toString();
            if( id.isNotEmpty() && getProgramById( id ) != nullptr )
                return; // Already present in programs list

            // Re-instantiate from undo/redo
            createProgram( childWhichHasBeenAdded, false );
        }
    }

    void MTAudioProgramManager::valueTreeChildRemoved( juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenRemoved, int /*index*/ )
    {
        if( parentTree == vTree && childWhichHasBeenRemoved.hasType( IDs::PROGRAM ) )
        {
            auto id = childWhichHasBeenRemoved.getProperty( IDs::id ).toString();
            for( size_t i = 0; i < programs.size(); ++i )
            {
                if( programs[i]->getID() == id )
                {
                    auto ptr = std::move( programs[i] );
                    programBeingDeleted( ptr.get() );
                    programs.erase( programs.begin() + static_cast<std::ptrdiff_t>( i ) );
                    updateProgramList();
                    if( selected.program >= (int)programs.size() )
                        setSelectedProgram( (int)programs.size() - 1 );
                    break;
                }
            }
        }
    }

    int MTAudioProgramManager::getProgramIndex( MTIProgram* program )
    {
        for( uint8_t index = 0; index < programs.size(); index++ )
        {
            if( program == programs[(size_t)index].get() )
            {
                return ( index );
            }
        }
        return ( -1 );
    }

    MTIProgram* MTAudioProgramManager::getProgram( int index )
    {
        if( juce::isPositiveAndBelow( index, programs.size() ) )
            return programs[(size_t)index].get();
        return nullptr;
    }

    MTIProgram* MTAudioProgramManager::getProgramByName( const juce::String& Name )
    {
        if( Name.isNotEmpty() )
            for( auto& it : programs )
                if( Name == it->getName() )
                    return it.get();
        return nullptr;
    }

    MTIProgram* MTAudioProgramManager::getProgramById( const juce::String& ID )
    {
        if( ID.isNotEmpty() )
        {
            for( auto& it : programs )
                if( ID == it->getID() )
                    return it.get();

            // Fallback: match by program name if ID didn't match directly
            for( auto& it : programs )
                if( ID == it->getName() )
                    return it.get();
        }
        return nullptr;
    }

    int MTAudioProgramManager::getProgramIndexByName( const juce::String& Name )
    {
        if( Name.isNotEmpty() )
            for( int index = 0; index < (int)programs.size(); ++index )
                if( Name == programs[(size_t)index]->getName() )
                    return ( index );
        return -1;
    }

    int MTAudioProgramManager::getProgramIndexById( const juce::String& ID )
    {
        if( ID.isNotEmpty() )
            for( int index = 0; index < (int)programs.size(); ++index )
                if( ID == programs[(size_t)index]->getID() )
                    return ( index );
        return -1;
    }

    juce::String MTAudioProgramManager::getProgramName( const int Index )
    {
        if( auto program = getProgram( Index ) )
            return program->getName();
        return "";
    }

    void MTAudioProgramManager::forAllPrograms( std::function<void( MTIProgram* P )> func )
    {
        if( func )
        {
            for( const auto& it : programs )
            {
                func( it.get() );
            }
        }
    }

    void MTAudioProgramManager::updateProgramList() noexcept
    {
        programList.clear();

        // Update program name list..
        forAllPrograms( [&]( MTIProgram* P ) {
            P->setIndex( (int)programList.size() );
            programList.push_back( P->getName() );
        } );

        // Call listeners...
        if( progListChanged ) progListChanged();
    }

    int MTAudioProgramManager::setLevelForSelectedProgram( int Index, int Level )
    {
        if( auto prog = getProgram( selected.program ) )
        {
            if( 0 > Index )
                prog->setLevel( Level );
            else
            {
                auto gain = CommonUtils::midiToGain( Level );
                prog->setNoteGain( Index, gain );
            }
        }
        return 0;
    }

    int MTAudioProgramManager::setMuteForSelectedProgram( int Index, bool Mute )
    {
        if( auto prog = getProgram( selected.program ) )
        {
            if( 0 > Index )
                prog->setMute( Mute );
            else
                prog->setNoteMute( Index, Mute );
        }
        return 0;
    }

    int MTAudioProgramManager::setPanForSelectedProgram( int Index, double Pan )
    {
        if( auto prog = getProgram( selected.program ) )
        {
            if( 0 > Index )
                prog->setPan( (float)Pan );
            else
                prog->setNotePan( Index, Pan );
        }
        return 0;
    }

    int MTAudioProgramManager::setSemiForSelectedProgram( int Index, int Semi )
    {
        if( auto prog = getProgram( selected.program ) )
        {
            if( 0 > Index )
                prog->setSemi( Semi );
            else
                prog->setNoteSemi( Index, Semi );
        }
        return 0;
    }

    int MTAudioProgramManager::setFineForSelectedProgram( int Index, int Fine )
    {
        if( auto prog = getProgram( selected.program ) )
        {
            if( 0 > Index )
                prog->setSemi( Fine );
            else
                prog->setNoteFine( Index, Fine );
        }
        return 0;
    }

    int MTAudioProgramManager::setPitchForSelectedProgram( int Index, int Semi, int Fine )
    {
        if( auto prog = getProgram( selected.program ) )
        {
            if( 0 > Index )
                prog->setPitch( Semi, Fine );
            else
                prog->setNotePitch( Index, Semi, Fine );
        }
        return 0;
    }

    void MTAudioProgramManager::processMidiMeters()
    {
        forAllPrograms( []( MTIProgram* P ) {
            P->processMidiMeter();
        } );
    }

}
