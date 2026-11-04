// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test of the project name rules and the template -> project decision (fake file system).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t ProjectNamesTests.cpp
#include "../../modules/kick_ui/recent_project/kick_project_names.h"
#include <cstdio>
#include <set>

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

using namespace kick_project;

struct FakeFs
{
    std::set<std::string> folders;
    bool                  templateOk = true, makeOk = true, copyOk = true;
    int                   removed    = 0;
    bool exists( const std::string& p ) { return folders.count( p ) > 0; }
    bool hasTemplateProject() { return templateOk; }
    bool makeFolder( const std::string& p ) { if( !makeOk ) return false; folders.insert( p ); return true; }
    bool copyTemplateInto( const std::string& ) { return copyOk; }
    void removeFolder( const std::string& p ) { folders.erase( p ); ++removed; }
};

int main()
{
    CHECK( trimName( "  my song \t" ) == "my song" );
    CHECK( trimName( "   " ).empty() );
    CHECK( trimName( "" ).empty() );
    CHECK( trimName( "a b" ) == "a b" );

    CHECK( checkName( "My Song 01" ) == NameProblem::Ok );
    CHECK( checkName( "dal-2_x.v2" ) == NameProblem::Ok );
    CHECK( checkName( "" ) == NameProblem::Empty );
    CHECK( checkName( std::string( 65, 'a' ) ) == NameProblem::TooLong );
    CHECK( checkName( std::string( 64, 'a' ) ) == NameProblem::Ok );
    for( const char* bad : { "a/b", "a\\b", "a:b", "a*b", "a?b", "a\"b", "a<b", "a>b", "a|b", "../x", "x\x01y", "x\x7f", "name." } )
        CHECK( checkName( bad ) == NameProblem::BadCharacter );
    CHECK( checkName( "." ) == NameProblem::DotsOnly );
    CHECK( checkName( ".." ) == NameProblem::DotsOnly );
    CHECK( checkName( "..." ) == NameProblem::DotsOnly );
    CHECK( checkName( ".hidden" ) == NameProblem::Ok );
    for( auto p : { NameProblem::Empty, NameProblem::TooLong, NameProblem::BadCharacter, NameProblem::DotsOnly } )
        CHECK( describe( p )[0] != 0 );

    // Template -> project.
    {
        FakeFs fs;
        std::string folder;
        CHECK( createFromTemplate( "  Beat 1 ", "/data/projects", fs, &folder ) == CreateResult::Ok );
        CHECK( folder == "/data/projects/Beat 1" && fs.exists( folder ) );
        // The same name again: refused, nothing overwritten or removed.
        CHECK( createFromTemplate( "Beat 1", "/data/projects", fs ) == CreateResult::AlreadyExists );
        CHECK( fs.removed == 0 && fs.exists( folder ) );
    }
    {   // A folder that exists on disk but is not on any recent list is still a collision.
        FakeFs fs;
        fs.folders.insert( "/data/projects/Old" );
        CHECK( createFromTemplate( "Old", "/data/projects", fs ) == CreateResult::AlreadyExists );
    }
    {   // Bad / empty names touch nothing.
        FakeFs fs;
        CHECK( createFromTemplate( "", "/r", fs ) == CreateResult::BadName );
        CHECK( createFromTemplate( "   ", "/r", fs ) == CreateResult::BadName );
        CHECK( createFromTemplate( "../evil", "/r", fs ) == CreateResult::BadName );
        CHECK( fs.folders.empty() && fs.removed == 0 );
    }
    {   // No .kickPrj in the template: no folder is created.
        FakeFs fs;
        fs.templateOk = false;
        CHECK( createFromTemplate( "x", "/r", fs ) == CreateResult::NoTemplateFile );
        CHECK( fs.folders.empty() );
    }
    {   // Copy fails: the folder this call made is removed again.
        FakeFs fs;
        fs.copyOk = false;
        CHECK( createFromTemplate( "x", "/r", fs ) == CreateResult::CopyFailed );
        CHECK( fs.folders.empty() && fs.removed == 1 );
    }
    {   // Folder cannot be made: nothing to clean up.
        FakeFs fs;
        fs.makeOk = false;
        CHECK( createFromTemplate( "x", "/r", fs ) == CreateResult::CopyFailed );
        CHECK( fs.removed == 0 );
    }
    for( auto r : { CreateResult::BadName, CreateResult::AlreadyExists, CreateResult::NoTemplateFile, CreateResult::CopyFailed } )
        CHECK( describe( r )[0] != 0 );

    if( g_fail == 0 ) std::printf( "ProjectNamesTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
