#pragma once
namespace ui
{
    // The ONE project load path (message thread API, any thread may call startLoading): requests from the Projects list,
    // the file browser, auto-load and the hardware bridge's default project all end up here. One load runs at a time on a
    // worker thread with the loading animation; further requests wait in a queue (kick_load::LoadQueue) and a request for a
    // path that is already running or queued is dropped. RecentProject::isProjectLoading is true from the first request until the
    // queue is drained. Results go to kick_startup::state() (step "Project"), failures also to a dialog and kick_log.
    // LoadSource says where a request came from: only User / Auto loads enter the Recent list (done here, on the message thread,
    // after a successful load: no global flag). Before the open project is left, the unsaved-changes question is asked
    // (project_actions::leaveProject). A missing project is an error, never created, unless createNew (New project).
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

        void startLoading( juce::File project, LoadSource source = LoadSource::User, bool createNew = false );

        // Device default project (the MPC bridge sets Demo 01) used by the start-up decision when nothing else applies.
        static void setDefaultProject( const juce::File& project ) { defaultProject() = project; }

        // The single start-up decision (kick_load::chooseStartupProject): last project / default project / none (safe mode,
        // auto-load off). Call once, when the Projects screen first shows.
        void requestStartupProject();

        // True while startup project decision has not settled or an async project load / post-load screen switch is in flight.
        bool isTransitionPending() const noexcept { return ui::RecentProject::isProjectLoading || !startupRequested || postLoadSwitchPending; }

    private:
        LoadProjectAndAnimation();

        static juce::File& defaultProject()
        {
            static juce::File f;
            return f;
        }

        struct Meta
        {
            LoadSource source{ LoadSource::User };
            bool       createNew{ false };
        };
        juce::File            proj;
        Meta                  current;
        std::map<std::string, Meta> meta; // by path, for the queued / running requests
        bool                  gateOpen{ false }; // the unsaved-changes question is on screen
        kick_load::LoadQueue  queue; // message thread only
        unsigned              generation{ 0 };
        bool                  startupRequested{ false };
        bool                  postLoadSwitchPending{ false };

        void enqueue( const juce::File& project, LoadSource source, bool createNew );
        void begin( const juce::File& project );
        void finished( bool loaded, const juce::File& project, const juce::String& error, LoadSource source );
        void run() override;
    };
}
