namespace ui
{
    bool RecentProject::enableRecentProjVTreeEntry{ true };
    bool RecentProject::isProjectLoading{ false };

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

        longPressComp->onLoadTriggered = [&]( int Index ) {
            if( juce::isPositiveAndBelow( Index, recentItems.size() ) )
                recentItems[Index]->onMouseDown( Index, recentItems[Index]->getPath() );
        };

        longPressComp->onRemoveTriggered = [&]( int Index ) {
            auto idxOffset = demoList.getSize() + templateList.getSize();
            auto p         = new FilePopup{ "Do you want to remove from this list", true, true };
            mt::utils::showDialog(
                "WARNING!", p, [&, p, Index, idxOffset]( int R ) {

                            if( R == UI::APPLY )
                            {
                                juce::String currProj    = settings.projects.currentProject();
                                auto         reversedRow = getReversedRow( Index - idxOffset );

                                if( !ae.isProjectLoaded() || settings.projects.getProject( reversedRow ) != currProj )
                                {
                                    removeFileFromVTree( Index - idxOffset );
                                    setNumOptions( getNumOfChild() );
                                    longPressComp->setVisible( false );
                                }
                                else
                                {
                                    auto pp = new FilePopup{ "Current Project is not allowed to Remove", false, false };
                                    mt::utils::showDialog(
                                        "WARNING!", pp, [&, pp]( int R ) { delete pp; }, { ui::layout::warningPopup }, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
                                }
                            }
                            delete p; }, { ui::layout::warningPopup }, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
        };

        header.onAutoLoadClick = [&]() {
            auto p = new FilePopup{ "Automatically load last opened project on startup?", false, false };
            mt::utils::showOptions(
                "AUTO LOAD", p, [this, p]( int R ) {
					if( R == UI::APPLY )
					{
						header.setAutoLoadToggleState( true );
						settings.projects.isAutoLoad = ( true );

						header.setAutoLoadToggleState( settings.projects.isAutoLoad() );
					}
                    else
                    {
                        header.setAutoLoadToggleState( false );
                        settings.projects.isAutoLoad = ( false );

                        header.setAutoLoadToggleState( settings.projects.isAutoLoad() );
                    }
					delete p; }, ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
        };

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
        auto p = new FilePopup( message, false, false );
        mt::utils::showDialog(
            "WARNING!", p, [p]( int ) { delete p; },
            ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
    }

    void RecentProject::createProject( juce::String value )
    {
        // Same rules as a project made from a template: trimmed, a legal folder name, and not a folder that exists on the disk
        // (the recent list does not know every folder).
        const auto name = kick_project::trimName( value.toStdString() );
        if( const auto problem = kick_project::checkName( name ); problem != kick_project::NameProblem::Ok )
        {
            warn( kick_project::describe( problem ) );
            return;
        }
        const juce::String trimmedName( name );
        const auto         nChild = settings.projects.getNumChildren();
        for( auto i = 0; i < nChild; i++ )
        {
            if( trimmedName == juce::File( settings.projects.getProject( i ) ).getFileName() )
            {
                warn( kick_project::describe( kick_project::CreateResult::AlreadyExists ) );
                return;
            }
        }

        juce::File file = getAppDirs( MTProjects ).getChildFile( trimmedName );
        if( file.exists() )
        {
            warn( kick_project::describe( kick_project::CreateResult::AlreadyExists ) );
            return;
        }
        settings.projects.createProject( file );
        loadProject( file );
    }

    void RecentProject::sliderMoved( float )
    {}

    void RecentProject::onProjectLoaded( juce::File file )
    {
        if( enableRecentProjVTreeEntry )
            settings.projects.rearrangeProject( file );
        else
            enableRecentProjVTreeEntry = !enableRecentProjVTreeEntry;
    }

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
        loadProject( destinationFile );
    }

    void RecentProject::loadProject( juce::File& Project )
    {
        ui::LoadProjectAndAnimation::getInstance().startLoading( Project );
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
        auto idxOffset = demoList.getSize() + templateList.getSize();
        recentItems.erase( recentItems.begin() + idxOffset, recentItems.end() );

        projectList.clear();

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
            recentItemsPtr->setProperties( i, demoDirList[i].getFullPathName() );

            recentItemsPtr->clearAllShowBorderCallback = [&]() {
                clearAllShowBorder();
            };

            recentItemsPtr->updateSelectedIndex = [&]( int index ) {
                selectedIndex = index;
            };
            recentItemsPtr->onMouseDown = [&]( int index, juce::File project ) {
                enableRecentProjVTreeEntry = false;
                loadProject( project );
            };

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
        auto idxOffset = demoList.getSize();

        for( auto i = 0; i < templateDirList.size(); i++ )
        {
            auto recentItemsPtr = std::make_unique<RecentItems>();
            recentItemsPtr->setProperties( i + idxOffset, templateDirList[i].getFullPathName() );

            recentItemsPtr->clearAllShowBorderCallback = [&]() {
                clearAllShowBorder();
            };

            recentItemsPtr->updateSelectedIndex = [&]( int index ) {
                selectedIndex = index;
            };

            recentItemsPtr->onMouseDown = [&]( int index, juce::File project ) {
                openVirtualKeyBoard( project );
            };

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
                auto idxOffset      = demoList.getSize() + templateList.getSize();

                recentItemsPtr->setProperties( recentItems.size(), file.getFullPathName() );

                recentItemsPtr->clearAllShowBorderCallback = [&]() {
                    clearAllShowBorder();
                };
                recentItemsPtr->onLongPressed = [&, idxOffset]( int Index ) {
                    // Tile in this screen's coordinates (follows the list scroll and the layout; was a hard-coded +432 offset).
                    auto* tile = projectList.listOptions[Index - idxOffset];
                    auto  b    = getLocalArea( tile, tile->getLocalBounds() );

                    b.setY( b.getY() + 7 );
                    b.setWidth( b.getWidth() - 105 );
                    b.setX( b.getX() + b.getWidth() + 8 );
                    b.setHeight( b.getHeight() - 26 );

                    longPressComp->setBounds( getLocalBounds() );
                    longPressComp->setCompBounds( b );
                    longPressComp->showComp( Index );
                };

                recentItemsPtr->updateSelectedIndex = [&]( int index ) {
                    selectedIndex = index;
                };

                recentItemsPtr->onMouseDown = [&]( int index, juce::File project ) {
                    juce::MessageManager::callAsync( [this, index, project] {
                        auto idxOffset = demoList.getSize() + templateList.getSize();
                        if( idxOffset <= index )
                        {
                            juce::File file = getRecentProject( index - idxOffset );
                            loadProject( file );
                        }
                    } );
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

    void RecentProject::handleCommand( CPktMgr::HWMsg_t& hwMsg )
    {
        using dc = contracts::DeviceControlContracts;
        using H      = contracts::DclContractsHelper;

        // Data dial: move the selection.
        if( H::isKnobMessage( hwMsg ) && H::getKnobCode( hwMsg ) == dc::KnobSequence::EN_KNOB_CENTER )
        {
            knobRotated( H::getKnobValue( hwMsg ) );
            return;
        }

        if( !H::isButtonMessage( hwMsg ) || H::getButtonState( hwMsg ) != dc::BTN_PRESSED )
            return;

        // Front-panel buttons on this screen (they used to do nothing without a project).
        switch( H::getButtonCode( hwMsg ) )
        {
            case dc::BtnLedSequence::EN_KNOB_ENTER: knobClicked(); break;                                     // open the selected tile
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

    void RecentProject::knobRotated( double value )
    {
        if( longPressComp->isVisible() )
            longPressComp->setVisible( false );

        if( juce::isPositiveAndBelow( selectedIndex, recentItems.size() ) )
            recentItems[selectedIndex]->showBorder( false );

        selectedIndex = selectedIndex + value;

        if( selectedIndex < 0 )
            selectedIndex = 0;

        if( selectedIndex >= recentItems.size() )
            selectedIndex = recentItems.size() - 1;

        if( juce::isPositiveAndBelow( selectedIndex, recentItems.size() ) )
            recentItems[selectedIndex]->showBorder( true );

        auto projectsStartIndex = demoList.getSize() + templateList.getSize();
        if( juce::isPositiveAndBelow( selectedIndex, recentItems.size() ) && selectedIndex >= projectsStartIndex )
        {
            auto visibleWidth    = projectList.getWidth();
            auto componentBounds = recentItems[selectedIndex]->getBounds();
            auto viewPosition    = projectList.getViewPositionX();

            // Calculate the right edge position of the component within the list
            auto componentRightEdge = componentBounds.getRight() - viewPosition;

            // If the right edge of the component is outside the visible area, adjust the view position
            if( componentRightEdge > visibleWidth )
            {
                projectList.setViewPosition( componentBounds.getX(), projectList.getViewPositionY() );
            }
            // If the left edge of the component is outside the visible area, adjust the view position
            else if( componentBounds.getX() < viewPosition )
            {
                projectList.setViewPosition( componentBounds.getX(), projectList.getViewPositionY() );
            }
        }
    }

    void RecentProject::visibilityChanged()
    {
        auto isProjectLoaded = ae.isProjectLoaded();
        header.showBackButton( isProjectLoaded );

        if( isVisible() )
        {
            settings.projects.refreshMaxNumOfRecentProj();
            setNumOptions( getNumOfChild() );
            auto projectStartIndex = demoList.getSize() + templateList.getSize();
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

    void RecentItems::setProperties( int Index, const juce::String& Path )
    {
        index = Index;
        path  = Path;

        if( path.isDirectory() )
        {
            juce::Array<juce::File> files;
            path.findChildFiles( files, juce::File::findFiles, false, "*.png" );

            if( files.isEmpty() )
            {
                label.setText( path.getFileNameWithoutExtension(), juce::dontSendNotification );
                timeStamp.setText( path.getLastModificationTime().toString( false, true, false, false ), juce::dontSendNotification );
                date.setText( path.getLastModificationTime().toString( true, false, false, false ), juce::dontSendNotification );

                // Component visibility
                mt::utils::makeVisible( date, timeStamp, icon );
            }
            else
            {
                image.setImage( "recent_project" + juce::File::getSeparatorString() + files[0].getFileName() );
                label.setText( files[0].getFileNameWithoutExtension(), juce::dontSendNotification );

                // Component visibility
                mt::utils::makeHidden( date, timeStamp, icon );
            }
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
        startTimer( longPressThreshold ); // Start the timer for long press detection
    }

    void RecentItems::mouseDrag( const juce::MouseEvent& e )
    {
        if( e.getDistanceFromDragStart() > dragThreshold )
        {
            DBG( "MouseDrag -> isDragging" );

            isDragging = true;
            stopTimer(); // Stop the timer if dragging is detected
        }
    }

    void RecentItems::mouseUp( const juce::MouseEvent& e )
    {
        if( isLongPressTriggered && !isDragging )
        {
            DBG( "LongPressed " );
            if( onLongPressed )
                onLongPressed( index );
        }
        else if( e.mouseWasClicked() && !isDragging ) //Ignore dragging
        {
            startTimer( juce::MouseEvent::getDoubleClickTimeout() + 5 ); //Let it happend if it is double-click
        }
    }

    void RecentItems::mouseDoubleClick( const juce::MouseEvent& e )
    {
        isDoubleClicked = true;
    }

    void RecentItems::timerCallback()
    {
        if( !isDragging && getTimerInterval() == longPressThreshold )
        {
            DBG( "Longpressed timer " );

            isLongPressTriggered = true;
        }
        else //mouseDown or mouseDoubleClick
        {
            juce::Component::SafePointer<RecentItems> safe( this );
            juce::Timer::callAfterDelay( 20, [safe]() { //Give some delay to draw the border
                if( safe != nullptr && safe->onMouseDown )
                    safe->onMouseDown( safe->index, safe->path );
            } );
            isDoubleClicked = false;
        }
        stopTimer();
    }

    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////         LongPressComp      ////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

    LongPressComp::LongPressComp()
    {
        mt::utils::addAndMakeVisible( this, loadBtn, removeBtn );
        loadBtn.onClick = [&]() {
            if( onLoadTriggered )
                onLoadTriggered( currentIndex );
        };

        removeBtn.onClick = [&]() {
            if( onRemoveTriggered )
                onRemoveTriggered( currentIndex );
        };
    }

    void LongPressComp::setCompBounds( juce::Rectangle<int> Bounds )
    {
        bounds = Bounds;
        resized();
    }

    void LongPressComp::showComp( int Index )
    {
        currentIndex = Index;
        setVisible( true );
        enterModalState( true, juce::ModalCallbackFunction::create( [this]( int R ) {
                             setVisible( false );
                         } ),
                         false );
    }

    void LongPressComp::mouseDown( const juce::MouseEvent& e )
    {
        if( !bounds.contains( e.getPosition() ) ) //checks whether clicking on outside
            setVisible( false );
    }

    void LongPressComp::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, bounds, ui::Theme::backgroundColour( 1 ), 5.0f, true, true, true, true );
        mt::utils::renderRoundedRect( g, bounds, ui::Theme::textColour( 1 ), 1.0f, 2.0f, true, true, true, true );
    }

    void LongPressComp::resized()
    {
        auto b = bounds;
        loadBtn.setBounds( b.removeFromTop( bounds.getHeight() / 2 ).withTrimmedTop( 10 ).withTrimmedBottom( 5 ).withTrimmedLeft( 10 ).withTrimmedRight( 10 ) );
        removeBtn.setBounds( b.removeFromBottom( bounds.getHeight() / 2 ).withTrimmedTop( 5 ).withTrimmedBottom( 10 ).withTrimmedLeft( 10 ).withTrimmedRight( 10 ) );
    }
}
