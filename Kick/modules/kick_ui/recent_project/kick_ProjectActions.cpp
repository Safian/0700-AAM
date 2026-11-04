// SPDX-License-Identifier: AGPL-3.0-or-later
namespace ui
{
    ///////////////////////////////////////////////////////////////////////////
    // ProgressPopup
    ///////////////////////////////////////////////////////////////////////////
    ProgressPopup::ProgressPopup( const juce::String& text, bool cancellable )
        : info( text )
    {
        mt::utils::addAndMakeVisible( this, info, detail );
        for( auto* l : { &info, &detail } )
        {
            l->setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
            l->setValueJustification( juce::Justification::centred );
            l->setTextColour( ui::Theme::textColour( 1 ) );
        }
        info.setFontHeight( 18.0f );
        detail.setFontHeight( 15.0f );
        info.setText( text, juce::dontSendNotification );
        addChildComponent( cancelBtn );
        cancelBtn.setVisible( cancellable );
        cancelBtn.onClick = [this]() {
            cancelBtn.setEnabled( false );
            detail.setText( "Cancelling...", juce::dontSendNotification );
            if( onCancel )
                onCancel();
        };
    }

    void ProgressPopup::setProgress( int p, const juce::String& text )
    {
        percent = juce::jlimit( 0, 100, p );
        detail.setText( text, juce::dontSendNotification );
        repaint();
    }

    void ProgressPopup::paint( juce::Graphics& g )
    {
        if( !showBar )
            return;
        auto bar = getLocalBounds().withSizeKeepingCentre( getWidth() * 0.8f, 14 ).withY( static_cast<int>( getHeight() * 0.55f ) );
        mt::utils::fillRoundedRect( g, bar, ui::Theme::backgroundColour( 9 ), 4.0f );
        mt::utils::fillRoundedRect( g, bar.withWidth( bar.getWidth() * percent / 100 ), ui::Theme::outlineColour( 1 ), 4.0f );
    }

    void ProgressPopup::resized()
    {
        info.setBoundsRelative( 0.05f, 0.30f, 0.9f, 0.12f );
        detail.setBoundsRelative( 0.05f, 0.42f, 0.9f, 0.10f );
        cancelBtn.setBoundsRelative( 0.35f, 0.70f, 0.3f, 0.12f );
    }

    ///////////////////////////////////////////////////////////////////////////
    // BackgroundTask
    ///////////////////////////////////////////////////////////////////////////
    namespace
    {
        std::atomic<int>& activeTasks()
        {
            static std::atomic<int> n{ 0 };
            return n;
        }
    }

    int BackgroundTask::activeCount() { return activeTasks().load(); }

    BackgroundTask::BackgroundTask( Work w, std::function<void()> d )
        : juce::Thread( "KickTask" ), work( std::move( w ) ), done( std::move( d ) )
    {
    }

