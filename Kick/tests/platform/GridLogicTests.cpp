// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for Grid pure logic helpers (pad-to-note mapping, button state machine, Q-Link zoom/BBT scaling).

#include "../../modules/kick_ui/audio_components/gridview/kick_grid_logic.h"
#include <cmath>
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

static void testPadToNote()
{
    using namespace kick_ui::grid_logic;

    // Bank A (offset 0): pad 0..15 -> note 0..15
    for( int pad = 0; pad < 16; ++pad )
    {
        check( padToNote( pad, 0 ) == pad, "Bank A pad maps directly to note 0..15" );
    }

    // Bank B (offset 16): pad 0..15 -> note 16..31
    for( int pad = 0; pad < 16; ++pad )
    {
        int note = padToNote( pad, 16 );
        check( note == pad + 16, "Bank B pad maps to note 16..31" );
        // The old buggy code did: padNo += bankOffset (16), then later padNo + ae.getBankOffset() (16 + 16 = 32)
        int buggyNote = pad + 16 + 16;
        check( note != buggyNote, "padToNote avoids the double bank offset bug (32..47)" );
    }

    // Bank C (offset 32): pad 0..15 -> note 32..47
    check( padToNote( 0, 32 ) == 32, "Bank C pad 0 is note 32" );
    check( padToNote( 15, 32 ) == 47, "Bank C pad 15 is note 47" );

    // Bank D (offset 48): pad 0..15 -> note 48..63
    check( padToNote( 0, 48 ) == 48, "Bank D pad 0 is note 48" );
    check( padToNote( 15, 48 ) == 63, "Bank D pad 15 is note 63" );

    // Bank H (offset 112): pad 0..15 -> note 112..127
    check( padToNote( 0, 112 ) == 112, "Bank H pad 0 is note 112" );
    check( padToNote( 15, 112 ) == 127, "Bank H pad 15 is note 127" );

    std::puts( "testPadToNote passed" );
}

static void testButtonStateMachine()
{
    using namespace kick_ui::grid_logic;
    GridButtonStateMachine sm;

    // --- COPY button ---
    // Press: should trigger Copy
    check( sm.processButton( dcl_buttons::EN_COPY, ButtonState::Pressed ) == ButtonAction::Copy,
           "COPY press triggers Copy action" );
    check( sm.isButtonPressed( dcl_buttons::EN_COPY ), "COPY button is down" );

    // Release: MUST NOT trigger Copy again (old bug ran copy on release too)
    check( sm.processButton( dcl_buttons::EN_COPY, ButtonState::Released ) == ButtonAction::None,
           "COPY release returns None (does NOT paste twice)" );
    check( !sm.isButtonPressed( dcl_buttons::EN_COPY ), "COPY button is released" );

    // --- ERASE button ---
    // Press: should trigger Erase
    check( sm.processButton( dcl_buttons::EN_ERASE, ButtonState::Pressed ) == ButtonAction::Erase,
           "ERASE press triggers Erase action" );
    check( sm.isButtonPressed( dcl_buttons::EN_ERASE ), "ERASE button is down" );

    // Release: MUST NOT trigger Erase again (old bug ran erase on release too)
    check( sm.processButton( dcl_buttons::EN_ERASE, ButtonState::Released ) == ButtonAction::None,
           "ERASE release returns None (does NOT erase twice)" );
    check( !sm.isButtonPressed( dcl_buttons::EN_ERASE ), "ERASE button is released" );

    // --- Transport buttons ---
    check( sm.processButton( dcl_buttons::EN_PLAY, ButtonState::Pressed ) == ButtonAction::Play, "PLAY press" );
    check( sm.processButton( dcl_buttons::EN_PLAY, ButtonState::Released ) == ButtonAction::None, "PLAY release" );

    check( sm.processButton( dcl_buttons::EN_PLAYSTART, ButtonState::Pressed ) == ButtonAction::PlayStart, "PLAYSTART press" );
    check( sm.processButton( dcl_buttons::EN_PLAYSTART, ButtonState::Released ) == ButtonAction::None, "PLAYSTART release" );

    check( sm.processButton( dcl_buttons::EN_STOP, ButtonState::Pressed ) == ButtonAction::Stop, "STOP press" );
    check( sm.processButton( dcl_buttons::EN_STOP, ButtonState::Released ) == ButtonAction::None, "STOP release" );

    // --- Knob CW / CCW buttons ---
    check( sm.processButton( dcl_buttons::EN_KNOB_CW, ButtonState::Pressed ) == ButtonAction::KnobCw, "KNOB_CW press" );
    check( sm.processButton( dcl_buttons::EN_KNOB_CW, ButtonState::Released ) == ButtonAction::None, "KNOB_CW release" );

    check( sm.processButton( dcl_buttons::EN_KNOB_CCW, ButtonState::Pressed ) == ButtonAction::KnobCcw, "KNOB_CCW press" );
    check( sm.processButton( dcl_buttons::EN_KNOB_CCW, ButtonState::Released ) == ButtonAction::None, "KNOB_CCW release" );

    // --- Held repeat / edge triggering ---
    check( sm.processButton( dcl_buttons::EN_COPY, ButtonState::Pressed ) == ButtonAction::Copy, "First press triggers" );
    check( sm.processButton( dcl_buttons::EN_COPY, ButtonState::Pressed ) == ButtonAction::None, "Duplicate press while held is ignored" );
    sm.processButton( dcl_buttons::EN_COPY, ButtonState::Released );
    check( sm.processButton( dcl_buttons::EN_COPY, ButtonState::Pressed ) == ButtonAction::Copy, "Press after release triggers again" );
    sm.reset();
    check( !sm.isButtonPressed( dcl_buttons::EN_COPY ), "Reset clears button state" );

    // --- Unhandled button code ---
    check( sm.processButton( 99, ButtonState::Pressed ) == ButtonAction::None, "Unhandled button returns None" );

    std::puts( "testButtonStateMachine passed" );
}

