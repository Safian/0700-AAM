// SPDX-License-Identifier: AGPL-3.0-or-later
// Text entry state of the numeric pad (digits, decimal point, sign, backspace, clear). Pure (no JUCE): tested in
// Kick/tests/platform/NumPadTapTempoTests.cpp.
#pragma once

#include <cstdlib>
#include <string>

namespace ui
{
    class NumPadEntry
    {
    public:
        static constexpr size_t kMaxDigits = 9;

        void clear()
        {
            buf_.clear();
            neg_ = false;
        }

        // True while something is typed (or a value is loaded).
        bool isEmpty() const { return buf_.empty() && !neg_; }

        // Appends a digit 0..9; ignored once kMaxDigits digits are in. No leading zeros ("05" -> "5").
        void digit( int d )
        {
            if( d < 0 || d > 9 )
                return;
            if( digitCount() >= kMaxDigits )
                return;
            if( buf_ == "0" )
                buf_.clear();
            buf_ += (char)( '0' + d );
        }

        // Decimal point; only when the pad edits a decimal value and none is there yet.
        void dot( bool allowDecimal )
        {
            if( !allowDecimal || buf_.find( '.' ) != std::string::npos )
                return;
            if( buf_.empty() )
                buf_ = "0";
            buf_ += '.';
        }

        // Removes the last typed character; the sign goes with the last digit.
        void backspace()
        {
            if( !buf_.empty() )
                buf_.pop_back();
            if( buf_.empty() )
                neg_ = false;
        }

        void toggleSign() { neg_ = !neg_; }

        // Loads a value (inc/dec, x2, /2, slider, default): up to 3 decimals, trailing zeros trimmed.
        void setValue( double v )
        {
            neg_ = v < 0.0;
            if( neg_ )
                v = -v;
            char tmp[48];
            std::snprintf( tmp, sizeof( tmp ), "%.3f", v );
            buf_ = tmp;
            while( !buf_.empty() && buf_.back() == '0' && buf_.find( '.' ) != std::string::npos )
                buf_.pop_back();
            if( !buf_.empty() && buf_.back() == '.' )
                buf_.pop_back();
            if( buf_.empty() )
                buf_ = "0";
            if( buf_ == "0" )
                neg_ = false;
        }

        double value() const
        {
            const double v = buf_.empty() ? 0.0 : std::atof( buf_.c_str() );
            return neg_ ? -v : v;
        }

        // What the display shows: "-", "-12.", "7.25" ...
        std::string text() const { return ( neg_ ? "-" : "" ) + buf_; }

    private:
        size_t digitCount() const
        {
            size_t n = 0;
            for( char c : buf_ )
                if( c >= '0' && c <= '9' )
                    ++n;
            return n;
        }
        std::string buf_;
        bool        neg_{ false };
    };
}
