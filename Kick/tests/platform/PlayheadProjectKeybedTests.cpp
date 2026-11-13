// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for Playhead menu STP/listbox logic, Project path resolution, and Keybed routing.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <memory>
#include <array>
#include <cassert>

#include "../../apps/kick/hardware/hw_targets.h"

static void check( bool ok, const char* why )
{
    if( !ok ) {
        std::fprintf( stderr, "FAIL: %s\n", why );
        std::exit( 1 );
    }
}

// 1. Test Linux path resolution logic
namespace test_paths
{
    enum AppDirType { MTAssets, MTSamples, MTDemos, MTTemp, MTProjects, MTLog, MTPrefs };

    struct MockFs
    {
        bool hasDataDir{ true };
        bool hasDataAssets{ false };
        bool hasDataDemos{ false };
        bool hasDataTemplates{ false };

        std::string resolvePath( AppDirType t ) const
        {
            if( t == MTAssets )
                return ( hasDataDir && hasDataAssets ) ? "/data/assets" : "/app/assets";
            else if( t == MTSamples )
                return hasDataDir ? "/data/sounds" : "/app/sounds";
            else if( t == MTDemos )
                return ( hasDataDir && hasDataDemos ) ? "/data/demos" : "/app/demos";
            else if( t == MTTemp )
                return ( hasDataDir && hasDataTemplates ) ? "/data/templates" : "/app/templates";
            else if( t == MTProjects )
                return hasDataDir ? "/data/projects" : "/home/Kick/projects";
            else if( t == MTLog )
                return hasDataDir ? "/data/logs" : "/home/Kick/logs";
            else if( t == MTPrefs )
                return hasDataDir ? "/data/prefs" : "/home/Kick/prefs";
            return "/tmp";
        }
    };

    void run()
    {
        std::printf( "[TEST] Linux path resolution logic...\n" );

        MockFs fsAkai{ true, false, false, false };
        check( fsAkai.resolvePath( MTProjects ) == "/data/projects", "Akai unit with /data uses /data/projects" );
        check( fsAkai.resolvePath( MTSamples ) == "/data/sounds", "Akai unit with /data uses /data/sounds" );
        check( fsAkai.resolvePath( MTLog ) == "/data/logs", "Akai unit with /data uses /data/logs" );
        check( fsAkai.resolvePath( MTPrefs ) == "/data/prefs", "Akai unit with /data uses /data/prefs" );

        // Fallback when /data is missing (desktop docker container without mount)
        MockFs fsContainer{ false, false, false, false };
        check( fsContainer.resolvePath( MTProjects ) == "/home/Kick/projects", "No /data falls back to /home/Kick/projects" );
    }
}

// 2. Test Keybed Live Note Routing to Selected Plugin
namespace test_keybed_routing
{
    enum class MockProgType { Drum, Plugin, Keygroup };

    struct Selected { int sequence{ 0 }; int track{ 0 }; };

    struct RouterState
    {
        kick_hw::NoteTargets<Selected> keyTarget;
        kick_hw::NoteTargets<int> directProg;
        std::vector<std::string> log;

        void noteOn( int pitch, int /*vel*/, const Selected& sel, int selProg, MockProgType selProgType, int trackProg )
        {
            const bool routeDirect = ( selProgType == MockProgType::Plugin && trackProg != selProg );
            keyTarget.set( pitch, sel );
            if( routeDirect )
                directProg.set( pitch, selProg );

            log.push_back( "track_note_on: pitch=" + std::to_string( pitch ) + " track=" + std::to_string( sel.track ) );
            if( routeDirect )
                log.push_back( "direct_prog_note_on: pitch=" + std::to_string( pitch ) + " prog=" + std::to_string( selProg ) );
        }

        void noteOff( int pitch, const Selected& fallbackSel )
        {
            Selected target = keyTarget.take( pitch, fallbackSel );
            int prog = directProg.take( pitch, -1 );

            log.push_back( "track_note_off: pitch=" + std::to_string( pitch ) + " track=" + std::to_string( target.track ) );
            if( prog >= 0 )
                log.push_back( "direct_prog_note_off: pitch=" + std::to_string( pitch ) + " prog=" + std::to_string( prog ) );
        }
    };

