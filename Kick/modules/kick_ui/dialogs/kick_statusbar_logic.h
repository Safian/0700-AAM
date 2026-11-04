// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) logic of the status bar: the Box Info message line (a message fades back to the standing state after a
// while, a long text is cut on a character boundary) and the tempo / time-signature texts. Host-tested in
// Kick/tests/platform/StatusbarLogicTests.cpp.
#pragma once

#include <string>

namespace kick_ui::statusbar_logic
{
    constexpr double kHoldMs  = 6000.0; // how long a message stays before the standing state shows again
    constexpr size_t kMaxChars = 64;    // the Box Info line is one line of 18 pt text

    // At most maxChars characters (UTF-8 aware: never cuts inside a multi-byte character); "..." marks a cut.
    inline std::string limitText( const std::string& text, size_t maxChars = kMaxChars )
    {
        size_t chars = 0, i = 0;
        while( i < text.size() )
        {
            if( ( (unsigned char)text[i] & 0xC0 ) != 0x80 ) // first byte of a character
            {
                if( chars == maxChars )
                    break;
                ++chars;
            }
            ++i;
        }
        if( i >= text.size() )
            return text;
        std::string out = text.substr( 0, i );
        while( !out.empty() && out.back() == ' ' )
            out.pop_back();
        return out + "...";
    }

    // One line for the messages (save results, load errors, hints) and one standing state ("Unsaved project changes", an audio
    // device fault) that comes back when the message has been shown long enough.
    class BoxInfoState
    {
    public:
        // A message; replaces the one before. nowMs: any monotonic millisecond clock.
        void setMessage( const std::string& text, double nowMs, double holdMs = kHoldMs )
        {
            message_   = limitText( text );
            messageEnd_ = nowMs + holdMs;
            hasMessage_ = !text.empty();
        }
        // The standing state (empty = nothing to report). Shown whenever there is no live message.
        void setStanding( const std::string& text ) { standing_ = limitText( text ); }
        // The text to show at nowMs.
        const std::string& textAt( double nowMs ) const
        {
            return hasMessage_ && nowMs < messageEnd_ ? message_ : standing_;
        }
        const std::string& standing() const { return standing_; }

    private:
        std::string message_, standing_;
        double      messageEnd_{ 0.0 };
        bool        hasMessage_{ false };
    };

    // Which standing text wins: an audio fault (nothing sounds), then low storage (a save may fail), then the project state
    // ("Unsaved project changes" / a save error), then low memory. One line, so one text at a time.
    inline std::string chooseStanding( const std::string& audioFault, const std::string& lowStorage, const std::string& project, const std::string& lowMemory )
    {
        if( !audioFault.empty() ) return audioFault;
        if( !lowStorage.empty() ) return lowStorage;
        if( !project.empty() ) return project;
        return lowMemory;
    }

    inline std::string tempoText( int bpm ) { return bpm > 0 ? std::to_string( bpm ) + " BPM" : std::string( "-- BPM" ); }
    inline std::string timeSignatureText( int num, int den ) { return num > 0 && den > 0 ? std::to_string( num ) + "/" + std::to_string( den ) : std::string( "--" ); }
}
