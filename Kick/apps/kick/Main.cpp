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
#include "audio_engine/audio_engine.h"
#include "kick_device_adapter.h"
#include "kick_audio_engine_adapter.h"
#include "kick_audio_interface.h"
#include "kick_mobile_t_layout.h"
#if JUCE_MAC
 #include "kick_hardware_chassis.h"
#endif
#if JUCE_LINUX || __linux__
 #include "kick_hardware_bridge.h"
#endif
#if KICK_APP_SMOKE_TEST
 #include "../../tests/platform/AppSmoke.h"
#endif
#include "contracts/utils/keyboard/keyboard_detector.h"

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
            { "MenuSampler", Screen::MenuSampler }, { "MenuLooper", Screen::MenuLooper },
            { "MenuPadMute", Screen::MenuPadMute }, { "MenuTrackMute", Screen::MenuTrackMute },
            { "MenuPadMixer", Screen::MenuPadMixer }, { "MenuChannelMixer", Screen::MenuChannelMixer },
            { "MenuKnobEdit", Screen::MenuKnobEdit }, { "MenuListEdit", Screen::MenuListEdit },
            { "MenuPadPerform", Screen::MenuPadPerform }, { "MenuMidiControl", Screen::MenuMidiControl },
            { "MenuFileBrowser", Screen::MenuFileBrowser }, { "MenuNextSequence", Screen::MenuNextSequence },
            { "MenuSong", Screen::MenuSong }, { "MenuColorEdit", Screen::MenuColorEdit },
            { "RecentProject", Screen::RecentProject }, { "SystemResources", Screen::SystemResources },
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

        if( screen == Screen::Invalid )
        {
            std::cerr << "smoke: unknown screen '" << name << "'" << std::endl;
            setApplicationReturnValue( 2 );
            smokeScreenShots( names, dir, index + 1 );
            return;
        }

        kick_ui::signals::sigScreenClose( screen );
        juce::Timer::callAfterDelay( 1200, [this, names, dir, index, name] {
            if( auto* content = w != nullptr ? w->getContentComponent() : nullptr )
            {
                auto                  image = content->createComponentSnapshot( content->getLocalBounds() );
                auto                   shotFile = dir.getChildFile( name + ".png" );
                shotFile.deleteFile(); // FileOutputStream appends to an existing file
                juce::FileOutputStream output{ shotFile };
                juce::PNGImageFormat   png;
                if( !output.openedOk() || !png.writeImageToStream( image, output ) ) setApplicationReturnValue( 2 );
            }
            smokeScreenShots( names, dir, index + 1 );
        } );
    }

    void initialise( const juce::String& commandLine ) override
    {
        audioInterface.reset(new KickAudioInterface());
        engineAdapter.reset(new KickAudioEngineAdapter(audioInterface));
        
        adapter.reset( new KickDeviceAdapter() );
        // This method is where you should put your application's initialisation code ...
        const bool smoke = commandLine.contains("--smoke-test");
#if ! JUCE_IOS
        if (!smoke) juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(true, true);
#endif

        // Exit kiosk mode ...
        juce::Desktop::getInstance().setKioskModeComponent( nullptr );

        // Initialize Settings ...
        mt::getSettings();

        // Initialize Audio engine and interface ...
        mt::getAudioInterface();

        // Initialize User Interface...
        w = std::make_unique<HomeWindow>( getApplicationName() );

#if ! JUCE_MAC
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

        // Position ...
        w->setTopLeftPosition( 0, 0 );
#endif

        // Headless (no X server / display, e.g. the MPC framebuffer bridge): JUCE's Linux setKioskComponent()
        // dereferences getDisplayForRect() unchecked and a native peer cannot be created. The bridge renders
        // the component tree itself (peer-less touch path), so keep the window hidden and skip kiosk mode.
        const bool hasDisplay = ! juce::Desktop::getInstance().getDisplays().displays.isEmpty();

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
        }
