/*
 * Copyright (c) 2022 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "kick_ui/kick_ui.h"
#include "kick_ui/kick_ui_grid.h"
#include "program_edit_ui/program_edit_ui.h"
#include <kick_clap/PluginManager.h>
#include "hardware/source/kick_PlatformServices.h"
#include "audio_engine/audio_engine.h"
#include "kick_device_adapter.h"
#include "kick_audio_engine_adapter.h"
#include "kick_audio_interface.h"
#include "kick_fallback_display.h"
#if JUCE_LINUX || __linux__
 #include "kick_hardware_bridge.h"
 #include "hardware/power_key_watcher.h"
#endif
#if KICK_APP_SMOKE_TEST
 #include "../../tests/platform/AppSmoke.h"
#endif
#include "contracts/utils/keyboard/keyboard_detector.h"
#include "common/source/kick_logger.h"
#if JUCE_LINUX || defined(__APPLE__)
 #include <execinfo.h>
#endif
#if JUCE_LINUX
 #include <execinfo.h>
 #include <dlfcn.h>
 #include <sys/mman.h>
 #include <fcntl.h>
 #include <signal.h>
 #include <ucontext.h>
 #include <unistd.h>
#endif

#ifdef PERFETTO
    #include "melatonin_perfetto/melatonin_perfetto.h"
#endif

#define ENABLE_X11_FULLSCREEN ( 0 )

#if defined( ENABLE_X11_FULLSCREEN ) && ( 0 < ENABLE_X11_FULLSCREEN )
    #if( JUCE_LINUX || __linux__ )
namespace xlib
{
    extern "C"
    {
        #include <X11/Xlib.h>
        #include <X11/Xatom.h>

        void maximizeWindow( Window win )
        {
            auto display = XOpenDisplay( NULL );

            XEvent ev;
            ev.xclient.window       = win;
            ev.xclient.type         = ClientMessage;
            ev.xclient.format       = 32;
            ev.xclient.message_type = XInternAtom( display, "_NET_WM_STATE", False );
            ev.xclient.data.l[0]    = 1;
            ev.xclient.data.l[1]    = XInternAtom( display, "_NET_WM_STATE_MAXIMIZED_HORZ", False );
            ev.xclient.data.l[2]    = XInternAtom( display, "_NET_WM_STATE_MAXIMIZED_VERT", False );
            ev.xclient.data.l[3]    = 1;

            XSendEvent( display, DefaultRootWindow( display ), False, SubstructureRedirectMask | SubstructureNotifyMask, &ev );
            XCloseDisplay( display );
        }

        void fullscreenWindow( Window win )
        {
            auto display = XOpenDisplay( NULL );

        #if 1
            XEvent ev;
            ev.xclient.window       = win;
            ev.xclient.type         = ClientMessage;
            ev.xclient.format       = 32;
            ev.xclient.message_type = XInternAtom( display, "_NET_WM_STATE", False );
            ev.xclient.data.l[0]    = 2; // _NET_WM_STATE_TOGGLE
            ev.xclient.data.l[1]    = XInternAtom( display, "_NET_WM_STATE_FULLSCREEN", True );
            ;
            ev.xclient.data.l[2] = 0; // No second property to toggle
            ev.xclient.data.l[3] = 1;

            XSendEvent( display, DefaultRootWindow( display ), False, SubstructureRedirectMask | SubstructureNotifyMask, &ev );
        #else
            Atom wm_state      = XInternAtom( display, "_NET_WM_STATE", true );
            Atom wm_fullscreen = XInternAtom( display, "_NET_WM_STATE_FULLSCREEN", true );

            XChangeProperty( display, win, wm_state, XA_ATOM, 32, PropModeReplace, (unsigned char*)&wm_fullscreen, 1 );
        #endif

            XCloseDisplay( display );
        }
    }
}
    #endif
#endif

class TestWindow : public juce::Component
{
public:
    TestWindow()
    {
        setSize( ui::layout::getAppWidth(), ui::layout::getAppHeight() );
    }
    ~TestWindow() {}

private:
    void paint( juce::Graphics& g )
    {
        g.fillAll( juce::Colours::coral );
    }
};

//==============================================================================
class AudioEngineApplication : public juce::JUCEApplication
{
    bool quitAsked{ false }, quitConfirmed{ false }; // unsaved-changes question on a quit request
public:
    //==============================================================================
    const juce::String getApplicationName() override
    {
        return "Kick";
    }

    const juce::String getApplicationVersion() override
    {
        return "0.0.1";
    }

    bool moreThanOneInstanceAllowed() override
    {
        return false;
    }

    //==============================================================================
    // Headless screenshots (smoke test only): KICK_SMOKE_SCREENS="HomeGridView,MenuPadMixer,..." and
    // KICK_SMOKE_SHOTS_DIR=<dir> open each kick_ui::Screen in turn and save <dir>/<Screen>.png (1.2 s settle time each).
    void smokeScreenShots( juce::StringArray names, juce::File dir, int index )
    {
        using kick_ui::Screen;
        static const std::pair<const char*, Screen> table[] = {
            { "HomeOptions", Screen::HomeOptions }, { "HomeListView", Screen::HomeListView },
            { "HomeGridView", Screen::HomeGridView }, { "HomeSequencerView", Screen::HomeSequencerView },
            { "HomeXYView", Screen::HomeXYView }, { "HomePreferencesView", Screen::HomePreferencesView },
            { "MenuSampleEdit", Screen::MenuSampleEdit }, { "MenuProgramEdit", Screen::MenuProgramEdit },
            { "MenuSampler", Screen::MenuSampler }, { "MenuAssignSample", Screen::MenuAssignSample }, { "MenuLooper", Screen::MenuLooper },
            { "MenuPadMute", Screen::MenuPadMute }, { "MenuTrackMute", Screen::MenuTrackMute },
            { "MenuPadMixer", Screen::MenuPadMixer }, { "MenuChannelMixer", Screen::MenuChannelMixer },
            { "MenuKnobEdit", Screen::MenuKnobEdit }, { "MenuListEdit", Screen::MenuListEdit },
            { "MenuPadPerform", Screen::MenuPadPerform }, { "MenuPiano", Screen::MenuPiano }, { "MenuMidiControl", Screen::MenuMidiControl },
            { "MenuFileBrowser", Screen::MenuFileBrowser }, { "MenuNextSequence", Screen::MenuNextSequence },
            { "MenuSong", Screen::MenuSong }, { "MenuColorEdit", Screen::MenuColorEdit },
            { "RecentProject", Screen::RecentProject }, { "KickUpdate", Screen::KickUpdate },
        };

        if( index >= names.size() || w == nullptr || w->getContentComponent() == nullptr )
        {
            quit();
            return;
        }

        const auto name = names[index].trim();
        Screen     screen = Screen::Invalid;
        for( const auto& e : table )
            if( name.equalsIgnoreCase( e.first ) ) screen = e.second;
        // "HomeSequencerGrid": the Step Sequencer in its GRID view ("HomeSequencerView" shows CLASSIC).
        if( name.equalsIgnoreCase( "HomeSequencerGrid" ) || name.equalsIgnoreCase( "HomeSequencerView" ) )
        {
            screen = Screen::HomeSequencerView;
            if( auto* ss = dynamic_cast<kick_ui::StepSequencer*>( kick_ui::KickHome::Instance().findScreen( Screen::HomeSequencerView ) ) )
                ss->showGridView( name.equalsIgnoreCase( "HomeSequencerGrid" ) );
        }

        // "SystemResources" is a dialog, not a screen: open it over the current screen.
        const bool resourcesDialog = name.equalsIgnoreCase( "SystemResources" );
        if( resourcesDialog )
        {
            kick_ui::KickModeHeader::showSystemResources();
            screen = Screen::Invalid;
        }

        if( screen == Screen::Invalid && !resourcesDialog )
        {
            std::cerr << "smoke: unknown screen '" << name << "'" << std::endl;
            setApplicationReturnValue( 2 );
            smokeScreenShots( names, dir, index + 1 );
            return;
        }

        if( !resourcesDialog )
            kick_ui::signals::sigScreenClose( screen );
        // KICK_SMOKE_BROWSE=<folder>: the file browser lists that folder (a test fixture) before its screenshot.
        const auto browseDir = juce::SystemStats::getEnvironmentVariable( "KICK_SMOKE_BROWSE", {} );
        if( screen == Screen::MenuFileBrowser && browseDir.isNotEmpty() )
            juce::Timer::callAfterDelay( 600, [browseDir] { // after the screen has been shown (it lists its own folder when it shows)
                if( auto* fb = dynamic_cast<ui::FileBrowser*>( kick_ui::KickHome::Instance().findScreen( kick_ui::Screen::MenuFileBrowser ) ) )
                    fb->showFolder( browseDir );
            } );
        juce::Timer::callAfterDelay( 1200, [this, names, dir, index, name, resourcesDialog] {
            // KICK_SMOKE_QLINK="1:1,2:-1,3:1" injects Q-Link turns (knob 1..4 : signed ticks) through the same
            // dispatch path as the MPC bridge (on_encoder_turn), after the screen opened and before the snapshot.
            const auto qlinks = juce::SystemStats::getEnvironmentVariable( "KICK_SMOKE_QLINK", {} );
            for( const auto& item : juce::StringArray::fromTokens( qlinks, ",", "" ) )
            {
                const auto knob  = item.upToFirstOccurrenceOf( ":", false, false ).getIntValue();
                const auto delta = item.fromFirstOccurrenceOf( ":", false, false ).getIntValue();
                if( knob < 1 || knob > 4 || delta == 0 )
                    continue;
                CPktMgr::HWMsg_t msg;
                msg.pktType = CPktMgr::EN_PKT_TYPE_KNOBS;
                msg.pktCmd  = contracts::DeviceControlContracts::KnobCmd::EN_KNOB_QLINK;
                msg.data    = { contracts::DeviceControlContracts::KnobSequence::EN_KNOB_1 + knob - 1,
                                delta > 0 ? (int)contracts::DeviceControlContracts::KnobDirection::EN_KNOB_CLOCKWISE : (int)contracts::DeviceControlContracts::KnobDirection::EN_KNOB_ANTICLOCKWISE,
                                std::abs( delta ) };
                contracts::DclSignals::dispatchHardwareInput( msg );
                kick_log( "info", "[SMOKE] qlink %d %d on %s\n", knob, delta, name.toRawUTF8() );
            }
            // List Edit: how many event rows exist and how many have a component (virtual rows: only the visible window).
            // KICK_SMOKE_LISTEDIT_ROWS=<n>: at least that many event rows; the rows that have a component must stay far below it.
            if( name.equalsIgnoreCase( "MenuListEdit" ) )
                if( auto* le = dynamic_cast<kick_ui::ListEditMode*>( kick_ui::KickHome::Instance().findScreen( Screen::MenuListEdit ) ) )
                {
                    std::cout << "LISTEDIT rows=" << le->getNumEventRows() << " built=" << le->getNumBuiltRows() << std::endl;
                    const auto wanted = juce::SystemStats::getEnvironmentVariable( "KICK_SMOKE_LISTEDIT_ROWS", {} );
                    if( wanted.isNotEmpty() && ( le->getNumEventRows() < wanted.getIntValue() || le->getNumBuiltRows() > 60 || le->getNumBuiltRows() < 1 ) )
                    {
                        std::cerr << "FAIL: list edit rows " << le->getNumEventRows() << " built " << le->getNumBuiltRows() << " expected at least " << wanted << " rows and 1..60 built" << std::endl;
                        setApplicationReturnValue( 3 );
                    }
                }
            // KICK_SMOKE_BROWSE_EXPECT="a,b,c": the rows the browser shows for KICK_SMOKE_BROWSE, in this order (sorted, no hidden files).
            const auto expect = juce::SystemStats::getEnvironmentVariable( "KICK_SMOKE_BROWSE_EXPECT", {} );
            if( name.equalsIgnoreCase( "MenuFileBrowser" ) && expect.isNotEmpty() )
                if( auto* fb = dynamic_cast<ui::FileBrowser*>( kick_ui::KickHome::Instance().findScreen( Screen::MenuFileBrowser ) ) )
                {
                    const auto shown = fb->getShownNames().joinIntoString( "," );
                    std::cout << "BROWSER rows: " << shown << std::endl;
                    if( shown != expect )
                    {
                        std::cerr << "FAIL: browser rows '" << shown << "' expected '" << expect << "'" << std::endl;
                        setApplicationReturnValue( 3 );
                    }
                }
            if( auto* content = w != nullptr ? w->getContentComponent() : nullptr )
            {
                // The dialog is a popup window the content snapshot does not contain: photograph the dialog itself.
                juce::Component* shot  = resourcesDialog ? static_cast<juce::Component*>( &kick_ui::KSystemResources::Instance() ) : content;
                auto             image = shot->createComponentSnapshot( shot->getLocalBounds() );
                auto                   shotFile = dir.getChildFile( name + ".png" );
                shotFile.deleteFile(); // FileOutputStream appends to an existing file
                juce::FileOutputStream output{ shotFile };
                juce::PNGImageFormat   png;
                if( !output.openedOk() || !png.writeImageToStream( image, output ) ) setApplicationReturnValue( 2 );
            }
            smokeScreenShots( names, dir, index + 1 );
        } );
    }

    // KICK_SMOKE_POWER=1: the shutdown sequence against the real engine and dialogs (the desktop PlatformServices only logs the
    // power off, and the smoke refuses to run on any other platform type). Step 0: a changed factory demo (it always asks, also
    // with auto save on; the engine's own unsaved flag of a demo drops again after ~0.5 s, so the smoke does not check it): the Power window opens and closes (Cancel). Step 1: Shut Down asks the unsaved-changes question, Cancel
    // keeps the demo open and changed. Step 2: a changed normal project with auto save on (the default) restarts without a
    // question: saved, closed, the (logged) reboot is issued. Prints "POWER ..." per step and "PASS: power".
    void smokePower( int step )
    {
        const auto fail = [this]( const juce::String& m ) {
            std::cerr << "FAIL: " << m << std::endl;
            setApplicationReturnValue( 3 );
            juce::ModalComponentManager::getInstance()->cancelAllModalComponents();
            quit();
        };
        auto& ae = mt::getAudioInterface();
        namespace kp = kick_power;
        if( step == 0 )
        {
            if( mt::PlatformServices::getInstance().getPlatformType() != mt::PlatformType::Desktop )
                return fail( "power smoke: not a desktop platform, refusing to run a power off" );
            const auto root = juce::File( juce::SystemStats::getEnvironmentVariable( "KICK_DATA_ROOT", {} ) ).getChildFile( "power" );
            root.deleteRecursively();
            auto src = root.getChildFile( "DemoSrc/DemoSrc.kickPrj" );
            if( !src.getParentDirectory().createDirectory() || !ae.loadProject( src, true ) || !ae.saveProject() || !ae.closeProject() )
                return fail( "power smoke: cannot make the demo source" );
            std::atomic<bool> noCancel{ false };
            const auto demo = getAppDirs( MTDemos ).getChildFile( "PowerDemo" );
            demo.deleteRecursively();
            if( !kick_fileops::copyFolderTo( src.getParentDirectory().getFullPathName().toStdString(), demo.getFullPathName().toStdString(), noCancel ).ok
                || !demo.getChildFile( "DemoSrc.kickPrj" ).moveFileTo( demo.getChildFile( "PowerDemo.kickPrj" ) ) )
                return fail( "power smoke: cannot place the demo" );
            auto demoFile = demo.getChildFile( "PowerDemo.kickPrj" );
            if( !ae.loadProject( demoFile ) || !ae.isProjectReadOnly() )
                return fail( "power smoke: cannot open the demo" );
            ae.createProgram( mt::MTProgramType::Drum, "PowerDirty" );
            if( !ae.isProjectDirty() )
                return fail( "power smoke: the edit did not make the demo unsaved" );
            ui::power::showPowerWindow();
            juce::Timer::callAfterDelay( 700, [this, fail] {
                const bool open = ui::power::isWindowOpen() && mt::utils::isDialogOpen();
                std::cout << "POWER window open=" << open << std::endl;
                if( !open ) return fail( "power smoke: the Power window did not open" );
                const auto shots = juce::SystemStats::getEnvironmentVariable( "KICK_SMOKE_SHOTS_DIR", {} );
                if( shots.isNotEmpty() )
                {
                    juce::File( shots ).createDirectory();
                    auto image = mt::utils::snapshotTopDialog();
                    auto shotFile = juce::File( shots ).getChildFile( "Dialog_Power.png" );
                    shotFile.deleteFile();
                    juce::FileOutputStream output{ shotFile };
                    juce::PNGImageFormat   png;
                    if( image.isValid() && ( !output.openedOk() || !png.writeImageToStream( image, output ) ) ) setApplicationReturnValue( 2 );
                }
                mt::utils::closeTopDialogFromHardware( false ); // Cancel
                juce::Timer::callAfterDelay( 500, [this, fail] {
                    const bool ok = !ui::power::isWindowOpen() && !mt::utils::isDialogOpen() && !ui::power::isBusy() && mt::getAudioInterface().isProjectLoaded();
                    std::cout << "POWER window cancel ok=" << ok << std::endl;
                    if( !ok ) return fail( "power smoke: Cancel in the Power window changed something" );
                    smokePower( 1 );
                } );
            } );
            return;
        }
        if( step == 1 )
        {
            auto result = std::make_shared<kp::Result>( kp::Result::Busy );
            auto called = std::make_shared<bool>( false );
            ui::power::request( kp::Action::ShutDown, [result, called]( kp::Result r ) { *result = r; *called = true; } );
            juce::Timer::callAfterDelay( 700, [this, fail, result, called] {
                const bool asked = mt::utils::isDialogOpen() && ui::power::isBusy() && !*called;
                std::cout << "POWER step 1 unsaved question open=" << asked << std::endl;
                if( !asked ) return fail( "power smoke: the unsaved-changes question did not open" );
                mt::utils::closeTopDialogFromHardware( false ); // X = Cancel
                juce::Timer::callAfterDelay( 700, [this, fail, result, called] {
                    auto&      ae2 = mt::getAudioInterface();
                    const bool ok  = *called && *result == kick_power::Result::Cancelled && ae2.isProjectLoaded() && ae2.isProjectReadOnly() && !ui::power::isBusy();
                    std::cout << "POWER step 1 cancel ok=" << ok << " result=" << (int)*result << std::endl;
                    if( !ok ) return fail( "power smoke: Cancel at the unsaved question did not keep the project open" );
                    smokePower( 2 );
                } );
            } );
            return;
        }
        if( step == 2 )
        {
            ae.discardProjectChangesOnClose();
            getAppDirs( MTDemos ).getChildFile( "PowerDemo" ).deleteRecursively(); // the demo of step 0 (it is in the build tree)
            auto file = juce::File( juce::SystemStats::getEnvironmentVariable( "KICK_DATA_ROOT", {} ) ).getChildFile( "power/Project/Project.kickPrj" );
            if( !ae.closeProject() || !file.getParentDirectory().createDirectory() || !ae.loadProject( file, true ) || !ae.saveProject() )
                return fail( "power smoke: cannot make the project" );
            ae.createProgram( mt::MTProgramType::Drum, "PowerDirty" );
            if( !ae.isProjectDirty() || !ae.isProjectAutoSave() || ae.isProjectReadOnly() )
                return fail( "power smoke: expected a changed project with auto save on" );
            auto result = std::make_shared<kp::Result>( kp::Result::Busy );
            auto called = std::make_shared<bool>( false );
            ui::power::request( kp::Action::Restart, [result, called]( kp::Result r ) { *result = r; *called = true; } );
            juce::Timer::callAfterDelay( 900, [this, fail, result, called, file] {
                auto&      ae2 = mt::getAudioInterface();
                const bool ok  = *called && *result == kick_power::Result::Started && !ae2.isProjectLoaded() && ui::power::isBusy() && !mt::utils::isDialogOpen();
                std::cout << "POWER step 2 autosave+restart ok=" << ok << " result=" << (int)*result << std::endl;
                if( !ok ) return fail( "power smoke: the restart did not run through" );
                if( !file.loadFileAsString().contains( "PowerDirty" ) ) return fail( "power smoke: the project was not saved before the close" );
                std::cout << "PASS: power" << std::endl;
                quit();
            } );
        }
    }

    // KICK_SMOKE_DIALOGS="Metronome,TimeSignature,Locate,Stp,Tempo,UndoHistory,SystemResources" (+ KICK_SMOKE_SHOTS_DIR for the pictures,
    // KICK_SMOKE_DIALOG_CYCLES, default 3): each dialog is opened and closed the way the hardware closes it, the cycle is repeated with a
    // theme reload after every round (the slots of a closed dialog must not run). Prints "DIALOG ..." per step and "PASS: dialogs".
    void smokeDialogs( juce::StringArray names, juce::File dir, int index, int cycle, int cycles )
    {
        const auto fail = [this]( const juce::String& m ) {
            std::cerr << "FAIL: " << m << std::endl;
            setApplicationReturnValue( 3 );
        };
        if( index >= names.size() )
        {
            // A theme reload between the rounds: every slot that is still connected runs (a dead dialog would crash here).
            ui::Theme::loadTheme( ui::Theme::getCurrentTheme(), true );
            if( cycle + 1 >= cycles )
            {
                if( getApplicationReturnValue() == 0 )
                    std::cout << "PASS: dialogs" << std::endl;
                quit();
                return;
            }
            juce::Timer::callAfterDelay( 300, [this, names, dir, cycle, cycles] { smokeDialogs( names, dir, 0, cycle + 1, cycles ); } );
            return;
        }
        const auto name = names[index].trim();
        auto&      ae   = mt::getAudioInterface();
        auto&      bar  = kick_ui::AppStatusbar::Instance();
        if( name.equalsIgnoreCase( "Metronome" ) ) kick_ui::MetronomePopup::Show();
        else if( name.equalsIgnoreCase( "TimeSignature" ) ) kick_ui::TimeSignatureDialog::show( ae.getSelectedSequence() );
        else if( name.equalsIgnoreCase( "Locate" ) ) bar.showBbtPopup();
        else if( name.equalsIgnoreCase( "Stp" ) ) bar.showStpPopup();
        else if( name.equalsIgnoreCase( "Tempo" ) ) bar.showTempoEditor();
        else if( name.equalsIgnoreCase( "UndoHistory" ) ) kick_ui::UndoHistory::show();
        else if( name.equalsIgnoreCase( "SystemResources" ) ) kick_ui::KickModeHeader::showSystemResources();
        else if( name.equalsIgnoreCase( "Power" ) ) ui::power::showPowerWindow();
        else if( name.equalsIgnoreCase( "ClapInstrument" ) )
        {
            auto idx = ae.createProgram( mt::MTProgramType::Plugin, "SmokeSynthy" );
            if( auto* clapProg = dynamic_cast<mt::MTClapProgram*>( idx >= 0 ? ae.getProgram( idx ) : nullptr ) )
            {
                const auto instruments = kick::clap::PluginManager::getInstance().getInstrumentDescriptors();
                if( !instruments.empty() && clapProg->getPluginSession() )
                    clapProg->getPluginSession()->select( instruments.front() );
                if( kick_ui::ProgramEdit::pluginOpener() )
                    kick_ui::ProgramEdit::pluginOpener()( clapProg );
            }
        }
        else
        {
            fail( "unknown dialog " + name );
            quit();
            return;
        }
        juce::Timer::callAfterDelay( 800, [this, names, dir, index, cycle, cycles, name, fail] {
            const bool open = mt::utils::isDialogOpen();
            std::cout << "DIALOG " << name << " cycle " << cycle << " open=" << open << std::endl;
            if( !open )
                fail( "dialog " + name + " did not open" );
            if( cycle == 0 && dir != juce::File() )
            {
                auto image = mt::utils::snapshotTopDialog();
                if( image.isValid() )
                {
                    auto shotFile = dir.getChildFile( "Dialog_" + name + ".png" );
                    shotFile.deleteFile();
                    juce::FileOutputStream output{ shotFile };
                    juce::PNGImageFormat   png;
                    if( !output.openedOk() || !png.writeImageToStream( image, output ) ) setApplicationReturnValue( 2 );
                }
            }
            mt::utils::closeTopDialogFromHardware( false );
            juce::Timer::callAfterDelay( 500, [this, names, dir, index, cycle, cycles, name, fail] {
                const bool stillOpen = mt::utils::isDialogOpen();
                if( stillOpen )
                {
                    fail( "dialog " + name + " did not close" );
                    juce::ModalComponentManager::getInstance()->cancelAllModalComponents();
                }
                smokeDialogs( names, dir, index + 1, cycle, cycles );
            } );
        } );
    }

    // KICK_SMOKE_MENUTILES=1: every tile of the Menu is pressed (on its page), first without a project (screens that need one stay
    // closed and the tile is grayed out), then with one (each opens its screen). Prints "MENUTILE ..." per item and "PASS: menu tiles".
    void smokeMenuTiles( int item, bool withProject )
    {
        namespace ml = ui::menu_logic;
        using kick_ui::Screen;
        auto& home = kick_ui::KickHome::Instance();
        const auto fail = [this]( const juce::String& m ) {
            std::cerr << "FAIL: " << m << std::endl;
            setApplicationReturnValue( 3 );
        };
        if( item >= ml::kMenuItemCount )
        {
            if( !withProject )
            {
                auto file = juce::File( juce::SystemStats::getEnvironmentVariable( "KICK_DATA_ROOT", {} ) ).getChildFile( "menutiles/Project.kickPrj" );
                if( !file.getParentDirectory().createDirectory() || !mt::getAudioInterface().loadProject( file, true ) )
                {
                    fail( "menu tiles: cannot create the project" );
                    quit();
                    return;
                }
                smokeMenuTiles( 0, true );
                return;
            }
            std::cout << "PASS: menu tiles" << std::endl;
            quit();
            return;
        }
        if( home.getCurrentScreen() != Screen::HomeOptions )
            kick_ui::signals::sigScreenClose( Screen::HomeOptions );
        juce::Timer::callAfterDelay( 500, [this, item, withProject, fail] {
            auto& home = kick_ui::KickHome::Instance();
            auto* menu = dynamic_cast<kick_ui::AppMenuOptions*>( home.findScreen( Screen::HomeOptions ) );
            if( menu == nullptr || home.getCurrentScreen() != Screen::HomeOptions )
            {
                fail( "menu tiles: the Menu is not open for item " + juce::String( item ) );
                quit();
                return;
            }
            const bool shouldWork = withProject || !ml::itemNeedsProject<Screen>( item );
            const bool clicked    = menu->activateItem( item );
            juce::Timer::callAfterDelay( 700, [this, item, withProject, fail, shouldWork, clicked] {
                auto& home = kick_ui::KickHome::Instance();
                const Screen want = item < ml::kTileCount ? ml::tileScreen<Screen>( item ) : item == ml::kCommandKickUpdate ? Screen::KickUpdate : item == ml::kCommandFavorites ? Screen::MenuFileBrowser : Screen::HomeOptions;
                const Screen expect = shouldWork ? want : Screen::HomeOptions;
                const bool   ok     = clicked == shouldWork && home.getCurrentScreen() == expect;
                std::cout << "MENUTILE " << ( withProject ? "project" : "no-project" ) << " item " << item << " '" << ml::tileDef<Screen>( item ).name << "' clicked=" << clicked
                          << " screen=" << (int)home.getCurrentScreen() << " expected=" << (int)expect << ( ok ? " ok" : " FAIL" ) << std::endl;
                if( !ok )
                    fail( "menu tile " + juce::String( ml::tileDef<Screen>( item ).name ) + ( withProject ? " (project)" : " (no project)" ) );
                juce::ModalComponentManager::getInstance()->cancelAllModalComponents(); // the Resources dialog
                smokeMenuTiles( item + 1, withProject );
            } );
        } );
    }

    void initialise( const juce::String& commandLine ) override
    {
        kick_logger::init();
        kick_log( "info", "[INFO] [APP] Kick %s initialising... commandLine: '%s'\n", getApplicationVersion().toRawUTF8(), commandLine.toRawUTF8() );
        const bool smoke = commandLine.contains("--smoke-test");
        kick_startup::state().set( kick_startup::Step::AudioEngine, kick_startup::StepState::Running, "starting" );
#if JUCE_LINUX
        // Boot-loop guard: counts starts that did not stay up for kick_boot::kStableSeconds. After two such starts with a crash file
        // present, this start is a safe-mode start (no project, reason + crash file path shown). Not in the smoke test.
        if( !smoke )
        {
            const auto dir   = juce::SystemStats::getEnvironmentVariable( "KICK_BOOT_GUARD_DIR", {} ).toStdString();
            const auto paths = dir.empty() ? kick_boot::Paths() : kick_boot::Paths::inDir( dir );
            const auto d     = kick_boot::onBoot( paths );
            if( !d.counterWritten ) kick_log( "error", "[ERROR] [BOOT] cannot write %s: a boot loop cannot be detected\n", paths.countFile.c_str() );
            if( d.safeMode )
            {
                kick_startup::state().setSafeMode( true, d.crashPath );
                const auto p = kick_startup::state().fail( kick_startup::Fault::SafeMode, "crash file: " + d.crashPath );
                kick_startup::state().set( kick_startup::Step::Project, kick_startup::StepState::Skipped, "safe mode" );
                kick_log( "error", "[ERROR] [BOOT] SAFE MODE: %s (%s)\n", p.cause.c_str(), d.reason.c_str() );
            }
            juce::Timer::callAfterDelay( kick_boot::kStableSeconds * 1000, [paths] {
                if( kick_boot::markStable( paths ) ) kick_log( "info", "[INFO] [BOOT] stable start: boot counter cleared\n" );
                else kick_log( "error", "[ERROR] [BOOT] cannot clear %s\n", paths.countFile.c_str() );
            } );
        }
#endif
#if JUCE_LINUX
        // A crash (SIGSEGV/SIGBUS/...) leaves its signal, fault address, registers and call stack in /data/kick.crash
        // (module-relative addresses: resolve with arm-linux-gnueabihf-addr2line against the unstripped binary). On ARM the
        // backtrace() of a signal handler stops at the signal trampoline, so the faulting pc/lr are written from the context.
        // Without the file the MPC just falls back to the stock app.
        struct sigaction action {};
        action.sa_sigaction = []( int sig, siginfo_t* info, void* context ) {
            const int fd = open( "/data/kick.crash", O_WRONLY | O_CREAT | O_TRUNC, 0644 );
            if( fd >= 0 )
            {
                char line[160];
#if defined( __arm__ )
                const auto* uc = static_cast<ucontext_t*>( context );
                const int   len = snprintf( line, sizeof( line ), "signal=%d fault_addr=%p pc=%#lx lr=%#lx sp=%#lx\n", sig, info->si_addr,
                                            uc->uc_mcontext.arm_pc, uc->uc_mcontext.arm_lr, uc->uc_mcontext.arm_sp );
#else
                (void)context;
                const int len = snprintf( line, sizeof( line ), "signal=%d fault_addr=%p\n", sig, info->si_addr );
#endif
                if( len > 0 ) (void)!write( fd, line, (size_t)len );
#if defined( __arm__ )
                // The unwinder stops at the signal trampoline, so scan the faulting stack for words that point into this binary
                // (module offsets of probable return addresses, nearest the fault first).
                Dl_info    mod {};
                const auto* sp = reinterpret_cast<const unsigned long*>( uc->uc_mcontext.arm_sp );
                if( dladdr( reinterpret_cast<void*>( uc->uc_mcontext.arm_pc ), &mod ) != 0 )
                {
                    const auto base = reinterpret_cast<unsigned long>( mod.dli_fbase );
                    int        found = 0;
                    unsigned long checkedPage = 0;
                    for( int i = 0; i < 4096 && found < 80; ++i )
                    {
                        // Never read past the end of the stack mapping (that fault would replace the report): check each page once.
                        const auto page = reinterpret_cast<unsigned long>( sp + i ) & ~4095UL;
                        if( page != checkedPage )
                        {
                            unsigned char vec = 0;
                            if( mincore( reinterpret_cast<void*>( page ), 4096, &vec ) != 0 ) break;
                            checkedPage = page;
                        }
                        const auto w = sp[i];
                        if( w > base && w < base + 0x1800000 )
                        {
                            const int l = snprintf( line, sizeof( line ), "stack+%d: %#lx\n", i * 4, w - base );
                            if( l > 0 ) (void)!write( fd, line, (size_t)l );
                            ++found;
                        }
                    }
                }
#endif
                void*     frames[64];
                const int n = backtrace( frames, 64 );
                backtrace_symbols_fd( frames, n, fd );
                close( fd );
            }
            kick_logger::dump_crash_log_async_signal_safe( "/data/kick.crash.log" );
            signal( sig, SIG_DFL );
            raise( sig );
        };
        action.sa_flags = SA_SIGINFO | SA_RESETHAND;
        for( const int sig : { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT } ) sigaction( sig, &action, nullptr );
#endif
        audioInterface.reset(new KickAudioInterface());
        engineAdapter.reset(new KickAudioEngineAdapter(audioInterface));
        
        adapter.reset( new KickDeviceAdapter() );
        // This method is where you should put your application's initialisation code ...
        // Headless hardware (no X server): register a fallback display before any UI is built, so the many
        // parent-less setBoundsRelative() calls do not dereference a null Display (hardware issue 8).
        const bool usingFallbackDisplay = kick_app::ensureFallbackDisplay();
#if ! JUCE_IOS
        if (!smoke) juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(true, true);
#endif

        // Exit kiosk mode ...
        juce::Desktop::getInstance().setKioskModeComponent( nullptr );

        // Initialize Settings ...
        mt::getSettings();

        // Initialize Audio engine and interface ...
        mt::getAudioInterface();

#if JUCE_IOS
        if( !smoke )
        {
            const auto demo = getAppDirs( MTDemos ).getChildFile( "Demo 01/Demo 01.kickPrj" );
            if( demo.existsAsFile() )
                ui::LoadProjectAndAnimation::setDefaultProject( demo );
        }
#endif

        // Initialize User Interface...
        w = std::make_unique<HomeWindow>( getApplicationName() );

#if ! (JUCE_LINUX && defined(KICK_ENABLE_HARDWARE) && KICK_ENABLE_HARDWARE)
        // Populate non-lazy UI components (AppTopbar, RecentProject) with the active theme assets immediately.
        ui::Theme::loadTheme( ui::Theme::getCurrentTheme(), true );
#endif

#if JUCE_MAC
        w->setUsingNativeTitleBar( true );
        w->centreWithSize( 1024, 600 );
#else
        // Title bar ...
        w->setUsingNativeTitleBar( false );
        w->setTitleBarHeight( 0 );
#endif

#if defined( ENABLE_X11_FULLSCREEN ) && ( 0 < ENABLE_X11_FULLSCREEN )
    #if( JUCE_LINUX || __linux__ )
        //xlib::maximizeWindow( (xlib::Window)getPeer()->getNativeHandle() );
        xlib::fullscreenWindow( (xlib::Window)w->getPeer()->getNativeHandle() );
    #endif
#endif

#if ! JUCE_MAC
        // Dimension ...
        juce::Rectangle<int> bounds = ui::layout::screen;
#if JUCE_IOS
        if( auto* primary = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() )
            bounds = primary->userArea;
#endif
        w->setBounds( bounds );

#if ! JUCE_IOS
        // Position ...
        w->setTopLeftPosition( 0, 0 );
#endif
#endif

        // Headless (no X server / display, e.g. the MPC framebuffer bridge): JUCE's Linux setKioskComponent()
        // dereferences getDisplayForRect() unchecked and a native peer cannot be created. The bridge renders
        // the component tree itself (peer-less touch path), so keep the window hidden and skip kiosk mode.
        // The fallback display registered above makes the list non-empty, so use its return value instead:
        // a real display exists only if ensureFallbackDisplay() found one already there (behaviour as before).
        const bool hasDisplay = ! usingFallbackDisplay;

        // Visibility...
        w->setVisible(!smoke && hasDisplay);

#if JUCE_IOS
        if (!smoke && hasDisplay) juce::Desktop::getInstance().setKioskModeComponent( w.get(), true );
#elif( JUCE_ANDROID || JUCE_LINUX || __linux__ )
        if (!smoke && hasDisplay) juce::Desktop::getInstance().setKioskModeComponent( w.get(), false );
#endif
#if JUCE_LINUX || __linux__
        if (!smoke)
        {
            hwBridge = std::make_unique<KickHardwareBridge>();
            hwBridge->start( w.get(), &kick_ui::KickHome::Instance() );
            // Power button (short press): the Power window. A missing / unopenable device is logged; Menu > Power always works.
            kick_input::startPowerKeyWatcher( powerKey, [] { juce::MessageManager::callAsync( [] { ui::power::showPowerWindow( "power key" ); } ); } );
        }
        else
#endif
        {
            // No hardware bridge here (desktop, iOS, smoke test): nothing reports display / MIDI, so they are not applicable.
            kick_startup::state().set( kick_startup::Step::Display, kick_startup::StepState::Skipped, "no hardware bridge" );
            kick_startup::state().set( kick_startup::Step::Midi, kick_startup::StepState::Skipped, "no hardware bridge" );
        }
        if( smoke ) kick_startup::state().set( kick_startup::Step::Project, kick_startup::StepState::Skipped, "smoke test" );
        if (smoke) juce::Timer::callAfterDelay(1500, [this] {
#if KICK_APP_SMOKE_TEST
            const auto error = runKickAppSmoke();
            if (error.isNotEmpty()) {
                std::cerr << "FAIL: " << error << std::endl;
                setApplicationReturnValue(3);
            }
#endif
            const auto path = juce::SystemStats::getEnvironmentVariable("KICK_SMOKE_SNAPSHOT", {});
            if (path.isNotEmpty() && w->getContentComponent()) {
                auto image = w->getContentComponent()->createComponentSnapshot(w->getContentComponent()->getLocalBounds());
                juce::File(path).deleteFile(); // FileOutputStream appends to an existing file
                juce::FileOutputStream output{juce::File(path)};
                juce::PNGImageFormat png;
                if (!output.openedOk() || !png.writeImageToStream(image, output)) setApplicationReturnValue(2);
            }
            if (const auto dialogs = juce::SystemStats::getEnvironmentVariable("KICK_SMOKE_DIALOGS", {}); dialogs.isNotEmpty())
            {
                auto file = juce::File(juce::SystemStats::getEnvironmentVariable("KICK_DATA_ROOT", {})).getChildFile("dialogs/Project.kickPrj");
                auto& api = mt::getAudioInterface();
                if (!file.getParentDirectory().createDirectory() || !api.loadProject(file, true))
                {
                    std::cerr << "FAIL: dialogs smoke: cannot create the project" << std::endl;
                    setApplicationReturnValue(3);
                    quit();
                    return;
                }
                if (api.getNumSequences() == 0) api.createSequence("DialogSeq"); // the sequence dialogs need one
                api.setSelectedSequence(0);
                const auto shots = juce::SystemStats::getEnvironmentVariable("KICK_SMOKE_SHOTS_DIR", {});
                if (shots.isNotEmpty()) juce::File(shots).createDirectory();
                const auto cycles = juce::jmax(1, juce::SystemStats::getEnvironmentVariable("KICK_SMOKE_DIALOG_CYCLES", "3").getIntValue());
                smokeDialogs(juce::StringArray::fromTokens(dialogs, ",", ""), shots.isNotEmpty() ? juce::File(shots) : juce::File(), 0, 0, cycles);
                return;
            }
            if (juce::SystemStats::getEnvironmentVariable("KICK_SMOKE_POWER", {}).isNotEmpty())
            {
                smokePower(0);
                return;
            }
            if (juce::SystemStats::getEnvironmentVariable("KICK_SMOKE_MENUTILES", {}).isNotEmpty())
            {
                smokeMenuTiles(0, false);
                return;
            }
            const auto screens = juce::SystemStats::getEnvironmentVariable("KICK_SMOKE_SCREENS", {});
            const auto shotsDir = juce::SystemStats::getEnvironmentVariable("KICK_SMOKE_SHOTS_DIR", {});
            if (screens.isNotEmpty() && shotsDir.isNotEmpty())
            {
                juce::File(shotsDir).createDirectory();
                smokeScreenShots(juce::StringArray::fromTokens(screens, ",", ""), juce::File(shotsDir), 0);
                return;
            }
            kick_log( "info", "[INFO] [APP] smoke timer calling quit()\n" );
            quit();
        });

#if PERFETTO
        MelatoninPerfetto::get().beginSession();
#endif
    }

    void shutdown() override
    {
        kick_log( "info", "[INFO] [APP] Kick shutting down...\n" );
#if defined(__APPLE__)
        void* callstack[64];
        int frames = backtrace( callstack, 64 );
        char** strs = backtrace_symbols( callstack, frames );
        if( strs )
        {
            for( int i = 0; i < frames; ++i )
                kick_log( "info", "[SHUTDOWN BT] frame %d: %s\n", i, strs[i] );
            free( strs );
        }
#endif
#if PERFETTO
        MelatoninPerfetto::get().endSession();
#endif
        // Add your application's shutdown code here ...
#if JUCE_LINUX || __linux__
        powerKey.stop();
        if (hwBridge) hwBridge->stop();
        hwBridge.reset();
#endif
        Controls::InputEvents::shutdown();
        mt::getAudioInterface().closeProject();
        w = nullptr;
        adapter = nullptr;
#if JUCE_LINUX
        // A clean quit is not a crash: forget the unstable-start count (deploys / restarts must not trip safe mode).
        {
            const auto dir = juce::SystemStats::getEnvironmentVariable( "KICK_BOOT_GUARD_DIR", {} ).toStdString();
            kick_boot::markStable( dir.empty() ? kick_boot::Paths() : kick_boot::Paths::inDir( dir ) );
        }
#endif
        kick_logger::shutdown();
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        kick_log( "info", "[INFO] [APP] systemRequestedQuit CALLED!\n" );
#if JUCE_IOS
        kick_log( "info", "[INFO] [APP] iOS ignoring systemRequestedQuit to remain running\n" );
        return;
#endif
        if (ui::RecentProject::isProjectLoading || ui::BackgroundTask::activeCount() > 0) {
            juce::Timer::callAfterDelay(100, [this] { systemRequestedQuit(); });
            return;
        }
        // Unsaved changes: ask (Save / Don't save / Cancel) unless auto save covers them; then the project is saved while the UI
        // and live programs still exist. A rejected save leaves the project open and its global error visible for recovery.
        if (!quitConfirmed && kick_unsaved::needsPrompt(ui::project_actions::unsavedState())) {
            if (quitAsked) return; // the question is already on screen
            quitAsked = true;
            // Save: saved (or Save As done); Don't save: the next close of the project writes nothing. Either way the project is
            // settled when this runs again.
            ui::project_actions::leaveProject([this] { quitAsked = false; quitConfirmed = true; systemRequestedQuit(); }, [this] { quitAsked = false; });
            return;
        }
        // A demo / template cannot be saved in place (and the user was asked above when it had changes).
        if (!quitConfirmed && !mt::getAudioInterface().isProjectReadOnly() && !mt::getAudioInterface().saveProject()) return;
#if PERFETTO
        MelatoninPerfetto::get().endSession();
#endif

        // This is called when the app is being asked to quit: you can ignore this request and let the app
        // carry on running, or call quit() to allow the app to close.
        auto mgr = juce::ModalComponentManager::getInstance();
        if( mgr->cancelAllModalComponents() )
            juce::Timer::callAfterDelay( 100, [this] { systemRequestedQuit(); } );
        else
            quit();
    }

    void anotherInstanceStarted( const juce::String& /*commandLine*/ ) override
    {
        // When another instance of the app is launched while this one is running, this method is invoked,
        // and the commandLine parameter tells you what the other instance's command-line arguments were.
    }

    //==============================================================================
    // This class implements the desktop window that contains an instance of our MainComponent class.
