// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for Kick logging: timestamp, thread ID, category filtering, crash ring buffer, and rotation.
#include "../../modules/common/source/kick_logger.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

static int g_fail = 0;
static void check( bool ok, const char* what )
{
    if( !ok )
    {
        std::fprintf( stderr, "FAIL: %s\n", what );
        ++g_fail;
    }
}

static std::vector<std::string> read_lines( const std::string& path )
{
    std::vector<std::string> lines;
    std::ifstream file( path );
    std::string line;
    while( std::getline( file, line ) )
    {
        if( !line.empty() ) lines.push_back( line );
    }
    return lines;
}

int main()
{
    std::printf( "Starting LoggingTests...\n" );

    // 1. Uptime and Thread ID
    {
        const uint64_t t1 = kick_logger::get_uptime_ms();
        usleep( 5000 ); // 5 ms
        const uint64_t t2 = kick_logger::get_uptime_ms();
        check( t2 >= t1, "uptime is monotonically increasing" );

        const uint64_t tid = kick_logger::get_thread_id();
        check( tid > 0, "thread ID is non-zero" );
    }

    // 2. Category filtering
    {
        // Reset to default (empty KICK_LOG)
        unsetenv( "KICK_LOG" );
        kick_logger::init( nullptr, 0, false );

        check( kick_logger::is_category_enabled( "info" ), "default: info enabled" );
        check( kick_logger::is_category_enabled( "error" ), "default: error enabled" );
        check( kick_logger::is_category_enabled( "err" ), "default: err enabled" );
        check( kick_logger::is_category_enabled( "warn" ), "default: warn enabled" );
        check( kick_logger::is_category_enabled( "proj" ), "default: proj enabled" );
        check( !kick_logger::is_category_enabled( "touch" ), "default: touch disabled" );
        check( kick_logger::is_category_enabled( "cmd" ), "default: cmd enabled" );
        check( !kick_logger::is_category_enabled( "dial" ), "default: dial disabled" );
        check( kick_logger::is_category_enabled( "bridge" ), "default: bridge enabled" );
        check( !kick_logger::is_category_enabled( "frame" ), "default: frame disabled" );

        // Custom categories: touch, cmd, bridge, proj
        kick_logger::set_categories( "touch,cmd,bridge,proj" );
        check( kick_logger::is_category_enabled( "touch" ), "custom: touch enabled" );
        check( kick_logger::is_category_enabled( "cmd" ), "custom: cmd enabled" );
        check( kick_logger::is_category_enabled( "bridge" ), "custom: bridge enabled" );
        check( kick_logger::is_category_enabled( "proj" ), "custom: proj enabled" );
        check( kick_logger::is_category_enabled( "error" ), "custom: error always enabled" );
        check( !kick_logger::is_category_enabled( "info" ), "custom: info disabled" );
        check( !kick_logger::is_category_enabled( "frame" ), "custom: frame disabled" );

        // Case insensitivity and whitespace handling
        kick_logger::set_categories( " TOUCH ,  Cmd ; PROJ " );
        check( kick_logger::is_category_enabled( "touch" ), "case/space: touch enabled" );
        check( kick_logger::is_category_enabled( "TOUCH" ), "case/space: TOUCH enabled" );
        check( kick_logger::is_category_enabled( "cmd" ), "case/space: cmd enabled" );
        check( kick_logger::is_category_enabled( "proj" ), "case/space: proj enabled" );

        // Wildcard "all"
        kick_logger::set_categories( "all" );
        check( kick_logger::is_category_enabled( "touch" ), "all: touch enabled" );
        check( kick_logger::is_category_enabled( "cmd" ), "all: cmd enabled" );
        check( kick_logger::is_category_enabled( "bridge" ), "all: bridge enabled" );
        check( kick_logger::is_category_enabled( "info" ), "all: info enabled" );
        check( kick_logger::is_category_enabled( "proj" ), "all: proj enabled" );
        check( !kick_logger::is_category_enabled( "frame" ), "all: frame MUST BE DISABLED by default" );

        // Explicit "frame" enabling
        kick_logger::set_categories( "all,frame" );
        check( kick_logger::is_category_enabled( "frame" ), "all,frame: frame enabled" );

        kick_logger::set_categories( "frame" );
        check( kick_logger::is_category_enabled( "frame" ), "frame only: frame enabled" );
        check( !kick_logger::is_category_enabled( "touch" ), "frame only: touch disabled" );
    }

    // 3. Crash Ring Buffer (async-signal-safe dump)
    {
        kick_logger::g_crash_buffer.clear();
        kick_logger::set_categories( "all,frame" );

        const std::string crash_path = "/tmp/test_kick_crash.log";
        ::unlink( crash_path.c_str() );

        // Log 60 lines with sequential numbers
        for( int i = 1; i <= 60; ++i )
        {
            kick_log( "info", "Crash buffer test line %03d", i );
        }

        // Dump async-signal-safe
        kick_logger::dump_crash_log_async_signal_safe( crash_path.c_str() );

        auto lines = read_lines( crash_path );
        check( lines.size() == 50, "crash dump contains exactly 50 lines" );
        if( lines.size() == 50 )
        {
            // Oldest line in 50-line buffer should be line 11
            check( lines.front().find( "line 011" ) != std::string::npos, "first dumped line is line 11" );
            // Newest line in buffer should be line 60
            check( lines.back().find( "line 060" ) != std::string::npos, "last dumped line is line 60" );

            // Check formatting: begins with "[   ...ms] [t:..."
            check( lines.front().find( "ms] [t:" ) != std::string::npos, "line has [ms] and [t:tid] prefix" );
        }
        ::unlink( crash_path.c_str() );
    }

    // 4. Log Rotation: runtime (max size) and startup rotation
    {
        const std::string log_file = "/tmp/test_kick_rotation.log";
        const std::string rot_file = log_file + ".1";
        ::unlink( log_file.c_str() );
        ::unlink( rot_file.c_str() );

        // Initialize with a small max size: 400 bytes
        kick_logger::set_categories( "all" );
        kick_logger::init( log_file.c_str(), 400, false );

        // Write several lines to exceed 400 bytes
        // Each formatted line is ~45-50 bytes, so 15 lines is ~700 bytes
        for( int i = 1; i <= 15; ++i )
        {
            kick_log( "info", "Rotation test line message number %02d", i );
        }

        // Verify rotation occurred: rot_file should exist!
        struct stat st_rot {};
        check( ::stat( rot_file.c_str(), &st_rot ) == 0 && st_rot.st_size > 0, "rotated file .1 exists and is non-empty" );

        struct stat st_log {};
        check( ::stat( log_file.c_str(), &st_log ) == 0 && st_log.st_size > 0, "active log file exists and is non-empty" );

        kick_logger::shutdown();

        // 5. Startup rotation: when starting up with existing log_file, it rotates to .1
        const size_t prev_log_size = st_log.st_size;
        kick_logger::init( log_file.c_str(), 10000, false );

        // Check that rot_file now has the contents of the previous log_file
        struct stat st_rot2 {};
        check( ::stat( rot_file.c_str(), &st_rot2 ) == 0 && st_rot2.st_size == (off_t)prev_log_size,
               "startup rotation: previous log moved to .1" );

        // Active log file should be newly opened (empty until new writes)
        struct stat st_log2 {};
        check( ::stat( log_file.c_str(), &st_log2 ) == 0 && st_log2.st_size == 0,
               "startup rotation: active log restarted fresh" );

        kick_log( "info", "Fresh run after startup rotation" );
        check( ::stat( log_file.c_str(), &st_log2 ) == 0 && st_log2.st_size > 0,
               "new log line written to fresh active log" );

        kick_logger::shutdown();
        ::unlink( log_file.c_str() );
        ::unlink( rot_file.c_str() );
    }

    // 5. Step logs (INFO / PROJ): project, program, asset load/close with name & duration
    {
        const std::string step_log_file = "/tmp/test_kick_steplog.log";
        ::unlink( step_log_file.c_str() );

        // Test with default categories (info, error, warn, proj enabled)
        unsetenv( "KICK_LOG" );
        kick_logger::init( step_log_file.c_str(), 100000, false );

        // Simulate project load begin & end
        const auto p_start = kick_logger::get_uptime_ms();
        kick_log( "info", "[INFO] [PROJ] Project load begin: '%s'\n", "Demo 01.kickPrj" );
        usleep( 2000 ); // 2 ms
        kick_log( "info", "[INFO] [PROJ] Project load end: '%s' (success, took %llu ms, %zu programs)\n",
                  "Demo 01.kickPrj", static_cast<unsigned long long>( kick_logger::get_uptime_ms() - p_start ), (size_t)4 );

        // Simulate program load begin & end
        const auto pr_start = kick_logger::get_uptime_ms();
        kick_log( "info", "[INFO] [PROJ] Program load begin: '%s' (type: %d)\n", "Drum 01", 0 );
        usleep( 1000 );
        kick_log( "info", "[INFO] [PROJ] Program load end: '%s' (took %llu ms, success=%d)\n",
                  "Drum 01", static_cast<unsigned long long>( kick_logger::get_uptime_ms() - pr_start ), 1 );

        // Simulate asset load begin & end
        const auto a_start = kick_logger::get_uptime_ms();
        kick_log( "info", "[INFO] [PROJ] Asset load begin: '%s' (type: %d)\n", "Kick.wav", 0 );
        usleep( 1000 );
        kick_log( "info", "[INFO] [PROJ] Asset load end: '%s' (took %llu ms)\n",
                  "Kick.wav", static_cast<unsigned long long>( kick_logger::get_uptime_ms() - a_start ) );

        // Simulate project close begin & end
        const auto c_start = kick_logger::get_uptime_ms();
        kick_log( "info", "[INFO] [PROJ] Project close begin: '%s'\n", "Demo 01.kickPrj" );
        usleep( 1000 );
        kick_log( "info", "[INFO] [PROJ] Project close end: '%s' (took %llu ms)\n",
                  "Demo 01.kickPrj", static_cast<unsigned long long>( kick_logger::get_uptime_ms() - c_start ) );

        kick_logger::shutdown();

        auto lines = read_lines( step_log_file );
        check( lines.size() == 8, "8 step log lines were written" );
        if( lines.size() == 8 )
        {
            check( lines[0].find( "[INFO] [PROJ] Project load begin: 'Demo 01.kickPrj'" ) != std::string::npos, "proj load begin" );
            check( lines[1].find( "[INFO] [PROJ] Project load end: 'Demo 01.kickPrj' (success" ) != std::string::npos, "proj load end" );
            check( lines[2].find( "[INFO] [PROJ] Program load begin: 'Drum 01'" ) != std::string::npos, "prog load begin" );
            check( lines[3].find( "[INFO] [PROJ] Program load end: 'Drum 01'" ) != std::string::npos, "prog load end" );
            check( lines[4].find( "[INFO] [PROJ] Asset load begin: 'Kick.wav'" ) != std::string::npos, "asset load begin" );
            check( lines[5].find( "[INFO] [PROJ] Asset load end: 'Kick.wav'" ) != std::string::npos, "asset load end" );
            check( lines[6].find( "[INFO] [PROJ] Project close begin: 'Demo 01.kickPrj'" ) != std::string::npos, "proj close begin" );
            check( lines[7].find( "[INFO] [PROJ] Project close end: 'Demo 01.kickPrj'" ) != std::string::npos, "proj close end" );
        }
        ::unlink( step_log_file.c_str() );

        // Verify that setting KICK_LOG="touch,cmd" filters out step logs
        ::unlink( step_log_file.c_str() );
        setenv( "KICK_LOG", "touch,cmd", 1 );
        kick_logger::init( step_log_file.c_str(), 100000, false );
        kick_log( "info", "[INFO] [PROJ] Project load begin: '%s'\n", "Ignored.kickPrj" );
        kick_logger::shutdown();
        unsetenv( "KICK_LOG" );
        auto lines_filtered = read_lines( step_log_file );
        check( lines_filtered.empty(), "step logs silenced when categories are restricted to touch,cmd" );
        ::unlink( step_log_file.c_str() );
    }

    if( g_fail == 0 )
    {
        std::printf( "LoggingTests: ALL TESTS PASSED!\n" );
        return 0;
    }
    else
    {
        std::fprintf( stderr, "LoggingTests: %d TESTS FAILED!\n", g_fail );
        return 1;
    }
}
