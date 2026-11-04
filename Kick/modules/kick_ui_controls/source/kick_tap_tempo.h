// SPDX-License-Identifier: AGPL-3.0-or-later
// Tap tempo estimator shared by the hardware TAP TEMPO button and the Tempo editor. Pure (no JUCE): tested in
// Kick/tests/platform/NumPadTapTempoTests.cpp.
//
// N taps give N-1 intervals: the tempo is 60000 * (N-1) / (time of the last tap - time of the first tap) over a sliding window
// of the last `numTaps` taps. A gap longer than maxGapMs starts a new series (an old first tap never counts); a tap closer than
// minGapMs to the previous one is a bounce and is ignored.
#pragma once

#include <vector>

namespace ui
{
    class TapTempo
    {
    public:
        explicit TapTempo( int numTaps = 3, double maxGapMs = 2000.0, double minGapMs = 150.0, double minBpm = 30.0, double maxBpm = 300.0 )
            : taps_( numTaps < 2 ? 2 : numTaps ), maxGap_( maxGapMs ), minGap_( minGapMs ), minBpm_( minBpm ), maxBpm_( maxBpm ) {}

        void setNumTaps( int numTaps ) { taps_ = numTaps < 2 ? 2 : numTaps; trim(); }
        void reset() { times_.clear(); }
        int  count() const { return (int)times_.size(); }

        // Registers a tap at nowMs (any monotonic millisecond clock). Returns the tempo in BPM once enough taps are in, 0 before
        // that or when the estimate is outside [minBpm, maxBpm].
        double tap( double nowMs )
        {
            if( !times_.empty() )
            {
                const double gap = nowMs - times_.back();
                if( gap > maxGap_ )
                    times_.clear();
                else if( gap < minGap_ )
                    return 0.0;
            }
            times_.push_back( nowMs );
            trim();
            if( (int)times_.size() < taps_ )
                return 0.0;
            const double span = times_.back() - times_.front();
            if( span <= 0.0 )
                return 0.0;
            const double bpm = 60000.0 * ( (double)times_.size() - 1.0 ) / span;
            return ( bpm < minBpm_ || bpm > maxBpm_ ) ? 0.0 : bpm;
        }

    private:
        void trim()
        {
            while( (int)times_.size() > taps_ )
                times_.erase( times_.begin() );
        }
        int                 taps_;
        double              maxGap_, minGap_, minBpm_, maxBpm_;
        std::vector<double> times_;
    };
}
