// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../../hardware/source/kick_PlatformServices.h"
#if JUCE_LINUX || JUCE_BSD
    #include <unistd.h>
#endif

namespace ui::power
{
    namespace
    {
        constexpr int kShutDown = 11;
        constexpr int kRestart  = 12;

        kick_power::Runner& runner()
        {
            static kick_power::Runner r;
            return r;
        }
        bool& windowOpen()
        {
            static bool open = false;
            return open;
        }

        // Shut Down / Restart / Cancel. Cancel (and the dialog's own close / the hardware close key) = any other exit code.
        class PowerPopup : public juce::Component
        {
        public:
            PowerPopup()
                : message( "What do you want to do?" ), hintLabel( "Unsaved changes of the project are asked before the device goes down" )
            {
                mt::utils::addAndMakeVisible( this, message, hintLabel, shutDown, restart, cancel );
                for( auto* l : { &message, &hintLabel } )
                {
                    l->setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
                    l->setValueJustification( juce::Justification::centred );
                    l->setTextColour( ui::Theme::textColour( 1 ) );
                }
                message.setFontHeight( 20.0f );
                hintLabel.setFontHeight( 13.0f );
                message.setText( "What do you want to do?", juce::dontSendNotification );
                hintLabel.setText( "Unsaved changes are asked before the device goes down", juce::dontSendNotification );
                for( auto* b : { &shutDown, &restart, &cancel } )
                {
                    b->setValueColour( ui::Theme::textColour( 1 ) );
                    b->setValueFontHeight( 18 );
                }
                shutDown.onClick = [this]() { mt::utils::exitDialog( this, kShutDown ); };
                restart.onClick  = [this]() { mt::utils::exitDialog( this, kRestart ); };
                cancel.onClick   = [this]() { mt::utils::exitDialog( this, UI::CANCEL ); };
            }

        private:
            ui::KickLabel  message, hintLabel;
            ui::KickButton shutDown{ "ShutDown", "Shut Down" }, restart{ "Restart", "Restart" }, cancel{ "PowerCancel", "Cancel" };
            void           resized() override
            {
                message.setBoundsRelative( 0.05f, 0.08f, 0.9f, 0.14f );
                hintLabel.setBoundsRelative( 0.05f, 0.23f, 0.9f, 0.08f );
                shutDown.setBoundsRelative( 0.20f, 0.38f, 0.60f, 0.15f );
                restart.setBoundsRelative( 0.20f, 0.56f, 0.60f, 0.15f );
                cancel.setBoundsRelative( 0.20f, 0.74f, 0.60f, 0.15f );
            }
        };

        kick_power::Env makeEnv()
        {
            using namespace kick_power;
            Env e;
            e.status = [] {
                auto&  ae = mt::getAudioInterface();
                Status s;
                s.projectLoaded    = ae.isProjectLoaded();
                s.recording        = s.projectLoaded && ( ae.isSequenceRecording() || ae.isLooperRecording() );
                s.recording        = s.recording || ae.isRecorderRunning();
                s.transportRunning = s.projectLoaded && ae.isSequencePlaying();
                return s;
            };
            e.confirmRecording = []( Action a, std::function<void( bool )> cb ) {
                project_actions::confirm( "RECORDING", juce::String( "A recording is running. Stop it and " ) + kick_power::actionName( a ) + "?", std::move( cb ) );
            };
            e.stopTransport = [] {
                auto& ae = mt::getAudioInterface();
                if( ae.isLooperRecording() )
                    ae.setLooperRecord( false );
                if( ae.isRecorderRunning() )
                    ae.stopRecorder();
                ae.looperStop();
                ae.setSequenceStop();
                ae.stopPreview();
            };
            e.leaveProject = []( std::function<void()> proceed, std::function<void()> cancelled ) { project_actions::leaveProject( std::move( proceed ), std::move( cancelled ) ); };
            e.closeProject = [] { return mt::getAudioInterface().closeProject(); };
            e.report       = []( const std::string& m ) { project_actions::warn( juce::String( m ) ); };
            e.flushLogs    = [] { /* kick_log writes every line straight to the file (write(2), no stdio buffer) */ };
            e.syncDisk     = [] {
#if JUCE_LINUX || JUCE_BSD
                ::sync(); // projects, prefs and kick.log are on the flash: the poweroff -f / reboot -f that follows does not sync
#endif
            };
            e.powerOff = [] { return mt::PlatformServices::getInstance().powerOff(); };
            e.reboot   = [] { return mt::PlatformServices::getInstance().reboot(); };
            e.log      = []( const char* m ) { kick_log( "info", "[INFO] [POWER] %s\n", m ); };
            return e;
        }
    }

    bool isBusy() { return runner().busy(); }
    bool isWindowOpen() { return windowOpen(); }

    void request( kick_power::Action action, std::function<void( kick_power::Result )> finished )
    {
        runner().run( makeEnv(), action, [finished]( kick_power::Result r ) {
            if( r == kick_power::Result::Busy )
                kick_log( "info", "[INFO] [POWER] request ignored: another shutdown / restart is running\n" );
            if( finished )
                finished( r );
        } );
    }

    void showPowerWindow( const char* source )
    {
        if( !juce::MessageManager::getInstance()->isThisTheMessageThread() )
        {
            juce::MessageManager::callAsync( [source] { showPowerWindow( source ); } );
            return;
        }
        if( ui::RecentProject::isProjectLoading || ui::BackgroundTask::activeCount() > 0 )
        {
            juce::Timer::callAfterDelay( 200, [source] { showPowerWindow( source ); } ); // after the load / the copy, like a quit request
            return;
        }
        if( windowOpen() || runner().busy() )
        {
            kick_log( "info", "[INFO] [POWER] Power window request from %s ignored (%s)\n", source, windowOpen() ? "already open" : "a request is running" );
            return;
        }
        kick_log( "info", "[INFO] [POWER] Power window requested by %s\n", source );
        windowOpen() = true;
        auto p       = new PowerPopup();
        mt::utils::showDialog(
            "POWER", p, [p]( int R ) {
                delete p;
                windowOpen() = false;
                if( R == kShutDown )
                    request( kick_power::Action::ShutDown );
                else if( R == kRestart )
                    request( kick_power::Action::Restart );
            },
            ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
    }
}
