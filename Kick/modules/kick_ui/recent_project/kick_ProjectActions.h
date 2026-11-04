// SPDX-License-Identifier: AGPL-3.0-or-later
// Project actions shared by the Projects screen, the file browser, the Menu / hardware SAVE, the project load path and the quit
// request: the unsaved-changes question (Save / Don't save / Cancel), Save, Save As / Save a Copy / Save as Template, a background
// task runner with a progress dialog (copy, delete, duplicate: never the message thread, always cancellable) and the project
// thumbnail written on save. The decisions are in the JUCE-free kick_unsaved.h / kick_project_ops.h / kick_fileOps.h.
#pragma once

#include "kick_unsaved.h"
#include "kick_project_ops.h"
#include "../modes/filebrowser/kick_fileOps.h"

namespace ui
{
    // Where a project load came from: decides whether it enters the Recent list / becomes the "last project", and whether a missing
    // project may be created (only New project).
    enum class LoadSource
    {
        User,     // tapped in a list, opened in the file browser, Save As copy: Recent + last project
        Auto,     // auto-load of the last project at start-up: Recent + last project
        Demo,     // factory demo: never Recent
        Template, // never Recent (the project made from it is a User load)
        Bridge    // device default project (Demo 01 on the MPC): never Recent
    };

    inline bool entersRecentList( LoadSource s ) { return s == LoadSource::User || s == LoadSource::Auto; }

    // Progress dialog of a background task.
    class ProgressPopup : public juce::Component
    {
    public:
        ProgressPopup( const juce::String& text, bool cancellable );
        void setProgress( int percent, const juce::String& text );

        std::function<void()> onCancel;

    private:
        ui::KickLabel  info;
        ui::KickLabel  detail;
        ui::KickButton cancelBtn{ "Cancel", "Cancel" };
        int            percent{ 0 };
        bool           showBar{ true };
        void           paint( juce::Graphics& ) override;
        void           resized() override;
    };

    // Runs `work` on its own thread (message thread API). A dialog shows the progress; Cancel (or the hardware cancel key) sets the
    // flag `work` polls. `done` runs on the message thread afterwards (also after a cancel). While a task runs, activeCount() > 0
    // (quit and project loads wait for it).
    class BackgroundTask : private juce::Thread
    {
    public:
        using Progress = std::function<void( int percent, const juce::String& text )>;
        using Work     = std::function<void( const std::atomic<bool>& cancel, const Progress& progress )>;

        static void run( const juce::String& title, bool cancellable, Work work, std::function<void()> done = {} );
        static int  activeCount();

    private:
        BackgroundTask( Work w, std::function<void()> d );
        void run() override;

        Work                  work;
        std::function<void()> done;
        std::atomic<bool>     cancel{ false };
        ProgressPopup*        popup{ nullptr };
    };

    namespace project_actions
    {
        enum class SaveAsMode { SaveAs, SaveCopy, Template };

        void warn( const juce::String& message );
        void infoDialog( const juce::String& title, const juce::StringArray& lines );
        // Text input with the virtual keyboard; `done` runs on the message thread after the keyboard closed (Apply only).
        void askText( const juce::String& title, const juce::String& initial, std::function<void( juce::String )> done );
        // yes / no question; `done( true )` on yes.
        void confirm( const juce::String& title, const juce::String& question, std::function<void( bool )> done );

        kick_unsaved::State unsavedState();
        // Runs `proceed` when the open project may be left (nothing to save / saved / "Don't save"); asks first when needed.
        void leaveProject( std::function<void()> proceed, std::function<void()> cancelled = {} );
        // Save button / hardware SAVE / Menu Save. A demo / template opens Save As instead. Returns false when nothing was saved.
        bool saveProject();
        // New project (Projects screen, file browser): a trimmed legal name that is not a folder on the disk; the open project is
        // saved / discarded as the user decides (the load path asks), then the empty project is made and opened.
        void createProject( const juce::String& rawName );
        // reload = the copy becomes the open project (SaveAs mode): it is loaded again from its new folder, so the engine's asset
        // folder is the new one. false when the caller loads something else next anyway.
        void saveAs( SaveAsMode mode, std::function<void( bool )> done = {}, bool reload = true );

        // 400 x 240 PNG of the screen next to the project file (thumbnail.png); called after a successful save.
        void writeThumbnail( const juce::File& projectDir );
        bool isProtectedProject( const juce::File& projectDir ); // open project, demos, templates
    }
}
