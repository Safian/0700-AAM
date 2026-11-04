// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for Grid pure logic helpers (pad-to-note mapping, button state machine, Q-Link zoom/BBT scaling).

#include "../../modules/kick_ui/audio_components/gridview/kick_grid_logic.h"
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

int main()
{
    testPadToNote();
    testButtonStateMachine();
    testZoomScaling();
    testQLinkIndexMapping();
    std::puts( "All Grid logic tests PASSED!" );
    return 0;
}
