// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for local network scanning and WebDAV discovery.
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t NetworkScannerTests.cpp && /tmp/t
#include "../../modules/kick_ui/modes/filebrowser/kick_networkScanner.h"
#include <cstdio>
#include <cassert>

using namespace fileSource::discovery;

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

int main()
{
    // 1. Local IP extraction
    std::string lip = getLocalIp();
    std::printf( "Local IP discovered: %s\n", lip.c_str() );
    CHECK( !lip.empty() );

    // 2. Closed port probe should cleanly fail and timeout without crashing
    DiscoveredServer ds;
    bool closedResult = probeEndpoint( "127.0.0.1", 65432, 50, ds );
    CHECK( !closedResult );

    // 3. Active WebDAV server probe (if 127.0.0.1:8080 or local network WebDAV is running)
    bool local8080 = probeEndpoint( "127.0.0.1", 8080, 200, ds );
    if( local8080 )
    {
        std::printf( "Discovered local WebDAV: %s at %s\n", ds.name.c_str(), ds.url.c_str() );
        CHECK( ds.url == "http://127.0.0.1:8080" );
        CHECK( !ds.name.empty() );
    }

    // 4. scanLocalWebDavServers scan test
    auto servers = scanLocalWebDavServers( 250 );
    std::printf( "scanLocalWebDavServers found %zu servers\n", servers.size() );
    for( auto& s : servers )
    {
        std::printf( "  Found: %s -> %s\n", s.name.c_str(), s.url.c_str() );
        CHECK( !s.url.empty() );
        CHECK( !s.name.empty() );
    }

    std::printf( "NetworkScannerTests: %s (%d failures)\n", g_fail == 0 ? "PASSED" : "FAILED", g_fail );
    return g_fail != 0 ? 1 : 0;
}