static void testZoomScaling()
{
    using namespace kick_ui::grid_logic;

    // On Key 61, rotary encoder turn sends delta = +/-1 (or small ints like +/-2, +/-3)
    // Old bug: (float)value / 100 gave 0.01 px, virtually zero zoom effect on 432px bar
    check( calculateHorizontalZoomDelta( 1 ) == 20.0f, "Encoder +1 -> +20px horizontal zoom" );
    check( calculateHorizontalZoomDelta( -1 ) == -20.0f, "Encoder -1 -> -20px horizontal zoom" );
    check( calculateHorizontalZoomDelta( 2 ) == 40.0f, "Encoder +2 -> +40px horizontal zoom" );
    check( calculateHorizontalZoomDelta( 0 ) == 0.0f, "Encoder 0 -> 0px" );

    // Vertical zoom: note height default 26px, step = 1.0f
    check( calculateVerticalZoomDelta( 1 ) == 1.0f, "Encoder +1 -> +1px vertical zoom" );
    check( calculateVerticalZoomDelta( -1 ) == -1.0f, "Encoder -1 -> -1px vertical zoom" );
    check( calculateVerticalZoomDelta( 3 ) == 3.0f, "Encoder +3 -> +3px vertical zoom" );

    std::puts( "testZoomScaling passed" );
}

static void testQLinkIndexMapping()
{
    using namespace kick_ui::grid_logic;

    // EN_KNOB_5 (5), EN_KNOB_6 (6), EN_KNOB_7 (7) -> BBT (0: Bar, 1: Beat, 2: Tick)
    check( knobToBbtIndex( 5 ) == 0, "Knob 5 -> BBT Bar (0)" );
    check( knobToBbtIndex( 6 ) == 1, "Knob 6 -> BBT Beat (1)" );
    check( knobToBbtIndex( 7 ) == 2, "Knob 7 -> BBT Tick (2)" );

    // EN_KNOB_8 (8), EN_KNOB_9 (9), EN_KNOB_10 (10) -> Metronome (0: Level, 1: Rate, 2: OnOff)
    check( knobToMetronomeIndex( 8 ) == 0, "Knob 8 -> Metronome Level (0)" );
    check( knobToMetronomeIndex( 9 ) == 1, "Knob 9 -> Metronome Rate (1)" );
    check( knobToMetronomeIndex( 10 ) == 2, "Knob 10 -> Metronome OnOff (2)" );

    std::puts( "testQLinkIndexMapping passed" );
}

static bool approx( float a, float b, float eps = 0.01f )
{
    return std::fabs( a - b ) <= eps;
}

