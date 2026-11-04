// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) decisions of the file browser: what the result of adding a sample means, the held pad table (pad release),
// the load target pad text, status texts for listings. Host-tested: Kick/tests/platform/FbLogicTests.cpp.
#pragma once

#include <array>
#include <string>
#include <vector>

namespace kick_fblogic
{
    // The engine's addAsset() answer (mt::MTAudioAssetManager): AssetAlreadyExists = 0, AssetAdded = 1, AssetRemoved = 2,
    // AssetInvalid = 3. The old code read 0 as the only failure and called every other value a success.
    enum class AssetStatus { Added, AlreadyExists, Removed, Invalid, NoProject, NoProgram };

    inline AssetStatus mapAssetResult( int engineCode, bool projectLoaded, int programIndex )
    {
        if( !projectLoaded )
            return AssetStatus::NoProject;
        switch( engineCode )
        {
            case 0: return programIndex < 0 ? AssetStatus::NoProgram : AssetStatus::AlreadyExists;
            case 1: return programIndex < 0 ? AssetStatus::NoProgram : AssetStatus::Added;
            case 2: return AssetStatus::Removed;
            default: return AssetStatus::Invalid;
        }
    }

    // A sample that is in the pool (new or already there) goes onto the pad; nothing else does.
    inline bool assignsToPad( AssetStatus s ) { return s == AssetStatus::Added || s == AssetStatus::AlreadyExists; }

    inline std::string describe( AssetStatus s, const std::string& name, const std::string& pad )
    {
        switch( s )
        {
            case AssetStatus::Added: return name + " loaded on " + pad;
            case AssetStatus::AlreadyExists: return name + " was already in the pool: assigned to " + pad;
            case AssetStatus::Removed: return name + ": nothing was loaded";
            case AssetStatus::Invalid: return name + " cannot be read (empty or not a supported audio file)";
            case AssetStatus::NoProject: return "Open a project first";
            case AssetStatus::NoProgram: return "The project has no program: create one in Sample Assign first";
        }
        return {};
    }

    // "A03" for pad 2 of bank A; the pad number is 0-based within the bank, bankOffset = 16 * bank.
    inline std::string padText( int pad, int bankOffset )
    {
        if( pad < 0 || pad > 15 )
            return "A01";
        const int  idx  = ( bankOffset / 16 ) % 8;
        const char bank = static_cast<char>( 'A' + idx );
        const int  n    = pad + 1;
        return std::string( 1, bank ) + ( n < 10 ? "0" : "" ) + std::to_string( n );
    }

    // First pad of the bank without a sample; 0 when all are taken. `assigned[i]` = pad i has a sample.
    inline int firstFreePad( const std::array<bool, 16>& assigned )
    {
        for( int i = 0; i < 16; ++i )
            if( !assigned[static_cast<size_t>( i )] )
                return i;
        return 0;
    }

    // Pads held down by the finger: the note that was started is the note that is ended, even when the bank changed in between
    // (the released pad of bank B must not leave a note of bank A ringing).
    class HeldPads
    {
    public:
        // press: remembers the note; returns it. A repeated press of an already held pad returns the held note.
        int press( int pad, int note )
        {
            if( pad < 0 || pad > 15 )
                return note;
            if( note_[static_cast<size_t>( pad )] >= 0 )
                return note_[static_cast<size_t>( pad )];
            note_[static_cast<size_t>( pad )] = note;
            return note;
        }
        // release: the note to end, or -1 when this pad was not held.
        int release( int pad )
        {
            if( pad < 0 || pad > 15 )
                return -1;
            const int n                       = note_[static_cast<size_t>( pad )];
            note_[static_cast<size_t>( pad )] = -1;
            return n;
        }
        // Everything still held (screen change): the notes to end.
        std::vector<int> releaseAll()
        {
            std::vector<int> out;
            for( auto& n : note_ )
                if( n >= 0 )
                {
                    out.push_back( n );
                    n = -1;
                }
            return out;
        }

    private:
        std::array<int, 16> note_{ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 } };
    };

    // What a hardware pad message does in the browser.
    enum class PadAction { None, NoteOn, NoteOff };

    inline PadAction padAction( bool isPadMessage, bool pressed )
    {
        if( !isPadMessage )
            return PadAction::None;
        return pressed ? PadAction::NoteOn : PadAction::NoteOff;
    }

    inline std::string listingStatus( bool readable, size_t entries, bool searching, bool truncated )
    {
        if( !readable )
            return "This folder cannot be read";
        if( entries == 0 )
            return searching ? "Nothing found" : "Empty folder";
        if( truncated )
            return "Showing the first " + std::to_string( entries ) + " matches";
        return {};
    }
}
