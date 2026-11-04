// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the Sampler pure logic (slice maths without truncation, time-ordered pad slices, file names).

#include "../../modules/kick_ui/audio_components/sampler/kick_sampler_logic.h"
#include <cstdio>
#include <cstdlib>
#include <set>

static void check( bool ok, const char* why )
{
    if( !ok )
    {
        std::fprintf( stderr, "FAIL: %s\n", why );
        std::exit( 1 );
    }
}

using namespace kick_ui::sampler_logic;

int main()
{
    // seconds -> frames: real rate, rounded, not truncated to whole seconds
    check( secondsToFrame( 1.5, 48000 ) == 72000, "1.5 s @ 48 kHz" );
    check( secondsToFrame( 1.5, 44100 ) == 66150, "1.5 s @ 44.1 kHz (old code gave 44100)" );
    check( secondsToFrame( 0.0, 48000 ) == 0 && secondsToFrame( -1.0, 48000 ) == 0, "zero / negative" );
    check( secondsToFrame( 100.0, 48000, 1000 ) == 1000, "clamped to total frames" );
    check( secondsToFrame( 1.0, 0.0 ) == 0, "no rate" );
    check( secondsToFrame( 0.4999 / 48000.0, 48000 ) == 0 && secondsToFrame( 0.5001 / 48000.0, 48000 ) == 1, "rounding" );

    // Slice mode
    auto s = sliceModeSlices( { 0.0, 1.5, 3.25 }, 5.0, 48000, 240000 );
    check( s.size() == 3, "3 slices" );
    check( s[0].start == 0 && s[0].end == 72000, "slice 0" );
    check( s[1].start == 72000 && s[1].end == 156000, "slice 1 keeps the fractional second (no truncation)" );
    check( s[2].start == 156000 && s[2].end == 240000, "slice 2 ends at the recording end" );
    check( s[0].pad == 0 && s[2].pad == 2, "slice i -> pad i" );
    check( sliceModeSlices( { 0.0 }, 2.0, 48000, 96000 ).size() == 1, "single marker = one slice" );
    check( sliceModeSlices( { 2.0, 2.0 }, 2.0, 48000, 96000 ).empty(), "empty slices dropped" );
    check( sliceModeSlices( {}, 2.0, 48000, 96000 ).empty(), "no markers" );
    auto unsorted = sliceModeSlices( { 3.0, 0.0, 1.0 }, 4.0, 1000, 4000 );
    check( unsorted.size() == 3 && unsorted[0].start == 0 && unsorted[1].start == 1000 && unsorted[2].start == 3000, "markers sorted" );

    // Pad slices: time order, missing ends completed
    std::map<int, double> starts{ { 5, 2.0 }, { 1, 0.0 }, { 9, 4.0 } };
    std::map<int, double> ends{ { 1, 1.5 } }; // pad 5 and 9 have no end point
    auto p = padSlices( starts, ends, 6.0, 1000, 6000 );
    check( p.size() == 3, "3 pad slices" );
    check( p[0].pad == 1 && p[0].start == 0 && p[0].end == 1500, "pad 1 explicit end" );
    check( p[1].pad == 5 && p[1].start == 2000 && p[1].end == 4000, "pad 5 ends at next start (no 0 end point)" );
    check( p[2].pad == 9 && p[2].start == 4000 && p[2].end == 6000, "pad 9 ends at the recording end" );
    std::map<int, double> bad{ { 3, 1.0 } };
    std::map<int, double> badEnd{ { 3, 0.5 } }; // end before start is ignored
    auto q = padSlices( bad, badEnd, 2.0, 1000, 2000 );
    check( q.size() == 1 && q[0].end == 2000, "end before start falls back" );
    check( padSlices( { { 1, 5.0 } }, {}, 5.0, 1000, 5000 ).empty(), "start at the very end = empty slice dropped" );

    // names
    std::set<std::string> taken{ "New Sample.wav", "New Sample 1.wav" };
    auto ex = [&]( const std::string& n ) { return taken.count( n ) > 0; };
    check( uniqueFileName( "New Sample", ".wav", ex ) == "New Sample 2.wav", "collision counter" );
    check( uniqueFileName( "Other", ".wav", ex ) == "Other.wav", "no collision" );
    check( uniqueFileName( "", ".wav", ex ) == "New Sample 2.wav", "empty name" );
    check( uniqueFileName( "a/b\\c", ".wav", ex ) == "a_b_c.wav", "separators replaced" );
    check( uniqueFileName( " .. ", ".wav", ex ) == "New Sample 2.wav", "dots and spaces only" );
    check( uniqueFileName( "../../etc/x", ".wav", ex ).find( '/' ) == std::string::npos, "no path traversal" );

    check( clampRecordSeconds( 1200 ) == 600.0 && clampRecordSeconds( 0.0 ) == 1.0 && clampRecordSeconds( 30 ) == 30.0, "max length" );

    // SA-08 RAM budget calculation (48 kHz, 2 ch, 4 bytes/float): 600 s = 230.4 MB, 20 s = 7.68 MB
    const double mb600 = estimateRamBudgetMB( 600.0, 48000.0, 2 );
    check( std::fabs( mb600 - 219.7265625 ) < 0.1 || std::fabs( mb600 - 230.4 ) < 15.0, "600 s RAM budget ~220-230 MB" );
    check( estimateRamBudgetMB( 0.0 ) == 0.0, "0 s RAM budget is 0" );
    check( formatRamBudget( 20.0 ).find( "RAM:" ) != std::string::npos, "format RAM budget string" );

    // SA-06 Device input status
    check( formatInputStatus( false, false, 0, 48000 ) == "NO INPUT", "no device" );
    check( formatInputStatus( true, false, 2, 48000 ) == "NO INPUT", "device not open" );
    check( formatInputStatus( true, true, 0, 48000 ) == "NO INPUT", "0 active inputs" );
    check( formatInputStatus( true, true, 2, 48000 ) == "48.0 kHz 2 In", "healthy 2-ch input" );
    check( formatInputStatus( true, true, 2, 48000, "Device busy" ) == "ERROR: Device busy", "error reported" );

    // SA-16 Marker removal
    std::vector<double> markers{ 0.0, 1.5, 3.0 };
    check( removeLastMarker( markers ), "marker removed" );
    check( markers.size() == 2 && markers.back() == 1.5, "last marker removed" );
    check( removeLastMarker( markers ), "second removed" );
    check( removeLastMarker( markers ), "first removed" );
    check( !removeLastMarker( markers ), "empty returns false" );

    std::puts( "SamplerLogicTests OK" );
    return 0;
}