#endif
        if (smoke) juce::Timer::callAfterDelay(1500, [this] {
#if KICK_APP_SMOKE_TEST
            const auto error = runKickAppSmoke();
            if (error.isNotEmpty()) {
                std::cerr << "FAIL: " << error << std::endl;
                setApplicationReturnValue(3);
            }
#endif
#if JUCE_MAC
            if (juce::SystemStats::getEnvironmentVariable("KICK_TEST_DISPLAY_ONLY", {}).isNotEmpty())
            {
                if (auto* hw = dynamic_cast<HomeWindow*>(w.get()))
                    hw->setViewMode(kick_mac::KickHardwareChassis::ViewMode::DisplayOnly);
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
            const auto screens = juce::SystemStats::getEnvironmentVariable("KICK_SMOKE_SCREENS", {});
            const auto shotsDir = juce::SystemStats::getEnvironmentVariable("KICK_SMOKE_SHOTS_DIR", {});
            if (screens.isNotEmpty() && shotsDir.isNotEmpty())
            {
                juce::File(shotsDir).createDirectory();
                smokeScreenShots(juce::StringArray::fromTokens(screens, ",", ""), juce::File(shotsDir), 0);
                return;
            }
            quit();
        });

#if PERFETTO
        MelatoninPerfetto::get().beginSession();
#endif
    }

    void shutdown() override
    {
#if PERFETTO
        MelatoninPerfetto::get().endSession();
#endif
        // Add your application's shutdown code here ...
#if JUCE_LINUX || __linux__
        if (hwBridge) hwBridge->stop();
        hwBridge.reset();
#endif
        Controls::InputEvents::shutdown();
        mt::getAudioInterface().closeProject();
        w = nullptr;
        adapter = nullptr;
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        if (ui::RecentProject::isProjectLoading) {
            juce::Timer::callAfterDelay(100, [this] { systemRequestedQuit(); });
            return;
        }
        // Check while the UI and live programs still exist. A rejected save
        // leaves the project open and its global error visible for recovery.
        if (!mt::getAudioInterface().saveProject()) return;
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
    class HomeWindow : public juce::DocumentWindow, public juce::KeyListener
    {
    public:
        explicit HomeWindow( const juce::String& name )
            : juce::DocumentWindow( name, juce::Colours::black, juce::DocumentWindow::allButtons )
        {
            setUsingNativeTitleBar( true );
            setResizable( true, true );

            chassisComponent = std::make_unique<kick_mac::KickHardwareChassis>();
            setContentNonOwned( chassisComponent.get(), false );

            addKeyListener( this );
#if defined( KICK_KEYBOARD_PANEL )
            addKeyListener( &contracts::getKeypressDetector() );
#endif

            menuModel = std::make_unique<kick_mac::KickMacMenuModel>( [this]( int cmd ) {
                handleMenuCommand( cmd );
            } );
            juce::MenuBarModel::setMacMainMenu( menuModel.get() );

            // Start in Hardware Chassis view by default
            setViewMode( kick_mac::KickHardwareChassis::ViewMode::HardwareChassis );
        }

        ~HomeWindow() override
        {
            removeKeyListener( this );
            juce::MenuBarModel::setMacMainMenu( nullptr );
            menuModel.reset();
            clearContentComponent();
            chassisComponent.reset();
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

        void handleMenuCommand( int cmd )
        {
            switch( cmd )
            {
                case kick_mac::KickMacMenuModel::idHardwareView:
                    setViewMode( kick_mac::KickHardwareChassis::ViewMode::HardwareChassis );
                    break;
                case kick_mac::KickMacMenuModel::idDisplayOnlyView:
                    setViewMode( kick_mac::KickHardwareChassis::ViewMode::DisplayOnly );
                    break;
                case kick_mac::KickMacMenuModel::idMobileView:
                    setViewMode( kick_mac::KickHardwareChassis::ViewMode::MobileTLayout );
                    break;
                case kick_mac::KickMacMenuModel::idMobileBottomPads:
                    if( chassisComponent != nullptr && chassisComponent->getMobileLayout() != nullptr )
                        chassisComponent->getMobileLayout()->setPanelState( kick_mobile::KickMobileTLayout::PanelState::BottomPads );
                    break;
                case kick_mac::KickMacMenuModel::idMobileMasterStrip:
                    if( chassisComponent != nullptr && chassisComponent->getMobileLayout() != nullptr )
                        chassisComponent->getMobileLayout()->setPanelState( kick_mobile::KickMobileTLayout::PanelState::MasterStrip );
                    break;
                case kick_mac::KickMacMenuModel::idMobileQLinkStrip:
                    if( chassisComponent != nullptr && chassisComponent->getMobileLayout() != nullptr )
                        chassisComponent->getMobileLayout()->setPanelState( kick_mobile::KickMobileTLayout::PanelState::QLinkStrip );
                    break;
                case kick_mac::KickMacMenuModel::idMobileReturnDaw:
                    if( chassisComponent != nullptr && chassisComponent->getMobileLayout() != nullptr )
                        chassisComponent->getMobileLayout()->setPanelState( kick_mobile::KickMobileTLayout::PanelState::DAW );
                    break;
                case kick_mac::KickMacMenuModel::idToggleView:
                    toggleViewMode();
                    break;
                case kick_mac::KickMacMenuModel::idToggleDrawer:
                    kick_ui::KickHome::Instance().setHardwareDrawerVisible( !kick_ui::KickHome::Instance().isHardwareDrawerVisible() );
                    if( menuModel ) menuModel->menuItemsChanged();
                    break;
                case kick_mac::KickMacMenuModel::idScale100:
                    setWindowScale( 1.0f );
                    break;
                case kick_mac::KickMacMenuModel::idScale120:
                    setWindowScale( 1.2f );
                    break;
                case kick_mac::KickMacMenuModel::idScale140:
                    setWindowScale( 1.4f );
                    break;
                case kick_mac::KickMacMenuModel::idScaleFit:
                    fitToScreen();
                    break;
                case kick_mac::KickMacMenuModel::idProjectDemo1:
                    loadDemoProject( 1 );
                    break;
                case kick_mac::KickMacMenuModel::idProjectBrowser:
                    kick_ui::signals::sigScreenClose( kick_ui::Screen::RecentProject );
                    break;
                case kick_mac::KickMacMenuModel::idProjectPlayPause:
                    if( chassisComponent != nullptr )
                        chassisComponent->sendButtonMessage( contracts::DeviceControlContracts::EN_PLAY, true );
                    break;
                default:
                    break;
            }
        }

        void loadDemoProject( int demoNumber )
        {
            auto demoName = juce::String( "Demo " ) + juce::String::formatted( "%02d", demoNumber );
            auto demoDir = getAppDirs( MTDemos ).getChildFile( demoName );
            auto demoFile = demoDir.getChildFile( demoName + ".kickPrj" );
            if( !demoFile.existsAsFile() )
            {
                demoFile = juce::File( "Kick/demos" ).getChildFile( demoName ).getChildFile( demoName + ".kickPrj" );
            }
            if( !demoFile.existsAsFile() )
            {
                demoFile = juce::File( "build/demos" ).getChildFile( demoName ).getChildFile( demoName + ".kickPrj" );
            }
            if( demoFile.existsAsFile() )
            {
                ui::LoadProjectAndAnimation::getInstance().startLoading( demoFile );
            }
            else
            {
                std::cerr << "Demo project not found: " << demoFile.getFullPathName().toStdString() << std::endl;
            }
        }

        void toggleViewMode()
        {
            if( chassisComponent == nullptr ) return;
            auto nextMode = ( chassisComponent->getViewMode() == kick_mac::KickHardwareChassis::ViewMode::HardwareChassis )
                                ? kick_mac::KickHardwareChassis::ViewMode::DisplayOnly
                                : kick_mac::KickHardwareChassis::ViewMode::HardwareChassis;
            setViewMode( nextMode );
        }

        void setViewMode( kick_mac::KickHardwareChassis::ViewMode mode )
        {
            if( chassisComponent == nullptr ) return;

            chassisComponent->setViewMode( mode );

            if( mode == kick_mac::KickHardwareChassis::ViewMode::HardwareChassis )
            {
                getConstrainer()->setFixedAspectRatio( 700.0 / 710.0 );
                setResizeLimits( 500, 507, 1600, 1622 );

                auto userArea = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
                int targetH = juce::jmin( 852, (int)( userArea.getHeight() * 0.90f ) );
                int targetW = (int)std::round( targetH * ( 700.0f / 710.0f ) );
                setContentComponentSize( targetW, targetH );
                centreWithSize( getWidth(), getHeight() );
                setName( juce::String::fromUTF8( "Behringer Kick - Hardver nézet" ) );
            }
            else if( mode == kick_mac::KickHardwareChassis::ViewMode::DisplayOnly )
            {
                getConstrainer()->setFixedAspectRatio( 1024.0 / 600.0 );
                setResizeLimits( 512, 300, 2048, 1200 );

                setContentComponentSize( 1024, 600 );
                centreWithSize( getWidth(), getHeight() );
                setName( juce::String::fromUTF8( "Behringer Kick - Kijelző nézet" ) );
            }
            else // MobileTLayout
            {
                getConstrainer()->setFixedAspectRatio( 852.0 / 393.0 );
                setResizeLimits( 500, 230, 1704, 786 );

                setContentComponentSize( 852, 393 );
                centreWithSize( getWidth(), getHeight() );
                setName( juce::String::fromUTF8( "Behringer Kick - Mobil nézet (iPhone)" ) );
            }

            if( menuModel != nullptr )
            {
                int modeIdx = ( mode == kick_mac::KickHardwareChassis::ViewMode::HardwareChassis ) ? 0
                            : ( mode == kick_mac::KickHardwareChassis::ViewMode::DisplayOnly ) ? 1 : 2;
                menuModel->setActiveViewMode( modeIdx );
            }
        }

        void setWindowScale( float scaleFactor )
        {
            if( chassisComponent == nullptr ) return;
            if( chassisComponent->getViewMode() == kick_mac::KickHardwareChassis::ViewMode::HardwareChassis )
            {
                int w = (int)std::round( 700.0f * scaleFactor );
                int h = (int)std::round( 710.0f * scaleFactor );
                setContentComponentSize( w, h );
                centreWithSize( getWidth(), getHeight() );
            }
            else
            {
                int w = (int)std::round( 1024.0f * scaleFactor );
                int h = (int)std::round( 600.0f * scaleFactor );
                setContentComponentSize( w, h );
                centreWithSize( getWidth(), getHeight() );
            }
        }

        void fitToScreen()
        {
            if( chassisComponent == nullptr ) return;
            auto userArea = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
            if( chassisComponent->getViewMode() == kick_mac::KickHardwareChassis::ViewMode::HardwareChassis )
            {
                int maxH = (int)( userArea.getHeight() * 0.92f );
                int maxW = (int)( userArea.getWidth() * 0.92f );
                float scale = juce::jmin( (float)maxW / 700.0f, (float)maxH / 710.0f );
                int w = (int)std::round( 700.0f * scale );
                int h = (int)std::round( 710.0f * scale );
                setContentComponentSize( w, h );
                centreWithSize( getWidth(), getHeight() );
            }
            else
            {
                int maxH = (int)( userArea.getHeight() * 0.92f );
                int maxW = (int)( userArea.getWidth() * 0.92f );
                float scale = juce::jmin( (float)maxW / 1024.0f, (float)maxH / 600.0f );
                int w = (int)std::round( 1024.0f * scale );
                int h = (int)std::round( 600.0f * scale );
                setContentComponentSize( w, h );
                centreWithSize( getWidth(), getHeight() );
            }
        }

        bool handleKeyPress( const juce::KeyPress& key )
        {
            // Cmd+1 or F1 -> Hardware Chassis View
            if( ( key.isKeyCode( '1' ) && key.getModifiers().isCommandDown() ) || key.isKeyCode( juce::KeyPress::F1Key ) )
            {
                setViewMode( kick_mac::KickHardwareChassis::ViewMode::HardwareChassis );
                return true;
            }

            // Cmd+2 or F2 -> Display Only View
            if( ( key.isKeyCode( '2' ) && key.getModifiers().isCommandDown() ) || key.isKeyCode( juce::KeyPress::F2Key ) )
            {
                setViewMode( kick_mac::KickHardwareChassis::ViewMode::DisplayOnly );
                return true;
            }

            // Cmd+3 or F3 -> Mobile T-Layout View
            if( ( key.isKeyCode( '3' ) && key.getModifiers().isCommandDown() ) || key.isKeyCode( juce::KeyPress::F3Key ) )
            {
                setViewMode( kick_mac::KickHardwareChassis::ViewMode::MobileTLayout );
                return true;
            }

            // Arrow Keys in Mobile mode (Up = Bottom Pads, Down = DAW, Right = Master, Left = Q-Links)
            if( chassisComponent != nullptr && chassisComponent->getViewMode() == kick_mac::KickHardwareChassis::ViewMode::MobileTLayout )
            {
                if( auto* ml = chassisComponent->getMobileLayout() )
                {
                    if( ml->handleKeyboardNav( key ) ) return true;
                }
            }

            // F5 -> Toggle View
            if( key.isKeyCode( juce::KeyPress::F5Key ) )
            {
                toggleViewMode();
                return true;
            }

            // Cmd+D -> Load Demo 01 (Drum & Bass Kit)
            if( ( key.isKeyCode( 'd' ) || key.isKeyCode( 'D' ) ) && key.getModifiers().isCommandDown() )
            {
                loadDemoProject( 1 );
                return true;
            }

            // Cmd+O -> Open Projects / Demos List
            if( ( key.isKeyCode( 'o' ) || key.isKeyCode( 'O' ) ) && key.getModifiers().isCommandDown() )
            {
                kick_ui::signals::sigScreenClose( kick_ui::Screen::RecentProject );
                return true;
            }

            return false;
        }

        bool keyPressed( const juce::KeyPress& key ) override
        {
            if( handleKeyPress( key ) ) return true;
            return juce::DocumentWindow::keyPressed( key );
        }

        bool keyPressed( const juce::KeyPress& key, juce::Component* /*originatingComponent*/ ) override
        {
            return handleKeyPress( key );
        }

    private:
        std::unique_ptr<kick_mac::KickHardwareChassis> chassisComponent;
        std::unique_ptr<kick_mac::KickMacMenuModel>    menuModel;
    };
#else
#if JUCE_IOS
    class HomeWindow : public juce::DocumentWindow
    {
    public:
        explicit HomeWindow( const juce::String& name )
            : juce::DocumentWindow( name, juce::Colours::black, 0 )
        {
            setTitle( name );
            mobileLayout = std::make_unique<kick_mobile::KickMobileTLayout>();
            setContentNonOwned( mobileLayout.get(), false );
        }

        ~HomeWindow() override
        {
            clearContentComponent();
            mobileLayout.reset();
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
        std::unique_ptr<kick_mobile::KickMobileTLayout> mobileLayout;
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
#endif

private:
    std::unique_ptr<juce::DocumentWindow> w;
    std::unique_ptr<KickDeviceAdapter>    adapter;
    std::unique_ptr<KickAudioEngineAdapter>    engineAdapter;
    std::shared_ptr<contracts::AudioInterface> audioInterface;
#if JUCE_LINUX || __linux__
    std::unique_ptr<KickHardwareBridge>        hwBridge;
#endif
#if PERFETTO
    std::unique_ptr<perfetto::TracingSession> tracingSession;
#endif
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION( AudioEngineApplication )
