#pragma once
namespace ui
{
    // The ONE project load path (message thread API, any thread may call startLoading): requests from the Projects list,
    // the file browser, auto-load and the hardware bridge's default project all end up here. One load runs at a time on a
    // worker thread with the loading animation; further requests wait in a queue (kick_load::LoadQueue) and a request for a
    // path that is already running or queued is dropped. RecentProject::isProjectLoading is true from the first request until the
    // queue is drained. Results go to kick_startup::state() (step "Project"), failures also to a dialog and kick_log.
    class LoadProjectAndAnimation
        : private juce::Thread
        , private mt::utils::LoadAnimation
    {
    public:
        ~LoadProjectAndAnimation();

        static LoadProjectAndAnimation& getInstance()
        {
            static auto* instance = new LoadProjectAndAnimation;
            return *instance;
        }

        void startLoading( juce::File project );

        // Device default project (the MPC bridge sets Demo 01) used by the start-up decision when nothing else applies.
        static void setDefaultProject( const juce::File& project ) { defaultProject() = project; }

        // The single start-up decision (kick_load::chooseStartupProject): last project / default project / none (safe mode,
        // auto-load off). Call once, when the Projects screen first shows.
        void requestStartupProject();

    private:
        LoadProjectAndAnimation();

        static juce::File& defaultProject()
        {
            static juce::File f;
            return f;
        }

        juce::File            proj;
        kick_load::LoadQueue  queue; // message thread only
        unsigned              generation{ 0 };
        bool                  startupRequested{ false };

        void begin( const juce::File& project );
        void finished( bool loaded, const juce::File& project );
        void run() override;
    };
}