static void testYToNoteMapping()
{
    using namespace kick_ui::grid_logic;

    const float noteH = 26.0f;
    for( int n = 0; n < 128; ++n )
    {
        const float top = noteToY( n, noteH );
        check( yToNote( top, noteH ) == n, "Exact top edge of row maps to same note (not off-by-1)" );
        check( yToNote( top + 0.5f * noteH, noteH ) == n, "Row center maps to same note" );
        check( yToNote( top + noteH - 0.01f, noteH ) == n, "Bottom edge of row maps to same note" );
    }

    check( yToNote( -10.0f, noteH ) == 127, "Above top clamps to note 127 (never 128)" );
    check( yToNote( 128.0f * noteH + 20.0f, noteH ) == 0, "Below bottom clamps to note 0 (never negative)" );

    std::puts( "testYToNoteMapping passed" );
}

static void testTouchIdentity()
{
    using namespace kick_ui::grid_logic;
    check( resolveTouchId( 3, 0 ) == 3, "Hardware TouchSlot overrides JUCE shared source index 0" );
    check( resolveTouchId( 0, 0 ) == 0, "Hardware slot 0 works cleanly" );
    check( resolveTouchId( -1, 1 ) == 1, "iOS/desktop uses JUCE source index when not in bridge scope" );
    std::puts( "testTouchIdentity passed" );
}

static void testSingleFingerPanAndPinchZoom()
{
    using namespace kick_ui::grid_logic;

    GridZoomBounds bounds{ 216.0f, 4096.0f, 6.0f, 52.0f };
    GridViewportState st;
    st.viewX      = 100.0f;
    st.viewY      = 500.0f;
    st.viewWidth  = 864.0f;
    st.viewHeight = 400.0f;
    st.barWidth   = 432.0f;
    st.noteHeight = 26.0f;
    st.numBars    = 4;
    st.maxNotes   = 128;

    // 1-finger pan: drag down-right (+40, +30) -> viewport scrolls up-left (-40, -30)
    auto pan = computeSingleFingerPan( st, { 200.0f, 200.0f }, { 240.0f, 230.0f } );
    check( approx( pan.viewX, 60.0f ) && approx( pan.viewY, 470.0f ), "1-finger drag scrolls smoothly in viewport space" );

    // Slow sub-pixel pan preserves float increment without integer stall
    auto slowPan = computeSingleFingerPan( st, { 200.0f, 200.0f }, { 200.4f, 200.3f } );
    check( approx( slowPan.viewX, 99.6f ) && approx( slowPan.viewY, 499.7f ), "Sub-pixel pan preserves fractional offset" );

    // Vertical pinch only ("zoom - pitch"): X span tiny (6px < 24px), Y span 100 -> 150px
    TouchPoint vp1Prev{ 300.0f, 150.0f }, vp2Prev{ 306.0f, 250.0f };
    TouchPoint vp1Cur{ 300.0f, 125.0f }, vp2Cur{ 306.0f, 275.0f };
    const float anchorContentY = st.viewY + 200.0f; // midpoint y = 200
    const float anchorRowFrac  = anchorContentY / st.noteHeight;
    auto vPinch = computePinchPan( st, bounds, vp1Prev, vp2Prev, vp1Cur, vp2Cur );
    check( approx( vPinch.barWidth, st.barWidth ), "Pure vertical pinch leaves horizontal barWidth unchanged" );
    check( vPinch.noteHeight > st.noteHeight, "Spreading fingers vertically increases noteHeight (pitch zoom in)" );
    const float newAnchorRowFrac = ( vPinch.viewY + 200.0f ) / vPinch.noteHeight;
    check( approx( newAnchorRowFrac, anchorRowFrac, 0.05f ), "Pitch row under pinch midpoint stays anchored" );

    // Reverse pinch vertically in next step (150px -> 100px) zooms OUT immediately
    st.noteHeight = vPinch.noteHeight;
    st.viewY      = vPinch.viewY;
    auto vPinchOut = computePinchPan( st, bounds, vp1Cur, vp2Cur, vp1Prev, vp2Prev );
    check( vPinchOut.noteHeight < vPinch.noteHeight, "Closing fingers immediately reverses into zoom out" );

    // Horizontal pinch only: Y span tiny (4px), X span 160 -> 240px
    st.barWidth   = 432.0f;
    st.noteHeight = 26.0f;
    st.viewX      = 200.0f;
    st.viewY      = 500.0f;
    const float anchorTickPx   = st.viewX + 300.0f;
    const float anchorBarFrac  = anchorTickPx / st.barWidth;
    auto hPinch = computePinchPan( st, bounds,
                                   { 220.0f, 180.0f }, { 380.0f, 184.0f },
                                   { 180.0f, 180.0f }, { 420.0f, 184.0f } );
    check( approx( hPinch.noteHeight, st.noteHeight ), "Pure horizontal pinch keeps vertical noteHeight steady" );
    check( hPinch.barWidth > 432.0f, "Spreading fingers horizontally zooms in horizontally" );
    check( approx( ( hPinch.viewX + 300.0f ) / hPinch.barWidth, anchorBarFrac, 0.02f ),
           "Musical time under horizontal pinch midpoint stays anchored" );

    // Diagonal pinch zooms BOTH axes simultaneously
    auto dPinch = computePinchPan( st, bounds,
                                   { 200.0f, 150.0f }, { 320.0f, 250.0f },
                                   { 170.0f, 125.0f }, { 350.0f, 275.0f } );
    check( dPinch.barWidth > st.barWidth && dPinch.noteHeight > st.noteHeight,
           "Diagonal pinch zooms both horizontal time and vertical pitch" );

    // Two-finger parallel pan (same span, midpoint moves by +30, -20 -> viewport shifts -30, +20)
    auto twoPan = computePinchPan( st, bounds,
                                   { 200.0f, 150.0f }, { 320.0f, 250.0f },
                                   { 230.0f, 130.0f }, { 350.0f, 230.0f } );
    check( approx( twoPan.barWidth, st.barWidth ) && approx( twoPan.noteHeight, st.noteHeight ),
           "Two-finger parallel drag preserves zoom levels" );
    check( approx( twoPan.viewX, st.viewX - 30.0f ) && approx( twoPan.viewY, st.viewY + 20.0f ),
           "Two-finger parallel drag pans viewport 1:1" );

    // Bounds clamp test
    auto clampedOut = computePinchPan( st, bounds,
                                       { 100.0f, 100.0f }, { 500.0f, 500.0f },
                                       { 100.0f, 100.0f }, { 130.0f, 130.0f } );
    check( clampedOut.barWidth >= bounds.minBarWidth && clampedOut.noteHeight >= bounds.minNoteHeight,
           "Pinch out respects min zoom bounds" );

    std::puts( "testSingleFingerPanAndPinchZoom passed" );
}