    void BackgroundTask::run( const juce::String& title, bool cancellable, Work work, std::function<void()> done )
    {
        // Message thread API: the dialog and the bookkeeping live there.
        if( !juce::MessageManager::getInstance()->isThisTheMessageThread() )
        {
            juce::MessageManager::callAsync( [title, cancellable, work = std::move( work ), done = std::move( done )]() mutable { BackgroundTask::run( title, cancellable, std::move( work ), std::move( done ) ); } );
            return;
        }
        auto* task = new BackgroundTask( std::move( work ), std::move( done ) );
        auto* popup = new ProgressPopup( title, cancellable );
        task->popup = popup;
        popup->onCancel = [task]() { task->cancel.store( true ); };
        ++activeTasks();
        mt::utils::showDialog(
            title, popup, [task, popup]( int ) {
                // Closed by the hardware cancel key (or at the end): the task is told, the popup goes.
                task->cancel.store( true );
                task->popup = nullptr;
                delete popup; },
            { ui::layout::warningPopup }, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
        task->startThread();
    }

    void BackgroundTask::run()
    {
        Progress progress = [this]( int pct, const juce::String& text ) {
            juce::MessageManager::callAsync( [this, pct, text]() {
                if( popup != nullptr )
                    popup->setProgress( pct, text );
            } );
        };
        work( cancel, progress );
        // Posted after every progress message (FIFO), so `this` is still alive for those.
        juce::MessageManager::callAsync( [this]() {
            waitForThreadToExit( -1 );
            if( popup != nullptr )
            {
                auto* p = popup; // the dialog callback deletes it and clears `popup`
                mt::utils::exitDialog( p, UI::APPLY );
            }
            auto d = std::move( done );
            --activeTasks();
            delete this;
            if( d )
                d();
        } );
    }

    ///////////////////////////////////////////////////////////////////////////
    // project_actions
    ///////////////////////////////////////////////////////////////////////////
    namespace project_actions
    {
        namespace
        {
            // Message + one extra answer ("Don't save"); the dialog's own Apply / Cancel are Save / stay.
            class UnsavedPopup : public juce::Component
            {
            public:
                UnsavedPopup( const juce::String& text, const juce::String& hint )
                    : message( text ), hintLabel( hint )
                {
                    mt::utils::addAndMakeVisible( this, message, hintLabel, dontSave );
                    for( auto* l : { &message, &hintLabel } )
                    {
                        l->setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
                        l->setValueJustification( juce::Justification::centred );
                        l->setTextColour( ui::Theme::textColour( 1 ) );
                    }
                    message.setFontHeight( 18.0f );
                    hintLabel.setFontHeight( 14.0f );
                    message.setText( text, juce::dontSendNotification );
                    hintLabel.setText( hint, juce::dontSendNotification );
                    dontSave.setValueColour( ui::Theme::textColour( 1 ) );
                    dontSave.setValueFontHeight( 16 );
                    dontSave.onClick = [this]() { mt::utils::exitDialog( this, 2 ); };
                }

            private:
                ui::KickLabel  message, hintLabel;
                ui::KickButton dontSave{ "DontSave", "Don't save" };
                void           resized() override
                {
                    message.setBoundsRelative( 0.05f, 0.18f, 0.9f, 0.14f );
                    hintLabel.setBoundsRelative( 0.05f, 0.33f, 0.9f, 0.10f );
                    dontSave.setBoundsRelative( 0.30f, 0.55f, 0.40f, 0.12f );
                }
            };

            class InfoPopup : public juce::Component
            {
            public:
                explicit InfoPopup( const juce::StringArray& lines )
                {
                    for( const auto& l : lines )
                    {
                        auto* label = labels.add( new ui::KickLabel );
                        label->setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
                        label->setValueJustification( juce::Justification::centredLeft );
                        label->setTextColour( ui::Theme::textColour( 1 ) );
                        label->setFontHeight( 16.0f );
                        label->setText( l, juce::dontSendNotification );
                        addAndMakeVisible( label );
                    }
                }

            private:
                juce::OwnedArray<ui::KickLabel> labels;
                void                            resized() override
                {
                    auto b = getLocalBounds().reduced( 40, 20 ).withTrimmedBottom( 70 );
                    for( auto* l : labels )
                        l->setBounds( b.removeFromTop( 26 ) );
                }
            };
        }

        void warn( const juce::String& message )
        {
            auto p = new FilePopup( message, false, false );
            mt::utils::showDialog(
                "WARNING!", p, [p]( int ) { delete p; },
                ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
        }

        void infoDialog( const juce::String& title, const juce::StringArray& lines )
        {
            auto p = new InfoPopup( lines );
            mt::utils::showOptions(
                title, p, [p]( int ) { delete p; },
                ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
        }

        void askText( const juce::String& title, const juce::String& initial, std::function<void( juce::String )> done )
        {
            auto& keyboard          = ui::KickVirtualKeyBoard::getWithText( initial );
            keyboard.onEnterPressed = [&keyboard]( int ) {
                mt::utils::exitDialog( keyboard.getCurrentlyFocusedComponent(), UI::APPLY );
            };
            keyboard.show(
                title, ui::KickVirtualKeyBoard::Type::KEYS_LOWERCASE_ALPHABETIC,
                [&keyboard, done]( int R ) {
                    if( R == UI::APPLY )
                    {
                        const auto text = keyboard.getInputText();
                        juce::MessageManager::callAsync( [done, text]() { done( text ); } ); // after this keyboard has closed
                    }
                },
                ui::layout::keyBoardPopup );
        }

        void confirm( const juce::String& title, const juce::String& question, std::function<void( bool )> done )
        {
            auto p = new FilePopup{ question, true, true };
            mt::utils::showDialog(
                title, p, [p, done]( int R ) {
                    delete p;
                    if( done )
                        done( R == UI::APPLY ); },
                ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
        }

        kick_unsaved::State unsavedState()
        {
            auto&                 ae = mt::getAudioInterface();
            kick_unsaved::State s;
            s.projectLoaded    = ae.isProjectLoaded();
            s.dirty            = s.projectLoaded && ae.isProjectDirty();
            s.autoSave         = ae.isProjectAutoSave();
            s.readOnlyLocation = s.projectLoaded && ae.isProjectReadOnly();
            return s;
        }

        namespace
        {
            juce::String openProjectName()
            {
                auto& ae = mt::getAudioInterface();
                return ae.isProjectLoaded() ? ae.getProjectDir().getFileName() : juce::String();
            }

            kick_unsaved::Env makeEnv()
            {
                kick_unsaved::Env env;
                env.state = [] { return unsavedState(); };
                env.ask   = []( std::function<void( kick_unsaved::Choice )> cb ) {
                    const auto name = openProjectName();
                    auto       p    = new UnsavedPopup( "'" + name + "' has unsaved changes", mt::getAudioInterface().isProjectReadOnly() ? "A demo cannot be saved in place: the check saves a copy (Save As)" : "Check = save, Don't save = discard the changes, X = stay" );
                    mt::utils::showOptions(
                        "UNSAVED CHANGES", p, [p, cb]( int R ) {
                            delete p;
                            cb( R == UI::APPLY ? kick_unsaved::Choice::Save : ( R == 2 ? kick_unsaved::Choice::DontSave : kick_unsaved::Choice::Cancel ) ); },
                        ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
                };
                env.save = [] {
                    auto& ae = mt::getAudioInterface();
                    if( ae.saveProject() )
                        return true;
                    warn( "The project could not be saved: " + ae.getProjectSaveError() );
                    return false;
                };
                env.saveAs   = []( std::function<void( bool )> cb ) { saveAs( SaveAsMode::SaveAs, cb, false ); };
                env.discard  = [] { mt::getAudioInterface().discardProjectChangesOnClose(); };
                return env;
            }
        }

        void leaveProject( std::function<void()> proceed, std::function<void()> cancelled )
        {
            kick_unsaved::leaveProject( makeEnv(), std::move( proceed ), std::move( cancelled ) );
        }

        bool isProtectedProject( const juce::File& projectDir )
        {
            auto&                    ae = mt::getAudioInterface();
            std::vector<std::string> places{ getAppDirs( MTProjects ).getFullPathName().toStdString(), getAppDirs( MTDemos ).getFullPathName().toStdString(), getAppDirs( MTTemp ).getFullPathName().toStdString() };
            const auto               path = projectDir.getFullPathName().toStdString();
            for( const auto& place : { places[1], places[2] } ) // factory demos and templates (and anything inside them)
                if( kick_fsguard::isInside( path, place ) )
                    return true;
            const std::string current = ae.isProjectLoaded() ? ae.getProjectDir().getFullPathName().toStdString() : std::string();
            return kick_fsguard::isProtected( path, current, places );
        }

        void writeThumbnail( const juce::File& projectDir )
        {
            if( !projectDir.isDirectory() )
                return;
            auto& home = kick_ui::KickHome::Instance();
            if( home.getWidth() <= 0 || home.getHeight() <= 0 )
                return;
            const float scale = 400.0f / static_cast<float>( home.getWidth() );
            auto        image = home.createComponentSnapshot( home.getLocalBounds(), true, scale );
            if( image.isNull() )
                return;
            const auto file = projectDir.getChildFile( "thumbnail.png" );
            juce::TemporaryFile temp( file );
            {
                juce::FileOutputStream out( temp.getFile() );
                juce::PNGImageFormat   png;
                if( !out.openedOk() || !png.writeImageToStream( image, out ) )
                    return;
            }
            temp.overwriteTargetFileWithTemporary();
        }

        bool saveProject()
        {
            auto& ae = mt::getAudioInterface();
            if( !ae.isProjectLoaded() )
                return false;
            if( ae.isProjectReadOnly() )
            {
                kick_ui::AppStatusbar::Instance().setBoxInfo( "Demo / template: Save As" );
                saveAs( SaveAsMode::SaveAs );
                return false;
            }
            if( !ae.saveProject() )
            {
                kick_ui::AppStatusbar::Instance().setBoxInfo( "UNSAVED: " + ae.getProjectSaveError() );
                return false;
            }
            kick_ui::AppStatusbar::Instance().setBoxInfo( "Saved" );
            writeThumbnail( ae.getProjectDir() );
            return true;
        }

        void createProject( const juce::String& rawName )
        {
            const auto name = kick_project::trimName( rawName.toStdString() );
            if( const auto problem = kick_project::checkName( name ); problem != kick_project::NameProblem::Ok )
            {
                warn( kick_project::describe( problem ) );
                return;
            }
            const auto file = getAppDirs( MTProjects ).getChildFile( juce::String( name ) );
            if( file.exists() )
            {
                warn( kick_project::describe( kick_project::CreateResult::AlreadyExists ) );
                return;
            }
            mt::getSettings().projects.createProject( file ); // Samples / Export folders
            ui::LoadProjectAndAnimation::getInstance().startLoading( file, LoadSource::User, true );
        }

        void saveAs( SaveAsMode mode, std::function<void( bool )> done, bool reload )
        {
            auto& ae = mt::getAudioInterface();
            if( !ae.isProjectLoaded() )
            {
                warn( "There is no open project" );
                if( done ) done( false );
                return;
            }
            const auto root    = mode == SaveAsMode::Template ? getAppDirs( MTTemp ) : getAppDirs( MTProjects );
            const auto initial = juce::String( kick_project::suggestCopyName( ae.getProjectDir().getFileName().toStdString(), root.getFullPathName().toStdString() ) );
            const auto title   = mode == SaveAsMode::SaveAs ? "SAVE AS" : ( mode == SaveAsMode::SaveCopy ? "SAVE A COPY" : "SAVE AS TEMPLATE" );
            askText( title, initial, [mode, root, done, reload]( juce::String text ) {
                const auto name = kick_project::trimName( text.toStdString() );
                if( const auto problem = kick_project::checkName( name ); problem != kick_project::NameProblem::Ok )
                {
                    warn( kick_project::describe( problem ) );
                    if( done ) done( false );
                    return;
                }
                const auto target = root.getChildFile( juce::String( name ) );
                if( target.exists() )
                {
                    warn( kick_project::describe( kick_project::OpResult::AlreadyExists ) );
                    if( done ) done( false );
                    return;
                }
                if( !root.isDirectory() && !root.createDirectory().wasOk() )
                {
                    warn( "The target folder cannot be created" );
                    if( done ) done( false );
                    return;
                }
                auto outcome = std::make_shared<mt::MTAudioInterface::SaveAsOutcome>();
                BackgroundTask::run(
                    mode == SaveAsMode::Template ? "SAVING TEMPLATE" : "SAVING PROJECT", true,
                    [mode, target, outcome]( const std::atomic<bool>& cancel, const BackgroundTask::Progress& progress ) {
                        auto& ae2    = mt::getAudioInterface();
                        auto  copyFn = [&]( const juce::File& from, const juce::File& to ) {
                            const auto r = kick_fileops::copyFolderTo( from.getFullPathName().toStdString(), to.getFullPathName().toStdString(), cancel,
                                                                       [&]( const kick_fileops::Progress& p ) { progress( p.percent(), juce::String( p.current ) ); } );
                            return r.ok;
                        };
                        *outcome = ae2.saveProjectAs( target, mode == SaveAsMode::SaveAs, copyFn, cancel );
                    },
                    [mode, target, outcome, done, reload]() {
                        if( !outcome->ok )
                        {
                            if( !outcome->cancelled )
                                warn( "Could not save: " + outcome->error );
                            if( done ) done( false );
                            return;
                        }
                        writeThumbnail( target );
                        kick_ui::AppStatusbar::Instance().setBoxInfo( ( mode == SaveAsMode::Template ? "Template saved: " : "Saved: " ) + target.getFileName() );
                        if( mode == SaveAsMode::SaveAs && reload )
                            ui::LoadProjectAndAnimation::getInstance().startLoading( target, LoadSource::User ); // the copy becomes the open project
                        if( done ) done( true );
                    } );
            } );
        }
    }
}
