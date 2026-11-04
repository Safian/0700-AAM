// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the Sample Assign pure logic (bank/pad index maths, pool mapping, Keygroup zones, audition debounce).

#include "../../modules/kick_ui/dialogs/kick_assign_sample_logic.h"
#include <cstdio>
#include <cstdlib>

static void check( bool ok, const char* why )
{
    if( !ok )
    {
        std::fprintf( stderr, "FAIL: %s\n", why );
        std::exit( 1 );
    }
}

using namespace kick_ui::assign_logic;

int main()
{
    // pad / bank maths
    check( hwPadIndex( 0, 0 ) == 0, "pad 0 bank A" );
    check( hwPadIndex( 15, 0 ) == 15, "pad 15 bank A" );
    check( hwPadIndex( 16, 0 ) == -1, "pad 16 outside bank A" );
    check( hwPadIndex( 16, 16 ) == 0, "pad 16 bank B" );
    check( hwPadIndex( 31, 16 ) == 15, "pad 31 bank B" );
    check( hwPadIndex( 15, 16 ) == -1, "pad 15 outside bank B" );
    check( hwPadIndex( -1, 0 ) == -1, "no highlighted pad (-1) is invalid, also in bank A" );
    check( hwPadIndex( -1, 16 ) == -1, "no highlighted pad in bank B" );
    check( hwPadIndex( 127, 112 ) == 15, "last pad of bank H" );

    // pool mapping ("Clear Assignment" is list index 0)
    check( poolIndexFromListIndex( 0, 5 ) == -1, "list 0 = Clear Assignment" );
    check( poolIndexFromListIndex( 1, 5 ) == 0, "list 1 = pool 0" );
    check( poolIndexFromListIndex( 5, 5 ) == 4, "list 5 = pool 4" );
    check( poolIndexFromListIndex( 6, 5 ) == -1, "list beyond pool" );
    check( poolIndexFromListIndex( 1, 0 ) == -1, "empty pool" );
    check( poolIndexFromListIndex( -3, 5 ) == -1, "negative list index" );

    // program types
    check( programTypeAcceptsSamples( true, false, false ), "drum" );
    check( programTypeAcceptsSamples( false, true, false ), "clip" );
    check( programTypeAcceptsSamples( false, false, true ), "keygroup" );
    check( !programTypeAcceptsSamples( false, false, false ), "audio / midi / cv / plugin" );

    // zones: drum pad, keygroup zone
    check( zoneCoversNote( 5, 5, 5, 5 ) && !zoneCoversNote( 5, 5, 5, 6 ), "drum pad covers its key only" );
    check( zoneCoversNote( 48, 59, 53, 48 ) && zoneCoversNote( 48, 59, 53, 59 ), "keygroup zone ends inclusive" );
    check( !zoneCoversNote( 48, 59, 53, 47 ) && !zoneCoversNote( 48, 59, 53, 60 ), "outside keygroup zone" );
    check( zoneCoversNote( 59, 48, 53, 50 ), "reversed zone is normalised" );

    const auto drum = zoneNotes( 7, 7, 7 );
    check( drum.first == 7 && drum.last == 7, "drum range" );
    const auto kg = zoneNotes( 48, 59, 53 );
    check( kg.first == 48 && kg.last == 59, "keygroup labels start at minNote, not 0" );
    const auto full = zoneNotes( 0, 127, 60 );
    check( full.first == 0 && full.last == 127, "full keyboard zone" );
    const auto clamp = zoneNotes( -5, 300, 60 );
    check( clamp.first == 0 && clamp.last == 127, "clamped" );

    // Keygroup pads: only the pads whose note is in the zone are assigned
    std::vector<ZoneInfo> zones{ { 16, 20, 18, true } };
    check( !padAssigned( zones, 0, 3 ), "bank A pad outside zone not assigned" );
    check( padAssigned( zones, 16, 0 ) && padAssigned( zones, 16, 4 ), "bank B pads 0..4 inside zone" );
    check( !padAssigned( zones, 16, 5 ), "bank B pad 5 outside zone" );
    zones[0].hasSource = false;
    check( !padAssigned( zones, 16, 0 ), "zone without source is not assigned" );
    std::vector<ZoneInfo> drumZones{ { 3, 3, 3, true }, { 9, 9, 9, true } };
    check( padAssigned( drumZones, 0, 3 ) && padAssigned( drumZones, 0, 9 ) && !padAssigned( drumZones, 0, 4 ), "drum pads" );

    // debounce: only the latest request survives
    AuditionDebounce d;
    const auto t1 = d.request();
    const auto t2 = d.request();
    check( !d.isCurrent( t1 ) && d.isCurrent( t2 ), "latest ticket wins" );
    d.cancel();
    check( !d.isCurrent( t2 ), "cancel invalidates" );

    // AS-10 layer indicators
    check( formatLayerIndicator( false, false, false, false ).empty(), "no layers assigned is empty" );
    check( formatLayerIndicator( true, false, false, false ) == "[1 - - -]", "layer 1 only" );
    check( formatLayerIndicator( false, true, false, false ) == "[- 2 - -]", "layer 2 only" );
    check( formatLayerIndicator( true, true, true, true ) == "[1 2 3 4]", "all layers" );
    check( formatLayerIndicator( true, false, true, false ) == "[1 - 3 -]", "layers 1 and 3" );

    // AS-11 sample usage check
    std::vector<std::string> used{ "/data/projects/test/samples/Kick.wav", "/data/projects/test/samples/Snare.wav" };
    check( isSampleUsed( "/data/projects/test/samples/Kick.wav", used ), "exact path used" );
    check( isSampleUsed( "Kick.wav", used ), "filename match used" );
    check( isSampleUsed( "/other/dir/Kick.wav", used ), "diff dir same filename used" );
    check( !isSampleUsed( "HiHat.wav", used ), "unused sample" );
    check( !isSampleUsed( "", used ), "empty sample unused" );

    // AS-13 pad drop position (Akai 4x4 layout: row 0 bottom, row 3 top)
    // Grid: x: 100..500 (w: 400), y: 100..500 (h: 400), col width: 100, row height: 100
    // Pad 0 is bottom-left (x: 100..199, y: 400..499)
    check( padIndexFromGridPoint( 150, 450, 100, 100, 400, 400, 0 ) == 0, "bottom-left is pad 0" );
    // Pad 3 is bottom-right (x: 400..499, y: 400..499)
    check( padIndexFromGridPoint( 450, 450, 100, 100, 400, 400, 0 ) == 3, "bottom-right is pad 3" );
    // Pad 12 is top-left (x: 100..199, y: 100..199)
    check( padIndexFromGridPoint( 150, 150, 100, 100, 400, 400, 0 ) == 12, "top-left is pad 12" );
    // Pad 15 is top-right (x: 400..499, y: 100..199)
    check( padIndexFromGridPoint( 450, 150, 100, 100, 400, 400, 0 ) == 15, "top-right is pad 15" );
    // Bank B offset = 16
    check( padIndexFromGridPoint( 150, 450, 100, 100, 400, 400, 16 ) == 16, "bank B pad 0" );
    // Outside grid returns -1
    check( padIndexFromGridPoint( 50, 450, 100, 100, 400, 400, 0 ) == -1, "outside left" );
    check( padIndexFromGridPoint( 550, 450, 100, 100, 400, 400, 0 ) == -1, "outside right" );
    check( padIndexFromGridPoint( 150, 50, 100, 100, 400, 400, 0 ) == -1, "outside top" );
    check( padIndexFromGridPoint( 150, 550, 100, 100, 400, 400, 0 ) == -1, "outside bottom" );

    std::puts( "AssignSampleLogicTests OK" );
    return 0;
}
