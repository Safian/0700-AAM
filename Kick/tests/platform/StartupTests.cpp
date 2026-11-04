// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the startup state machine, safe-mode guard, status strip data and project load queue.
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t StartupTests.cpp && /tmp/t
#include "../../modules/common/source/kick_startup_state.h"
#include "../../modules/common/source/kick_boot_guard.h"
#include "../../modules/common/source/kick_status_strip.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>
#include <unistd.h>

static int g_fail = 0;
#define CHECK( cond )                                                  \
    do {                                                               \
        if( !( cond ) ) {                                              \
            std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond ); \
            ++g_fail;                                                  \
        }                                                              \
    } while( 0 )
#define CHECK_STR( a, b )                                                                                   \
    do {                                                                                                    \
        const std::string sa = ( a ), sb = ( b );                                                           \
        if( sa != sb ) {                                                                                    \
            std::printf( "FAIL %s:%d  %s = '%s', expected '%s'\n", __FILE__, __LINE__, #a, sa.c_str(), sb.c_str() ); \
            ++g_fail;                                                                                       \
        }                                                                                                   \
    } while( 0 )

using namespace kick_startup;

static void test_state_machine()
{
    StartupState s;
    CHECK( !s.isComplete() ); // all pending
    s.set( Step::AudioEngine, StepState::Ok, "ALSA" );
    s.set( Step::Midi, StepState::Ok );
    s.set( Step::Display, StepState::Skipped );
    CHECK( !s.isComplete() ); // project still pending
    s.set( Step::Project, StepState::Running, "Demo 01" );
    CHECK( !s.isComplete() ); // running is not final
    s.set( Step::Project, StepState::Ok );
    CHECK( s.isComplete() );
    CHECK( s.info( Step::AudioEngine ).detail == "ALSA" );

    const auto before = s.snapshot().revision;
    s.set( Step::Midi, StepState::Running );
    CHECK( s.snapshot().revision > before );
    CHECK( !s.isComplete() );
}

static void test_failures_and_dialog_queue()
{
    StartupState s;
    for( int i = 0; i < (int)Step::Count; ++i ) s.set( (Step)i, StepState::Ok );
    const auto p = s.fail( Fault::Framebuffer, "open /dev/fb0: No such file" );
    CHECK( s.info( Step::Display ).state == StepState::Failed );
    CHECK( s.info( Step::Display ).detail == "open /dev/fb0: No such file" );
    CHECK( p.cause.find( "/dev/fb0" ) != std::string::npos ); // the raw reason is shown
    CHECK( !p.action.empty() );                               // and what to do
    s.fail( Fault::ProjectLoad, "Demo 01" );
    Problem got;
    CHECK( s.takeUnshownProblem( got ) && got.fault == Fault::Framebuffer );
    CHECK( s.takeUnshownProblem( got ) && got.fault == Fault::ProjectLoad );
    CHECK( !s.takeUnshownProblem( got ) ); // every problem is handed out once
    CHECK( s.isComplete() );               // failed is a final state: the startup screen must not wait forever
    CHECK( s.snapshot().problems.size() == 2 );

    for( Fault f : { Fault::AudioDevice, Fault::AudioDeviceMissing, Fault::MidiPort, Fault::Touch, Fault::Framebuffer, Fault::ProjectLoad, Fault::SafeMode } )
    {
        const auto q = describeProblem( f, "x" );
        CHECK( !q.title.empty() && !q.cause.empty() && !q.action.empty() );
    }
    CHECK( stepOfFault( Fault::Touch ) == Step::Display );
    CHECK( stepOfFault( Fault::MidiPort ) == Step::Midi );
}

static void test_step_lines()
{
    CHECK_STR( stepLine( Step::AudioEngine, { StepState::Ok, "ALSA" } ), "Audio engine... OK (ALSA)" );
    CHECK_STR( stepLine( Step::Midi, { StepState::Failed, "no port" } ), "MIDI... FAILED: no port" );
    CHECK_STR( stepLine( Step::Display, { StepState::Skipped, "" } ), "Display... skipped" );
    CHECK_STR( stepLine( Step::Project, { StepState::Running, "loading Demo 01" } ), "Project... loading Demo 01" );
    CHECK_STR( stepLine( Step::Project, { StepState::Pending, "" } ), "Project..." );
}

static void test_threads()
{
    StartupState s;
    std::thread a( [&] { for( int i = 0; i < 2000; ++i ) s.set( Step::Midi, StepState::Running, std::to_string( i ) ); } );
    std::thread b( [&] { for( int i = 0; i < 2000; ++i ) s.fail( Fault::MidiPort, "e" ); } );
    for( int i = 0; i < 2000; ++i ) (void)s.snapshot();
    a.join();
    b.join();
    CHECK( s.snapshot().problems.size() == 2000 );
}

static void test_build_info()
{
    const std::string file = "VERSION=0.3.3-12-gabcdef\nGIT_COMMIT=0123456789abcdef0123\nBUILD_DATE=2026-11-04\nKICK_SHA256=ff\n";
    auto              b    = resolveBuildInfo( file, "0.3.3", "deadbee", "Nov  4 2026" );
    CHECK( b.fromInstallFile );
    CHECK_STR( b.version, "0.3.3-12-gabcdef" );
    CHECK_STR( b.commit, "0123456789ab" );
    CHECK_STR( b.text(), "0.3.3-12-gabcdef (0123456789ab) 2026-11-04" );

    b = resolveBuildInfo( "", "0.3.3", "deadbee", "Nov  4 2026" ); // no install file: what was compiled in
    CHECK( !b.fromInstallFile );
    CHECK_STR( b.text(), "0.3.3 (deadbee) Nov  4 2026" );
    b = resolveBuildInfo( "garbage\nno equals", "0.3.3", "", "" );
    CHECK_STR( b.text(), "0.3.3" );
    b = resolveBuildInfo( "VERSION=1.0\r\nGIT_COMMIT=abc\r\n", "", "", "" ); // CRLF from a Windows-edited stick
    CHECK_STR( b.text(), "1.0 (abc)" );
}

static std::string makeTmpDir()
{
    char tmpl[] = "/tmp/kick_startup_test_XXXXXX";
    const char* d = mkdtemp( tmpl );
    return d ? d : "";
}

static void test_boot_guard_pure()
{
    CHECK( !kick_boot::shouldEnterSafeMode( 0, true ) );
    CHECK( !kick_boot::shouldEnterSafeMode( 1, true ) );
    CHECK( kick_boot::shouldEnterSafeMode( 2, true ) );
    CHECK( kick_boot::shouldEnterSafeMode( 7, true ) );
    CHECK( !kick_boot::shouldEnterSafeMode( 5, false ) ); // no crash file: not a crash loop (power cuts, SIGKILL)
}

static void test_boot_guard_files()
{
    const auto dir = makeTmpDir();
    CHECK( !dir.empty() );
    const auto paths = kick_boot::Paths::inDir( dir );

    // start 1: clean
    auto d = kick_boot::onBoot( paths );
    CHECK( !d.safeMode && d.previousCount == 0 && d.counterWritten );
    CHECK( kick_boot::readCount( paths.countFile ) == 1 );
    // it crashes (the handler writes the crash file), start 2
    { std::ofstream( paths.crashFile ) << "signal=11\n"; }
    d = kick_boot::onBoot( paths );
    CHECK( !d.safeMode && d.previousCount == 1 );
    // crashes again, start 3 -> safe mode, with the crash file path
    d = kick_boot::onBoot( paths );
    CHECK( d.safeMode );
    CHECK_STR( d.crashPath, paths.crashFile );
    CHECK( !d.reason.empty() );
    CHECK( kick_boot::readCount( paths.countFile ) == 3 ); // still counting: a crash in safe mode keeps it safe
    d = kick_boot::onBoot( paths );
    CHECK( d.safeMode );

    // a stable run clears the counter; the next start is normal even though the old crash file is still there
    CHECK( kick_boot::markStable( paths ) );
    CHECK( !kick_boot::fileExists( paths.countFile ) );
    d = kick_boot::onBoot( paths );
    CHECK( !d.safeMode && d.previousCount == 0 && d.crashFileExists );
    CHECK( kick_boot::markStable( paths ) );
    CHECK( kick_boot::markStable( paths ) ); // idempotent

    // counter without crash file never triggers
    { std::ofstream( paths.countFile ) << "9\n"; }
    unlink( paths.crashFile.c_str() );
    d = kick_boot::onBoot( paths );
    CHECK( !d.safeMode && d.previousCount == 9 );

    // garbage and cap
    { std::ofstream( paths.countFile ) << "not a number"; }
    CHECK( kick_boot::readCount( paths.countFile ) == 0 );
    { std::ofstream( paths.countFile ) << "100000\n"; }
    CHECK( kick_boot::readCount( paths.countFile ) == kick_boot::kCounterCap );
    { std::ofstream( paths.countFile ) << "-3\n"; }
    CHECK( kick_boot::readCount( paths.countFile ) == 0 );

    // atomic write leaves no temp file, and replaces the content
    CHECK( kick_boot::writeFileAtomic( paths.countFile, "5\n" ) );
    CHECK( !kick_boot::fileExists( paths.countFile + ".tmp" ) );
    CHECK( kick_boot::readCount( paths.countFile ) == 5 );

    // unwritable directory: reported, not fatal
    const auto bad = kick_boot::onBoot( kick_boot::Paths::inDir( dir + "/does/not/exist" ) );
    CHECK( !bad.counterWritten && !bad.safeMode );

    unlink( paths.countFile.c_str() );
    rmdir( dir.c_str() );
}

static void test_install_state()
{
    CHECK_STR( kick_status::installStateFromLog( "" ), "n/a" );
    const std::string fresh =
        "[tkgl_bootstrap t] === Kick bootstrap starting ===\n"
        "[tkgl_bootstrap t] Kick is not installed at /data/Kick. Fresh installation required.\n"
        "[tkgl_bootstrap t] Installation/update finished successfully.\n"
        "[tkgl_bootstrap t] Preparing to launch Kick from internal flash (/data/Kick)...\n";
    CHECK_STR( kick_status::installStateFromLog( fresh ), "installed" );
    const std::string upd =
        fresh + "[tkgl_bootstrap t] === Kick bootstrap starting ===\n"
                "[tkgl_bootstrap t] Version difference detected: local '0.3.2' vs package '0.3.3'. Update/rollback required.\n"
                "[tkgl_bootstrap t] Installation/update finished successfully.\n";
    CHECK_STR( kick_status::installStateFromLog( upd ), "updated 0.3.2->0.3.3" );
    const std::string same =
        upd + "[tkgl_bootstrap t] === Kick bootstrap starting ===\n"
              "[tkgl_bootstrap t] Installed version matches package (0.3.3). Flash write skipped.\n";
    CHECK_STR( kick_status::installStateFromLog( same ), "unchanged" );
    const std::string err =
        same + "[tkgl_bootstrap t] === Kick bootstrap starting ===\n"
               "[tkgl_bootstrap t] Version difference detected: local 'a' vs package 'b'. Update/rollback required.\n"
               "[tkgl_bootstrap t] ERROR: SHA256 mismatch for Kick.new (expected x, got y)!\n";
    CHECK( kick_status::installStateFromLog( err ).rfind( "error: ERROR: SHA256", 0 ) == 0 || kick_status::installStateFromLog( err ).rfind( "error: ", 0 ) == 0 );
    // a tail that cut the start marker off still works
    CHECK_STR( kick_status::installStateFromLog( "Installed version matches package (1). Flash write skipped.\n" ), "unchanged" );
    CHECK_STR( kick_status::installStateFromLog( "something unrelated\n" ), "n/a" );
}

static void test_strip_format()
{
    CHECK_STR( kick_status::formatBytes( -1 ), "n/a" );
    CHECK_STR( kick_status::formatBytes( 512 ), "512 B" );
    CHECK_STR( kick_status::formatBytes( 1536 ), "1.5 KB" );
    CHECK_STR( kick_status::formatBytes( 3LL * 1024 * 1024 * 1024 ), "3.0 GB" );
    CHECK_STR( kick_status::formatBytes( 250LL * 1024 * 1024 ), "250 MB" );

    kick_status::StripInfo i;
    CHECK_STR( kick_status::formatStrip( i ), "MIDI: n/a   |   Audio: n/a   |   /data: n/a   |   Install: n/a" );
    i.midiDevices   = 2;
    i.audio         = "ALSA hw:0";
    i.dataFreeBytes = 1536;
    i.install       = "unchanged";
    CHECK_STR( kick_status::formatStrip( i ), "MIDI: 2   |   Audio: ALSA hw:0   |   /data: 1.5 KB free   |   Install: unchanged" );

    // unreadable paths are silent n/a
    const auto g = kick_status::gatherFsInfo( "/definitely/not/here" );
    CHECK( g.dataFreeBytes == -1 );
    CHECK_STR( g.install, "n/a" );
    CHECK( kick_status::readTail( "/definitely/not/here/x" ).empty() );
    const auto here = kick_status::gatherFsInfo( "/tmp" );
    CHECK( here.dataFreeBytes > 0 );
}

static void test_load_queue()
{
    using kick_load::LoadQueue;
    LoadQueue q;
    CHECK( !q.busy() );
    CHECK( q.request( "/p/a" ) == LoadQueue::Result::StartNow );
    CHECK( q.busy() );
    CHECK( q.request( "/p/a" ) == LoadQueue::Result::Duplicate ); // the double load of the same project
    CHECK( q.request( "/p/b" ) == LoadQueue::Result::Queued );
    CHECK( q.request( "/p/b" ) == LoadQueue::Result::Duplicate );
    CHECK( q.request( "/p/c" ) == LoadQueue::Result::Queued );
    CHECK( q.pending() == 2 );

    std::string next;
    CHECK( q.onFinished( next ) && next == "/p/b" ); // FIFO, still busy
    CHECK( q.busy() && q.current() == "/p/b" );
    CHECK( q.request( "/p/b" ) == LoadQueue::Result::Duplicate ); // running one is a duplicate too
    CHECK( q.onFinished( next ) && next == "/p/c" );
    CHECK( !q.onFinished( next ) ); // queue drained
    CHECK( !q.busy() );             // isProjectLoading can drop
    CHECK( q.request( "/p/a" ) == LoadQueue::Result::StartNow ); // finished path may be loaded again
}

static void test_startup_policy()
{
    using namespace kick_load;
    StartupInputs in;
    in.lastProject = "/data/projects/mine";
    in.lastExists  = true;
    in.fallbackProject = "/data/demos/Demo 01/Demo 01.kickPrj";
    in.fallbackExists  = true;

    in.autoLoad = true;
    auto c = chooseStartupProject( in );
    CHECK( c.path == in.lastProject && c.reason == StartupReason::LastProject ); // one load, not two

    in.autoLoad = false; // old behaviour: the bridge loaded Demo 01 regardless
    c = chooseStartupProject( in );
    CHECK( c.path == in.fallbackProject && c.reason == StartupReason::Fallback );

    in.autoLoad   = true;
    in.lastExists = false; // moved/deleted last project falls back, not to a dead end
    c = chooseStartupProject( in );
    CHECK( c.path == in.fallbackProject );

    in.fallbackExists = false;
    c = chooseStartupProject( in );
    CHECK( c.path.empty() && c.reason == StartupReason::NothingToLoad );
    in.autoLoad = false;
    CHECK( chooseStartupProject( in ).reason == StartupReason::AutoLoadOff );

    in.autoLoad = true; in.lastExists = true; in.fallbackExists = true; in.safeMode = true;
    c = chooseStartupProject( in );
    CHECK( c.path.empty() && c.reason == StartupReason::SafeMode ); // safe mode never loads
    in.fallbackProject.clear();
    in.safeMode = false; in.autoLoad = false;
    CHECK( chooseStartupProject( in ).path.empty() ); // desktop: no fallback
}

int main()
{
    test_state_machine();
    test_failures_and_dialog_queue();
    test_step_lines();
    test_threads();
    test_build_info();
    test_boot_guard_pure();
    test_boot_guard_files();
    test_install_state();
    test_strip_format();
    test_load_queue();
    test_startup_policy();
    if( g_fail == 0 )
        std::printf( "startup tests OK\n" );
    else
        std::printf( "startup tests FAILED: %d errors\n", g_fail );
    return g_fail ? 1 : 0;
}
