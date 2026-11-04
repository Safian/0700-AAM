namespace ui
{
    bool RecentProject::isProjectLoading{ false };

    namespace
    {
        // Start-up options: what happens when the device starts. Tap on "Auto Load" in the Projects header.
        class LaunchPopup : public juce::Component
        {
        public:
            LaunchPopup()
            {
                auto& st = mt::getSettings().projects;
                mode     = st.onLaunch() == "New Project" ? 2 : ( st.onLaunch() == "Last Used" || st.isAutoLoad() ? 1 : 0 );
                templates.add( juce::String() ); // "" = an empty project
                juce::Array<juce::File> dirs;
                getAppDirs( MTTemp ).findChildFiles( dirs, juce::File::findDirectories, false );
                std::sort( dirs.begin(), dirs.end(), []( const juce::File& x, const juce::File& y ) { return x.getFileName().compareNatural( y.getFileName() ) < 0; } );
                for( auto& d : dirs )
                    templates.add( d.getFullPathName() );
                templateIndex = juce::jmax( 0, templates.indexOf( st.launchTemplate() ) );
                for( auto* b : { &projectsBtn, &lastBtn, &newBtn, &templateBtn } )
                {
                    addAndMakeVisible( b );
                    b->setValueColour( ui::Theme::textColour( 1 ) );
                    b->setValueFontHeight( 16 );
                }
                projectsBtn.onClick = [this] { mode = 0; refresh(); };
                lastBtn.onClick     = [this] { mode = 1; refresh(); };
                newBtn.onClick      = [this] { mode = 2; refresh(); };
                templateBtn.onClick = [this] { templateIndex = ( templateIndex + 1 ) % templates.size(); refresh(); };
                refresh();
            }

            void apply()
            {
                auto& st         = mt::getSettings().projects;
                st.onLaunch      = mode == 2 ? "New Project" : ( mode == 1 ? "Last Used" : "Selector" );
                st.isAutoLoad    = ( mode == 1 );
                st.launchTemplate = templates[templateIndex];
            }

        private:
            int                 mode{ 0 }, templateIndex{ 0 };
            juce::StringArray   templates;
            ui::KickButton      projectsBtn{ "OnLaunchProjects", "" }, lastBtn{ "OnLaunchLast", "" }, newBtn{ "OnLaunchNew", "" }, templateBtn{ "OnLaunchTemplate", "" };

            void refresh()
            {
                projectsBtn.setButtonText( juce::String( mode == 0 ? "(x) " : "( ) " ) + "Show the Projects screen" );
                lastBtn.setButtonText( juce::String( mode == 1 ? "(x) " : "( ) " ) + "Open the last project" );
                newBtn.setButtonText( juce::String( mode == 2 ? "(x) " : "( ) " ) + "Start a new project" );
                const auto t = templates[templateIndex];
                templateBtn.setButtonText( "New project from: " + ( t.isEmpty() ? juce::String( "Empty" ) : juce::File( t ).getFileName() ) );
                templateBtn.setEnabled( mode == 2 );
            }
            void resized() override
            {
                projectsBtn.setBoundsRelative( 0.15f, 0.12f, 0.7f, 0.12f );
                lastBtn.setBoundsRelative( 0.15f, 0.27f, 0.7f, 0.12f );
                newBtn.setBoundsRelative( 0.15f, 0.42f, 0.7f, 0.12f );
                templateBtn.setBoundsRelative( 0.15f, 0.57f, 0.7f, 0.12f );
            }
        };

        // A tile of a project folder that no longer exists is marked, not hidden: an SD card that is out may come back.
        bool isFactoryPath( const juce::File& f )
        {
            const auto p = f.getFullPathName().toStdString();
            return kick_fsguard::isInside( p, getAppDirs( MTDemos ).getFullPathName().toStdString() ) || kick_fsguard::isInside( p, getAppDirs( MTTemp ).getFullPathName().toStdString() );
        }
    }

    RecentProject::RecentProject()
        : kick_ui::IScreen( kick_ui::Screen::RecentProject ), longPressComp( std::make_unique<LongPressComp>() )
    {
        ae.addListener( this );

        setHasFocusOutline( false );

        mt::utils::addAndMakeVisible( this, header, fileBrowser, newProj, lastProjectBtn, statusStrip, demoList, templateList, projectList, longPressComp.get() );
        lastProjectBtn.onClick = [this]() { loadLastProject(); };
        longPressComp->setVisible( false );
        addDemos();
        addTemplates();

        header.onBackButtonPressed = [this]( bool ) { closeScreen( kick_ui::Screen::HomeListView ); };
        header.showOptions( false, false );
        header.showAutoLoad( true );
        header.setAutoLoadToggleState( settings.projects.isAutoLoad() );

        header.onAutoLoadClick = [this]() { openLaunchOptions(); };

        fileBrowser.onClick = [&]() {
            kick_ui::signals::sigScreenOpen( kick_ui::Screen::MenuFileBrowser );
        };

        maxNumOfRecentProject = settings.projects.maxRecentProjects();

        themeChanged.connect( [&]() {
            fileBrowser.setImages( "Browser_Recent" );
            header.setIcon( "RecentProject" );
        } );

        onKeyBoardEnd = [&]( juce::String text, juce::File project ) {
            copyProjIntoProjectDir( text, project );
        };

        newProj.onClick = [&]() {
            juce::String str = settings.projects.namePrefix();

            auto& keyboard = ui::KickVirtualKeyBoard::getWithText( str );
            keyboard.show(
                "NEW PROJECT", ui::KickVirtualKeyBoard::Type::KEYS_LOWERCASE_ALPHABETIC,
                [&keyboard, this]( int R ) {
                    if( UI::BUTTONS::APPLY == R )
                    {
                        createProject( keyboard.getInputText() );
                    }
                },
                ui::layout::keyBoardPopup );
        };
    }

    void RecentProject::warn( const juce::String& message )
    {
        project_actions::warn( message );
    }

    void RecentProject::openLaunchOptions()
    {
        auto p = new LaunchPopup;
        mt::utils::showOptions(
            "ON LAUNCH", p, [this, p]( int R ) {
                if( R == UI::APPLY )
                    p->apply();
                header.setAutoLoadToggleState( settings.projects.isAutoLoad() ); // a cancel leaves everything as it was
                delete p; },
            ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
    }

    void RecentProject::createProject( juce::String value )
    {
        project_actions::createProject( value ); // the same checked path as the file browser's New project
    }

    void RecentProject::sliderMoved( float )
    {}

    void RecentProject::openVirtualKeyBoard( juce::File project )
    {
        auto& keyboard = ui::KickVirtualKeyBoard::getWithText( project.getFileNameWithoutExtension() );
        keyboard.show(
            "", ui::KickVirtualKeyBoard::Type::KEYS_LOWERCASE_ALPHABETIC,
            [&keyboard, this, project]( int R ) {
                if( UI::BUTTONS::APPLY == R )
                {
                    if( onKeyBoardEnd )
                        onKeyBoardEnd( keyboard.getInputText(), project );
                }
            },
            ui::layout::keyBoardPopup );
    }

    void RecentProject::copyProjIntoProjectDir( juce::String text, juce::File project )
    {
        // juce::File behind the injected file system of kick_project::createFromTemplate.
        struct TemplateFs
        {
            juce::File   templateProject;
            juce::String projectFileName;
            bool         exists( const std::string& p ) { return juce::File( juce::String( p ) ).exists(); }
            bool         hasTemplateProject() { return templateProject != juce::File(); }
            bool         makeFolder( const std::string& p ) { return juce::File( juce::String( p ) ).createDirectory().wasOk(); }
            bool         copyTemplateInto( const std::string& folder ) { return templateProject.copyFileTo( juce::File( juce::String( folder ) ).getChildFile( projectFileName ) ); }
            void         removeFolder( const std::string& p ) { juce::File( juce::String( p ) ).deleteRecursively(); }
        };

        const auto              name = juce::String( kick_project::trimName( text.toStdString() ) );
        juce::Array<juce::File> sourceFiles;
        project.findChildFiles( sourceFiles, juce::File::findFiles, false, "*.kickPrj" );
        TemplateFs fs{ sourceFiles.isEmpty() ? juce::File() : sourceFiles[0], name + mt::__projectFileSuffix };

        std::string folder;
        const auto  result = kick_project::createFromTemplate( name.toStdString(), getAppDirs( MTProjects ).getFullPathName().toStdString(), fs, &folder );
        if( result == kick_project::CreateResult::BadName )
        {
            warn( kick_project::describe( kick_project::checkName( kick_project::trimName( text.toStdString() ) ) ) );
            return;
        }
        if( result != kick_project::CreateResult::Ok )
        {
            const auto p = kick_startup::state().fail( kick_startup::Fault::ProjectLoad, ( "new project '" + name + "' from template '" + project.getFileName() + "': " + kick_project::describe( result ) ).toStdString() );
            kick_log( "error", "[ERROR] [PROJ] %s\n", p.cause.c_str() );
            warn( kick_project::describe( result ) );
            return;
        }
        // Load the new project.
        juce::File destinationFile( juce::String::fromUTF8( folder.c_str() ) );
        loadProject( destinationFile, LoadSource::User, false );
    }

    void RecentProject::loadProject( const juce::File& Project, LoadSource source, bool createNew )
    {
        ui::LoadProjectAndAnimation::getInstance().startLoading( Project, source, createNew );
    }

    juce::File RecentProject::lastProjectFile() const
    {
        const auto last = settings.projects.currentProject();
        if( last.isEmpty() ) return {};
        juce::File f( last );
        return f.exists() ? f : juce::File();
    }

    void RecentProject::updateLastProjectButton()
    {
        // One touch back into the project that was open at the last shutdown (hidden when there is none).
        const auto last = lastProjectFile();
        lastProjectBtn.setVisible( last != juce::File() );
        if( last != juce::File() )
            lastProjectBtn.setButtonText( "Last: " + last.getFileNameWithoutExtension() );
    }

    void RecentProject::loadLastProject()
    {
        auto last = lastProjectFile();
        if( last != juce::File() )
            loadProject( last );
    }

    int RecentProject::getNumOfChild()
    {
        return settings.projects.getNumChildren();
    }

    juce::File RecentProject::getRecentProject( int row )
    {
        reversedRow = getNumOfChild() - row - 1;
        return juce::File( settings.projects.getProject( reversedRow ) );
    }

    int RecentProject::getReversedRow( int row )
    {
        return getNumOfChild() - row - 1;
    }

    void RecentProject::removeFileFromVTree( int row )
    {
        reversedRow = getReversedRow( row );
        settings.projects.removeProject( reversedRow );
    }

    void RecentProject::resized()
    {
        auto b = getLocalBounds();
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );

        fileBrowser.setBoundsRelative( .8492f, .0267f, .0429f, .0733f );

        statusStrip.setBounds( b.removeFromBottom( 24 ) );

        auto listHeight = b.getHeight() / 3;
        demoList.setBounds( b.removeFromTop( listHeight ).reduced( 10, 10 ) );
        templateList.setBounds( b.removeFromTop( listHeight ).reduced( 10, 10 ) );

        auto rem = b.reduced( 10, 5 );
        auto col = rem.removeFromRight( 150 );
        lastProjectBtn.setBounds( col.removeFromTop( col.getHeight() / 2 ).reduced( 0, 5 ) );
        newProj.setBounds( col.reduced( 0, 5 ) );
        projectList.setBounds( rem );
    }

    void RecentProject::setNumOptions( int NumOptions )
    {
        // The tiles leave the list first: the list never holds a pointer to a destroyed tile.
        projectList.clear();
        auto idxOffset = static_cast<int>( demoDirList.size() + templateDirList.size() );
        if( static_cast<int>( recentItems.size() ) > idxOffset )
            recentItems.erase( recentItems.begin() + idxOffset, recentItems.end() );

        for( int index = 0; index < NumOptions; ++index )
            addProjectList( index );

        resized();
    }

    void RecentProject::addDemos()
    {
        juce::File demoPath = getAppDirs( MTDemos );
        demoPath.findChildFiles( demoDirList, juce::File::findDirectories, false );
        // Sort the list numerically based on the number in the file name
        std::sort( demoDirList.begin(), demoDirList.end(),
                   []( const juce::File& a, const juce::File& b ) {
                       // Extract the numeric part from the file name
                       int numA = a.getFileName().retainCharacters( "0123456789" ).getIntValue();
                       int numB = b.getFileName().retainCharacters( "0123456789" ).getIntValue();

                       return numA < numB;
                   } );
        for( auto i = 0; i < demoDirList.size(); i++ )
        {
            auto recentItemsPtr = std::make_unique<RecentItems>();
            recentItemsPtr->setProperties( i, demoDirList[i].getFullPathName(), true );

            recentItemsPtr->clearAllShowBorderCallback = [&]() {
                clearAllShowBorder();
            };

            recentItemsPtr->updateSelectedIndex = [&]( int index ) {
                selectedIndex = index;
            };
            recentItemsPtr->onMouseDown = [&]( int index, juce::File project ) {
                loadProject( project, LoadSource::Demo ); // a demo never becomes the "last project" / a Recent entry
            };
            recentItemsPtr->onLongPressed = [this]( int index ) { showTileActions( index ); };

            demoList.addComp( recentItemsPtr.get() );
            recentItems.emplace_back( std::move( recentItemsPtr ) );
        }
        resized();
    }

    void RecentProject::addTemplates()
    {
        juce::File templatePath = getAppDirs( MTTemp );
        templatePath.findChildFiles( templateDirList, juce::File::findDirectories, false );
        // Sort the list numerically based on the number in the file name
        std::sort( templateDirList.begin(), templateDirList.end(),
                   []( const juce::File& a, const juce::File& b ) {
                       // Extract the numeric part from the file name
                       int numA = a.getFileName().retainCharacters( "0123456789" ).getIntValue();
                       int numB = b.getFileName().retainCharacters( "0123456789" ).getIntValue();

                       return numA < numB;
                   } );
        auto idxOffset = static_cast<int>( demoDirList.size() );

        for( auto i = 0; i < templateDirList.size(); i++ )
        {
            auto recentItemsPtr = std::make_unique<RecentItems>();
            recentItemsPtr->setProperties( i + idxOffset, templateDirList[i].getFullPathName(), true );

            recentItemsPtr->clearAllShowBorderCallback = [&]() {
                clearAllShowBorder();
            };

            recentItemsPtr->updateSelectedIndex = [&]( int index ) {
                selectedIndex = index;
            };

            recentItemsPtr->onMouseDown = [&]( int index, juce::File project ) {
                openVirtualKeyBoard( project );
            };
            recentItemsPtr->onLongPressed = [this]( int index ) { showTileActions( index ); };

            templateList.addComp( recentItemsPtr.get() );
            recentItems.emplace_back( std::move( recentItemsPtr ) );
        }
    }

    void RecentProject::addProjectList( int index )
    {
        auto sz  = projectList.listOptions.size();
        auto idx = index + 1;

        if( sz < idx )
        {
            for( auto i = sz; i < idx; ++i )
            {
                auto file = getRecentProject( index );

                auto recentItemsPtr = std::make_unique<RecentItems>();

                recentItemsPtr->setProperties( static_cast<int>( recentItems.size() ), file.getFullPathName() );

                recentItemsPtr->clearAllShowBorderCallback = [&]() {
                    clearAllShowBorder();
                };
                recentItemsPtr->onLongPressed = [this]( int Index ) { showTileActions( Index ); };

                recentItemsPtr->updateSelectedIndex = [&]( int index ) {
                    selectedIndex = index;
                };

                recentItemsPtr->onMouseDown = [this]( int index, juce::File project ) {
                    // The tile holds its own path: a tap, the Data Dial and the menu all load the same project.
                    loadProject( project, LoadSource::User );
                };

                projectList.addComp( recentItemsPtr.get() );
                recentItems.emplace_back( std::move( recentItemsPtr ) );
            }
        }
    }

    void RecentProject::clearAllShowBorder()
    {
        for( const auto& it : recentItems )
        {
            it->showBorder( false );
            it->repaint();
        }
    }

    int RecentProject::rowStart( int row ) const
    {
        const int nd = static_cast<int>( demoDirList.size() ), nt = static_cast<int>( templateDirList.size() );
        return row == 0 ? 0 : ( row == 1 ? nd : nd + nt );
    }

    int RecentProject::rowOf( int index ) const
    {
        const int nd = static_cast<int>( demoDirList.size() ), nt = static_cast<int>( templateDirList.size() );
        return index < nd ? 0 : ( index < nd + nt ? 1 : 2 );
    }

    void RecentProject::handleCommand( CPktMgr::HWMsg_t& hwMsg )
    {
        using dc = contracts::DeviceControlContracts;
        using H      = contracts::DclContractsHelper;

        // The tile menu is open: the Data Dial picks an entry, ENTER runs it, BACK / MAIN close it.
        if( longPressComp->isOpen() )
        {
            if( H::isKnobMessage( hwMsg ) && H::getKnobCode( hwMsg ) == dc::KnobSequence::EN_KNOB_CENTER )
                longPressComp->hwStep( H::getKnobValue( hwMsg ) > 0 ? 1 : -1 );
            else if( H::isButtonMessage( hwMsg ) && H::getButtonState( hwMsg ) == dc::BTN_PRESSED )
            {
                const auto code = H::getButtonCode( hwMsg );
                if( code == dc::BtnLedSequence::EN_KNOB_ENTER )
                    longPressComp->hwActivate();
                else if( code == dc::BtnLedSequence::EN_BACK || code == dc::BtnLedSequence::EN_MAINMODE )
                    longPressComp->close();
            }
            return;
        }

        // Data dial: move the selection; with Shift it jumps to the start of the next / previous row (Demos / Templates / Recent).
        if( H::isKnobMessage( hwMsg ) && H::getKnobCode( hwMsg ) == dc::KnobSequence::EN_KNOB_CENTER )
        {
            if( H::isShiftPressed() )
            {
                const int row    = rowOf( juce::jmax( 0, selectedIndex ) );
                const int target = juce::jlimit( 0, 2, row + ( H::getKnobValue( hwMsg ) > 0 ? 1 : -1 ) );
                if( target != row && rowStart( target ) < static_cast<int>( recentItems.size() ) )
                {
                    knobRotated( rowStart( target ) - selectedIndex );
                    return;
                }
            }
            knobRotated( H::getKnobValue( hwMsg ) );
            return;
        }

        if( !H::isButtonMessage( hwMsg ) || H::getButtonState( hwMsg ) != dc::BTN_PRESSED )
            return;

        // Front-panel buttons on this screen (they used to do nothing without a project).
        switch( H::getButtonCode( hwMsg ) )
        {
            case dc::BtnLedSequence::EN_KNOB_ENTER:
                if( H::isShiftPressed() ) showTileActions( selectedIndex ); // Shift+ENTER = the tile menu (no long press needed)
                else knobClicked();                                         // open the selected tile
                break;
            case dc::BtnLedSequence::EN_PLUS:
            case dc::BtnLedSequence::EN_KNOB_CW: knobRotated( 1 ); break;                                     // next tile
            case dc::BtnLedSequence::EN_MINUS:
            case dc::BtnLedSequence::EN_KNOB_CCW: knobRotated( -1 ); break;                                   // previous tile
            case dc::BtnLedSequence::EN_MAINMODE:
                if( !ae.isProjectLoaded() ) loadLastProject();                                                 // one press back into the last project
                break;
            case dc::BtnLedSequence::EN_BACK:
                if( ae.isProjectLoaded() ) closeScreen( kick_ui::Screen::HomeListView );                       // same as the header back arrow
                break;
            default: break;
        }
    }

    void RecentProject::knobClicked()
    {
        if( juce::isPositiveAndBelow( selectedIndex, recentItems.size() ) )
            if( recentItems[selectedIndex]->onMouseDown )
                recentItems[selectedIndex]->onMouseDown( selectedIndex, recentItems[selectedIndex]->getPath() );
    }

    void RecentProject::ensureVisible( int index )
    {
        if( !juce::isPositiveAndBelow( index, recentItems.size() ) )
            return;
        auto& list = rowOf( index ) == 0 ? demoList : ( rowOf( index ) == 1 ? templateList : projectList );
        auto  tile = recentItems[index]->getBounds();
        auto  view = list.getViewPositionX();
        if( tile.getRight() - view > list.getWidth() || tile.getX() < view )
            list.setViewPosition( tile.getX(), list.getViewPositionY() );
    }

    void RecentProject::knobRotated( double value )
    {
        if( longPressComp->isVisible() )
            longPressComp->close();

        if( juce::isPositiveAndBelow( selectedIndex, recentItems.size() ) )
            recentItems[selectedIndex]->showBorder( false );

        selectedIndex = selectedIndex + static_cast<int>( value );

        if( selectedIndex < 0 )
            selectedIndex = 0;

        if( selectedIndex >= recentItems.size() )
            selectedIndex = static_cast<int>( recentItems.size() ) - 1;

        if( juce::isPositiveAndBelow( selectedIndex, recentItems.size() ) )
            recentItems[selectedIndex]->showBorder( true );
        ensureVisible( selectedIndex );
    }

    void RecentProject::rebuildAll()
    {
        // Demos / templates are read again each time the screen shows (a new template, a copy made from a demo), the Recent row
        // too. The tiles leave the lists before they are destroyed.
        longPressComp->close();
        demoList.clear();
        templateList.clear();
        projectList.clear();
        recentItems.clear();
        demoDirList.clear();
        templateDirList.clear();
        addDemos();
        addTemplates();
        setNumOptions( getNumOfChild() );
        demoList.setPlaceholder( "No demos found" );
        templateList.setPlaceholder( "No templates found" );
        projectList.setPlaceholder( "No recent projects yet: tap New project, or open one with the File Browser" );
    }

    void RecentProject::visibilityChanged()
    {
        auto isProjectLoaded = ae.isProjectLoaded();
        header.showBackButton( isProjectLoaded );

        if( isVisible() )
        {
            settings.projects.refreshMaxNumOfRecentProj();
            rebuildAll();
            auto projectStartIndex = static_cast<int>( demoDirList.size() + templateDirList.size() );
            selectedIndex          = ( 0 < ( projectList.getSize() ) ) ? projectStartIndex : 0;

            clearAllShowBorder();
            if( juce::isPositiveAndBelow( selectedIndex, recentItems.size() ) )
                recentItems[selectedIndex]->showBorder( true );

            updateLastProjectButton();

            // First launch: the single start-up decision (safe mode / last project / device default project / none).
            if( bFirstLaunch )
            {
                bFirstLaunch = false;
                ui::LoadProjectAndAnimation::getInstance().requestStartupProject();
            }
        }
        else
            longPressComp->close();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Project actions on a tile (long press / Shift+ENTER): Load, Rename, Duplicate, Delete, Info, Remove from the list
    ///////////////////////////////////////////////////////////////////////////////////////////
    void RecentProject::showTileActions( int index )
    {
        if( !juce::isPositiveAndBelow( index, recentItems.size() ) )
            return;
        auto actions = actionsFor( index );
        if( actions.empty() )
            return;
        auto* tile = recentItems[index].get();
        longPressComp->setBounds( getLocalBounds() );
        longPressComp->showActions( std::move( actions ), getLocalArea( tile, tile->getLocalBounds() ) );
    }

    std::vector<LongPressComp::Action> RecentProject::actionsFor( int index )
    {
        std::vector<LongPressComp::Action> a;
        const auto                         dir = recentItems[index]->getPath();
        const auto                         row = rowOf( index );
        if( row == 1 )
        {
            a.push_back( { "New project", [this, index, dir] { openVirtualKeyBoard( dir ); } } );
            a.push_back( { "Info", [this, dir] { showInfoOf( dir ); } } );
            return a;
        }
        if( row == 0 )
        {
            a.push_back( { "Load", [this, dir] { loadProject( dir, LoadSource::Demo ); } } );
            a.push_back( { "Save a copy", [this, dir] { duplicateProjectAt( dir ); } } );
            a.push_back( { "Info", [this, dir] { showInfoOf( dir ); } } );
            return a;
        }
        a.push_back( { "Load", [this, dir] { loadProject( dir, LoadSource::User ); } } );
        a.push_back( { "Rename", [this, dir] { renameProjectAt( dir ); } } );
        a.push_back( { "Duplicate", [this, dir] { duplicateProjectAt( dir ); } } );
        a.push_back( { "Delete", [this, dir] { deleteProjectAt( dir ); } } );
        a.push_back( { "Info", [this, dir] { showInfoOf( dir ); } } );
        a.push_back( { "Remove from list", [this, dir] {
                          juce::File d = dir;
                          const auto cur = settings.projects.currentProject();
                          if( ae.isProjectLoaded() && ae.getProjectDir() == dir )
                          {
                              warn( "The open project is not allowed to be removed from the list" );
                              return;
                          }
                          settings.projects.removeProject( d );
                          setNumOptions( getNumOfChild() );
                          selectedIndex = juce::jlimit( 0, juce::jmax( 0, static_cast<int>( recentItems.size() ) - 1 ), selectedIndex );
                      } } );
        return a;
    }

    void RecentProject::renameProjectAt( const juce::File& dir )
    {
        if( project_actions::isProtectedProject( dir ) )
        {
            warn( kick_project::describe( kick_project::OpResult::Protected ) );
            return;
        }
        project_actions::askText( "RENAME PROJECT", dir.getFileName(), [this, dir]( juce::String text ) {
            std::string newDir;
            const auto  r = kick_project::rename(
                dir.getFullPathName().toStdString(), text.toStdString(),
                []( const std::string& p ) { return project_actions::isProtectedProject( juce::File( juce::String( p ) ) ); }, &newDir );
            if( r != kick_project::OpResult::Ok )
            {
                warn( kick_project::describe( r ) );
                return;
            }
            juce::File oldF = dir, newF( juce::String::fromUTF8( newDir.c_str() ) );
            settings.projects.renameProject( oldF, newF );
            if( settings.projects.currentProject() == dir.getFullPathName() )
                settings.projects.currentProject = newF.getFullPathName();
            setNumOptions( getNumOfChild() );
            kick_ui::AppStatusbar::Instance().setBoxInfo( "Renamed to " + newF.getFileName() );
        } );
    }

    void RecentProject::duplicateProjectAt( const juce::File& dir )
    {
        const auto root = getAppDirs( MTProjects );
        project_actions::askText( "DUPLICATE PROJECT", juce::String( kick_project::suggestCopyName( dir.getFileName().toStdString(), root.getFullPathName().toStdString() ) ), [this, dir, root]( juce::String text ) {
            auto result = std::make_shared<std::pair<kick_project::OpResult, std::string>>();
            BackgroundTask::run(
                "COPYING PROJECT", true,
                [dir, root, text, result]( const std::atomic<bool>& cancel, const BackgroundTask::Progress& progress ) {
                    result->first = kick_project::duplicate( dir.getFullPathName().toStdString(), root.getFullPathName().toStdString(), text.toStdString(), cancel,
                                                             [&]( const kick_fileops::Progress& p ) { progress( p.percent(), juce::String( p.current ) ); }, &result->second );
                },
                [this, result]() {
                    if( result->first == kick_project::OpResult::Cancelled )
                        return;
                    if( result->first != kick_project::OpResult::Ok )
                    {
                        warn( kick_project::describe( result->first ) );
                        return;
                    }
                    juce::File made( juce::String::fromUTF8( result->second.c_str() ) );
                    settings.projects.rearrangeProject( made ); // the copy shows in the Recent row
                    setNumOptions( getNumOfChild() );
                    kick_ui::AppStatusbar::Instance().setBoxInfo( "Copied as " + made.getFileName() );
                } );
        } );
    }

    void RecentProject::deleteProjectAt( const juce::File& dir )
    {
        if( project_actions::isProtectedProject( dir ) )
        {
            warn( kick_project::describe( kick_project::OpResult::Protected ) );
            return;
        }
        project_actions::confirm( "DELETE PROJECT", "Delete '" + dir.getFileName() + "' and all its samples for good?", [this, dir]( bool yes ) {
            if( !yes )
                return;
            auto result = std::make_shared<kick_project::OpResult>( kick_project::OpResult::Failed );
            BackgroundTask::run(
                "DELETING PROJECT", true,
                [dir, result]( const std::atomic<bool>& cancel, const BackgroundTask::Progress& progress ) {
                    // Protection was checked on the message thread just before; the folder must still hold a project file.
                    *result = kick_project::remove( dir.getFullPathName().toStdString(), {}, cancel,
                                                    [&]( const kick_fileops::Progress& p ) { progress( p.percent(), juce::String( p.current ) ); } );
                },
                [this, dir, result]() {
                    if( *result == kick_project::OpResult::Cancelled )
                    {
                        setNumOptions( getNumOfChild() );
                        return;
                    }
                    if( *result != kick_project::OpResult::Ok )
                    {
                        warn( kick_project::describe( *result ) );
                        return;
                    }
                    juce::File d = dir;
                    settings.projects.removeProject( d );
                    if( settings.projects.currentProject() == dir.getFullPathName() )
                        settings.projects.currentProject = juce::String();
                    updateLastProjectButton();
                    setNumOptions( getNumOfChild() );
                    selectedIndex = juce::jlimit( 0, juce::jmax( 0, static_cast<int>( recentItems.size() ) - 1 ), selectedIndex );
                    kick_ui::AppStatusbar::Instance().setBoxInfo( "Deleted " + dir.getFileName() );
                } );
        } );
    }

    void RecentProject::showInfoOf( const juce::File& dir )
    {
        auto info = std::make_shared<kick_project::Info>();
        BackgroundTask::run(
            "PROJECT INFO", false,
            [dir, info]( const std::atomic<bool>&, const BackgroundTask::Progress& ) { *info = kick_project::info( dir.getFullPathName().toStdString() ); },
            [dir, info]() {
                if( !info->valid )
                {
                    project_actions::warn( kick_project::describe( kick_project::OpResult::NotAProject ) );
                    return;
                }
                juce::StringArray lines;
                lines.add( "Name: " + dir.getFileName() );
                lines.add( "Location: " + dir.getParentDirectory().getFullPathName() );
                lines.add( "Size: " + juce::String( kick_project::formatBytes( info->bytes ) ) + " (" + juce::String( info->samples ) + " samples)" );
                lines.add( "Modified: " + juce::Time( info->modifiedSec * 1000 ).toString( true, true, false, true ) );
                lines.add( "Programs: " + juce::String( info->programs ) + "   Sequences: " + juce::String( info->sequences ) );
                project_actions::infoDialog( "PROJECT INFO", lines );
            } );
    }

    //////////////////////////////////////////////////////////////////////////////////////////////
    //////                            Recent List
    //////////////////////////////////////////////////////////////////////////////////////////////

    RecentList::RecentList()
    {
        setLookAndFeel( KickLooknFeel::getLnf() );
        setScrollOnDragMode( juce::Viewport::ScrollOnDragMode::all );
        setScrollBarsShown( false, true, false, false );
        setScrollBarThickness( 7 );
        setWantsKeyboardFocus( false );
        listComp.setWantsKeyboardFocus( false );
        setViewedComponent( &listComp, false );
    }

    void RecentList::clear() noexcept
    {
        listComp.removeAllChildren();
        listOptions.clear();
        resized();
    }

    void RecentList::addComp( RecentItems* recentItem ) noexcept
    {
        listOptions.push_back( recentItem );
        mt::utils::addAndMakeVisible( &listComp, recentItem );
        resized();
    }

    void RecentList::resized()
    {
        auto bounds = getLocalBounds();
        if( bounds.isEmpty() )
            return;

        auto optionHeight = bounds.getHeight() - 10;
        auto optionWidth  = 200;

        auto x = 0;
        for( auto index = 0; index < listOptions.size(); ++index )
        {
            listOptions[index]->setBounds( x, 0, optionWidth, optionHeight );
            x += optionWidth;
        }
        listComp.setSize( x, optionHeight );
    }

    void RecentList::visibilityChanged()
    {}

    void RecentList::paint( juce::Graphics& g )
    {
        if( listOptions.empty() && placeholder.isNotEmpty() )
            mt::utils::renderText( g, placeholder, getLocalBounds().reduced( 20 ), ui::Theme::textColour( 1 ).withAlpha( 0.6f ), juce::Justification::centred, 18.0f );
    }

    juce::File RecentList::getRecentProject( int index )
    {
        juce::File file = juce::File( mt::getSettings().projects.getProject( numOptions - index - 1 ) );
        return file;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////
    //////                            New Project Btn
    //////////////////////////////////////////////////////////////////////////////////////////////

    NewProjectBtn::NewProjectBtn()
    {
        addAndMakeVisible( icon );
        icon.setInterceptsMouseClicks( false, false );
    }

    void NewProjectBtn::paint( juce::Graphics& g )
    {
        auto bounds     = getLocalBounds();
        auto background = bounds.removeFromRight( bounds.proportionOfWidth( 0.9 ) );

        mt::utils::fillRoundedRect( g, bounds.removeFromLeft( 3 ), ui::Theme::foregroundColour( 1 ) );
        mt::utils::fillRoundedRect( g, background, ui::Theme::backgroundColour( 0 ) );
    }

    void NewProjectBtn::resized()
    {
        auto bounds = getLocalBounds();
        if( bounds.isEmpty() )
            return;

        icon.setBoundsRelative( 0.1f, 0.2f, 0.9f, 0.6f );
    }

    //////////////////////////////////////////////////////////////////////////////////////////////
    //////                            Recent Items
    //////////////////////////////////////////////////////////////////////////////////////////////

    RecentItems::RecentItems()
    {
        label.setFontHeight( 18 );

        label.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        date.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        timeStamp.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        date.setTextColour( ui::Theme::textColour( 1 ) );
        timeStamp.setTextColour( ui::Theme::textColour( 1 ) );

        mt::utils::addChildComponents( this, label, date, timeStamp, image, icon );
        mt::utils::makeVisible( image, label );

        date.setInterceptsMouseClicks( false, false );
        timeStamp.setInterceptsMouseClicks( false, false );
        icon.setInterceptsMouseClicks( false, false );
    }

    void RecentItems::setProperties( int Index, const juce::String& Path, bool factory )
    {
        index = Index;
        path  = Path;

        missing = !path.isDirectory();
        if( missing )
        {
            // Folder gone (removed card, deleted from the file browser): the tile stays, marked; a tap explains and offers to drop it.
            label.setText( path.getFileName(), juce::dontSendNotification );
            timeStamp.setText( "not found", juce::dontSendNotification );
            date.setText( "", juce::dontSendNotification );
            label.setTextColour( juce::Colours::orange );
            mt::utils::makeVisible( date, timeStamp, icon );
            mt::utils::makeHidden( image );
            return;
        }
        label.setTextColour( ui::Theme::textColour( 1 ) );

        // A user project shows its own snapshot (thumbnail.png, written when the project is saved); the factory demos / templates
        // use the picture of the theme that carries their name. No picture: the folder icon with the date.
        const auto thumb = path.getChildFile( "thumbnail.png" );
        if( !factory && thumb.existsAsFile() )
        {
            auto img = juce::ImageFileFormat::loadFrom( thumb );
            if( img.isValid() )
            {
                image.setImage( img );
                label.setText( path.getFileNameWithoutExtension(), juce::dontSendNotification );
                mt::utils::makeVisible( image );
                mt::utils::makeHidden( date, timeStamp, icon );
                return;
            }
        }
        juce::Array<juce::File> files;
        if( factory )
            path.findChildFiles( files, juce::File::findFiles, false, "*.png" );

        if( files.isEmpty() )
        {
            label.setText( path.getFileNameWithoutExtension(), juce::dontSendNotification );
            timeStamp.setText( path.getLastModificationTime().toString( false, true, false, false ), juce::dontSendNotification );
            date.setText( path.getLastModificationTime().toString( true, false, false, false ), juce::dontSendNotification );
            mt::utils::makeVisible( date, timeStamp, icon );
        }
        else
        {
            image.setImage( "recent_project" + juce::File::getSeparatorString() + files[0].getFileName() );
            label.setText( files[0].getFileNameWithoutExtension(), juce::dontSendNotification );
            mt::utils::makeVisible( image );
            mt::utils::makeHidden( date, timeStamp, icon );
        }
    }

    void RecentItems::showBorder( bool status )
    {
        if( bShowBorder != status )
        {
            bShowBorder = status;
            repaint();
        }
    }

    void RecentItems::paint( juce::Graphics& g )
    {
        if( icon.isVisible() )
            mt::utils::fillRoundedRect( g, image.getBounds(), ui::Theme::backgroundColour( 0 ) );

        if( bShowBorder )
            mt::utils::renderRoundedRect( g, getLocalBounds(), ui::Theme::outlineColour( 1 ), 2 );
    }

    void RecentItems::resized()
    {
        auto bounds = getLocalBounds();
        if( bounds.isEmpty() )
            return;

        image.setBoundsRelative( 0.05f, 0.02f, 0.9f, 0.83f );
        label.setBoundsRelative( 0.07f, 0.85f, 0.87f, 0.13f );
        icon.setBoundsRelative( 0.1f, 0.3f, 0.22f, 0.4f );
        timeStamp.setBoundsRelative( 0.35f, 0.35f, 0.55f, 0.15f );
        date.setBoundsRelative( 0.35f, 0.5f, 0.55f, 0.15f );
    }

    void RecentItems::mouseDown( const juce::MouseEvent& e )
    {
        if( clearAllShowBorderCallback )
            clearAllShowBorderCallback();

        if( updateSelectedIndex )
            updateSelectedIndex( index );

        showBorder( true );
        repaint();
        isLongPressTriggered = false;
        isDragging           = false; // a drag on an earlier touch must not block this one
        startTimer( longPressThreshold ); // long press detection
    }

    void RecentItems::mouseDrag( const juce::MouseEvent& e )
    {
        if( e.getDistanceFromDragStart() > dragThreshold )
        {
            isDragging = true;
            stopTimer(); // scrolling the row, not a tap
        }
    }

    void RecentItems::mouseUp( const juce::MouseEvent& e )
    {
        stopTimer();
        if( isLongPressTriggered && !isDragging )
        {
            isLongPressTriggered = false;
            if( onLongPressed )
                onLongPressed( index );
        }
        else if( e.mouseWasClicked() && !isDragging )
        {
            // A tap opens at once (no double-tap wait); the short delay lets the border draw first.
            juce::Component::SafePointer<RecentItems> safe( this );
            juce::Timer::callAfterDelay( 20, [safe]() {
                if( safe != nullptr && safe->onMouseDown )
                    safe->onMouseDown( safe->index, safe->path );
            } );
        }
    }

    void RecentItems::timerCallback()
    {
        // Only the long press uses the timer now.
        stopTimer();
        if( !isDragging )
            isLongPressTriggered = true;
    }

    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////         LongPressComp      ////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

    LongPressComp::LongPressComp()
    {
        setWantsKeyboardFocus( false );
    }

    void LongPressComp::showActions( std::vector<Action> acts, juce::Rectangle<int> anchor )
    {
        close();
        actions = std::move( acts );
        buttons.clear();
        selected = 0;
        for( int i = 0; i < static_cast<int>( actions.size() ); ++i )
        {
            auto b = std::make_unique<ui::KickButton>( "TileAction" + juce::String( i ), actions[static_cast<size_t>( i )].label );
            b->setValueColour( ui::Theme::textColour( 1 ) );
            b->setValueFontHeight( 16 );
            b->onClick = [this, i]() { activate( i ); };
            addAndMakeVisible( *b );
            buttons.push_back( std::move( b ) );
        }
        const int rowH = 38, w = 190, h = static_cast<int>( actions.size() ) * rowH + 16;
        auto      all = getLocalBounds();
        // beside the tile (right, else left), vertically aligned with it and kept inside the screen
        int x = anchor.getRight() + 6 + w <= all.getWidth() ? anchor.getRight() + 6 : anchor.getX() - 6 - w;
        x     = juce::jlimit( 0, juce::jmax( 0, all.getWidth() - w ), x );
        int y = juce::jlimit( 0, juce::jmax( 0, all.getHeight() - h ), anchor.getY() );
        bounds = { x, y, w, h };
        auto col = bounds.reduced( 8 );
        for( auto& b : buttons )
            b->setBounds( col.removeFromTop( rowH ).reduced( 0, 2 ) );
        setVisible( true );
        enterModalState( true, juce::ModalCallbackFunction::create( [this]( int ) { setVisible( false ); } ), false );
        repaint();
    }

    void LongPressComp::close()
    {
        if( isCurrentlyModal() )
            exitModalState( 0 );
        setVisible( false );
        buttons.clear();
        actions.clear();
    }

    void LongPressComp::activate( int index )
    {
        if( !juce::isPositiveAndBelow( index, actions.size() ) )
            return;
        auto run = actions[static_cast<size_t>( index )].run;
        close();
        if( run )
            juce::MessageManager::callAsync( run ); // after the menu is gone (the action may open a dialog)
    }

    void LongPressComp::hwStep( int delta )
    {
        if( buttons.empty() )
            return;
        selected = ( selected + delta + static_cast<int>( buttons.size() ) ) % static_cast<int>( buttons.size() );
        repaint();
    }

    void LongPressComp::hwActivate()
    {
        activate( selected );
    }

    void LongPressComp::mouseDown( const juce::MouseEvent& e )
    {
        if( !bounds.contains( e.getPosition() ) ) // a tap outside closes the menu
            close();
    }

    void LongPressComp::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, bounds, ui::Theme::backgroundColour( 1 ), 5.0f, true, true, true, true );
        mt::utils::renderRoundedRect( g, bounds, ui::Theme::textColour( 1 ), 1.0f, 2.0f, true, true, true, true );
        if( juce::isPositiveAndBelow( selected, buttons.size() ) ) // the Data Dial entry
            mt::utils::renderRoundedRect( g, buttons[static_cast<size_t>( selected )]->getBounds().expanded( 2 ), ui::Theme::outlineColour( 1 ), 2, 4.0f );
    }
}
