#include "../../modules/engine/audio_engine/engine/mt_tree_sync.h"
#include <cstdio>
#include <string>
using namespace mt::treesync;
static int fails = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL line %d: %s\n", __LINE__, #c ); ++fails; } } while( 0 )

struct Obj { std::string id; int serial; };
static int g_serial = 0;
static int g_destroyed = 0;
using List = std::vector<std::unique_ptr<Obj>>;

static List make( std::initializer_list<const char*> ids )
{
    List l;
    for( auto* i : ids ) l.push_back( std::make_unique<Obj>( Obj{ i, ++g_serial } ) );
    return l;
}
static bool run( List& l, const std::vector<std::string>& nodes )
{
    return reconcile( l, nodes,
                      []( const Obj& o, const std::string& n ) { return o.id == n; },
                      []( const std::string& n ) { return std::make_unique<Obj>( Obj{ n, ++g_serial } ); },
                      []( Obj& ) { ++g_destroyed; } );
}
static std::string ids( const List& l ) { std::string s; for( auto& o : l ) s += o->id; return s; }

int main()
{
    { // nothing to do: same objects, no change
        auto l = make( { "a", "b", "c" } );
        const Obj* a = l[0].get();
        CHECK( !run( l, { "a", "b", "c" } ) );
        CHECK( ids( l ) == "abc" && l[0].get() == a );
    }
    { // undo of a create: the tree lost a node
        auto l = make( { "a", "b", "c" } );
        g_destroyed = 0;
        CHECK( run( l, { "a", "b" } ) );
        CHECK( ids( l ) == "ab" && g_destroyed == 1 );
    }
    { // redo of a create: the tree has a new node at the end
        auto l = make( { "a", "b" } );
        CHECK( run( l, { "a", "b", "c" } ) );
        CHECK( ids( l ) == "abc" );
    }
    { // undo of a delete in the middle: the new object lands in tree position, the others keep their address
        auto l = make( { "a", "c" } );
        const Obj* a = l[0].get();
        const Obj* c = l[1].get();
        CHECK( run( l, { "a", "b", "c" } ) );
        CHECK( ids( l ) == "abc" && l[0].get() == a && l[2].get() == c );
    }
    { // delete + create at once, and a reorder
        auto l = make( { "a", "b", "c" } );
        g_destroyed = 0;
        CHECK( run( l, { "c", "d", "a" } ) );
        CHECK( ids( l ) == "cda" && g_destroyed == 1 );
    }
    { // pure reorder counts as a change
        auto l = make( { "a", "b" } );
        CHECK( run( l, { "b", "a" } ) );
        CHECK( ids( l ) == "ba" );
    }
    { // everything removed / everything created
        auto l = make( { "a", "b" } );
        CHECK( run( l, {} ) );
        CHECK( l.empty() );
        CHECK( run( l, { "x" } ) );
        CHECK( ids( l ) == "x" );
    }
    { // a failed create (null) skips the node
        auto l = make( { "a" } );
        bool ch = reconcile( l, std::vector<std::string>{ "a", "bad" },
                             []( const Obj& o, const std::string& n ) { return o.id == n; },
                             []( const std::string& n ) { return n == "bad" ? std::unique_ptr<Obj>() : std::make_unique<Obj>( Obj{ n, ++g_serial } ); },
                             []( Obj& ) {} );
        CHECK( !ch && ids( l ) == "a" );
    }
    std::puts( fails ? "FAILED" : "OK" );
    return fails ? 1 : 0;
}