#if JUCE_MAC
    class HomeWindow : public juce::DocumentWindow
    {
    public:
        explicit HomeWindow( const juce::String& name )
            : juce::DocumentWindow( name, juce::Colours::black, juce::DocumentWindow::closeButton )
        {
            setUsingNativeTitleBar( true );
            setTitle( name );
            setContentNonOwned( &kick_ui::KickHome::Instance(), true );
            setResizable( false, false );
#if defined( KICK_KEYBOARD_PANEL )
            addKeyListener( &contracts::getKeypressDetector() );
#endif
        }

        ~HomeWindow() override
        {
            clearContentComponent();
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

        void inputAttemptWhenModal() override
        {
            auto numModal = getNumCurrentlyModalComponents();
            auto index    = juce::jmax( 0, numModal - 1 );
            if( auto p = getCurrentlyModalComponent( index ) )
                p->toFront( true );
        }
    };
#elif JUCE_IOS
    class KickMobileView : public juce::Component
    {
    public:
        KickMobileView()
        {
            setOpaque( true );
            dawStage.setOpaque( true );
            dawStage.setBounds( ui::layout::screen );
            addAndMakeVisible( dawStage );
            kickHome = &kick_ui::KickHome::Instance();
            kickHome->setBounds( ui::layout::screen );
            dawStage.addAndMakeVisible( kickHome );
            mt::utils::setOverlayHost( &dawStage );
        }

        ~KickMobileView() override
        {
            mt::utils::setOverlayHost( nullptr );
            dawStage.removeAllChildren();
            if( kickHome != nullptr )
                kickHome->setTransform( juce::AffineTransform() );
        }

        void paint( juce::Graphics& g ) override
        {
            g.fillAll( juce::Colour( 0xff121215 ) );
        }

        void resized() override
        {
            int w = getWidth();
            int h = getHeight();
            if( w <= 0 || h <= 0 ) return;

            const float targetAspect = 1024.0f / 600.0f;
            float dawW, dawH;

            if( (float)w / (float)h > targetAspect )
            {
                // Screen is wider than 1024:600 -> pillarbox
                dawH = (float)h;
                dawW = (float)h * targetAspect;
            }
            else
            {
                // Screen is taller -> letterbox
                dawW = (float)w;
                dawH = (float)w / targetAspect;
            }

            float scale = dawW / 1024.0f;
            float dawX = ( (float)w - dawW ) * 0.5f;
            float dawY = ( (float)h - dawH ) * 0.5f;

            dawStage.setBounds( ui::layout::screen );
            dawStage.setTransform(
                juce::AffineTransform::scale( scale, scale )
                                     .translated( dawX, dawY )
            );
            if( kickHome != nullptr )
                kickHome->setBounds( ui::layout::screen );
        }

    private:
        juce::Component    dawStage;
        kick_ui::KickHome* kickHome{ nullptr };
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( KickMobileView )
    };

    class HomeWindow : public juce::DocumentWindow
    {
    public:
        explicit HomeWindow( const juce::String& name )
            : juce::DocumentWindow( name, juce::Colours::black, 0 )
        {
            setTitle( name );
            mobileView = std::make_unique<KickMobileView>();
            setContentNonOwned( mobileView.get(), false );
        }

        ~HomeWindow() override
        {
            clearContentComponent();
            mobileView.reset();
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

        void inputAttemptWhenModal() override
        {
            auto numModal = getNumCurrentlyModalComponents();
            auto index    = juce::jmax( 0, numModal - 1 );
            if( auto p = getCurrentlyModalComponent( index ) )
                p->toFront( true );
        }

    private:
        std::unique_ptr<KickMobileView> mobileView;
    };
#else
    class HomeWindow : public juce::DocumentWindow
    {
    public:
        explicit HomeWindow( const juce::String& name )
            : juce::DocumentWindow( name, juce::Colours::black, 0 )
        {
            setTitle( name );
            setContentOwned( &kick_ui::KickHome::Instance(), true );
#if defined( KICK_KEYBOARD_PANEL ) && !defined( _WIN32 ) && !defined( _WIN64 )
            // KickHome clears the keyboard focus on every screen change. With nothing focused, JUCE sends keys to this window, which
            // is outside KickHome's own key listener, so listen here as well.
            addKeyListener( &contracts::getKeypressDetector() );
#endif
        }

        ~HomeWindow() final = default;

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

        void inputAttemptWhenModal() override
        {
            auto numModal = getNumCurrentlyModalComponents();
            auto index    = juce::jmax( 0, numModal - 1 );
            if( auto p = getCurrentlyModalComponent( index ) )
                p->toFront( true );
        }
    };
#endif

private:
    std::unique_ptr<juce::DocumentWindow> w;
    std::unique_ptr<KickDeviceAdapter>    adapter;
    std::unique_ptr<KickAudioEngineAdapter>    engineAdapter;
    std::shared_ptr<contracts::AudioInterface> audioInterface;
#if JUCE_LINUX || __linux__
    std::unique_ptr<KickHardwareBridge>        hwBridge;
    kick_input::PowerKeyWatcher                powerKey;
#endif
#if PERFETTO
    std::unique_ptr<perfetto::TracingSession> tracingSession;
#endif
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION( AudioEngineApplication )
