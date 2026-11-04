// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test of "new project from a template" against a REAL temp directory (PRJ-23): name collision (the existing project is not
// touched), empty / invalid name (nothing is created), a template without a project file, a copy that fails (the half-made folder
// is removed, a neighbour project survives), a folder that cannot be made, and the start-up project name on a real folder.
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t ProjectTemplateFsTests.cpp
#include "../../modules/kick_ui/recent_project/kick_project_names.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using namespace kick_project;

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

static void        write( const fs::path& p, const std::string& text ) { fs::create_directories( p.parent_path() ); std::ofstream( p ) << text; }
static std::string read( const fs::path& p ) { std::ifstream f( p ); std::stringstream ss; ss << f.rdbuf(); return ss.str(); }

// The file system behind createFromTemplate, as kick_RecentProject.cpp builds it with juce::File (copy the template's project file into
// the new folder under <name>.kickPrj).
struct RealFs
{
    fs::path templateProject; // empty = the template folder has no .kickPrj
    fs::path templateFolder;
    bool exists( const std::string& p ) { return fs::exists( p ); }
    bool hasTemplateProject() { return !templateProject.empty(); }
    bool makeFolder( const std::string& p ) { std::error_code ec; fs::create_directory( p, ec ); return !ec && fs::is_directory( p ); }
    bool copyTemplateInto( const std::string& folder )
    {
        std::error_code ec;
        const std::string name = fs::path( folder ).filename().string();
        if( !fs::copy_file( templateProject, fs::path( folder ) / ( name + ".kickPrj" ), fs::copy_options::none, ec ) )
            return false;
        return !ec;
    }
    void removeFolder( const std::string& p ) { std::error_code ec; fs::remove_all( p, ec ); }
};

int main()
{
    const fs::path root = fs::temp_directory_path() / "kick_project_template_test";
    fs::remove_all( root );
    const fs::path projects = root / "projects", templ = root / "templates" / "Empty";
    fs::create_directories( projects );
    write( templ / "Empty.kickPrj", "<PROJECT template=\"1\"/>" );
    write( projects / "Beat" / "Beat.kickPrj", "<PROJECT mine=\"yes\"/>" );
    write( projects / "Beat" / "notes.txt", "keep" );

    RealFs good{ templ / "Empty.kickPrj", templ };
    std::string folder;

    // Success: folder + <name>.kickPrj with the template's content; the name is trimmed.
    CHECK( createFromTemplate( "  My Song  ", projects.string(), good, &folder ) == CreateResult::Ok );
    CHECK( folder == ( projects / "My Song" ).string() );
    CHECK( fs::is_regular_file( projects / "My Song" / "My Song.kickPrj" ) && read( projects / "My Song" / "My Song.kickPrj" ) == "<PROJECT template=\"1\"/>" );
    CHECK( fs::is_regular_file( templ / "Empty.kickPrj" ) ); // the template is untouched

    // Name collision: the existing project is not touched or merged, nothing new appears.
    folder.clear();
    CHECK( createFromTemplate( "Beat", projects.string(), good, &folder ) == CreateResult::AlreadyExists );
    CHECK( folder.empty() && read( projects / "Beat" / "Beat.kickPrj" ) == "<PROJECT mine=\"yes\"/>" && read( projects / "Beat" / "notes.txt" ) == "keep" );
    CHECK( createFromTemplate( " My Song", projects.string(), good ) == CreateResult::AlreadyExists ); // the trimmed name collides

    // Empty / invalid names: nothing is created.
    {
        size_t before = 0;
        for( auto& e : fs::directory_iterator( projects ) ) { (void)e; ++before; }
        for( const char* bad : { "", "   ", "a/b", "a:b", "..", ".", "x\x01", "name." } )
            CHECK( createFromTemplate( bad, projects.string(), good ) == CreateResult::BadName );
        CHECK( createFromTemplate( std::string( 65, 'z' ), projects.string(), good ) == CreateResult::BadName );
        size_t after = 0;
        for( auto& e : fs::directory_iterator( projects ) ) { (void)e; ++after; }
        CHECK( before == after );
    }

    // A template without a project file (the folder has no .kickPrj): refused before anything is made.
    {
        RealFs noFile{ {}, root / "templates" / "Broken" };
        CHECK( createFromTemplate( "FromBroken", projects.string(), noFile ) == CreateResult::NoTemplateFile );
        CHECK( !fs::exists( projects / "FromBroken" ) );
    }

    // The copy fails (the template file vanished after the check): the folder this call made is removed, the neighbours stay.
    {
        RealFs vanished{ templ / "Gone.kickPrj", templ };
        CHECK( createFromTemplate( "HalfMade", projects.string(), vanished ) == CreateResult::CopyFailed );
        CHECK( !fs::exists( projects / "HalfMade" ) );
        CHECK( fs::is_directory( projects / "Beat" ) && fs::is_directory( projects / "My Song" ) );
    }

    // The folder cannot be made (the root is a file): CopyFailed, nothing else created.
    {
        write( root / "file_as_root", "x" );
        CHECK( createFromTemplate( "Anything", ( root / "file_as_root" ).string(), good ) == CreateResult::CopyFailed );
        CHECK( fs::is_regular_file( root / "file_as_root" ) );
    }

    // Two creations in a row keep both; the second name collides only with itself.
    CHECK( createFromTemplate( "Second", projects.string(), good ) == CreateResult::Ok );
    CHECK( createFromTemplate( "Second", projects.string(), good ) == CreateResult::AlreadyExists );
    CHECK( fs::is_directory( projects / "My Song" ) && fs::is_directory( projects / "Second" ) );

    // Start-up project name against the real folder: free name, then " 2", " 3".
    {
        const auto exists = [&]( const std::string& n ) { return fs::exists( projects / n ); };
        const std::string a = launchProjectName( "", "2026-11-04 15.30", exists );
        CHECK( a == "Project 2026-11-04 15.30" );
        fs::create_directories( projects / a );
        const std::string b = launchProjectName( "", "2026-11-04 15.30", exists );
        CHECK( b == a + " 2" );
        fs::create_directories( projects / b );
        CHECK( launchProjectName( "", "2026-11-04 15.30", exists ) == a + " 3" );
        CHECK( checkName( a ) == NameProblem::Ok );
    }

    fs::remove_all( root );
    if( g_fail == 0 ) std::printf( "ProjectTemplateFsTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
