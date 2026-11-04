// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) logic of the mode menu: pad -> tile, dial stepping, tile <-> screen table, project gate and the
// screen back-stack with the MENU toggle. Host-tested in Kick/tests/platform/MenuLogicTests.cpp.
// The Screen enum is a template parameter so the test can use a stand-in with the same enumerator names.
#pragma once

#include <stack>

namespace ui::menu_logic
{
    constexpr int kPadCount = 16;

    // The MPC reports pad index 0 = Pad 1 = bottom left, then left to right, bottom to top (4x4). The tiles are numbered
    // left to right, top to bottom, so pad row r (from the bottom) and column c select tile (3 - r) * 4 + c.
    // Returns -1 when the pad does not map to an existing tile.
    inline int padToTile( int pad, int tileCount )
    {
        if( pad < 0 || pad >= kPadCount )
            return -1;
        const int tile = ( 3 - pad / 4 ) * 4 + pad % 4;
        return tile < tileCount ? tile : -1;
    }

    // One dial detent moves one tile, Shift + dial one row. Wraps around the ends. delta may be several detents.
    inline int stepTile( int current, int delta, bool shift, int tileCount, int columns )
    {
        if( tileCount <= 0 )
            return 0;
        const int step = delta * ( shift ? ( columns > 0 ? columns : 1 ) : 1 );
        int       next = ( current + step ) % tileCount;
        if( next < 0 )
            next += tileCount;
        return next;
    }

    // Tile index -> screen. Same order as the tiles added in AppMenuOptions. Invalid for an unknown index.
    template<class S>
    inline S tileScreen( int tile )
    {
        switch( tile )
        {
            case 0: return S::MenuFileBrowser;
            case 1: return S::MenuChannelMixer;
            case 2: return S::MenuTrackMute;
            case 3: return S::MenuNextSequence;
            case 4: return S::MenuListEdit;
            case 5: return S::MenuPadMixer;
            case 6: return S::MenuPadMute;
            case 7: return S::MenuSong;
            case 8: return S::MenuColorEdit;
            case 9: return S::MenuSampler;
            case 10: return S::MenuSampleEdit;
            case 11: return S::MenuProgramEdit;
            case 12: return S::MenuPadPerform;
            case 13: return S::MenuLooper;
            case 14: return S::MenuKnobEdit;
            case 15: return S::MenuMidiControl;
            case 16: return S::MenuPiano;
            default: return S::Invalid;
        }
    }
    constexpr int kTileCount = 17; // tiles that open a screen
    // Text-only command tiles after them (no icon in the theme).
    constexpr int kCommandSave       = 17;
    constexpr int kCommandResources  = 18;
    constexpr int kCommandKickUpdate = 19;
    constexpr int kMenuItemCount     = 20;

    // Reverse lookup: the tile of a screen, -1 when the screen has no tile.
    template<class S>
    inline int screenTile( S screen )
    {
        for( int i = 0; i < kTileCount; ++i )
            if( tileScreen<S>( i ) == screen )
                return i;
        return -1;
    }

    // Screens that work without a loaded project. Everything else (editors, mixers, sampler, grid, ...) assumes one.
    template<class S>
    inline bool screenNeedsProject( S screen )
    {
        switch( screen )
        {
            case S::Invalid:
            case S::RecentProject:
            case S::HomeOptions:
            case S::HomePreferencesView:
            case S::HomePreferences:
            case S::MenuFileBrowser:
            case S::SWUpdateView:
            case S::SWUpdateWindow:
            case S::KickUpdate:
            case S::Temp: return false;
            default: return true;
        }
    }

    // Back-stack of the screen switcher. open() = a screen opens another one (Back returns here), close() = a screen switch
    // by a hardware/top-bar button (replaces the current screen) or a Back (target Invalid).
    template<class S>
    class ScreenNav
    {
    public:
        // The screen a Back would return to; HomeListView when the stack is empty.
        S previous() const { return stack_.empty() ? S::HomeListView : stack_.top(); }
        S lastNonMenu() const { return lastNonMenu_; }

        // Called when `leaving` is replaced by another screen.
        void noteLeaving( S leaving )
        {
            if( leaving != S::HomeOptions && leaving != S::Invalid )
                lastNonMenu_ = leaving;
        }

        // A screen opens another one from `current`. The Menu is pushed (menu tile -> screen -> Back = Menu); only the main
        // view and the not-yet-shown state have nothing to return to.
        void open( S current )
        {
            if( current != S::HomeListView && current != S::Invalid )
                stack_.push( current );
        }

        // Returns the screen to load.
        S close( S current, S target )
        {
            S loadScreen = target;
            while( !stack_.empty() && ( stack_.top() == current || stack_.top() == S::Invalid ) )
                stack_.pop();
            if( target == S::HomeOptions && current == S::HomeOptions )
            {
                // MENU pressed on the Menu itself: back to the mode it was opened from.
                return lastNonMenu_ != S::Invalid ? lastNonMenu_ : S::HomeListView;
            }
            if( !stack_.empty() )
            {
                if( target == S::Invalid )
                {
                    loadScreen = stack_.top();
                    stack_.pop();
                }
            }
            else if( loadScreen == S::Invalid )
            {
                loadScreen = S::HomeListView;
            }
            return loadScreen;
        }

    private:
        std::stack<S> stack_;
        S             lastNonMenu_{ S::Invalid };
    };
}
