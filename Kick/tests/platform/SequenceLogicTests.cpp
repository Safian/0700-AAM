// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the JUCE-free Next Sequence / Song logic (kick_SequenceLogic.h, kick_SongLogic.h).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined SequenceLogicTests.cpp
#include "../../modules/kick_ui/modes/kick_SequenceLogic.h"
#include "../../modules/kick_ui/modes/kick_SongLogic.h"
#include <cstdio>

static int g_fail = 0;
#define CHECK_EQ( a, b )                                                                                                   \
    do {                                                                                                                   \
        long long va = (long long)( a ), vb = (long long)( b );                                                            \
        if( va != vb ) { std::printf( "FAIL %s:%d  %s = %lld, expected %lld\n", __FILE__, __LINE__, #a, va, vb ); ++g_fail; } \
    } while( 0 )
#define CHECK( a ) CHECK_EQ( ( a ) ? 1 : 0, 1 )

using namespace kick_ui::sequence_logic;
namespace sl = kick_ui::song_logic;

int main()
{
    // ---- Q-Link (NS-02 / NS-06): missing sequence = no edit, ranges kept
    SeqValues none;
    CHECK( sequenceQLink( 1, none, 1 ).kind == QLinkEdit::Kind::None );

    SeqValues s;
    s.valid = true; s.bpm = 120; s.length = 4; s.loopStart = 1; s.loopEnd = 3;
    auto e = sequenceQLink( 1, s, 5 );
    CHECK( e.kind == QLinkEdit::Kind::Bpm ); CHECK_EQ( e.a, 125 );
    CHECK_EQ( sequenceQLink( 1, s, 500 ).a, 240 );
    CHECK_EQ( sequenceQLink( 1, s, -500 ).a, 20 );
    e = sequenceQLink( 2, s, -10 );
    CHECK( e.kind == QLinkEdit::Kind::Length ); CHECK_EQ( e.a, 1 );
    CHECK_EQ( sequenceQLink( 2, s, 1000 ).a, 100 );
    e = sequenceQLink( 3, s, -5 );
    CHECK( e.kind == QLinkEdit::Kind::LoopPoints ); CHECK_EQ( e.a, 0 ); CHECK_EQ( e.b, 3 );
    e = sequenceQLink( 4, s, 2 );
    CHECK( e.kind == QLinkEdit::Kind::LoopPoints ); CHECK_EQ( e.a, 1 ); CHECK_EQ( e.b, 5 );
    CHECK( sequenceQLink( 5, s, 1 ).kind == QLinkEdit::Kind::None ); // no 5th encoder on the Key 61
    CHECK( sequenceQLink( 1, s, 0 ).kind == QLinkEdit::Kind::None );

    // ---- NS-01: the two reproduction paths of the old std::abort()
    // (a) PLAY, queue pad 3, STOP, bank change -> restore with the engine stopped.
    NextState st{ 0, -1, true };
    st = queueNext( st, 3 );
    CHECK_EQ( st.next, 3 );
    auto r = restore( { st.playing, st.next, true /*screen still believes it plays*/ }, false /*engine stopped*/, 0 );
    CHECK( !r.state.isPlaying );
    CHECK_EQ( r.state.next, -1 ); // dropped, no abort
    CHECK( r.clearEngineNext );
    CHECK_EQ( r.state.playing, 0 );
    // (b) playing, queued, leave the screen, STOP elsewhere, return -> same restore.
    r = restore( { 2, 5, true }, false, 2 );
    CHECK_EQ( r.state.next, -1 ); CHECK( r.clearEngineNext );
    // Playing engine keeps its queue.
    r = restore( { 2, 5, false }, true, 2 );
    CHECK_EQ( r.state.next, 5 ); CHECK( !r.clearEngineNext ); CHECK( r.state.isPlaying );
    // No playing sequence known: the engine's selection is used.
    r = restore( { -1, -1, false }, false, 7 );
    CHECK_EQ( r.state.playing, 7 ); CHECK( !r.clearEngineNext );

    // Clear with no playing sequence used to abort.
    auto c = clearNext( { -1, 4, false }, 6 );
    CHECK_EQ( c.playing, 6 ); CHECK_EQ( c.next, -1 );
    c = clearNext( { 2, 4, false }, 6 );
    CHECK_EQ( c.playing, 2 ); CHECK_EQ( c.next, -1 );

    // queueNext rules
    NextState q{ 1, -1, true };
    q = queueNext( q, 4 ); CHECK_EQ( q.next, 4 );
    q = queueNext( q, 4 ); CHECK_EQ( q.next, 4 );   // same pad again keeps it
    q = queueNext( q, 6 ); CHECK_EQ( q.next, 6 );   // another replaces it
    q = queueNext( q, 1 ); CHECK_EQ( q.next, -1 );  // the playing one drops the queue
    q = queueNext( q, 1 ); CHECK_EQ( q.next, -1 );

    // hold / hidden
    CHECK_EQ( holdOn( { 1, 4, true } ).next, -1 );
    CHECK_EQ( holdOn( { 1, 4, false } ).next, 4 );
    auto h = hiddenWhileStopped( { 1, 4, false } );
    CHECK_EQ( h.playing, 4 ); CHECK_EQ( h.next, -1 );
    h = hiddenWhileStopped( { 1, 4, true } );
    CHECK_EQ( h.playing, 1 ); CHECK_EQ( h.next, 4 );

    // ---- Song (SG-06 / SG-11)
    // Insert with no selection goes to the END.
    auto p = sl::planInsert( 3, -1, 2, -1 );
    CHECK( p.kind == sl::InsertPlan::Kind::Add ); CHECK_EQ( p.index, 3 ); CHECK_EQ( p.selectAfter, 3 );
    // Insert with a selected step goes after it.
    p = sl::planInsert( 3, 1, 2, 0 );
    CHECK( p.kind == sl::InsertPlan::Kind::Add ); CHECK_EQ( p.index, 2 ); CHECK_EQ( p.selectAfter, 2 );
    // Same sequence as the selected step: repeat + 1 on that step.
    p = sl::planInsert( 3, 1, 2, 2 );
    CHECK( p.kind == sl::InsertPlan::Kind::RepeatPlus1 ); CHECK_EQ( p.index, 1 ); CHECK_EQ( p.selectAfter, 1 );
    // Empty song.
    p = sl::planInsert( 0, -1, 0, -1 );
    CHECK( p.kind == sl::InsertPlan::Kind::Add ); CHECK_EQ( p.index, 0 );
    // Stale selection past the end behaves like none.
    p = sl::planInsert( 2, 5, 1, 1 );
    CHECK( p.kind == sl::InsertPlan::Kind::Add ); CHECK_EQ( p.index, 2 );

    // Delete only with a real selection (getSelectedRow() used to give 0 for none).
    CHECK( !sl::canDelete( 3, -1 ) );
    CHECK( !sl::canDelete( 0, 0 ) );
    CHECK( !sl::canDelete( 3, 3 ) );
    CHECK( sl::canDelete( 3, 0 ) );
    CHECK_EQ( sl::selectionAfterDelete( 3, 1 ), 1 );
    CHECK_EQ( sl::selectionAfterDelete( 3, 2 ), 1 );
    CHECK_EQ( sl::selectionAfterDelete( 1, 0 ), -1 );
    CHECK_EQ( sl::selectionAfterDelete( 2, 0 ), 0 );

    // Name validation (SG-05).
    std::vector<std::string> existing{ "Verse", "Chorus" };
    CHECK( sl::checkName( "", existing ) == sl::NameCheck::Empty );
    CHECK( sl::checkName( "   ", existing ) == sl::NameCheck::Empty );
    CHECK( sl::checkName( "Create New Song", existing ) == sl::NameCheck::Reserved );
    CHECK( sl::checkName( "  create new song ", existing ) == sl::NameCheck::Reserved );
    CHECK( sl::checkName( "verse", existing ) == sl::NameCheck::Duplicate );
    CHECK( sl::checkName( "Bridge", existing ) == sl::NameCheck::Ok );
    CHECK( sl::checkName( "VERSE", existing, "Verse" ) == sl::NameCheck::Ok ); // renaming to its own name (case change)
    CHECK( sl::checkName( "Chorus", existing, "Verse" ) == sl::NameCheck::Duplicate );
    CHECK( sl::trim( "  a b  " ) == "a b" );

    // Missing step / song end.
    CHECK( sl::isMissingStep( -1 ) );
    CHECK( !sl::isMissingStep( 0 ) );
    CHECK( sl::unlockAfterSongEnd( true ) );
    CHECK( !sl::unlockAfterSongEnd( false ) );

    if( g_fail == 0 )
        std::printf( "SequenceLogicTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
