// SPDX-License-Identifier: AGPL-3.0-or-later
// Asset test of the mode menu (MENU-14): every icon the Menu registry names exists in each theme given on the command line
// (theme.xml -> <Image name=".." path="..">, and the image file next to it).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t MenuAssetTests.cpp && /tmp/t <theme.xml>...
#include "../../modules/kick_ui_components/source/kick_menu_logic.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

enum class S
{
    Invalid = -9999, MenuSampleEdit, MenuProgramEdit, MenuSampler, MenuLooper, MenuPadMute, MenuTrackMute, MenuPadMixer, MenuChannelMixer, MenuKnobEdit,
    MenuListEdit, MenuPadPerform, MenuMidiControl, MenuFileBrowser, MenuNextSequence, MenuSong, MenuColorEdit, MenuPiano
};

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

namespace fs = std::filesystem;

static std::string attr( const std::string& tag, const std::string& key )
{
    const auto k = tag.find( key + "=\"" );
    if( k == std::string::npos ) return {};
    const auto b = k + key.size() + 2;
    const auto e = tag.find( '"', b );
    return e == std::string::npos ? std::string() : tag.substr( b, e - b );
}

int main( int argc, char** argv )
{
    if( argc < 2 ) { std::printf( "usage: MenuAssetTests <theme.xml>...\n" ); return 2; }
    for( int a = 1; a < argc; ++a )
    {
        const fs::path theme = argv[a];
        std::ifstream  in( theme );
        CHECK( in.good() );
        std::stringstream ss;
        ss << in.rdbuf();
        const std::string text = ss.str();

        std::map<std::string, std::string> images; // name -> path
        for( size_t pos = text.find( "<Image" ); pos != std::string::npos; pos = text.find( "<Image", pos + 1 ) )
        {
            const auto end = text.find( '>', pos );
            if( end == std::string::npos ) break;
            const std::string tag = text.substr( pos, end - pos );
            images[attr( tag, "name" )] = attr( tag, "path" );
        }

        int checked = 0;
        for( int i = 0; i < ui::menu_logic::kMenuItemCount; ++i )
        {
            const auto& d = ui::menu_logic::tileDef<S>( i );
            if( !d.icon[0] ) continue; // text tile
            ++checked;
            const auto it = images.find( d.icon );
            if( it == images.end() ) { std::printf( "FAIL %s: icon '%s' of tile '%s' is not in the theme\n", theme.string().c_str(), d.icon, d.name ); ++g_fail; continue; }
            const fs::path file = theme.parent_path() / it->second;
            if( !fs::is_regular_file( file ) ) { std::printf( "FAIL %s: image file %s missing\n", theme.string().c_str(), file.string().c_str() ); ++g_fail; }
        }
        CHECK( checked == ui::menu_logic::kTileCount );
    }
    if( g_fail == 0 ) std::printf( "MenuAssetTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
