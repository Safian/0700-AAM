// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test of the recent-projects list logic (PRJ-22): open again, new entry at the limit, a lowered maximum, rename, remove,
// the order (oldest first) and the property that the list never exceeds its maximum or holds a path twice.
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t RecentListTests.cpp
#include "../../modules/engine/project/kick_recent_list.h"
#include <cstdio>
#include <random>

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

using List = std::vector<std::string>;
using namespace kick_recent;

static List open( List l, const std::string& p, bool exists, int max )
{
    apply( l, planOpen( l, p, exists, max ), p );
    return l;
}

int main()
{
    CHECK( effectiveMax( 5 ) == 5 && effectiveMax( 0 ) == 1 && effectiveMax( -3 ) == 1 );

    // A new project is appended as the newest entry.
    CHECK( open( {}, "/p/a", true, 5 ) == List( { "/p/a" } ) );
    CHECK( open( { "/p/a" }, "/p/b", true, 5 ) == List( { "/p/a", "/p/b" } ) );

    // Opening a project that is in the list moves it to the end; nothing else goes.
    CHECK( open( { "/p/a", "/p/b", "/p/c" }, "/p/a", true, 5 ) == List( { "/p/b", "/p/c", "/p/a" } ) );
    CHECK( open( { "/p/a", "/p/b", "/p/c" }, "/p/c", true, 5 ) == List( { "/p/a", "/p/b", "/p/c" } ) );
    CHECK( open( { "/p/a", "/p/b", "/p/c" }, "/p/b", true, 3 ) == List( { "/p/a", "/p/c", "/p/b" } ) ); // full, but a re-open frees its own slot

    // A full list drops the oldest for a new project.
    CHECK( open( { "/p/a", "/p/b", "/p/c" }, "/p/d", true, 3 ) == List( { "/p/b", "/p/c", "/p/d" } ) );
    // A list longer than the maximum (the maximum was lowered) is cut by the next open: size == max afterwards.
    CHECK( open( { "/p/a", "/p/b", "/p/c", "/p/d", "/p/e" }, "/p/f", true, 3 ) == List( { "/p/d", "/p/e", "/p/f" } ) );
    {
        const List r = open( { "/p/a", "/p/b", "/p/c", "/p/d", "/p/e" }, "/p/e", true, 3 );
        CHECK( r == List( { "/p/c", "/p/d", "/p/e" } ) );
    }
    // Maximum 1: only the newest.
    CHECK( open( { "/p/a" }, "/p/b", true, 1 ) == List( { "/p/b" } ) );
    CHECK( open( { "/p/a" }, "/p/b", true, 0 ) == List( { "/p/b" } ) );

    // A project whose file does not exist (or an empty path) changes nothing.
    CHECK( open( { "/p/a", "/p/b" }, "/p/x", false, 5 ) == List( { "/p/a", "/p/b" } ) );
    CHECK( open( { "/p/a" }, "", true, 5 ) == List( { "/p/a" } ) );
    CHECK( !planOpen( { "/p/a" }, "/p/x", false, 5 ).add );

    // A duplicate in a damaged list: every copy goes, one comes back.
    CHECK( open( { "/p/a", "/p/b", "/p/a" }, "/p/a", true, 5 ) == List( { "/p/b", "/p/a" } ) );

    // indicesOf is highest first; excess; renamed.
    CHECK( indicesOf( { "x", "y", "x" }, "x" ) == std::vector<int>( { 2, 0 } ) );
    CHECK( indicesOf( { "x" }, "z" ).empty() );
    CHECK( excess( 5, 5 ) == 0 && excess( 7, 5 ) == 2 && excess( 3, 5 ) == 0 && excess( 4, 0 ) == 3 );
    CHECK( renamed( { "/p/a", "/p/b" }, "/p/a", "/p/z" ) == List( { "/p/z", "/p/b" } ) );
    CHECK( renamed( { "/p/a", "/p/b" }, "/p/q", "/p/z" ) == List( { "/p/a", "/p/b" } ) );
    CHECK( renamed( { "/p/a", "/p/b", "/p/c" }, "/p/a", "/p/c" ) == List( { "/p/b", "/p/c" } ) ); // the newest of the two stays
    CHECK( renamed( { "/p/c", "/p/b", "/p/a" }, "/p/a", "/p/c" ) == List( { "/p/b", "/p/c" } ) );

    // Property: any sequence of opens keeps at most max entries, no path twice, the last opened path at the end.
    {
        std::mt19937 rng( 7 );
        List         l;
        for( int step = 0; step < 2000; ++step )
        {
            const int         max  = 1 + (int)( rng() % 6 );
            const std::string path = "/p/" + std::to_string( rng() % 9 );
            l                      = open( l, path, true, max );
            CHECK( l.size() <= (size_t)max );
            CHECK( !l.empty() && l.back() == path );
            for( size_t i = 0; i < l.size(); ++i )
                for( size_t j = i + 1; j < l.size(); ++j )
                    CHECK( l[i] != l[j] );
        }
    }

    if( g_fail == 0 ) std::printf( "RecentListTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
