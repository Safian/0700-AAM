// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) logic of the mode menu: pad -> tile, dial stepping, tile <-> screen table, project gate and the
// screen back-stack with the MENU toggle. Host-tested in Kick/tests/platform/MenuLogicTests.cpp.
// The Screen enum is a template parameter so the test can use a stand-in with the same enumerator names.
#pragma once

#include <algorithm>
#include <cstdlib>
#include <stack>
#include <string>
#include <vector>

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

    constexpr int kTileCount = 17; // tiles that open a screen
    // Text-only command tiles after them (no icon in the theme).
    constexpr int kCommandSave       = 17;
    constexpr int kCommandResources  = 18;
    constexpr int kCommandKickUpdate = 19;
    constexpr int kCommandFavorites  = 20; // the browser, on its next assigned favourite (the SOUNDS button's second press does the same)
    constexpr int kCommandPower      = 21; // the Power window (Shut Down / Restart); works with and without a project
    constexpr int kMenuItemCount     = 22;

    // The mode registry: ONE table of every menu item (name = the id of the button, icon = the theme image name or "" for a
    // text tile, label = the readable name shown in the status bar while the dial moves over it, screen = what it opens).
    // The Menu is built from it and the tests check it against the screen enum and the theme assets. Command tiles
    // (Save / Resources / Kick Update) have no screen. Indexes are the stable item ids; the order the user sees is MenuLayout.
    template<class S>
    struct TileDef
    {
        const char* name;
        const char* icon;
        const char* label;
        S           screen;
    };

    template<class S>
    inline const TileDef<S>& tileDef( int item )
    {
        static const TileDef<S> table[kMenuItemCount] = {
            { "Browser", "Browser", "Browser", S::MenuFileBrowser },
            { "Track Mixer", "Track Mixer", "Track Mixer", S::MenuChannelMixer },
            { "Track Mutes", "Track Mutes", "Track Mutes", S::MenuTrackMute },
            { "Sequences", "Sequences", "Next Sequence", S::MenuNextSequence },
            { "listedit", "listedit", "List Edit", S::MenuListEdit },
            { "Padmixer", "Padmixer", "Pad Mixer", S::MenuPadMixer },
            { "Padmutes", "Padmutes", "Pad Mutes", S::MenuPadMute },
            { "songs", "songs", "Song", S::MenuSong },
            { "Coloreditor", "Coloreditor", "Color Editor", S::MenuColorEdit },
            { "Sampler", "Sampler", "Sampler", S::MenuSampler },
            { "Sample editor", "Sample editor", "Sample Edit", S::MenuSampleEdit },
            { "Program edit", "Program edit", "Program Edit", S::MenuProgramEdit },
            { "Padnotes", "Padnotes", "Pad Perform", S::MenuPadPerform },
            { "Looper", "Looper", "Looper", S::MenuLooper },
            { "Qlinks", "Qlinks", "Q-Link Edit", S::MenuKnobEdit },
            { "Midicontrol", "Midicontrol", "MIDI Control", S::MenuMidiControl },
            { "Keyboard", "Keyboard", "Keyboard", S::MenuPiano },
            { "Save", "", "Save the project", S::Invalid },
            { "Resources", "", "System Resources", S::Invalid },
            { "Kick Update", "", "Kick Update", S::Invalid },
            { "Favorites", "", "Favorites (Browser)", S::Invalid },
            { "Power", "", "Power (Shut Down / Restart)", S::Invalid },
        };
        static const TileDef<S> none{ "", "", "", S::Invalid };
        return item >= 0 && item < kMenuItemCount ? table[item] : none;
    }

    // Tile index -> screen (Invalid for a command tile or an unknown index).
    template<class S>
    inline S tileScreen( int tile )
    {
        return tile >= 0 && tile < kTileCount ? tileDef<S>( tile ).screen : S::Invalid;
    }

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
            case S::MenuFileBrowser:
            case S::SWUpdateView:
            case S::SWUpdateWindow:
            case S::KickUpdate: return false;
            default: return true;
        }
    }

    // A menu item (tile or command) that needs a loaded project: the screens that do, and Save.
    template<class S>
    inline bool itemNeedsProject( int item )
    {
        if( item == kCommandSave )
            return true;
        const S screen = tileScreen<S>( item );
        return screen != S::Invalid && screenNeedsProject( screen );
    }

    // The order and the pages of the Menu. Items keep their ids (the table above); the layout is a permutation of them,
    // 16 per page (the 16 pads of one page select its tiles). It is stored as "3,0,1,..." and is read back leniently: unknown or
    // repeated ids are dropped and missing ones appended in the default order, so an old or damaged file never loses an item.
    class MenuLayout
    {
    public:
        static constexpr int kPageSize = 16;

        explicit MenuLayout( int itemCount = kMenuItemCount ) : count_( itemCount < 0 ? 0 : itemCount ) { reset(); }

        int itemCount() const { return count_; }
        int pageCount() const { return count_ <= 0 ? 1 : ( count_ + kPageSize - 1 ) / kPageSize; }
        // Number of items on a page (0 for a page that does not exist).
        int pageItems( int page ) const
        {
            if( page < 0 || page >= pageCount() )
                return 0;
            const int left = count_ - page * kPageSize;
            return left < kPageSize ? left : kPageSize;
        }
        static int pageOfPosition( int pos ) { return pos < 0 ? 0 : pos / kPageSize; }
        static int slotOfPosition( int pos ) { return pos < 0 ? 0 : pos % kPageSize; }
        // The item shown at a position (page * 16 + slot); -1 outside.
        int itemAt( int pos ) const { return pos >= 0 && pos < count_ ? order_[(size_t)pos] : -1; }
        int positionOf( int item ) const
        {
            for( int i = 0; i < count_; ++i )
                if( order_[(size_t)i] == item )
                    return i;
            return -1;
        }
        // Moves the item at `from` to `to`; the items in between shift by one (the dial moves a picked-up tile like this).
        bool move( int from, int to )
        {
            if( from < 0 || to < 0 || from >= count_ || to >= count_ )
                return false;
            if( from == to )
                return true;
            const int item = order_[(size_t)from];
            order_.erase( order_.begin() + from );
            order_.insert( order_.begin() + to, item );
            return true;
        }
        // Exchanges two positions (a touch picks one tile and drops it on another).
        bool swapPositions( int a, int b )
        {
            if( a < 0 || b < 0 || a >= count_ || b >= count_ )
                return false;
            std::swap( order_[(size_t)a], order_[(size_t)b] );
            return true;
        }
        void reset()
        {
            order_.resize( (size_t)count_ );
            for( int i = 0; i < count_; ++i )
                order_[(size_t)i] = i;
        }
        bool isDefault() const
        {
            for( int i = 0; i < count_; ++i )
                if( order_[(size_t)i] != i )
                    return false;
            return true;
        }
        std::string serialize() const
        {
            std::string out;
            for( int i = 0; i < count_; ++i )
                out += ( i ? "," : "" ) + std::to_string( order_[(size_t)i] );
            return out;
        }
        // Returns true when the text held at least one valid id. The layout is always a complete permutation afterwards.
        bool deserialize( const std::string& text )
        {
            std::vector<int>  parsed;
            std::vector<bool> used( (size_t)count_, false );
            bool              any = false;
            size_t            pos = 0;
            while( pos <= text.size() )
            {
                auto comma = text.find( ',', pos );
                if( comma == std::string::npos )
                    comma = text.size();
                const std::string tok = text.substr( pos, comma - pos );
                pos                   = comma + 1;
                char* end             = nullptr;
                const long v          = std::strtol( tok.c_str(), &end, 10 );
                if( tok.empty() || end == tok.c_str() || *end != '\0' || v < 0 || v >= count_ || used[(size_t)v] )
                    continue;
                used[(size_t)v] = true;
                parsed.push_back( (int)v );
                any = true;
            }
            for( int i = 0; i < count_; ++i )
                if( !used[(size_t)i] )
                    parsed.push_back( i );
            order_ = parsed;
            return any;
        }

    private:
        int              count_;
        std::vector<int> order_;
    };

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
