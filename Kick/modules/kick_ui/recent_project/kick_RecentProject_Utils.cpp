#include "kick_ui/recent_project/kick_RecentProject_Utils.h"
namespace ui
{
    /////////////////////////////////////////////////////////////////
    ////////   LoadProjectAndAnimation   ////////////////////////////
    /////////////////////////////////////////////////////////////////

    LoadProjectAndAnimation::LoadProjectAndAnimation()
        : juce::Thread( "LoadProject" )
    {
    }

    LoadProjectAndAnimation::~LoadProjectAndAnimation()
    {
        stopThread(-1);
    }

    void LoadProjectAndAnimation::startLoading( juce::File project )
    {
        // Everything below (queue, animation, state) lives on the message thread.
        if( !juce::MessageManager::getInstance()->isThisTheMessageThread() )
        {
            juce::MessageManager::callAsync( [this, project] { startLoading( project ); } );
            return;
        }

        const auto path = project.getFullPathName().toStdString();
        switch( queue.request( path ) )
        {
            case kick_load::LoadQueue::Result::StartNow:
                begin( project );
                break;
            case kick_load::LoadQueue::Result::Queued:
                ui::RecentProject::isProjectLoading = true;
                kick_log( "info", "[INFO] [PROJ] load of '%s' queued behind '%s' (%zu waiting)\n", path.c_str(), queue.current().c_str(), queue.pending() );
                break;
            case kick_load::LoadQueue::Result::Duplicate:
                kick_log( "info", "[INFO] [PROJ] load of '%s' ignored: already loading or queued\n", path.c_str() );
                break;
        }
    }

    void LoadProjectAndAnimation::begin( const juce::File& project )
    {
        ++generation;
        ui::RecentProject::isProjectLoading = true;
        proj                                = project;
        kick_startup::state().set( kick_startup::Step::Project, kick_startup::StepState::Running, ( "loading " + project.getFileNameWithoutExtension() ).toStdString() );
        // The previous worker has posted its result and is leaving run(); make sure it is gone before the thread is reused.
        waitForThreadToExit( 5000 );
        startThread();
        if( !isAnimationRunning() ) startAnimation();
    }

    void LoadProjectAndAnimation::run()
    {
        // Worker thread: only the engine load. Everything else is published on the message thread.
        juce::File file   = proj;
        const auto loaded = mt::getAudioInterface().loadProject( file );
        // Publish completion after all message-thread persistence barriers.
        // Quit can then join this worker without blocking a barrier it needs.
        juce::MessageManager::callAsync( [this, loaded, file] { finished( loaded, file ); } );
    }

    void LoadProjectAndAnimation::finished( bool loaded, const juce::File& project )
    {
        auto&      startup = kick_startup::state();
        const auto name    = project.getFileNameWithoutExtension();
        if( loaded )
        {
            startup.set( kick_startup::Step::Project, kick_startup::StepState::Ok, name.toStdString() );
        }
        else
        {
            const auto reason = project.exists() ? juce::String( "the project file is invalid or cannot be read; see /data/kick.log" ) : juce::String( "not found: " ) + project.getFullPathName();
            const auto p      = startup.fail( kick_startup::Fault::ProjectLoad, ( name + ": " + reason ).toStdString() );
            kick_log( "error", "[ERROR] [PROJ] %s\n", p.cause.c_str() );
        }

        std::string next;
        if( queue.onFinished( next ) )
        {
            begin( juce::File( juce::String( next ) ) ); // the animation keeps running; one screen switch after the last load
            return;
        }

        ui::RecentProject::isProjectLoading = false;
        const auto gen                      = generation;
        if( loaded )
        {
            // Let the engine's last UI updates land, then leave the animation and show the main screen.
            juce::Timer::callAfterDelay( 200, [this, gen]() {
                if( gen != generation ) return; // a newer load started meanwhile; it owns the animation now
                stopAnimation();
                kick_ui::signals::sigScreenOpen( kick_ui::Screen::HomeListView );
            } );
        }
        else
            stopAnimation();
    }

    void LoadProjectAndAnimation::requestStartupProject()
    {
        if( startupRequested ) return;
        startupRequested = true;

        auto&                   settings = mt::getSettings();
        kick_load::StartupInputs in;
        in.safeMode       = kick_startup::state().safeMode();
        in.autoLoad       = settings.projects.isAutoLoad();
        const auto last   = settings.projects.currentProject();
        in.lastProject    = last.toStdString();
        in.lastExists     = last.isNotEmpty() && juce::File( last ).exists();
        const auto& dflt  = defaultProject();
        in.fallbackProject = dflt.getFullPathName().toStdString();
        in.fallbackExists  = dflt != juce::File() && dflt.exists();

        const auto choice = kick_load::chooseStartupProject( in );
        kick_log( "info", "[INFO] [PROJ] startup project: %s ('%s')\n", kick_load::reasonText( choice.reason ), choice.path.c_str() );
        if( choice.path.empty() )
        {
            if( !in.safeMode ) kick_startup::state().set( kick_startup::Step::Project, kick_startup::StepState::Skipped, kick_load::reasonText( choice.reason ) );
            return;
        }
        // The default project (a demo) must not become the "last project" / recent entry.
        if( choice.reason == kick_load::StartupReason::Fallback ) ui::RecentProject::enableRecentProjVTreeEntry = false;
        startLoading( juce::File( juce::String( choice.path ) ) );
    }
}