static void testNoteEraseHitTesting()
{
    using namespace kick_ui::grid_logic;

    // Unsorted notes (regression case where old MTNoteList::find early break missed notes after an earlier start>target)
    const NoteSpan notes[] = {
        { 36, 960, 120 },
        { 36, 480, 60 },
        { 37, 480, 120 },
        { 36, 0, 120 },
    };

    const int slop = ticksFromPixelSlop( 6.0f, 432.0f, 1920 ); // ~27 ticks
    check( slop >= 20 && slop <= 35, "6px slop at 432px/bar maps cleanly to ~27 ticks" );

    // Exact hit on unsorted earlier note
    check( findBestNoteMatch( notes, 4, 36, 500, slop ) == 1, "Finds exact note even when ValueTree is unsorted" );

    // Touch slop hit slightly before note start (e.g. 1/32 note edge on finger tap)
    check( findBestNoteMatch( notes, 4, 36, 465, slop ) == 1, "Tap just before thin note start hits within slop" );

    // Touch slop hit just after note end [480..540)
    check( findBestNoteMatch( notes, 4, 36, 548, slop ) == 1, "Tap just after thin note end hits within slop" );

    // Far from any note on row 36
    check( findBestNoteMatch( notes, 4, 36, 700, slop ) == -1, "Tap in empty space far from notes does not match" );

    // Different pitch row never cross-matches
    check( findBestNoteMatch( notes, 4, 38, 480, slop ) == -1, "Pitch row isolation prevents accidental adjacent erase" );

    std::puts( "testNoteEraseHitTesting passed" );
}

int main()
{
    testPadToNote();
    testButtonStateMachine();
    testZoomScaling();
    testQLinkIndexMapping();
    testYToNoteMapping();
    testTouchIdentity();
    testSingleFingerPanAndPinchZoom();
    testNoteEraseHitTesting();
    std::puts( "All Grid logic tests PASSED!" );
    return 0;
}
