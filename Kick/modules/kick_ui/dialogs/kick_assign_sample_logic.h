// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) logic of the Sample Assign screen: pad/bank index maths, pool-index to asset mapping,
// Keygroup zone coverage, assign / clear guards, layer indicators, drop math.
// Host-tested in Kick/tests/platform/AssignSampleLogicTests.cpp.
#pragma once

#include <string>
#include <vector>

namespace kick_ui::assign_logic
{
    constexpr int kPadsInBank = 16;

    // Pad index inside the visible bank (0..15), or -1 when the absolute pad is outside the bank / invalid.
    inline int hwPadIndex( int absolutePad, int bankOffset, int padsInBank = kPadsInBank )
    {
        if( absolutePad < 0 )
            return -1;
        const int hw = absolutePad - bankOffset;
        return ( hw >= 0 && hw < padsInBank ) ? hw : -1;
    }

    // The list shows "Clear Assignment" at index 0, the pool samples follow. Returns the pool index (>= 0) or -1 for
    // "Clear Assignment" / out of range.
    inline int poolIndexFromListIndex( int listIndex, int poolSize )
    {
        const int pool = listIndex - 1;
        return ( pool >= 0 && pool < poolSize ) ? pool : -1;
    }

    // Program types that can hold pad samples (Drum, Clip, Keygroup).
    inline bool programTypeAcceptsSamples( bool isDrum, bool isClip, bool isKeygroup ) { return isDrum || isClip || isKeygroup; }

    // Does a sample zone [minNote, maxNote] (root keyNote) cover the absolute pad / note?
    // Drum / Clip samples have minNote == maxNote and sit on keyNote only.
    inline bool zoneCoversNote( int minNote, int maxNote, int keyNote, int note )
    {
        if( minNote == maxNote )
            return keyNote == note;
        const int lo = minNote < maxNote ? minNote : maxNote;
        const int hi = minNote < maxNote ? maxNote : minNote;
        return note >= lo && note <= hi;
    }

    // First / last absolute note a zone has to label (clamped to [0,127]); empty range when first > last.
    struct NoteRange
    {
        int first;
        int last;
        bool empty() const { return first > last; }
    };
    inline NoteRange zoneNotes( int minNote, int maxNote, int keyNote )
    {
        if( minNote == maxNote )
            return { keyNote, keyNote };
        int lo = minNote < maxNote ? minNote : maxNote;
        int hi = minNote < maxNote ? maxNote : minNote;
        if( lo < 0 )
            lo = 0;
        if( hi > 127 )
            hi = 127;
        return { lo, hi };
    }

    struct ZoneInfo
    {
        int  minNote;
        int  maxNote;
        int  keyNote;
        bool hasSource;
    };

    // Is the pad (index inside the bank) assigned? Works for every program type.
    inline bool padAssigned( const std::vector<ZoneInfo>& zones, int bankOffset, int hwPad )
    {
        const int note = bankOffset + hwPad;
        for( const auto& z : zones )
            if( z.hasSource && zoneCoversNote( z.minNote, z.maxNote, z.keyNote, note ) )
                return true;
        return false;
    }

    // Debounce: only the selection that is still current when the delay elapses is auditioned.
    struct AuditionDebounce
    {
        unsigned generation{ 0 };
        unsigned request() { return ++generation; }
        bool     isCurrent( unsigned ticket ) const { return ticket == generation; }
        void     cancel() { ++generation; }
    };

    // AS-10: Formats layer assignment indicator for a pad button row (e.g. "[1 - - -]" or "[1 2 3 4]").
    inline std::string formatLayerIndicator( bool l1, bool l2, bool l3, bool l4 )
    {
        if( !l1 && !l2 && !l3 && !l4 )
            return "";
        std::string s = "[";
        s += ( l1 ? "1 " : "- " );
        s += ( l2 ? "2 " : "- " );
        s += ( l3 ? "3 " : "- " );
        s += ( l4 ? "4]" : "-]" );
        return s;
    }

    // AS-11: Check if a sample path or filename is referenced by any of the program's sample sources.
    inline bool isSampleUsed( const std::string& targetSample, const std::vector<std::string>& assignedSources )
    {
        if( targetSample.empty() )
            return false;
        for( const auto& src : assignedSources )
        {
            if( src.empty() )
                continue;
            if( src == targetSample )
                return true;
            // Also check filename equality if full paths differ
            const auto slashA = targetSample.find_last_of( "/\\" );
            const auto nameA = ( slashA == std::string::npos ) ? targetSample : targetSample.substr( slashA + 1 );
            const auto slashB = src.find_last_of( "/\\" );
            const auto nameB = ( slashB == std::string::npos ) ? src : src.substr( slashB + 1 );
            if( nameA == nameB )
                return true;
        }
        return false;
    }

    // AS-13: Compute pad index from drag/drop coordinate relative to a 4x4 pad grid.
    // Pads are laid out from bottom to top (Akai standard):
    // Row 3 (top):    12, 13, 14, 15
    // Row 2:           8,  9, 10, 11
    // Row 1:           4,  5,  6,  7
    // Row 0 (bottom):  0,  1,  2,  3
    inline int padIndexFromGridPoint( int px, int py, int gridX, int gridY, int gridW, int gridH, int bankOffset )
    {
        if( px < gridX || px >= gridX + gridW || py < gridY || py >= gridY + gridH )
            return -1;
        const int col = ( ( px - gridX ) * 4 ) / gridW;
        const int rawRow = ( ( py - gridY ) * 4 ) / gridH; // 0 is top
        const int row = 3 - rawRow; // 0 is bottom
        if( col < 0 || col > 3 || row < 0 || row > 3 )
            return -1;
        const int pad = row * 4 + col;
        return bankOffset + pad;
    }
}
