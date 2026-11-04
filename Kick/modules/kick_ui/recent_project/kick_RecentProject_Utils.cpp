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

    void LoadProjectAndAnimation::startLoading( juce::File project, LoadSource source, bool createNew )
    {
        // Everything below (queue, animation, state) lives on the message thread.
        if( !juce::MessageManager::getInstance()->isThisTheMessageThread() )
        {
            juce::MessageManager::callAsync( [this, project, source, createNew] { startLoading( project, source, createNew ); } );
            return;
        }
        // A background task (Save As, copy, delete) works on the file system: the load waits for it.
        if( BackgroundTask::activeCount() > 0 )
        {
            juce::Timer::callAfterDelay( 200, [this, project, source, createNew] { startLoading( project, source, createNew ); } );
            return;
        }
        // The open project is left: nothing to save / saved / "Don't save", else the user stays where they are.
        if( !queue.busy() )
        {
            if( gateOpen )
            {
                kick_log( "info", "[INFO] [PROJ] load of '%s' ignored: the unsaved-changes question is open\n", project.getFullPathName().toRawUTF8() );
                return;
            }
            gateOpen = true;
            project_actions::leaveProject(
                [this, project, source, createNew] {
                    gateOpen = false;
                    enqueue( project, source, createNew );
                },
                [this] { gateOpen = false; } );
            return;
        }
        enqueue( project, source, createNew );
    }

    void LoadProjectAndAnimation::enqueue( const juce::File& project, LoadSource source, bool createNew )
    {
        const auto path = project.getFullPathName().toStdString();
        switch( queue.request( path ) )
        {
            case kick_load::LoadQueue::Result::StartNow:
                meta[path] = { source, createNew };
                begin( project );
                break;
            case kick_load::LoadQueue::Result::Queued:
                meta[path] = { source, createNew };
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
        const auto key                      = project.getFullPathName().toStdString();
        current                             = meta.count( key ) ? meta[key] : Meta{};
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
        const auto loaded = mt::getAudioInterface().loadProject( file, current.createNew );
        // Publish completion after all message-thread persistence barriers.
        // Quit can then join this worker without blocking a barrier it needs.
        const auto error = loaded ? juce::String() : mt::getAudioInterface().getProjectLoadError();
        const auto meta_ = current;
        juce::MessageManager::callAsync( [this, loaded, file, error, meta_] { finished( loaded, file, error, meta_.source ); } );
    }

    void LoadProjectAndAnimation::finished( bool loaded, const juce::File& project, const juce::String& error, LoadSource source )
    {
        auto&      startup = kick_startup::state();
        const auto name    = project.getFileNameWithoutExtension();
        meta.erase( project.getFullPathName().toStdString() );
        if( loaded )
        {
            startup.set( kick_startup::Step::Project, kick_startup::StepState::Ok, name.toStdString() );
            if( entersRecentList( source ) )
            {
                // The Recent list stores project folders; the source (not a global flag) decides, and a failed load never leaves a
                // flag behind.
                juce::File dir = project.existsAsFile() ? project.getParentDirectory() : project;
                mt::getSettings().projects.rearrangeProject( dir );
            }
        }
        else
        {
            const auto reason = error.isNotEmpty() ? error : ( project.exists() ? juce::String( "the project file is invalid or cannot be read; see /data/kick.log" ) : juce::String( "not found: " ) + project.getFullPathName() );
            const auto p      = startup.fail( kick_startup::Fault::ProjectLoad, ( name + ": " + reason ).toStdString() );
            kick_log( "error", "[ERROR] [PROJ] %s\n", p.cause.c_str() );
            const bool missing = error.startsWith( "The project was not found" );
            juce::File dir     = project.existsAsFile() ? project.getParentDirectory() : project;
            if( missing && entersRecentList( source ) && !project.exists() )
            {
                project_actions::confirm( "PROJECT NOT FOUND", "'" + name + "' is gone. Remove it from the list?", [dir]( bool yes ) {
                    if( yes )
                    {
                        juce::File d = dir;
                        mt::getSettings().projects.removeProject( d );
                    }
                } );
            }
            else
                project_actions::warn( "The project could not be opened: " + reason.substring( 0, 120 ) );
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

        // "New Project" on launch: a new project (from the chosen template, or empty) named by the Project Defaults naming style.
        if( !in.safeMode && settings.projects.onLaunch() == "New Project" && !mt::getAudioInterface().isProjectLoaded() )
        {
            const auto root  = getAppDirs( MTProjects );
            const auto style = settings.projects.namingStyle();
            const auto stamp = juce::Time::getCurrentTime().formatted( "%Y-%m-%d %H.%M" ).toStdString();
            const auto name  = kick_project::launchProjectName( style == "CUSTOM" ? settings.projects.namePrefix().toStdString() : std::string(), stamp,
                                                               [&]( const std::string& n ) { return root.getChildFile( juce::String( n ) ).exists(); } );
            const juce::File dir  = root.getChildFile( juce::String( name ) );
            const juce::File tmpl( settings.projects.launchTemplate() );
            bool             fromTemplate = false;
            if( settings.projects.launchTemplate().isNotEmpty() && tmpl.isDirectory() )
            {
                std::atomic<bool> noCancel{ false };
                fromTemplate = kick_project::duplicate( tmpl.getFullPathName().toStdString(), root.getFullPathName().toStdString(), name, noCancel ) == kick_project::OpResult::Ok;
                if( !fromTemplate )
                    kick_log( "warn", "[WARN] [PROJ] launch template '%s' could not be copied; starting with an empty project\n", tmpl.getFullPathName().toRawUTF8() );
            }
            if( !fromTemplate )
                settings.projects.createProject( dir );
            kick_log( "info", "[INFO] [PROJ] startup project: new project '%s'%s\n", name.c_str(), fromTemplate ? " from the template" : "" );
            startLoading( dir, LoadSource::User, !fromTemplate );
            return;
        }

        const auto choice = kick_load::chooseStartupProject( in );
        kick_log( "info", "[INFO] [PROJ] startup project: %s ('%s')\n", kick_load::reasonText( choice.reason ), choice.path.c_str() );
        if( choice.path.empty() )
        {
            if( !in.safeMode ) kick_startup::state().set( kick_startup::Step::Project, kick_startup::StepState::Skipped, kick_load::reasonText( choice.reason ) );
            return;
        }
        // The default project (a demo) must not become the "last project" / recent entry: its source says so.
        startLoading( juce::File( juce::String( choice.path ) ), choice.reason == kick_load::StartupReason::Fallback ? LoadSource::Bridge : LoadSource::Auto );
    }
}
