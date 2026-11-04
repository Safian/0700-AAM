// SPDX-License-Identifier: AGPL-3.0-or-later
// Insert favourites, user presets and A/B state (kick_InsertStore.h): the text form round trip, hostile input, the A/B toggle.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "../../modules/kick_ui/dialogs/kick_InsertStore.h"

static int g_fail = 0;
#define CHECK( c )                                                                   \
    do                                                                               \
    {                                                                                \
        if( !( c ) )                                                                 \
        {                                                                            \
            std::printf( "FAIL %s:%d  %s\n", __FILE__, __LINE__, #c );               \
            ++g_fail;                                                                \
        }                                                                            \
    } while( 0 )

using namespace kick_ui::insert_store;

int main()
{
    Store s;
    CHECK( !s.isFavourite( "Reverb" ) );
    CHECK( s.toggleFavourite( "Reverb" ) );
    CHECK( s.isFavourite( "Reverb" ) );
    CHECK( !s.toggleFavourite( "Reverb" ) );
    CHECK( !s.isFavourite( "Reverb" ) );
    CHECK( !s.toggleFavourite( "" ) );
    s.toggleFavourite( "Reverb" );
    s.toggleFavourite( "Chamber2" );
    s.savePreset( "Reverb", { 0.f, 0.25f, 1.f } );
    s.savePreset( "Empty", {} );                       // nothing to keep
    s.savePreset( "TooBig", std::vector<float>( kMaxParams + 1, 0.5f ) );
    s.savePreset( "Wild", { -4.f, 7.f, std::nanf( "" ) } ); // clamped, NaN -> 0
    CHECK( s.numPresets() == 2 );
    CHECK( s.preset( "Empty" ) == nullptr && s.preset( "TooBig" ) == nullptr );
    CHECK( s.preset( "Wild" ) != nullptr && ( *s.preset( "Wild" ) )[0] == 0.f && ( *s.preset( "Wild" ) )[1] == 1.f && ( *s.preset( "Wild" ) )[2] == 0.f );

    const auto text = s.serialize();
    const auto back = Store::parse( text );
    CHECK( back.favourites() == s.favourites() );
    CHECK( back.numPresets() == 2 );
    CHECK( back.preset( "Reverb" ) != nullptr && *back.preset( "Reverb" ) == *s.preset( "Reverb" ) );
    CHECK( back.serialize() == text ); // stable

    // hostile / damaged input: skipped line by line, never throws
    const auto bad = Store::parse( "garbage\nfav\n fav\t\npreset\tX\nfav\tKept\r\npreset\tY\t0.5\tnope\npreset\tZ\t0.5\t0.75\n\n\t\t\n" );
    CHECK( bad.isFavourite( "Kept" ) );
    CHECK( bad.favourites().size() == 1 );
    CHECK( bad.preset( "Y" ) == nullptr );
    CHECK( bad.preset( "Z" ) != nullptr && bad.preset( "Z" )->size() == 2 );
    CHECK( Store::parse( "" ).numPresets() == 0 );

    // ---- A/B
    AbSlot ab;
    CHECK( ab.toggle( "Reverb", { 0.1f, 0.2f } ) == nullptr ); // first time: B starts as a copy, nothing to change
    CHECK( ab.onB );
    CHECK( ab.b == std::vector<float>( { 0.1f, 0.2f } ) );
    const auto* toA = ab.toggle( "Reverb", { 0.9f, 0.8f } ); // edited on B, back to A: A comes back
    CHECK( toA != nullptr && *toA == std::vector<float>( { 0.1f, 0.2f } ) );
    CHECK( !ab.onB );
    const auto* toB = ab.toggle( "Reverb", { 0.3f, 0.3f } ); // edited on A, to B: B has what was left there
    CHECK( toB != nullptr && *toB == std::vector<float>( { 0.9f, 0.8f } ) );
    CHECK( ab.a == std::vector<float>( { 0.3f, 0.3f } ) );
    CHECK( ab.toggle( "Chamber2", { 0.5f } ) == nullptr ); // another effect in the slot: starts over
    CHECK( ab.effect == "Chamber2" && ab.onB && ab.b.size() == 1 );
    ab.reset();
    CHECK( ab.effect.empty() && !ab.onB && ab.a.empty() && ab.b.empty() );

    if( g_fail == 0 )
        std::printf( "InsertStoreTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