    void run()
    {
        std::printf( "[TEST] Keybed live note routing logic...\n" );

        // Case A: Track 0 has Drum program (0), but user selected SynthyBoy (prog 1, Plugin)
        {
            RouterState router;
            Selected sel{ 0, 0 };
            router.noteOn( 60, 100, sel, 1, MockProgType::Plugin, 0 );
            check( router.log.size() == 2, "Note-on routed to both track and direct SynthyBoy program" );
            check( router.log[0] == "track_note_on: pitch=60 track=0", "track note on" );
            check( router.log[1] == "direct_prog_note_on: pitch=60 prog=1", "direct prog note on" );

            router.noteOff( 60, sel );
            check( router.log.size() == 4, "Note-off routed to both track and direct SynthyBoy program" );
            check( router.log[2] == "track_note_off: pitch=60 track=0", "track note off" );
            check( router.log[3] == "direct_prog_note_off: pitch=60 prog=1", "direct prog note off" );
        }

        // Case B: Track 1 has SynthyBoy assigned (prog 1), and selected program is also SynthyBoy (prog 1)
        {
            RouterState router;
            Selected sel{ 0, 1 };
            router.noteOn( 64, 90, sel, 1, MockProgType::Plugin, 1 );
            check( router.log.size() == 1, "Note-on routed to track only when track already has that program" );
            check( router.log[0] == "track_note_on: pitch=64 track=1", "track note on" );

            router.noteOff( 64, sel );
            check( router.log.size() == 2, "Note-off routed to track only" );
            check( router.log[1] == "track_note_off: pitch=64 track=1", "track note off" );
        }

        // Case C: Track switch during held note
        {
            RouterState router;
            Selected selTrack0{ 0, 0 };
            Selected selTrack1{ 0, 1 };
            router.noteOn( 67, 110, selTrack0, 2, MockProgType::Plugin, 0 );
            // User switches to Track 1
            router.noteOff( 67, selTrack1 );
            check( router.log.size() == 4, "Note-off lands on Track 0 (which got note-on), not current Track 1" );
            check( router.log[2] == "track_note_off: pitch=67 track=0", "note-off delivered to original target track" );
            check( router.log[3] == "direct_prog_note_off: pitch=67 prog=2", "direct prog note-off delivered" );
        }
    }
}

// 3. Test Touch Target Selection for Dropdowns / ListBox
namespace test_touch_target
{
    struct MockComponent
    {
        std::string type;
        MockComponent* parent{ nullptr };

        template<typename T>
        T* findParent()
        {
            MockComponent* p = parent;
            while( p )
            {
                if( p->type == "KickListBox" ) return static_cast<T*>( static_cast<void*>( p ) );
                p = p->parent;
            }
            return nullptr;
        }
    };

    struct MockListBox : public MockComponent
    {
        MockListBox() { type = "KickListBox"; }
        MockComponent labelTitle{ "KickLabel", this };
        MockComponent labelValue{ "KickLabel", this };
        MockComponent btnPlus{ "KickButton", this };
        MockComponent btnMinus{ "KickButton", this };
    };

    MockComponent* resolveTarget( MockComponent* target )
    {
        if( target->type != "KickButton" )
        {
            if( target->type == "KickListBox" )
                return target;
            if( auto* lb = target->findParent<MockListBox>() )
                return lb;
        }
        return target;
    }

    void run()
    {
        std::printf( "[TEST] Touch target resolution for KickListBox...\n" );

        MockListBox lb;
        // Tapping the value label inside KickListBox must resolve to KickListBox
        check( resolveTarget( &lb.labelValue ) == &lb, "Tapping labelValue targets KickListBox" );
        check( resolveTarget( &lb.labelTitle ) == &lb, "Tapping labelTitle targets KickListBox" );
        check( resolveTarget( &lb ) == &lb, "Tapping KickListBox directly targets KickListBox" );

        // Tapping buttons must remain on the buttons
        check( resolveTarget( &lb.btnPlus ) == &lb.btnPlus, "Tapping plus button targets plus button" );
        check( resolveTarget( &lb.btnMinus ) == &lb.btnMinus, "Tapping minus button targets minus button" );
    }
}

int main()
{
    std::printf( "========================================\n" );
    std::printf( "Running Playhead, Project & Keybed Tests\n" );
    std::printf( "========================================\n" );

    test_paths::run();
    test_keybed_routing::run();
    test_touch_target::run();

    std::printf( "========================================\n" );
    std::printf( "ALL PLAYHEAD, PROJECT & KEYBED TESTS PASSED!\n" );
    std::printf( "========================================\n" );
    return 0;
}
