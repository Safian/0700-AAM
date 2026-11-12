#pragma once

namespace ui
{
    namespace
    {
        std::vector<kick_mounts::Volume> readVolumes()
        {
#if defined( LINUX ) || defined( __linux__ )
            return kick_mounts::parse( juce::File( "/proc/mounts" ).loadFileAsString().toStdString() );
#else
            return {};
#endif
        }

        bool toolOnPath( const std::string& tool )
        {
            juce::StringArray dirs;
            dirs.addTokens( juce::SystemStats::getEnvironmentVariable( "PATH", "/usr/sbin:/sbin:/usr/bin:/bin" ), ":", "" );
            dirs.add( "/sbin" );
            dirs.add( "/usr/sbin" );
            for( const auto& d : dirs )
                if( juce::File( d ).getChildFile( juce::String( tool ) ).existsAsFile() )
                    return true;
            return false;
        }

        juce::String iconFor( const kick_filelist::Entry& e, const juce::String& projectExt )
        {
            const juce::File f( juce::String::fromUTF8( e.name.c_str() ) );
            if( e.isDir )
                return e.isProject ? "Icon_ProjectFile" : "Vector";
            if( f.hasFileExtension( projectExt ) )
                return "Icon_ProjectFile";
            if( f.hasFileExtension( fileUtils::sampleExtensions() ) )
                return "Icon_SampleFile";
            return "Icon_DefaultFile";
        }

        // Toggles of the browser options (Options button).
        class OptionsPopup : public juce::Component
        {
        public:
            explicit OptionsPopup( BrowserOptions o, std::function<void()> driveInfo )
                : opts( o ), onDriveInfo( std::move( driveInfo ) )
            {
                for( auto* b : { &recursive, &hideSys, &hideData, &copyExt, &dateB, &timeB, &sizeB, &infoB, &filterB } )
                {
                    addAndMakeVisible( b );
                    b->setValueColour( ui::Theme::textColour( 1 ) );
                    b->setValueFontHeight( 15 );
                }
                recursive.onClick = [this] { opts.recursiveSearch = !opts.recursiveSearch; refresh(); };
                hideSys.onClick   = [this] { opts.hideSystem = !opts.hideSystem; refresh(); };
                hideData.onClick  = [this] { opts.hideProjectData = !opts.hideProjectData; refresh(); };
                copyExt.onClick   = [this] { opts.copyExternal = !opts.copyExternal; refresh(); };
                dateB.onClick     = [this] { opts.showDate = !opts.showDate; refresh(); };
                timeB.onClick     = [this] { opts.showTime = !opts.showTime; refresh(); };
                sizeB.onClick     = [this] { opts.showSize = !opts.showSize; refresh(); };
                infoB.onClick     = [this] { if( onDriveInfo ) onDriveInfo(); };
                filterB.onClick   = [this] { opts.filter = ( opts.filter + 1 ) % 4; refresh(); };
                refresh();
            }
            BrowserOptions result() const { return opts; }

        private:
            BrowserOptions        opts;
            std::function<void()> onDriveInfo;
            KickButton            recursive{ "OptRecursive", "" }, hideSys{ "OptHideSys", "" }, hideData{ "OptHideData", "" }, copyExt{ "OptCopy", "" };
            KickButton            dateB{ "OptDate", "" }, timeB{ "OptTime", "" }, sizeB{ "OptSize", "" }, infoB{ "OptInfo", "Drive info" }, filterB{ "OptFilter", "" };
            static juce::String   onOff( const juce::String& t, bool v ) { return t + ( v ? ": ON" : ": OFF" ); }
            void                  refresh()
            {
                recursive.setButtonText( onOff( "Search in sub-folders", opts.recursiveSearch ) );
                hideSys.setButtonText( onOff( "Hide system files", opts.hideSystem ) );
                hideData.setButtonText( onOff( "Hide project data folders", opts.hideProjectData ) );
                copyExt.setButtonText( onOff( "Copy samples into the project", opts.copyExternal ) );
                dateB.setButtonText( onOff( "Date", opts.showDate ) );
                timeB.setButtonText( onOff( "Time", opts.showTime ) );
                sizeB.setButtonText( onOff( "Size", opts.showSize ) );
                static const char* names[] = { "Projects", "Samples", "All files", "MIDI files" };
                filterB.setButtonText( juce::String( "Show: " ) + names[juce::jlimit( 0, 3, opts.filter )] );
            }
            void resized() override
            {
                recursive.setBoundsRelative( 0.10f, 0.04f, 0.80f, 0.10f );
                hideSys.setBoundsRelative( 0.10f, 0.15f, 0.80f, 0.10f );
                hideData.setBoundsRelative( 0.10f, 0.26f, 0.80f, 0.10f );
                copyExt.setBoundsRelative( 0.10f, 0.37f, 0.80f, 0.10f );
                dateB.setBoundsRelative( 0.10f, 0.48f, 0.25f, 0.10f );
                timeB.setBoundsRelative( 0.375f, 0.48f, 0.25f, 0.10f );
                sizeB.setBoundsRelative( 0.65f, 0.48f, 0.25f, 0.10f );
                filterB.setBoundsRelative( 0.10f, 0.59f, 0.80f, 0.10f );
                infoB.setBoundsRelative( 0.10f, 0.70f, 0.80f, 0.10f );
            }
        };

        // Touch-friendly selector popup for + Cloud actions.
        class CloudSourcePopup : public juce::Component
        {
        public:
            enum class Choice { None = 0, ScanNetwork, GoogleDrive, FilesImport, Splice, WebDav };
            Choice selected{ Choice::None };

            CloudSourcePopup()
            {
                auto setup = [this]( KickButton& b, Choice c ) {
                    addAndMakeVisible( b );
                    b.setValueColour( ui::Theme::textColour( 1 ) );
                    b.setValueBackgroundColour( ui::Theme::backgroundColour( 2 ) );
                    b.setValueFontHeight( 16.0f );
                    b.onClick = [this, c]() {
                        selected = c;
                        mt::utils::exitDialog( this, UI::APPLY );
                    };
                };
                setup( scanBtn, Choice::ScanNetwork );
#if JUCE_IOS || JUCE_MAC
                setup( googleBtn, Choice::GoogleDrive );
#endif
#if JUCE_IOS
                setup( filesBtn, Choice::FilesImport );
#endif
                setup( spliceBtn, Choice::Splice );
                setup( webdavBtn, Choice::WebDav );
            }

            void resized() override
            {
                auto area = getLocalBounds().reduced( 28, 18 );
                std::vector<KickButton*> rows;
                rows.push_back( &scanBtn );
#if JUCE_IOS || JUCE_MAC
                rows.push_back( &googleBtn );
#endif
#if JUCE_IOS
                rows.push_back( &filesBtn );
#endif
                rows.push_back( &spliceBtn );
                rows.push_back( &webdavBtn );
                const int gap   = 10;
                const int total = static_cast<int>( rows.size() );
                const int rowH  = juce::jmax( 34, ( area.getHeight() - gap * ( total - 1 ) ) / total );
                for( int i = 0; i < total; ++i )
                {
                    rows[static_cast<size_t>( i )]->setBounds( area.removeFromTop( rowH ) );
                    if( i + 1 < total )
                        area.removeFromTop( gap );
                }
            }

        private:
            KickButton scanBtn{ "CloudScan", "Scan Network (Mac / NAS)" };
#if JUCE_IOS || JUCE_MAC
            KickButton googleBtn{ "CloudGoogle", "Google Drive (Account / Link)" };
#endif
#if JUCE_IOS
            KickButton filesBtn{ "CloudFiles", "Import from Files / Drive App" };
#endif
            KickButton spliceBtn{ "CloudSplice", "Splice Folder Path" };
            KickButton webdavBtn{ "CloudWebdav", "WebDAV / NAS Server" };
        };
    }

    FileBrowser::FileBrowser()
        : kick_ui::IScreen( kick_ui::Screen::MenuFileBrowser ), ae( mt::getAudioInterface() ), sampleAnimation( std::make_unique<LoadSamplesAndAnimation>() )
    {
        startTimer( 1000 ); // drives
        loadOptions();

        bottom.volumeBtn.setToggleState( false, juce::dontSendNotification );
        addMouseListener( this, true );
        bottom.volumeBtn.removeMouseListener( this );
        mt::utils::addChildComponents( this, left, right, top, bottom );

        pwd = placeRoot = ae.getInternalDir().getFullPathName();
        bottom.allButton.setToggleState( true, juce::dontSendNotification );

        auto fixed = [this]( Place::Kind k, const juce::String& label, const juce::String& icon, const juce::String& active, float w, const juce::File& dir ) {
            Place p;
            p.kind       = k;
            p.label      = label;
            p.icon       = icon;
            p.iconActive = active;
            p.width      = w;
            p.height     = 16.0f;
            p.path       = dir.getFullPathName();
            fixedPlaces.push_back( p );
        };
        fixed( Place::Kind::Internal, "Internal", "Icon_Internal", "Icon_InternalActive", 16.0f, ae.getInternalDir() );
#if JUCE_IOS
        fixed( Place::Kind::Library, "Samples", "Icon_Library", "Icon_LibraryActive", 18.0f, ae.getSoundsDir() );
#else
        fixed( Place::Kind::Library, "Factory Sample Library", "Icon_Library", "Icon_LibraryActive", 18.0f, ae.getSoundsDir() );
#endif
        fixed( Place::Kind::Demos, "Demos", "Icon_Demo", "Icon_DemoActive", 16.0f, ae.getDemosDir() );
        {
            const auto prefs = getAppDirs( MTPrefs );
            hub              = std::make_unique<fileSource::SourceHub>( prefs.getFullPathName().toStdString(), prefs.getSiblingFile( "source_cache" ).getFullPathName().toStdString(), 512ll * 1024 * 1024 );
        }
        rebuildShortcuts();
        refreshSourceRows();
        left.sourceButton.onClick = [this]() { addOrEditCloudSource(); };
        left.folderButton.onClick = [this]() { editLocalSource(); };
        left.places.onClick       = [this]() { setLeftTab( LeftTab::Places ); };
        left.content.onClick      = [this]() { setLeftTab( LeftTab::Content ); };
        left.expansion.onClick    = [this]() { setLeftTab( LeftTab::Expansion ); };
        left.places.setToggleState( true, juce::dontSendNotification );
        mt::utils::addAndMakeVisible( this, header, left, right, bottom, pathLabel, top, auditionComp );
        mt::utils::makeHidden( auditionComp );

        auditionComp.onGainChanged = [&]( double Value ) {
            settings.preview.mute = auditionComp.isMuted();
            ae.setPreviewGain( Value );
        };

        auditionComp.onMuteChanged = [&]( bool State ) {
            settings.preview.mute = State;

            if( State )
            {
                mt::utils::makeHidden( bottom.volumeBtn );
                mt::utils::makeVisible( bottom.volumeMuteBtn );
                ae.setPreviewMute( State );
            }
            else
            {
                mt::utils::makeHidden( bottom.volumeMuteBtn );
                mt::utils::makeVisible( bottom.volumeBtn );
                ae.setPreviewMute( State );
            }
        };

        pathLabel.setText( "Folder Path/Folder", juce::dontSendNotification );
        header.showNewProjectButton( true );
        header.showOptions( false, false );
        header.onBackButtonPressed = [this]( bool ) {
            closeScreen();
        };

        //Adding favourite Image Buttons
        for( int i = 5; i > 0; --i )
        {
            std::stringstream buttonName;
            buttonName << "button" << i;

            std::stringstream imagePath;
            imagePath << "Header/buttons/" << i << ".svg";

            std::stringstream activeImagePath;
            activeImagePath << "Header/buttons/ActiveButtons/"
                            << "Button - Memory " << i + 5 << ".svg";

            grpNum.push_back( std::make_unique<ui::KickImageButton>( buttonName.str(), imagePath.str(), activeImagePath.str(), activeImagePath.str() ) );
        }
        favouriteButtonImplementation();

        // External request (hardware Shift+SOUNDS): act once the screen has been loaded.
        kick_ui::signals::sigBrowserShowFavourites.connect( [this]() { favouritesRequested = true; } );
        kick_ui::signals::onScreenLoaded.connect( [this]( kick_ui::Screen Scrn ) {
            if( Scrn == kick_ui::Screen::MenuFileBrowser && favouritesRequested )
            {
                favouritesRequested = false;
                showNextFavourite();
            }
        } );

        //TOP BUTTONS
        top.searchLabel.setText( formatDisplayPath( pwd ), juce::dontSendNotification );

        top.searchLabel.onKeyBoardEnd = [&]() {
            if( remoteActive )
            {
                remoteSearch = top.searchLabel.getText().trim().toLowerCase();
                applyRemoteFilter();
                right.table->refresh();
                right.backButton.setEnabled( true );
                return;
            }
            startSearch( top.searchLabel.getText() );
        };
        top.deleteBtn.onClick = [&]() {
            if( remoteActive )
                return; // remote locations are read-only
            deleteSystem();
        };

        auto filterClicked = [this]( int which, KickImageButton& b ) {
            if( !b.getToggleState() )
                return;
            filterNum = which;
            if( remoteActive )
            {
                applyRemoteFilter();
                right.table->refresh();
            }
            else
                navigate( searchMode ? pwd : pwd ); // the same folder, listed with the new filter
        };
        bottom.sampleButton.onClick  = [this, filterClicked]() { filterClicked( Samples, bottom.sampleButton ); };
        bottom.projectButton.onClick = [this, filterClicked]() { filterClicked( Projects, bottom.projectButton ); };
        bottom.allButton.onClick     = [this, filterClicked]() { filterClicked( All, bottom.allButton ); };

        right.onHeaderClicked = [this]( int key ) {
            // The same column again reverses the order.
            if( opts.sortKey == key )
                opts.sortAscending = !opts.sortAscending;
            else
            {
                opts.sortKey       = key;
                opts.sortAscending = true;
            }
            saveOptions();
            right.setSortIndicator( opts.sortKey, opts.sortAscending );
            if( !remoteActive && !searchMode )
                navigate( pwd );
        };
        right.setSortIndicator( opts.sortKey, opts.sortAscending );

        right.backButton.onClick = [&]() {
            if( remoteActive )
            {
                remoteBack();
                right.backButton.setWantsKeyboardFocus( false );
                return;
            }
            if( searchMode )
                navigate( pwd );
            else
                exitDirectory();
            right.backButton.setWantsKeyboardFocus( false );
        };

        header.onNewProjectClick = [&]() {
            header.setNewProjectToggleState( false );
            // The same checked path as the Projects screen: trimmed legal name, nothing that exists on the disk is replaced.
            project_actions::askText( "CREATE NEW PROJECT", settings.projects.namePrefix(), []( juce::String text ) { project_actions::createProject( text ); } );
        };

        //BOTTOM BUTTONS
        settings.preview.isOn ? bottom.playIconButton.setEnabled( true ) : bottom.playIconButton.setEnabled( false );

        bottom.playIconButton.onClick = [&]() {
            if( !settings.preview.isAuto )
                return; // MAN: the sound plays while the button is held (mouseDown / mouseUp)
            if( ( bottom.playIconButton.getToggleState() ) && !selectedFile.isEmpty() && ( selectedFile.getLast().hasFileExtension( fileUtils::sampleExtensions() ) ) )
                audition( juce::File( selectedFile.getLast() ) );
            if( bottom.playIconButton.getToggleState() == false )
                ae.stopPreview();
        };

        bottom.openButton.onClick = [&]() { openSelected(); };
        bottom.optionsButton.onClick = [&]() { showOptions(); };
        bottom.loadButton.onClick    = [&]() { loadSelected(); };
        bottom.loadAllButton.onClick = [&]() { loadAllVisible(); };
        bottom.auditionButton.onClick = [&]() {
            bottom.auditionButton.incrementSelection();

            int sel = bottom.auditionButton.getCurrentSelection();
            if( sel == 0 )
            {
                settings.preview.isOn = false;
                ae.stopPreview();
                bottom.auditionButton.setButtonText( "OFF" );
                bottom.playIconButton.setToggleState( false, juce::dontSendNotification );
            }
            else if( sel == 1 )
            {
                settings.preview.isOn   = true;
                settings.preview.isAuto = true;
                bottom.auditionButton.setButtonText( "AUTO" );
                bottom.playIconButton.setToggleState( true, juce::dontSendNotification );
            }
            else if( sel == 2 )
            {
                settings.preview.isOn   = true;
                settings.preview.isAuto = false;
                bottom.auditionButton.setButtonText( "MAN" );
                bottom.playIconButton.setToggleState( false, juce::dontSendNotification );
            }
            settings.preview.isOn ? bottom.playIconButton.setEnabled( true ) : bottom.playIconButton.setEnabled( false );
        };
        //Edit Filter
        mt::utils::makeVisible( top.deleteBtn );
        mt::utils::makeHidden( top.cutBtn, top.copyBtn, top.pasteBtn, top.renameBtn, top.selectAllBtn );

        top.selectBtn.onClick = [&]() {
            selectBtnOnClick();
        };
        top.selectAllBtn.onClick = [&]() {
            right.table->selectAll();
            syncSelection();
        };
        top.renameBtn.onClick = [&]() {
            renamePopup();
        };

        top.copyBtn.onClick = [&]() {
            top.copyBtn.setToggleState( false, juce::dontSendNotification );
            if( selectedFile.isEmpty() )
            {
                warn( "Please Select the File or Folder to copy" );
                return;
            }
            clip      = Clip::Copy;
            clipFiles = selectedFile;
            statusBar.setBoxInfo( juce::String( clipFiles.size() ) + " item(s) copied: open a folder and press Paste" );
        };
        top.cutBtn.onClick = [&]() {
            top.cutBtn.setToggleState( false, juce::dontSendNotification );
            if( selectedFile.isEmpty() )
            {
                warn( "Please Select the File or Folder to cut" );
                return;
            }
            if( checkIsCurrProj() )
                return;
            clip      = Clip::Cut;
            clipFiles = selectedFile;
            statusBar.setBoxInfo( juce::String( clipFiles.size() ) + " item(s) cut: open a folder and press Paste" );
        };
        top.pasteBtn.onClick = [&]() {
            top.pasteBtn.setToggleState( false, juce::dontSendNotification );
            pasteHere();
        };

        auto handleVolumeOnClick = [&]() {
            bool isAuditionComponentVisible = auditionComp.isVisible();
            if( isAuditionComponentVisible )
                hideAuditionComponent();
            else
                showAuditionComponent();
        };
        bottom.volumeBtn.onClick     = handleVolumeOnClick;
        bottom.volumeMuteBtn.onClick = handleVolumeOnClick;
        //TABLE
        right.table->enableVirtualRows( true ); // thousands of files: only the visible rows get components
        right.table->enableSelectCursor( true );
        right.table->onTableKnobClicked = [&]( int ) { knobClicked(); };
        left.table->enableSetFocus( false );
        setupProvider();

        themeChanged.connect( [&]() {
            auto i = 5;
            for( auto& it : grpNum )
            {
                auto path1 = "Header/buttons/" + juce::String( i ) + ".svg";
                auto path2 = "Header/buttons/ActiveButtons/Button - Memory " + juce::String( i + 5 ) + ".svg";
                it->setImages( path1, path2, path2 );
                --i;
            }
            header.setIcon( "Icon_Browser" );
        } );
    }

    ///////////////////////////////////////
    // options
    ///////////////////////////////////////
    void FileBrowser::loadOptions()
    {
        auto& b = settings.browser;
        auto  flag = [&b]( const char* name, bool dflt ) { const auto v = b.getOption( name ); return v.isVoid() ? dflt : static_cast<bool>( v ); };
        opts.recursiveSearch = flag( "optRecursive", false );
        opts.hideSystem      = flag( "optHideSystem", true );
        opts.hideProjectData = flag( "optHideData", false );
        opts.copyExternal    = flag( "optCopyExternal", true );
        opts.showDate        = flag( "optShowDate", true );
        opts.showTime        = flag( "optShowTime", true );
        opts.showSize        = flag( "optShowSize", true );
        opts.sortAscending   = flag( "optSortAsc", true );
        const auto k         = b.getOption( "optSortKey" );
        opts.sortKey         = k.isVoid() ? 0 : juce::jlimit( 0, 2, static_cast<int>( k ) );
    }

    void FileBrowser::saveOptions()
    {
        auto& b = settings.browser;
        b.setOption( "optRecursive", opts.recursiveSearch );
        b.setOption( "optHideSystem", opts.hideSystem );
        b.setOption( "optHideData", opts.hideProjectData );
        b.setOption( "optCopyExternal", opts.copyExternal );
        b.setOption( "optShowDate", opts.showDate );
        b.setOption( "optShowTime", opts.showTime );
        b.setOption( "optShowSize", opts.showSize );
        b.setOption( "optSortAsc", opts.sortAscending );
        b.setOption( "optSortKey", opts.sortKey );
    }

    void FileBrowser::applyColumnOptions()
    {
        right.setColumns( opts.showDate, opts.showTime, opts.showSize );
        right.table->refresh();
    }

    void FileBrowser::showOptions()
    {
        opts.filter = filterNum;
        auto p      = new OptionsPopup( opts, [this]() { showDriveInfo(); } );
        mt::utils::showOptions(
            "BROWSER OPTIONS", p, [this, p]( int R ) {
                if( R == UI::APPLY )
                {
                    const auto before = opts;
                    opts              = p->result();
                    saveOptions();
                    applyColumnOptions();
                    const bool filterChanged = opts.filter != filterNum;
                    if( filterChanged )
                    {
                        filterNum = opts.filter;
                        bottom.projectButton.setToggleState( filterNum == Projects, juce::dontSendNotification );
                        bottom.sampleButton.setToggleState( filterNum == Samples, juce::dontSendNotification );
                        bottom.allButton.setToggleState( filterNum == All, juce::dontSendNotification );
                        if( remoteActive )
                        {
                            applyRemoteFilter();
                            right.table->refresh();
                        }
                    }
                    if( !remoteActive && ( filterChanged || before.hideSystem != opts.hideSystem || before.hideProjectData != opts.hideProjectData ) )
                        navigate( pwd );
                }
                delete p; },
            ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
    }

    void FileBrowser::showDriveInfo()
    {
        const juce::File where( remoteActive ? juce::String() : pwd );
        juce::StringArray lines;
        const auto*       p = currentPlaceRow();
        lines.add( "Place: " + ( p != nullptr ? p->label : juce::String( "-" ) ) );
        lines.add( "Folder: " + where.getFullPathName() );
        if( where.isDirectory() )
        {
            const auto total = where.getVolumeTotalSize();
            const auto freeB = where.getBytesFreeOnVolume();
            lines.add( "Free: " + juce::String( kick_project::formatBytes( static_cast<uint64_t>( juce::jmax<juce::int64>( 0, freeB ) ) ) ) + " of " + juce::String( kick_project::formatBytes( static_cast<uint64_t>( juce::jmax<juce::int64>( 0, total ) ) ) ) );
            for( const auto& v : readVolumes() )
                if( p != nullptr && p->isDrive() && juce::String( v.mountPoint ) == p->path )
                    lines.add( "Device: " + juce::String( v.device ) + "  (" + juce::String( v.fsType ) + ")" );
        }
        project_actions::infoDialog( "DRIVE INFO", lines );
    }

    ///////////////////////////////////////
    // left list: places
    ///////////////////////////////////////
    void FileBrowser::rebuildShortcuts()
    {
        // Content: quick links into what is on the device: the top folders of the factory library, the demos, the projects.
        shortcutPlaces.clear();
        auto add = [this]( const juce::String& label, const juce::File& dir, const juce::String& icon, const juce::String& active ) {
            if( !dir.isDirectory() )
                return;
            Place p;
            p.kind       = Place::Kind::Shortcut;
            p.label      = label;
            p.icon       = icon;
            p.iconActive = active;
            p.path       = dir.getFullPathName();
            shortcutPlaces.push_back( p );
        };
        juce::Array<juce::File> libraryDirs;
        ae.getSoundsDir().findChildFiles( libraryDirs, juce::File::findDirectories, false );
        std::sort( libraryDirs.begin(), libraryDirs.end(), []( const juce::File& a, const juce::File& b ) { return a.getFileName().compareNatural( b.getFileName() ) < 0; } );
        for( int i = 0; i < libraryDirs.size() && i < 12; ++i )
            if( !kick_filelist::isHiddenOrSystem( libraryDirs[i].getFileName().toStdString() ) )
                add( libraryDirs[i].getFileName(), libraryDirs[i], "Icon_Library", "Icon_LibraryActive" );
        add( "My Projects", ae.getInternalDir(), "Icon_Internal", "Icon_InternalActive" );
        add( "Demos", ae.getDemosDir(), "Icon_Demo", "Icon_DemoActive" );
        if( mt::getSettings().cloudSources.enableSplice.get() )
            add( "Splice", findBestSpliceDirectory(), "Icon_Library", "Icon_LibraryActive" );
    }

    void FileBrowser::rebuildShown()
    {
        shownPlaces.clear();
        auto append = [this]( const std::vector<Place>& v ) { shownPlaces.insert( shownPlaces.end(), v.begin(), v.end() ); };
        switch( leftTab )
        {
            case LeftTab::Places:
                append( fixedPlaces );
                append( sourcePlaces );
                append( drivePlaces );
                break;
            case LeftTab::Content: append( shortcutPlaces ); break;
            case LeftTab::Expansion:
                append( sourcePlaces );
                append( drivePlaces );
                break;
        }
    }

    const Place* FileBrowser::currentPlaceRow() const
    {
        const int r = left.table != nullptr ? left.table->getSelectedRowOrNone() : -1;
        return juce::isPositiveAndBelow( r, static_cast<int>( shownPlaces.size() ) ) ? &shownPlaces[static_cast<size_t>( r )] : nullptr;
    }

    // The row (in the current tab) whose folder is the deepest one containing `path`; -1 when none does.
    int FileBrowser::rowOfPlaceContaining( const juce::String& path ) const
    {
        int    best    = -1;
        size_t bestLen = 0;
        for( size_t i = 0; i < shownPlaces.size(); ++i )
        {
            const auto& p = shownPlaces[i];
            if( p.kind == Place::Kind::Source || p.path.isEmpty() )
                continue;
            if( kick_fsguard::isInside( path.toStdString(), p.path.toStdString() ) && p.path.length() >= static_cast<int>( bestLen ) )
            {
                best    = static_cast<int>( i );
                bestLen = static_cast<size_t>( p.path.length() );
            }
        }
        return best;
    }

    void FileBrowser::setLeftTab( LeftTab t )
    {
        leftTab = t;
        left.places.setToggleState( t == LeftTab::Places, juce::dontSendNotification );
        left.content.setToggleState( t == LeftTab::Content, juce::dontSendNotification );
        left.expansion.setToggleState( t == LeftTab::Expansion, juce::dontSendNotification );
        rebuildShown();
        left.table->clearSelection();
        left.table->refresh();
        const int row = rowOfPlaceContaining( pwd );
        if( row >= 0 )
            showPlace( row, false );
        else if( !shownPlaces.empty() )
            showPlace( 0, true );
        else
            statusBar.setBoxInfo( t == LeftTab::Expansion ? "No drive or extra location" : "Nothing here" );
    }

    void FileBrowser::updateEditButtons()
    {
        const auto* p        = currentPlaceRow();
#if JUCE_IOS
        const bool  readOnly = p != nullptr && ( p->kind == Place::Kind::Demos || p->kind == Place::Kind::Source );
        // On iOS there is no read-only factory library partition: MTSamples (Documents/Kick/sounds) holds user/shared samples.
        const bool inReadOnlyTree = kick_fsguard::isInside( pwd.toStdString(), ae.getDemosDir().getFullPathName().toStdString() );
#else
        const bool  readOnly = p != nullptr && ( p->kind == Place::Kind::Library || p->kind == Place::Kind::Demos || p->kind == Place::Kind::Source );
        // A shortcut into the library or the demos is read-only as well.
        const bool inReadOnlyTree = kick_fsguard::isInside( pwd.toStdString(), ae.getSoundsDir().getFullPathName().toStdString() ) || kick_fsguard::isInside( pwd.toStdString(), ae.getDemosDir().getFullPathName().toStdString() );
#endif
        const bool allow          = !( readOnly || inReadOnlyTree );
        top.pasteBtn.setEnabled( allow );
        top.renameBtn.setEnabled( allow );
        top.cutBtn.setEnabled( allow );
        top.deleteBtn.setEnabled( allow );
        header.setEnableNewProject( p != nullptr && p->kind == Place::Kind::Internal );
    }

    void FileBrowser::showPlace( int row, bool doLoad )
    {
        if( !juce::isPositiveAndBelow( row, static_cast<int>( shownPlaces.size() ) ) )
            return;
        const auto p = shownPlaces[static_cast<size_t>( row )];
        left.table->selectRow( row, juce::dontSendNotification );
        for( auto* it : left.table->getRowComponents() )
            if( auto* comp = static_cast<FileLeftEntry*>( it ) )
                comp->setHighlight( comp->getRowID() == row );
        if( p.kind == Place::Kind::Source )
        {
            updateEditButtons();
            top.pasteBtn.setEnabled( false );
            top.renameBtn.setEnabled( false );
            top.cutBtn.setEnabled( false );
            top.deleteBtn.setEnabled( false );
            header.setEnableNewProject( false );
            if( doLoad )
                openSource( p );
            return;
        }
        leaveSource();
        placeRoot = p.path;
        updateEditButtons();
        if( doLoad )
            navigate( p.path );
    }

    // 1 Hz (visible screen only) and when the screen shows: follow the mount table.
    void FileBrowser::pollDrives()
    {
#if defined( LINUX ) || defined( __linux__ )
        const auto               vols = readVolumes();
        std::vector<std::string> known;
        for( const auto& p : drivePlaces )
            known.push_back( p.device.toStdString() );
        const auto d = kick_mounts::reconcile( vols, known, ejectedDevices );
        if( d.add.empty() && d.removeDevices.empty() )
            return;
        bool removedShown = false;
        for( const auto& dev : d.removeDevices )
            for( auto it = drivePlaces.begin(); it != drivePlaces.end(); ++it )
                if( it->device.toStdString() == dev )
                {
                    if( kick_fsguard::isInside( pwd.toStdString(), it->path.toStdString() ) )
                        removedShown = true;
                    drivePlaces.erase( it );
                    break;
                }
        for( const auto& v : d.add )
        {
            Place p;
            p.kind       = v.kind == kick_mounts::Kind::Sd ? Place::Kind::Sd : Place::Kind::Usb;
            p.path       = juce::String::fromUTF8( v.mountPoint.c_str() );
            p.device     = juce::String( v.device );
            p.icon       = v.kind == kick_mounts::Kind::Sd ? "SD" : "USB";
            p.iconActive = v.kind == kick_mounts::Kind::Sd ? "SDactive" : "UDBactive";
            p.width      = 12.0f;
            p.height     = 16.0f;
            int same     = 0;
            for( const auto& o : drivePlaces )
                same += o.kind == p.kind ? 1 : 0;
            const juce::String base = v.kind == kick_mounts::Kind::Sd ? "External (SD Card)" : "External (USB)";
            p.label                 = same == 0 ? base : base + ": " + juce::String::fromUTF8( v.label().c_str() ); // several drives / partitions
            drivePlaces.push_back( p );
        }
        rebuildShown();
        left.table->refresh();
        if( removedShown )
        {
            statusBar.setBoxInfo( "Drive removed" );
            showPlace( 0, true );
        }
        else if( const int r = rowOfPlaceContaining( pwd ); r >= 0 )
            showPlace( r, false );
#endif
    }

    bool FileBrowser::driveHoldsOpenProject( const Place& p ) const
    {
        return ae.isProjectLoaded() && kick_fsguard::isInside( ae.getProjectDir().getFullPathName().toStdString(), p.path.toStdString() );
    }

    void FileBrowser::ejectDrive( const Place& p )
    {
        if( driveHoldsOpenProject( p ) )
        {
            warn( "The open project is on this drive. Open another project first." );
            return;
        }
        auto result = std::make_shared<kick_mounts::EjectResult>( kick_mounts::EjectResult::Failed );
        const auto mount = p.path.toStdString();
        const auto place = p;
        BackgroundTask::run(
            "EJECTING DRIVE", false,
            [mount, result]( const std::atomic<bool>&, const BackgroundTask::Progress& progress ) {
                progress( 30, "Writing pending data..." );
                *result = kick_mounts::eject( mount, kick_mounts::realSync, kick_mounts::realUmount );
            },
            [this, result, place]() {
                if( *result == kick_mounts::EjectResult::Ok || *result == kick_mounts::EjectResult::NotMounted )
                {
                    // Hidden until it was unplugged: an automounter may mount it again at once.
                    ejectedDevices.insert( place.device.toStdString() );
                    for( auto it = drivePlaces.begin(); it != drivePlaces.end(); ++it )
                        if( it->device == place.device )
                        {
                            drivePlaces.erase( it );
                            break;
                        }
                    const bool wasShown = kick_fsguard::isInside( pwd.toStdString(), place.path.toStdString() );
                    rebuildShown();
                    left.table->refresh();
                    if( wasShown )
                        showPlace( 0, true );
                    statusBar.setBoxInfo( kick_mounts::describe( kick_mounts::EjectResult::Ok ) );
                    return;
                }
                warn( kick_mounts::describe( *result ) ); // busy / failed: the drive stays in the list
            } );
    }

    void FileBrowser::formatDrive( const Place& p )
    {
#if defined( LINUX ) || defined( __linux__ )
        if( !p.isDrive() )
            return;
        if( driveHoldsOpenProject( p ) )
        {
            warn( "The open project is on this drive. Open another project first." );
            return;
        }
        auto which = []( const std::string& t ) { return toolOnPath( t ); };
        auto kind  = kick_mounts::FsKind::ExFat;
        auto tool  = kick_mounts::formatTool( kind, which );
        if( tool.empty() )
        {
            kind = kick_mounts::FsKind::Fat32;
            tool = kick_mounts::formatTool( kind, which );
        }
        if( tool.empty() )
        {
            warn( "Formatting is not available on this device (no mkfs tool is installed)" );
            return;
        }
        const auto place = p;
        const auto label = p.label;
        project_actions::confirm( "FORMAT DRIVE", "Format '" + label + "' as " + kick_mounts::fsName( kind ) + "? All its data is lost.", [this, place, tool, kind, label]( bool yes ) {
            if( !yes )
                return;
            project_actions::confirm( "FORMAT DRIVE", "Last chance: everything on '" + label + "' will be erased.", [this, place, tool, kind]( bool really ) {
                if( !really )
                    return;
                // The drive must still be what the user was shown.
                bool still = false;
                for( const auto& v : readVolumes() )
                    still = still || ( juce::String( v.device ) == place.device && juce::String::fromUTF8( v.mountPoint.c_str() ) == place.path && kick_mounts::formattable( v ) );
                if( !still )
                {
                    warn( "The drive has changed: nothing was formatted" );
                    return;
                }
                auto outcome = std::make_shared<juce::String>();
                BackgroundTask::run(
                    "FORMATTING DRIVE", false,
                    [place, tool, kind, outcome]( const std::atomic<bool>&, const BackgroundTask::Progress& progress ) {
                        progress( 20, "Unmounting..." );
                        const auto e = kick_mounts::eject( place.path.toStdString(), kick_mounts::realSync, kick_mounts::realUmount );
                        if( e != kick_mounts::EjectResult::Ok )
                        {
                            *outcome = kick_mounts::describe( e );
                            return;
                        }
                        progress( 50, "Formatting..." );
                        const auto            argv = kick_mounts::formatArgv( tool, kind, place.device.toStdString(), "KICK" );
                        juce::StringArray     args;
                        for( const auto& a : argv )
                            args.add( juce::String( a ) );
                        juce::ChildProcess proc;
                        if( !proc.start( args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr ) )
                        {
                            *outcome = "The format tool could not be started";
                            return;
                        }
                        proc.waitForProcessToFinish( 10 * 60 * 1000 );
                        if( proc.getExitCode() != 0 )
                            *outcome = "The format failed: " + proc.readAllProcessOutput().trim().substring( 0, 100 );
                    },
                    [this, outcome]() {
                        if( outcome->isNotEmpty() )
                            warn( *outcome );
                        else
                            statusBar.setBoxInfo( "Formatted. If the drive does not show up, take it out and put it in again." );
                        pollDrives();
                    } );
            } );
        } );
#else
        juce::ignoreUnused( p );
        warn( "Formatting a drive is available on the device only" );
#endif
    }

    ///////////////////////////////////////
    // providers
    ///////////////////////////////////////
    void FileBrowser::setupProvider()
    {
        left.provider.numRows = [&]() -> int {
            return static_cast<int>( shownPlaces.size() );
        };

        left.provider.cellComponent = [&]( int Row, Component* Existing ) -> juce::Component* {
            if( !juce::isPositiveAndBelow( Row, static_cast<int>( shownPlaces.size() ) ) )
                return nullptr;
            auto comp = static_cast<FileLeftEntry*>( Existing );
            if( comp == nullptr )
            {
                comp = new FileLeftEntry( Row );
                comp->setInterceptsMouseClicks( false, true );
            }
            comp->ejectCallback  = [this]( int rowId ) {
                if( juce::isPositiveAndBelow( rowId, static_cast<int>( shownPlaces.size() ) ) )
                    ejectDrive( shownPlaces[static_cast<size_t>( rowId )] );
            };
            comp->formatCallback = [this]( int rowId ) {
                if( juce::isPositiveAndBelow( rowId, static_cast<int>( shownPlaces.size() ) ) )
                    formatDrive( shownPlaces[static_cast<size_t>( rowId )] );
            };
            comp->setProperties( shownPlaces[static_cast<size_t>( Row )] );
            return comp;
        };

        left.provider.onRowSelected = [&]( int Row ) {
            showPlace( Row, true );
        };

        right.provider.numRows = [&]() -> int {
            if( remoteActive )
                return static_cast<int>( remoteShown.size() );
            return static_cast<int>( entries.size() );
        };

        right.provider.cellComponent = [&]( int Row, Component* Existing ) -> juce::Component* {
            if( remoteActive )
                return remoteCell( Row, Existing );
            auto comp = static_cast<FileRightEntry*>( Existing );
            if( comp == nullptr )
            {
                comp = new FileRightEntry();
                comp->setInterceptsMouseClicks( false, true );
            }
            comp->setColumns( opts.showDate, opts.showTime, opts.showSize );

            if( !juce::isPositiveAndBelow( Row, static_cast<int>( entries.size() ) ) )
                return comp;

            // Everything shown comes from the listing (one scan): no stat per row here.
            const auto&        e        = entries[static_cast<size_t>( Row )];
            const auto         icon     = iconFor( e, projectFileList );
            const bool         isFolder = icon == "Vector" || icon == "Icon_DefaultFile";
            const float        w        = isFolder ? 22.0f : 14.0f;
            const juce::Time   t( e.mtimeMs );
            const juce::String dateStr  = t.toString( true, false, false, false );
            const juce::String timeStr  = t.toString( false, true, false, true );
            const juce::File   file( juce::String::fromUTF8( e.path.c_str() ) );
            comp->setCurrentPath( file.getFullPathName() );

            if( !e.isDir )
            {
                comp->setProperties( icon, isFolder, w, 16.0f, file.getFileName(), dateStr, timeStr, e.size >= 0 ? juce::File::descriptionOfSizeInBytes( e.size ) : juce::String( "-" ) );
                return comp;
            }
            int64_t cachedSize = 0;
            auto&   cache      = fileUtils::getFolderSizeCache();
            if( cache.get( e.path, e.mtimeMs, cachedSize ) )
            {
                comp->setProperties( icon, isFolder, w, 16.0f, file.getFileName(), dateStr, timeStr, juce::File::descriptionOfSizeInBytes( cachedSize ) );
                return comp;
            }
            // On a drive's root the scan of every folder can take minutes: the size is worked out for the folder that is selected.
            const auto* place  = currentPlaceRow();
            const bool  manual = place != nullptr && place->isDrive() && pwd == placeRoot && !manualSizeRequested.count( e.path );
            static const juce::String pendingStr = juce::CharPointer_UTF8( "\xe2\x80\xa6" );
            comp->setProperties( icon, isFolder, w, 16.0f, file.getFileName(), dateStr, timeStr, manual ? juce::String( "-" ) : pendingStr );
            if( !manual )
                queueFolderSizeCalculation( file, e.mtimeMs, comp ); // only rows that are on screen get here (virtual rows)
            return comp;
        };

        // Double tap / Data Dial press
        right.table->onRowDoubleClicked = [&]( int Row ) {
            if( top.selectBtn.getToggleState() )
                return; // Select mode: a tap marks, it does not open
            activateRow( Row );
        };

        // Single tap / Data Dial turn
        right.provider.onRowSelected = [&]( int Row ) {
            if( remoteActive )
            {
                selectedFile.clear(); // nothing local to audition/copy/delete until the file is downloaded
                return;
            }
            syncSelection();
            updateLoadTargetStatus();
            // The folder under the finger gets its size worked out (drive roots do not do it by themselves).
            if( juce::isPositiveAndBelow( Row, static_cast<int>( entries.size() ) ) && entries[static_cast<size_t>( Row )].isDir && !top.selectBtn.getToggleState() )
            {
                const auto& e = entries[static_cast<size_t>( Row )];
                int64_t     dummy = 0;
                if( manualSizeRequested.insert( e.path ).second && !fileUtils::getFolderSizeCache().get( e.path, e.mtimeMs, dummy ) )
                    if( auto* cell = static_cast<FileRightEntry*>( right.table->getRowComponents()[Row] ) )
                        queueFolderSizeCalculation( juce::File( juce::String::fromUTF8( e.path.c_str() ) ), e.mtimeMs, cell );
            }
            const bool hasSample = !selectedFile.isEmpty() && selectedFile.getLast().hasFileExtension( fileUtils::sampleExtensions() );
            if( settings.preview.isAuto && hasSample )
                audition( selectedFile.getLast() );
            else if( ae.isPreviewRunning() && settings.preview.isAuto )
            {
                bottom.playIconButton.setToggleState( false, juce::dontSendNotification );
                ae.stopPreview();
            }
        };
    }

    void FileBrowser::syncSelection()
    {
        selectedFile.clear();
        for( const int r : right.table->getSelectedRows() )
            if( juce::isPositiveAndBelow( r, static_cast<int>( entries.size() ) ) )
                selectedFile.add( juce::File( juce::String::fromUTF8( entries[static_cast<size_t>( r )].path.c_str() ) ) );
    }

    void FileBrowser::updateLoadTargetStatus()
    {
        if( remoteActive || !ae.isProjectLoaded() || selectedFile.isEmpty() )
            return;
        if( selectedFile.getLast().hasFileExtension( fileUtils::sampleExtensions() ) )
            statusBar.setBoxInfo( "Load target: " + juce::String( kick_fblogic::padText( fileUtils::targetPad(), ae.getBankOffset() ) ) );
    }

    ///////////////////////////////////////
    // folder listing (worker thread, newest request wins)
    ///////////////////////////////////////
    kick_filelist::ListOptions FileBrowser::listOptions() const
    {
        kick_filelist::ListOptions o;
        o.hideSystem      = opts.hideSystem;
        o.key             = static_cast<kick_filelist::SortKey>( juce::jlimit( 0, 2, opts.sortKey ) );
        o.ascending       = opts.sortAscending;
        o.detectProjects  = true;
        o.hideProjectData = opts.hideProjectData && kick_project::isProjectFolder( pwd.toStdString() );
        if( filterNum == Projects )
            o.extensions = projectFileList.toStdString();
        else if( filterNum == Samples )
            o.extensions = fileUtils::sampleExtensions();
        else if( filterNum == Midi )
            o.extensions = ".mid;.midi";
        return o;
    }

    bool FileBrowser::canGoUp() const
    {
        return pwd != placeRoot && kick_fsguard::isInside( pwd.toStdString(), placeRoot.toStdString() );
    }

    juce::String FileBrowser::formatDisplayPath( const juce::String& path ) const
    {
        const auto spliceRoot = findBestSpliceDirectory().getFullPathName();
        if( spliceRoot.isNotEmpty() )
        {
            if( path == spliceRoot )
                return "[Splice]";
            if( kick_fsguard::isInside( path.toStdString(), spliceRoot.toStdString() ) )
                return "[Splice]" + path.substring( spliceRoot.length() );
        }
        return fileUtils::getFilePath( path, replacementPath );
    }

    void FileBrowser::navigate( const juce::String& dir, const juce::String& selectName )
    {
        if( searchToken )
            *searchToken = true; // a running search is over
        searchMode = false;
        cancelFolderSizeCalculations();
        pwd = dir;
        top.searchLabel.setText( formatDisplayPath( pwd ), juce::dontSendNotification );
        right.backButton.setEnabled( canGoUp() );
        startListing( selectName );
    }

    void FileBrowser::startListing( const juce::String& selectName )
    {
        const auto gen = ++listGen;
        const auto dir = pwd.toStdString();
        const auto o   = listOptions();
        juce::Component::SafePointer<FileBrowser> safe( this );
        listPool.addJob( [safe, gen, dir, o, selectName]() {
            auto l = kick_filelist::list( dir, o );
            juce::MessageManager::callAsync( [safe, gen, l = std::move( l ), selectName]() mutable {
                if( safe != nullptr && safe->listGen == gen )
                    safe->onListed( gen, std::move( l ), selectName, false, false );
            } );
        } );
    }

    void FileBrowser::onListed( uint64_t, kick_filelist::Listing l, const juce::String& selectName, bool wasSearch, bool truncated )
    {
        if( !l.ok && !wasSearch && pwd != placeRoot )
        {
            // The folder is gone (drive pulled, deleted elsewhere): back to the root of the place.
            statusBar.setBoxInfo( l.error );
            navigate( placeRoot );
            return;
        }
        entries = std::move( l.entries );
        manualSizeRequested.clear();
        selectedFile.clear();
        right.table->clearSelection();
        right.table->refresh();
        int sel = entries.empty() ? -1 : 0;
        if( selectName.isNotEmpty() )
            for( size_t i = 0; i < entries.size(); ++i )
                if( juce::String::fromUTF8( entries[i].name.c_str() ) == selectName )
                    sel = static_cast<int>( i );
        if( sel >= 0 && !top.selectBtn.getToggleState() )
            right.table->selectRow( sel );
        right.backButton.setEnabled( canGoUp() || searchMode );
        if( pendingImportBanner.isNotEmpty() )
        {
            statusBar.setBoxInfo( pendingImportBanner );
            pendingImportBanner.clear();
        }
        else
        {
            const auto msg = kick_fblogic::listingStatus( l.ok, entries.size(), wasSearch, truncated );
            statusBar.setBoxInfo( msg.empty() ? juce::String( "Box Info" ) : juce::String( msg ) );
        }
    }

    void FileBrowser::startSearch( const juce::String& text )
    {
        const auto needle = text.trim();
        if( needle.isEmpty() || needle == formatDisplayPath( pwd ) )
        {
            navigate( pwd ); // nothing to look for: the folder as it is
            return;
        }
        if( searchToken )
            *searchToken = true;
        auto token  = std::make_shared<std::atomic<bool>>( false );
        searchToken = token;
        searchMode  = true;
        cancelFolderSizeCalculations();
        right.backButton.setEnabled( true );
        const auto gen       = ++listGen;
        const auto dir       = pwd.toStdString();
        const auto o         = listOptions();
        const bool recursive = opts.recursiveSearch;
        statusBar.setBoxInfo( "Searching..." );
        juce::Component::SafePointer<FileBrowser> safe( this );
        listPool.addJob( [safe, gen, dir, o, token, recursive, needle]() {
            auto r = kick_filelist::search( dir, needle.toStdString(), recursive, 500, *token, o );
            if( r.cancelled )
                return;
            kick_filelist::Listing l;
            l.entries = std::move( r.entries );
            const bool truncated = r.truncated;
            juce::MessageManager::callAsync( [safe, gen, l = std::move( l ), truncated]() mutable {
                if( safe != nullptr && safe->listGen == gen )
                    safe->onListed( gen, std::move( l ), {}, true, truncated );
            } );
        } );
    }

    void FileBrowser::exitDirectory()
    {
        if( !canGoUp() )
            return;
        const auto child = juce::File( pwd ).getFileName();
        navigate( juce::File( pwd ).getParentDirectory().getFullPathName(), child ); // the folder we left stays selected
    }

    ///////////////////////////////////////
    // open / load
    ///////////////////////////////////////
    void FileBrowser::loadProjectFolder( const juce::File& dir )
    {
        if( ui::RecentProject::isProjectLoading )
        {
            statusBar.setBoxInfo( "A project is loading..." );
            return;
        }
        const bool demo = kick_fsguard::isInside( dir.getFullPathName().toStdString(), ae.getDemosDir().getFullPathName().toStdString() );
        // The unsaved-changes question of the open project and the error / Recent handling are in the load path.
        ui::LoadProjectAndAnimation::getInstance().startLoading( dir, demo ? LoadSource::Demo : LoadSource::User );
    }

    void FileBrowser::activateRow( int Row )
    {
        if( remoteActive )
        {
            remoteActivate( Row );
            return;
        }
        if( ui::RecentProject::isProjectLoading || !juce::isPositiveAndBelow( Row, static_cast<int>( entries.size() ) ) )
            return;
        const auto&      e = entries[static_cast<size_t>( Row )];
        const juce::File f( juce::String::fromUTF8( e.path.c_str() ) );
        if( e.isDir )
        {
            if( e.isProject )
                loadProjectFolder( f ); // a project folder opens as a project ("Open" looks inside)
            else
                navigate( f.getFullPathName() );
        }
        else if( f.hasFileExtension( projectFileList ) )
            loadProjectFolder( f.getParentDirectory() );
        else if( f.hasFileExtension( fileUtils::sampleExtensions() ) )
            loadSample( juce::Array<juce::File>( f ), false );
        else
            statusBar.setBoxInfo( "This file type cannot be opened here" );
    }

    void FileBrowser::openSelected()
    {
        if( remoteActive )
        {
            const int r = right.table->getSelectedRowOrNone();
            if( r >= 0 )
                remoteActivate( r );
            return;
        }
        const int r = right.table->getSelectedRowOrNone();
        if( !juce::isPositiveAndBelow( r, static_cast<int>( entries.size() ) ) )
            return;
        if( entries[static_cast<size_t>( r )].isDir )
            navigate( juce::String::fromUTF8( entries[static_cast<size_t>( r )].path.c_str() ) ); // inside, also a project folder
        else
            activateRow( r );
    }

    // Load: a project opens as a project, a folder is entered, a sample goes on the target pad. Nothing is made up.
    void FileBrowser::loadSelected()
    {
        if( remoteActive )
        {
            const int r = right.table->getSelectedRowOrNone();
            if( juce::isPositiveAndBelow( r, static_cast<int>( remoteShown.size() ) ) && !remoteShown[static_cast<size_t>( r )].isDir )
                remoteActivate( r );
            return;
        }
        const int r = right.table->getSelectedRowOrNone();
        if( !juce::isPositiveAndBelow( r, static_cast<int>( entries.size() ) ) )
        {
            statusBar.setBoxInfo( "Select a file first" );
            return;
        }
        activateRow( r );
    }

    // Load All: every sample of the folder as it is shown (filter, search) goes into the project's pool; Sample Assign puts them on pads.
    void FileBrowser::loadAllVisible()
    {
        if( remoteActive )
        {
            statusBar.setBoxInfo( "Load All works on local folders only" );
            return;
        }
        juce::Array<juce::File> files;
        for( const auto& e : entries )
            if( !e.isDir && juce::File( juce::String::fromUTF8( e.name.c_str() ) ).hasFileExtension( fileUtils::sampleExtensions() ) )
                files.add( juce::File( juce::String::fromUTF8( e.path.c_str() ) ) );
        if( files.isEmpty() )
        {
            statusBar.setBoxInfo( "No samples here" );
            return;
        }
        loadSample( files, true );
    }

    void FileBrowser::loadSample( const juce::Array<juce::File>& assetFiles, bool IsMultiple, std::function<void()> onFinished )
    {
        if( !ae.isProjectLoaded() )
        {
            statusBar.setBoxInfo( "Open a project first" );
            return;
        }
        if( sampleAnimation->busy() )
        {
            statusBar.setBoxInfo( "A sample is loading..." );
            return;
        }
        const int pad = IsMultiple ? 0 : fileUtils::targetPad();
        sampleAnimation->startLoading( assetFiles, IsMultiple, pad, opts.copyExternal, [this, onFinished]() {
            sampleAnimation->stopAnimation();
            if( onFinished )
                onFinished();
            statusBar.setBoxInfo( sampleAnimation->message() );
        } );
    }

    void FileBrowser::audition( juce::File f )
    {
        ae.stopPreview();
        if( f.existsAsFile() )
        {
            ae.createPreview( f, settings.preview.gain );
            if( !bottom.playIconButton.getToggleState() )
                bottom.playIconButton.setToggleState( true, juce::dontSendNotification );
        }
    }

    ///////////////////////////////////////
    // select mode, cut / copy / paste / rename / delete
    ///////////////////////////////////////
    void FileBrowser::selectBtnOnClick()
    {
        const bool on = top.selectBtn.getToggleState();
        right.table->enableSelectMode( on );
        right.table->enableSelectCursor( true ); // resets the cursor
        // Leaving or entering Select mode starts from nothing marked: Delete never acts on rows the user cannot see marked.
        right.table->clearSelection();
        selectedFile.clear();
        if( on )
        {
            mt::utils::makeVisible( top.cutBtn, top.copyBtn, top.pasteBtn, top.renameBtn, top.selectAllBtn );
            top.onElementChangedCallback();
        }
        else
        {
            mt::utils::makeHidden( top.cutBtn, top.copyBtn, top.pasteBtn, top.renameBtn, top.selectAllBtn );
            top.onElementChangedCallback();
            if( !entries.empty() )
                right.table->selectRow( 0 );
        }
        right.table->repaint();
    }

    std::vector<std::string> FileBrowser::placeRootsForGuard() const
    {
        std::vector<std::string> places;
        for( const auto& p : drivePlaces )
            places.push_back( p.path.toStdString() );
        for( const auto& p : fixedPlaces )
            places.push_back( p.path.toStdString() );
        places.push_back( getAppDirs( MTProjects ).getFullPathName().toStdString() );
        return places;
    }

    bool FileBrowser::isProtectedPath( const juce::File& f ) const
    {
        return kick_fsguard::isProtected( f.getFullPathName().toStdString(), mt::getSettings().projects.currentProject().toStdString(), placeRootsForGuard() )
               || ( ae.isProjectLoaded() && kick_fsguard::isProtected( f.getFullPathName().toStdString(), ae.getProjectDir().getFullPathName().toStdString(), placeRootsForGuard() ) );
    }

    void FileBrowser::warn( const juce::String& message )
    {
        project_actions::warn( message );
    }

    bool FileBrowser::checkIsCurrProj()
    {
        for( auto sel : selectedFile )
        {
            if( isProtectedPath( sel ) )
            {
                warn( "The open project, a drive or library folder and system folders are not allowed to Cut/Delete/Rename" );
                top.deleteBtn.setToggleState( false, juce::dontSendNotification );
                return true;
            }
        }
        return false;
    }

    // Caches and the list after a change on disk.
    void FileBrowser::afterFileChange( const juce::Array<juce::File>& touched )
    {
        for( const auto& f : touched )
            fileUtils::invalidateFolderSizes( f.getParentDirectory() );
        fileUtils::invalidateFolderSizes( juce::File( pwd ) );
    }

    void FileBrowser::pasteHere()
    {
        if( clip == Clip::None || clipFiles.isEmpty() )
        {
            warn( "Please Select the File or Folder to paste" );
            return;
        }
        if( remoteActive )
            return;
        const bool cutting = clip == Clip::Cut;
        kick_fileops::Job job;
        job.kind    = cutting ? kick_fileops::Kind::Move : kick_fileops::Kind::Copy;
        job.destDir = pwd.toStdString();
        for( const auto& f : clipFiles )
            job.sources.push_back( f.getFullPathName().toStdString() );
        const auto guardPlaces  = placeRootsForGuard();
        const auto currentProj  = mt::getSettings().projects.currentProject().toStdString();
        const auto openProj     = ae.isProjectLoaded() ? ae.getProjectDir().getFullPathName().toStdString() : std::string();
        auto       result       = std::make_shared<kick_fileops::Result>();
        const auto touched      = clipFiles;
        BackgroundTask::run(
            cutting ? "MOVING" : "COPYING", true,
            [job, guardPlaces, currentProj, openProj, result]( const std::atomic<bool>& cancel, const BackgroundTask::Progress& progress ) {
                *result = kick_fileops::run(
                    job, cancel, [&]( const kick_fileops::Progress& p ) { progress( p.percent(), juce::String( p.current ) ); },
                    [&]( const std::string& path ) { return kick_fsguard::isProtected( path, currentProj, guardPlaces ) || ( !openProj.empty() && kick_fsguard::isProtected( path, openProj, guardPlaces ) ); } );
            },
            [this, result, cutting, touched]() {
                afterFileChange( touched );
                if( cutting )
                {
                    clip = Clip::None;
                    clipFiles.clear();
                    // A moved project may have been in the Recent list.
                    for( const auto& f : touched )
                        if( !f.exists() )
                        {
                            auto file = f;
                            settings.projects.removeProject( file );
                        }
                }
                top.cutBtn.setToggleState( false, juce::dontSendNotification );
                top.copyBtn.setToggleState( false, juce::dontSendNotification );
                const juce::String firstName = result->done.empty() ? juce::String() : juce::File( juce::String::fromUTF8( result->done[0].c_str() ) ).getFileName();
                if( !result->ok && !result->cancelled )
                    warn( juce::String( static_cast<int>( result->failed.size() ) ) + " item(s) not " + ( cutting ? "moved" : "copied" ) + ": " + juce::String( result->error ).substring( 0, 90 ) );
                else if( result->cancelled )
                    statusBar.setBoxInfo( "Cancelled: what was finished stays, the unfinished item was removed" );
                else
                    statusBar.setBoxInfo( juce::String( static_cast<int>( result->done.size() ) ) + " item(s) " + ( cutting ? "moved" : "copied" ) );
                if( !remoteActive )
                    navigate( pwd, firstName );
            } );
    }

    void FileBrowser::renamePopup()
    {
        if( selectedFile.isEmpty() )
        {
            warn( "Please Select the File or Folder to Rename" );
            return;
        }
        if( selectedFile.size() > 1 )
        {
            warn( "Rename works on one item: mark one" );
            return;
        }
        if( checkIsCurrProj() )
            return;
        const auto sel = selectedFile.getLast();
        project_actions::askText( "RENAME", sel.getFileNameWithoutExtension(), [this]( juce::String text ) { renameImplementation( text ); } );
    }

    void FileBrowser::renameImplementation( juce::String newFileName )
    {
        if( selectedFile.isEmpty() )
            return;
        juce::File sel = selectedFile.getLast();
        if( isProtectedPath( sel ) )
        {
            warn( "This item is protected and cannot be renamed" );
            return;
        }
        // A project (its folder, or the project file inside it) is renamed as a whole: folder and file together.
        juce::File projectFolder;
        if( sel.isDirectory() && kick_project::isProjectFolder( sel.getFullPathName().toStdString() ) )
            projectFolder = sel;
        else if( sel.hasFileExtension( projectFileList ) && sel.getFileNameWithoutExtension() == sel.getParentDirectory().getFileName() )
            projectFolder = sel.getParentDirectory();

        if( projectFolder != juce::File() )
        {
            if( isProtectedPath( projectFolder ) )
            {
                warn( "The open project cannot be renamed" );
                return;
            }
            std::string newDir;
            const auto  r = kick_project::rename( projectFolder.getFullPathName().toStdString(), newFileName.toStdString(), [this]( const std::string& p ) { return isProtectedPath( juce::File( juce::String::fromUTF8( p.c_str() ) ) ); }, &newDir );
            if( r != kick_project::OpResult::Ok )
            {
                warn( kick_project::describe( r ) );
                return;
            }
            juce::File oldF = projectFolder, newF( juce::String::fromUTF8( newDir.c_str() ) );
            settings.projects.renameProject( oldF, newF );
            if( settings.projects.currentProject() == projectFolder.getFullPathName() )
                settings.projects.currentProject = newF.getFullPathName();
            afterFileChange( { projectFolder } );
            navigate( pwd, newF.getFileName() );
            return;
        }

        // A legal name, and nothing of that name in the folder the item lives in (a move would replace it). The extension of the file
        // stays; one the user typed as well is not doubled.
        auto trimmed = kick_project::trimName( newFileName.toStdString() );
        const auto ext = sel.isDirectory() ? juce::String() : sel.getFileExtension();
        if( ext.isNotEmpty() && juce::String( trimmed ).endsWithIgnoreCase( ext ) )
            trimmed = trimmed.substr( 0, trimmed.size() - static_cast<size_t>( ext.length() ) );
        if( const auto problem = kick_project::checkName( trimmed ); problem != kick_project::NameProblem::Ok )
        {
            warn( kick_project::describe( problem ) );
            return;
        }
        const auto newName = juce::String( trimmed ) + ext;
        juce::File newFile = sel.getParentDirectory().getChildFile( newName );
        if( newFile.exists() && newFile != sel )
        {
            warn( "An item with the same name already exists here" );
            return;
        }
        if( newFile != sel && ( !sel.exists() || !sel.moveFileTo( newFile ) ) )
        {
            warn( "The item could not be renamed" );
            return;
        }
        afterFileChange( { sel } );
        navigate( pwd, newName );
    }

    void FileBrowser::deleteSystem()
    {
        if( selectedFile.isEmpty() )
        {
            warn( "Please Select the File or Folder to delete" );
            return;
        }
        if( checkIsCurrProj() )
            return;
        top.deleteBtn.setToggleState( false, juce::dontSendNotification );

        juce::String data, imageIcon, fileName, timeStamp;
        int          width, height;
        if( selectedFile.getLast().isDirectory() )
        {
            imageIcon = "Vector-1";
            width     = directoryWidth;
            height    = directoryHeight;
        }
        else
        {
            imageIcon = "Icon_DefaultFile1";
            width     = fileWidth;
            height    = fileHeight;
        }
        if( selectedFile.size() == 1 )
        {
            data      = "Are you sure you want to permanently delete this file?";
            fileName  = selectedFile.getLast().getFileNameWithoutExtension();
            timeStamp = selectedFile.getLast().getLastModificationTime().toString( true, false, false, false );
        }
        else
        {
            data      = "Are you sure you want to permanently delete these " + juce::String( selectedFile.size() ) + " files ? ";
            fileName  = "";
            timeStamp = juce::String( selectedFile.size() ) + " items";
        }
        auto popup = new FileLeftEntryPopup( data, imageIcon, width, height, fileName, timeStamp );
        mt::utils::showDialog(
            "DELETE CONFIRMATION", popup, [popup]( int R ) { delete popup; }, ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );

        // The popup and the selection are captured by value: the buttons are touched long after this function has returned.
        const auto toDelete = selectedFile;
        popup->button1.onClick = [popup]() { popup->close(); };
        popup->button2.onClick = [this, popup, toDelete]() {
            popup->close();
            kick_fileops::Job job;
            job.kind = kick_fileops::Kind::Delete;
            for( const auto& f : toDelete )
                job.sources.push_back( f.getFullPathName().toStdString() );
            const auto guardPlaces = placeRootsForGuard();
            const auto currentProj = mt::getSettings().projects.currentProject().toStdString();
            const auto openProj    = ae.isProjectLoaded() ? ae.getProjectDir().getFullPathName().toStdString() : std::string();
            auto       result      = std::make_shared<kick_fileops::Result>();
            BackgroundTask::run(
                "DELETING", true,
                [job, guardPlaces, currentProj, openProj, result]( const std::atomic<bool>& cancel, const BackgroundTask::Progress& progress ) {
                    // Protected items are refused again at the moment of the delete (the selection may have changed under the dialog).
                    *result = kick_fileops::run(
                        job, cancel, [&]( const kick_fileops::Progress& p ) { progress( p.percent(), juce::String( p.current ) ); },
                        [&]( const std::string& path ) { return kick_fsguard::isProtected( path, currentProj, guardPlaces ) || ( !openProj.empty() && kick_fsguard::isProtected( path, openProj, guardPlaces ) ); } );
                },
                [this, result, toDelete]() {
                    for( const auto& f : toDelete )
                        if( !f.exists() )
                        {
                            auto file = f;
                            settings.projects.removeProject( file ); // a deleted project leaves the Recent list
                            if( settings.projects.currentProject() == f.getFullPathName() )
                                settings.projects.currentProject = juce::String();
                        }
                    afterFileChange( toDelete );
                    if( result->cancelled )
                        statusBar.setBoxInfo( "Cancelled" );
                    else if( !result->ok )
                        warn( juce::String( static_cast<int>( result->failed.size() ) ) + " item(s) were not deleted: " + juce::String( result->error ).substring( 0, 90 ) );
                    else
                        statusBar.setBoxInfo( "Deleted" );
                    if( !remoteActive )
                        navigate( pwd );
                } );
        };
    }

    ///////////////////////////////////////
    // favourites: a folder path per slot (not a list row); Shift+tap assigns, a long press clears
    ///////////////////////////////////////
    void FileBrowser::favouriteButtonImplementation()
    {
        for( auto& button : grpNum )
        {
            header.addButton( button.get(), numButtonWidth );
            juce::String buttonString = settings.browser.getPathAssignedToButton( button->getName() );
            button->setToggleState( buttonString.isNotEmpty(), juce::dontSendNotification );

            button->onClick = [this, b = button.get()]() {
                if( !settings.browser.isValid() )
                    return;
                const auto stored = settings.browser.getPathAssignedToButton( b->getName() );
                const bool shift  = juce::ModifierKeys::currentModifiers.isShiftDown() || contracts::DclContractsHelper::isShiftPressed();
                const bool longPress = favDownBtn == b && favDownAt > 0 && ( juce::Time::currentTimeMillis() - favDownAt ) > 700;
                favDownAt = 0;
                if( longPress && stored.isNotEmpty() )
                {
                    settings.browser.setPathToButton( b->getName(), "" );
                    b->setToggleState( false, juce::dontSendNotification );
                    statusBar.setBoxInfo( "Favourite cleared" );
                    return;
                }
                if( stored.isEmpty() || shift )
                    onFirstClick( b );
                else
                    onSecondClick( b );
            };
        }

        onFirstClick = [this]( ui::KickImageButton* button ) {
            if( remoteActive )
            {
                button->setToggleState( !settings.browser.getPathAssignedToButton( button->getName() ).isEmpty(), juce::dontSendNotification );
                statusBar.setBoxInfo( "A favourite holds a folder of this device" );
                return;
            }
            // This folder is already on another slot: that button blinks instead.
            for( size_t j = 0; j < grpNum.size(); ++j )
            {
                const juce::String propertyName = "button" + juce::String( static_cast<int>( j ) + 1 );
                if( settings.browser.getPathAssignedToButton( propertyName ) == pwd && propertyName != button->getName() )
                {
                    button->setToggleState( !settings.browser.getPathAssignedToButton( button->getName() ).isEmpty(), juce::dontSendNotification );
                    buttonId = static_cast<int>( grpNum.size() - j - 1 );
                    timerInt = 1;
                    stopTimer();
                    startTimer( 100 ); //to blink the button
                    return;
                }
            }
            settings.browser.setPathToButton( button->getName(), pwd );
            button->setToggleState( true, juce::dontSendNotification );
            statusBar.setBoxInfo( "Favourite set: " + formatDisplayPath( pwd ) );
        };

        onSecondClick = [this]( ui::KickImageButton* button ) {
            recallFavourite( settings.browser.getPathAssignedToButton( button->getName() ) );
            button->setToggleState( true, juce::dontSendNotification );
        };

        onDoubleClick = []( ui::KickImageButton* ) {}; // clearing is a long press now (a double tap cleared by accident)
    }

    void FileBrowser::showFolder( const juce::String& path )
    {
        recallFavourite( path );
    }

    juce::StringArray FileBrowser::getShownNames() const
    {
        juce::StringArray names;
        for( const auto& e : entries )
            names.add( juce::String::fromUTF8( e.name.c_str() ) );
        return names;
    }

    // The slot holds a path: the place that contains it is selected (whatever row it has now: an SD card may have come or gone).
    void FileBrowser::recallFavourite( const juce::String& path )
    {
        if( path.isEmpty() )
            return;
        if( !juce::File( path ).isDirectory() )
        {
            statusBar.setBoxInfo( "Folder not found (is the drive in?): " + formatDisplayPath( path ) );
            return;
        }
        if( leftTab == LeftTab::Content && rowOfPlaceContaining( path ) < 0 )
            setLeftTab( LeftTab::Places );
        int row = rowOfPlaceContaining( path );
        if( row < 0 && leftTab != LeftTab::Places )
        {
            setLeftTab( LeftTab::Places );
            row = rowOfPlaceContaining( path );
        }
        if( row >= 0 )
            showPlace( row, false );
        leaveSource();
        navigate( path );
    }

    // Jumps to the next favourite slot that has a folder assigned (same as clicking its button).
    void FileBrowser::showNextFavourite()
    {
        if( !settings.browser.isValid() )
            return;

        const int n = (int)grpNum.size();
        for( int i = 1; i <= n; ++i )
        {
            const int k = ( favouriteCursor + i + n ) % n;
            if( settings.browser.getPathAssignedToButton( grpNum[static_cast<size_t>( k )]->getName() ).isNotEmpty() )
            {
                favouriteCursor = k;
                onSecondClick( grpNum[static_cast<size_t>( k )].get() );
                return;
            }
        }
        statusBar.setBoxInfo( "No favourite folder is assigned" );
    }

    ///////////////////////////////////////
    // folder size jobs (worker, newest navigation wins)
    ///////////////////////////////////////
    namespace
    {
        class FolderSizeJob : public juce::ThreadPoolJob
        {
        public:
            FolderSizeJob( juce::File folder,
                           int64_t mtimeMs,
                           uint64_t gen,
                           std::function<bool( uint64_t )> isCancelledFn,
                           std::function<void( const juce::File&, int64_t, int64_t, uint64_t )> onCompleteFn )
                : juce::ThreadPoolJob( "FolderSizeJob" )
                , folderToCalculate( std::move( folder ) )
                , mtime( mtimeMs )
                , generation( gen )
                , isCancelled( std::move( isCancelledFn ) )
                , onComplete( std::move( onCompleteFn ) )
            {
            }

            JobStatus runJob() override
            {
                if( shouldExit() || ( isCancelled && isCancelled( generation ) ) )
                    return jobHasFinished;

                auto totalSize = fileUtils::calculateFolderSize( folderToCalculate, [this]() {
                    return shouldExit() || ( isCancelled && isCancelled( generation ) );
                } );

                if( shouldExit() || ( isCancelled && isCancelled( generation ) ) || totalSize < 0 )
                    return jobHasFinished;

                if( onComplete )
                    onComplete( folderToCalculate, totalSize, mtime, generation );

                return jobHasFinished;
            }

        private:
            juce::File                                                            folderToCalculate;
            int64_t                                                               mtime{ 0 };
            uint64_t                                                              generation{ 0 };
            std::function<bool( uint64_t )>                                       isCancelled;
            std::function<void( const juce::File&, int64_t, int64_t, uint64_t )> onComplete;
        };
    }

    void FileBrowser::cancelFolderSizeCalculations()
    {
        generationTracker.nextGeneration();
        pendingPaths.clear();
        folderSizeThreadPool.removeAllJobs( false, 0 );
    }

    bool FileBrowser::isGenerationCancelled( uint64_t gen ) const noexcept
    {
        return generationTracker.isCancelled( gen );
    }

    void FileBrowser::queueFolderSizeCalculation( const juce::File& folder, int64_t mtimeMs, FileRightEntry* comp )
    {
        const juce::String pathStr = folder.getFullPathName();
        const std::string  pathStd = pathStr.toStdString();

        if( pendingPaths.count( pathStd ) > 0 )
            return;

        pendingPaths.insert( pathStd );
        const uint64_t gen = generationTracker.getGeneration();

        juce::Component::SafePointer<FileBrowser>    safeThis( this );
        juce::Component::SafePointer<FileRightEntry> safeComp( comp );

        folderSizeThreadPool.addJob(
            new FolderSizeJob(
                folder,
                mtimeMs,
                gen,
                [safeThis]( uint64_t g ) -> bool {
                    if( safeThis == nullptr )
                        return true;
                    return safeThis->isGenerationCancelled( g );
                },
                [safeThis, safeComp, pathStr]( const juce::File& f, int64_t totalSize, int64_t mtime, uint64_t g ) {
                    const std::string pStd = f.getFullPathName().toStdString();
                    fileUtils::getFolderSizeCache().put( pStd, totalSize, mtime );

                    const juce::String sizeString = juce::File::descriptionOfSizeInBytes( totalSize );

                    juce::MessageManager::callAsync( [safeThis, safeComp, pathStr, sizeString, g, pStd]() {
                        if( safeThis == nullptr )
                            return;

                        safeThis->pendingPaths.erase( pStd );

                        if( safeThis->isGenerationCancelled( g ) )
                            return;

                        if( safeComp != nullptr && safeComp->getCurrentPath() == pathStr )
                            safeComp->setSizeText( sizeString );
                    } );
                } ),
            true );
    }

    FileBrowser::~FileBrowser()
    {
        if( searchToken )
            *searchToken = true;
        if( hub )
        {
            hub->cancelListing();
            hub->getQueue().cancelAll();
        }
        cancelFolderSizeCalculations();
        folderSizeThreadPool.removeAllJobs( true, 2000 );
        listPool.removeAllJobs( true, 2000 );
    }

    ///////////////////////////////////////
    // hardware, visibility, mouse
    ///////////////////////////////////////
    void FileBrowser::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        using H  = contracts::DclContractsHelper;
        using dc = contracts::DeviceControlContracts;

        if( H::isKnobMessage( f_HwMsg ) && H::getKnobCode( f_HwMsg ) == dc::KnobSequence::EN_KNOB_CENTER )
            knobRotated( H::getKnobValue( f_HwMsg ) );

        if( H::isPadButtonMessage( f_HwMsg ) )
        {
            // Press and release both arrive here (the release used to be filtered out: a sustained / looped sound never ended).
            const auto padNo   = H::getPadNumber( f_HwMsg );
            const bool pressed = H::getPadState( f_HwMsg ) == dc::BTN_PRESSED;
            switch( kick_fblogic::padAction( true, pressed ) )
            {
                case kick_fblogic::PadAction::NoteOn:
                {
                    ui::fileUtils::setHighlightLED( padNo );
                    const int note = heldPads.press( padNo, padNo + ae.getBankOffset() ); // the note that starts is the note that ends
                    ae.addLiveMidiMsg( ae.getSelected(), juce::MidiMessage::noteOn( 1, note, (juce::uint8) H::getPadVelocity( f_HwMsg ) ) );
                    updateLoadTargetStatus();
                    break;
                }
                case kick_fblogic::PadAction::NoteOff:
                {
                    const int note = heldPads.release( padNo );
                    if( note >= 0 )
                        ae.addLiveMidiMsg( ae.getSelected(), juce::MidiMessage::noteOff( 1, note ) );
                    break;
                }
                case kick_fblogic::PadAction::None: break;
            }
            return;
        }
        if( H::isButtonMessage( f_HwMsg ) && ( H::getButtonState( f_HwMsg ) == dc::BTN_PRESSED ) )
        {
            auto  buttonCode = H::getButtonCode( f_HwMsg );
            auto& aeSel      = ae.getSelected();
            switch( buttonCode )
            {
                case dc::BtnLedSequence::EN_KNOB_ENTER:
                    if( H::isShiftPressed() )
                    {
                        if( remoteActive ) remoteBack();
                        else exitDirectory(); // Shift + Data Dial press = one folder up
                    }
                    else
                        knobClicked();
                    break;
                case dc::BtnLedSequence::EN_BACK:
                    if( remoteActive ) remoteBack();
                    else if( searchMode ) navigate( pwd );
                    else exitDirectory();
                    break;
                case dc::BtnLedSequence::EN_PLUS: knobRotated( 8 ); break;  // a page down
                case dc::BtnLedSequence::EN_MINUS: knobRotated( -8 ); break; // a page up
                case dc::EN_PLAY: ae.setSequencePlay( aeSel.sequence, false, true ); break;
                case dc::EN_PLAYSTART: ae.setSequencePlay( aeSel.sequence, true, true ); break;
                case dc::EN_STOP: ae.setSequencePause(); break;
                default: break;
            }
        }
    }

    void FileBrowser::visibilityChanged()
    {
        cancelFolderSizeCalculations();

        if( isShowing() || isVisible() )
        {
            pollDrives();
            rebuildShortcuts();
            refreshSourceRows();
            rebuildShown();
            left.table->refresh();
            right.table->setAutoScrollToSelection( false );
            applyColumnOptions();
            const auto keep = pendingImportSelectName.isNotEmpty()
                                  ? pendingImportSelectName
                                  : ( selectedFile.isEmpty() ? juce::String() : selectedFile.getLast().getFileName() );
            pendingImportSelectName.clear();
            const int  row  = rowOfPlaceContaining( pwd );
            if( !remoteActive )
            {
                showPlace( row >= 0 ? row : 0, false );
                navigate( row >= 0 ? pwd : placeRoot, keep ); // listed again: what changed on disk meanwhile shows
            }
            right.table->setAutoScrollToSelection( true );

            if( !ae.isProjectLoaded() )
            {
                bottom.loadAllButton.setEnabled( false );
                bottom.sampleAssignButton.setEnabled( false );
            }
            else
            {
                bottom.loadAllButton.setEnabled( true );
                hideAuditionComponent();
                bottom.sampleAssignButton.setEnabled( ae.getNumPrograms() != 0 );
            }
            bottom.loadButton.setEnabled( true ); // opens projects, enters folders, loads samples when a project is open
        }
        else
        {
            if( hub )
            {
                hub->cancelListing();
                hub->getQueue().cancelAll();
            }
            if( searchToken )
                *searchToken = true;
            for( const int note : heldPads.releaseAll() )
                ae.addLiveMidiMsg( ae.getSelected(), juce::MidiMessage::noteOff( 1, note ) ); // a pad still down when the screen leaves
            ae.stopPreview();
            bottom.playIconButton.setToggleState( false, juce::dontSendNotification );
            // The audition mode is reset consistently: the button text, its internal step and the setting.
            bottom.auditionButton.clearSelection();
            bottom.auditionButton.incrementSelection(); // step 0 = OFF
            bottom.auditionButton.setButtonText( "OFF" );
            bottom.playIconButton.setEnabled( false );
            settings.preview.isAuto = false;
            settings.preview.isOn   = false;
            top.selectBtn.setEnabled( true );
            top.selectBtn.setToggleState( false, juce::dontSendNotification );
            selectBtnOnClick();
            bottom.allButton.setToggleState( true, juce::dontSendNotification );
            filterNum = All;
            statusBar.setBoxInfo( "Box Info" );
        }
    }

    void FileBrowser::knobRotated( double value )
    {
        if( remoteActive )
        {
            right.table->knobRotated( value );
            return;
        }
        right.table->knobRotated( value );
        if( !top.selectBtn.getToggleState() )
            return;
        // Select mode: the Data Dial only moves the cursor.
    }

    void FileBrowser::knobClicked()
    {
        if( remoteActive )
        {
            const int r = right.table->getSelectedRowOrNone();
            if( r >= 0 )
                remoteActivate( r );
            return;
        }
        if( top.selectBtn.getToggleState() )
        {
            // Select mode: the Data Dial press marks / unmarks the row under the cursor.
            const int c = right.table->getCursorRow() >= 0 ? right.table->getCursorRow() : right.table->getSelectedRowOrNone();
            if( c >= 0 )
            {
                right.table->toggleRow( c );
                syncSelection();
            }
            return;
        }
        const int r = right.table->getSelectedRowOrNone();
        if( r >= 0 )
            activateRow( r );
    }

    void FileBrowser::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 5 ) );
    }

    void FileBrowser::resized()
    {
        auto b = getLocalBounds();

        auto headerBounds = b.removeFromTop( ui::layout::header.getHeight() );
        header.setBounds( headerBounds );
        top.setBoundsRelative( 0.0f, 0.1558f, 1.0f, 0.083f );
        right.setBoundsRelative( 0.354f, 0.254f, 0.6379f, 0.6429f );
        left.setBoundsRelative( 0.0097f, 0.254f, 0.3346f, 0.6429f );
        bottom.setBoundsRelative( 0.0f, 0.915f, 1.0f, 0.0668f );
        auditionComp.setBoundsRelative( .8894f, .43f, .1027f, .4582f );
    }

    void FileBrowser::hideAuditionComponent()
    {
        bottom.volumeBtn.setToggleState( false, juce::dontSendNotification );
        mt::utils::makeHidden( auditionComp );
    }

    void FileBrowser::showAuditionComponent()
    {
        auditionComp.setVolume( settings.preview.gain );
        mt::utils::makeVisible( auditionComp );
    }

    void FileBrowser::mouseDown( const juce::MouseEvent& e )
    {
        // Favourite slot buttons: when the finger went down (a long press clears the slot, see the button's onClick).
        favDownAt  = juce::Time::currentTimeMillis();
        favDownBtn = nullptr;
        for( auto& b : grpNum )
            if( e.originalComponent == b.get() || b->isParentOf( e.originalComponent ) )
                favDownBtn = b.get();

        // MAN audition: the sound plays while the Play button is held.
        if( settings.preview.isOn && !settings.preview.isAuto && ( e.originalComponent == &bottom.playIconButton || bottom.playIconButton.isParentOf( e.originalComponent ) ) )
        {
            if( !selectedFile.isEmpty() && selectedFile.getLast().hasFileExtension( fileUtils::sampleExtensions() ) )
            {
                held = true;
                audition( selectedFile.getLast() );
            }
        }

        // The volume popup closes on a tap outside it (all in this component's own coordinates).
        const auto p = e.getEventRelativeTo( this ).getPosition();
        if( auditionComp.isVisible() && !auditionComp.getBounds().contains( p ) && !bottom.volumeBtn.getBounds().translated( bottom.getX(), bottom.getY() ).contains( p ) )
            hideAuditionComponent();
    }

    void FileBrowser::mouseUp( const juce::MouseEvent& )
    {
        if( held )
        {
            held = false;
            ae.stopPreview();
            bottom.playIconButton.setToggleState( false, juce::dontSendNotification );
        }
    }

    // 1 Hz: the drives; 100 ms: the blink of a favourite button.
    void FileBrowser::timerCallback()
    {
        if( getTimerInterval() == 100 )
        {
            if( timerInt & 1 )
                grpNum[static_cast<size_t>( buttonId )]->setToggleState( false, juce::dontSendNotification );
            else
                grpNum[static_cast<size_t>( buttonId )]->setToggleState( true, juce::dontSendNotification );
            if( timerInt++ == 6 )
            {
                stopTimer();
                startTimer( 1000 );
            }
            return;
        }
        if( isShowing() ) // hidden screen: no drive work, no list changes behind the user's back
            pollDrives();
    }

    ///////////////////////////////////////
    // Extra locations: WebDAV / local-folder sources (FileBrowserSource). All network work runs on the hub's worker threads;
    // the callbacks below only marshal back to the message thread.
    ///////////////////////////////////////
    juce::File FileBrowser::findBestSpliceDirectory() const
    {
        auto&      cs   = mt::getSettings().cloudSources;
        const auto docs = juce::File::getSpecialLocation( juce::File::userDocumentsDirectory );
        const auto home = juce::File::getSpecialLocation( juce::File::userHomeDirectory );
        const auto mus  = juce::File::getSpecialLocation( juce::File::userMusicDirectory );

        std::vector<std::string> candidateRoots = {
            docs.getFullPathName().toStdString(),
            docs.getChildFile( "Kick" ).getFullPathName().toStdString(),
            ae.getSoundsDir().getFullPathName().toStdString(),
            docs.getChildFile( "samples" ).getFullPathName().toStdString(),
            docs.getChildFile( "Kick/samples" ).getFullPathName().toStdString(),
            ae.getInternalDir().getFullPathName().toStdString(),
            home.getFullPathName().toStdString(),
            mus.getFullPathName().toStdString(),
#if JUCE_LINUX || __linux__
            "/data",
#endif
        };

#if JUCE_IOS
        const std::string fallback = docs.getChildFile( "Splice" ).getFullPathName().toStdString();
#else
        const std::string fallback = home.getChildFile( "Splice" ).getFullPathName().toStdString();
#endif
        const std::string resolved = kick_incoming::resolveSpliceDir( cs.splicePath.get().toStdString(), candidateRoots, fallback );
        return juce::File( juce::String::fromUTF8( resolved.c_str() ) );
    }

    void FileBrowser::refreshSourceRows()
    {
        sourcePlaces.clear();
        auto& cs = mt::getSettings().cloudSources;
        const bool spliceEnabled = cs.enableSplice.get();
#if JUCE_IOS || JUCE_MAC
        const bool gdriveEnabled = cs.enableGoogleDrive.get();
#else
        const bool gdriveEnabled = false;
#endif
        const bool webdavEnabled = cs.enableWebDav.get() || ( hub != nullptr && !hub->allConfigs().empty() );

        // 1. Splice Place (auto-discovers Splice folder under Documents / Samples / Home / configured path)
        if( spliceEnabled )
        {
            const juce::File spliceDir = findBestSpliceDirectory();
            if( spliceDir.isDirectory() )
            {
                Place p;
                p.kind       = Place::Kind::Shortcut;
                p.label      = "Splice";
                p.icon       = "Icon_Library";
                p.iconActive = "Icon_LibraryActive";
                p.width      = 18.0f;
                p.height     = 16.0f;
                p.path       = spliceDir.getFullPathName();
                sourcePlaces.push_back( p );
            }
        }

        // 2. Local Google Drive Place (macOS/Desktop Drive for Desktop synced folder when present)
        if( gdriveEnabled )
        {
            juce::File gdriveDir;
            if( cs.googleDrivePath.get().isNotEmpty() )
                gdriveDir = juce::File( cs.googleDrivePath.get() );
            else
            {
                juce::File home = juce::File::getSpecialLocation( juce::File::userHomeDirectory );
                juce::File cloudStorage = home.getChildFile( "Library/CloudStorage" );
                if( cloudStorage.isDirectory() )
                {
                    for( auto iter : juce::RangedDirectoryIterator( cloudStorage, false, "*GoogleDrive*" ) )
                    {
                        if( iter.getFile().isDirectory() )
                        {
                            auto myDrive = iter.getFile().getChildFile( "My Drive" );
                            gdriveDir = myDrive.isDirectory() ? myDrive : iter.getFile();
                            break;
                        }
                    }
                }
                if( !gdriveDir.isDirectory() && home.getChildFile( "Google Drive" ).isDirectory() )
                    gdriveDir = home.getChildFile( "Google Drive" );
                else if( !gdriveDir.isDirectory() )
                {
                    auto docsDrive = juce::File::getSpecialLocation( juce::File::userDocumentsDirectory ).getChildFile( "Google Drive" );
                    if( docsDrive.isDirectory() )
                        gdriveDir = docsDrive;
                }
            }
            if( gdriveDir.isDirectory() )
            {
                Place p;
                p.kind       = Place::Kind::Shortcut;
                p.label      = "Google Drive (Local)";
                p.icon       = "Icon_Internal";
                p.iconActive = "Icon_InternalActive";
                p.width      = 18.0f;
                p.height     = 16.0f;
                p.path       = gdriveDir.getFullPathName();
                sourcePlaces.push_back( p );
            }
        }

        // 3. Configured Cloud & Folder hub sources (Google Drive OAuth/API, WebDAV, custom local folders)
        if( hub )
        {
            for( auto& cfg : hub->allConfigs() )
            {
                if( cfg.type == "webdav" && !webdavEnabled ) continue;
                if( cfg.type == "splice" && !spliceEnabled ) continue;
                if( cfg.type == "gdrive" && !gdriveEnabled ) continue;

                Place p;
                p.kind       = Place::Kind::Source;
                p.label      = juce::String::fromUTF8( cfg.name.c_str() );
                p.icon       = cfg.type == "gdrive" ? "Icon_Internal" : "Icon_Library";
                p.iconActive = cfg.type == "gdrive" ? "Icon_InternalActive" : "Icon_LibraryActive";
                p.width      = 18.0f;
                p.height     = 16.0f;
                p.path       = juce::String::fromUTF8( cfg.id.c_str() ); // the source id
                sourcePlaces.push_back( p );
            }
        }
        rebuildShown();
        if( left.table )
            left.table->refresh();
    }

    void FileBrowser::leaveSource()
    {
        if( !remoteActive )
            return;
        remoteActive = false;
        remoteSearch = {};
        remoteEntries.clear();
        remoteShown.clear();
        activeSource.reset();
        if( hub )
        {
            hub->cancelListing();
            hub->getQueue().cancelAll();
        }
    }

    void FileBrowser::openSource( const Place& place )
    {
        if( !hub )
            return;
        const auto id  = place.path.toStdString();
        auto       src = hub->find( id );
        if( !src )
            return;
        cancelFolderSizeCalculations();
        ae.stopPreview();
        leaveSource();
        activeSource = src;
        remoteActive = true;
        remotePath   = "/";
        entries.clear();
        selectedFile.clear();
        right.table->clearSelection();
        right.backButton.setEnabled( false );
        listRemote( "/" );
    }

    void FileBrowser::listRemote( const std::string& path )
    {
        if( !hub || !activeSource )
            return;
        remoteSearch = {};
        statusBar.setBoxInfo( "Loading..." );
        juce::Component::SafePointer<FileBrowser> safeThis( this );
        remoteReq = hub->listAsync( activeSource, path, [safeThis, path]( uint64_t gen, const fileSource::ListResult& lr ) {
            juce::MessageManager::callAsync( [safeThis, path, gen, lr]() {
                if( safeThis != nullptr )
                    safeThis->onRemoteListed( gen, path, lr );
            } );
        } );
    }

    void FileBrowser::onRemoteListed( uint64_t gen, const std::string& path, const fileSource::ListResult& lr )
    {
        if( !remoteActive || gen != remoteReq )
            return;
        if( !lr.result.ok() )
        {
            juce::String msg;
            const bool   isGdrive = activeSource && dynamic_cast<fileSource::GoogleDriveSource*>( activeSource.get() ) != nullptr;
            switch( lr.result.status )
            {
                case fileSource::Status::AuthFailed:
                    msg = isGdrive ? juce::String( "Google Drive sign-in needed - tap + Cloud to connect" )
                                   : juce::String( "Login failed: check user/password (+ WebDAV)" );
                    break;
                case fileSource::Status::NetworkError: msg = isGdrive ? "Cannot reach Google Drive" : "Cannot reach the server"; break;
                case fileSource::Status::Unsupported: msg = "https is not available here: use an http:// address"; break;
                case fileSource::Status::NotFound: msg = "Folder not found"; break;
                case fileSource::Status::Cancelled: return;
                default: msg = "Server error"; break;
            }
            statusBar.setBoxInfo( msg );
            kick_log( "FILESRC", "list failed: status=%d http=%d", static_cast<int>( lr.result.status ), lr.result.httpStatus );
            if( remoteEntries.empty() )
            {
                remoteShown.clear();
                right.table->refresh();
            }
            return;
        }
        remoteEntries = lr.entries;
        remotePath    = path;
        applyRemoteFilter();
        top.searchLabel.setText( juce::String( activeSource->displayName() ) + ":" + juce::String::fromUTF8( remotePath.c_str() ), juce::dontSendNotification );
        right.backButton.setEnabled( remotePath != "/" );
        right.table->refresh();
        right.table->clearSelection();
        statusBar.setBoxInfo( "Box Info" );
        if( !remoteShown.empty() )
            right.table->selectRow( 0 );
    }

    void FileBrowser::applyRemoteFilter()
    {
        remoteShown.clear();
        for( auto& e : remoteEntries )
        {
            const juce::File asFile( juce::String::fromUTF8( e.name.c_str() ) );
            if( !e.isDir )
            {
                if( filterNum == Samples && !asFile.hasFileExtension( fileUtils::sampleExtensions() ) )
                    continue;
                if( filterNum == Projects && !asFile.hasFileExtension( projectFileList ) )
                    continue;
                if( filterNum == Midi && !asFile.hasFileExtension( ".mid;.midi" ) )
                    continue;
            }
            if( remoteSearch.isNotEmpty() && !juce::String::fromUTF8( e.name.c_str() ).toLowerCase().contains( remoteSearch ) )
                continue;
            remoteShown.push_back( e );
        }
    }

    juce::Component* FileBrowser::remoteCell( int rowIndex, juce::Component* existing )
    {
        auto comp = static_cast<FileRightEntry*>( existing );
        if( comp == nullptr )
        {
            comp = new FileRightEntry();
            comp->setInterceptsMouseClicks( false, true );
        }
        if( !juce::isPositiveAndBelow( rowIndex, static_cast<int>( remoteShown.size() ) ) )
            return comp;
        const auto&        e    = remoteShown[static_cast<size_t>( rowIndex )];
        const auto         name = juce::String::fromUTF8( e.name.c_str() );
        const juce::File   asFile( name );
        juce::String       icon = "Icon_DefaultFile";
        bool               folderFlag = true;
        float              w = 22.0f, h = 16.0f;
        if( e.isDir )
            icon = "Vector";
        else if( asFile.hasFileExtension( projectFileList ) )
            icon = "Icon_ProjectFile", folderFlag = false, w = 14.0f;
        else if( asFile.hasFileExtension( fileUtils::sampleExtensions() ) )
            icon = "Icon_SampleFile", folderFlag = false, w = 14.0f;
        const juce::Time   t( e.mtimeMs );
        const juce::String dateStr = e.mtimeMs > 0 ? t.toString( true, false, false, false ) : juce::String();
        const juce::String timeStr = e.mtimeMs > 0 ? t.toString( false, true, false, true ) : juce::String();
        const juce::String sizeStr = e.isDir || e.size < 0 ? juce::String() : juce::File::descriptionOfSizeInBytes( e.size );
        comp->setCurrentPath( juce::String::fromUTF8( e.path.c_str() ) );
        comp->setProperties( icon, folderFlag, w, h, name, dateStr, timeStr, sizeStr );
        return comp;
    }

    void FileBrowser::remoteActivate( int rowIndex )
    {
        if( !juce::isPositiveAndBelow( rowIndex, static_cast<int>( remoteShown.size() ) ) )
            return;
        const auto e = remoteShown[static_cast<size_t>( rowIndex )];
        if( e.isDir )
            listRemote( e.path );
        else if( juce::File( juce::String::fromUTF8( e.name.c_str() ) ).hasFileExtension( fileUtils::sampleExtensions() ) )
            remoteFetchAndLoad( e );
        else
            statusBar.setBoxInfo( "Only samples can be loaded from a remote location" );
    }

    void FileBrowser::remoteBack()
    {
        if( remoteSearch.isNotEmpty() )
        {
            remoteSearch = {};
            applyRemoteFilter();
            top.searchLabel.setText( juce::String( activeSource ? activeSource->displayName() : std::string() ) + ":" + juce::String::fromUTF8( remotePath.c_str() ), juce::dontSendNotification );
            right.table->refresh();
            right.backButton.setEnabled( remotePath != "/" );
            return;
        }
        if( remotePath != "/" )
            listRemote( fileSource::webdav::parentPath( remotePath ) );
    }

    // Download (low-priority thread, resumable) into the cache; the sample is loaded only from the completely downloaded file.
    void FileBrowser::remoteFetchAndLoad( const fileSource::Entry& e )
    {
        if( !hub || !activeSource )
            return;
        hub->getQueue().cancelAll();
        lastDownloadPercent = -1;
        const auto  name    = juce::String::fromUTF8( e.name.c_str() );
        statusBar.setBoxInfo( "Downloading " + name );
        juce::Component::SafePointer<FileBrowser> safeThis( this );
        auto*                                    hubPtr = hub.get();
        const auto                               key    = fileSource::SourceCache::makeKey( activeSource->id(), e.path );

        fileSource::DownloadJob job;
        job.source = activeSource;
        job.remote = e;
        job.onProgress = [safeThis, name]( uint64_t, int64_t done, int64_t total ) {
            if( total <= 0 )
                return;
            const int pct = static_cast<int>( done * 100 / total );
            juce::MessageManager::callAsync( [safeThis, name, pct]() {
                if( safeThis == nullptr || pct == safeThis->lastDownloadPercent )
                    return;
                safeThis->lastDownloadPercent = pct;
                safeThis->statusBar.setBoxInfo( "Downloading " + name + " " + juce::String( pct ) + "%" );
            } );
        };
        job.onDone = [safeThis, name, hubPtr, key]( uint64_t, const fileSource::Result& res, const std::string& file ) {
            const auto status = res.status;
            const auto msg    = juce::String( res.message );
            if( res.ok() )
                hubPtr->getCache().pin( key ); // keep it from being evicted while the sample loads
            juce::MessageManager::callAsync( [safeThis, name, status, msg, file, hubPtr, key]() {
                if( safeThis == nullptr )
                    return;
                if( status != fileSource::Status::Ok )
                {
                    safeThis->statusBar.setBoxInfo( status == fileSource::Status::Cancelled ? juce::String( "Download cancelled" ) : "Download failed: " + name );
                    return;
                }
                juce::Array<juce::File> files;
                files.add( juce::File( juce::String::fromUTF8( file.c_str() ) ) );
                safeThis->loadSample( files, false, [hubPtr, key, safeThis]() {
                    if( safeThis != nullptr )
                        hubPtr->getCache().unpin( key );
                } );
            } );
        };
        remoteDownloadId = hub->getQueue().enqueue( std::move( job ) );
    }

    void FileBrowser::addOrEditCloudSource()
    {
        const auto* selPlace = currentPlaceRow();
        if( selPlace != nullptr )
        {
            if( selPlace->label == "Google Drive" || selPlace->label == "Google Drive (Local)" ) { editGoogleDriveSource(); return; }
            if( selPlace->kind == Place::Kind::Source && hub )
            {
                for( auto& c : hub->allConfigs() )
                {
                    if( c.id == selPlace->path.toStdString() )
                    {
                        if( c.type == "webdav" ) { editWebDavSource(); return; }
                        if( c.type == "splice" ) { editSpliceSource(); return; }
                        if( c.type == "gdrive" ) { editGoogleDriveSource(); return; }
                    }
                }
            }
        }

        juce::Component::SafePointer<FileBrowser> safeThis( this );
#if JUCE_IOS
        const juce::Rectangle<int> bounds( 0, 0, 500, 300 );
#else
        const juce::Rectangle<int> bounds( 0, 0, 500, 250 );
#endif
        mt::utils::showDialogOwned<CloudSourcePopup>(
            "CLOUD SOURCE",
            std::make_unique<CloudSourcePopup>(),
            [safeThis]( int R, CloudSourcePopup& p ) {
                if( R != UI::APPLY || safeThis == nullptr )
                    return;
                const auto chosen = p.selected;
                juce::MessageManager::callAsync( [safeThis, chosen]() {
                    if( safeThis == nullptr )
                        return;
                    switch( chosen )
                    {
                        case CloudSourcePopup::Choice::ScanNetwork:
                            safeThis->scanNetworkSources();
                            break;
                        case CloudSourcePopup::Choice::GoogleDrive:
#if JUCE_IOS || JUCE_MAC
                            safeThis->editGoogleDriveSource();
#endif
                            break;
                        case CloudSourcePopup::Choice::FilesImport:
                            safeThis->statusBar.setBoxInfo( "Choose audio files from Files / Google Drive..." );
                            kick_apple_cloud::pickFilesFromIosDocumentProvider( [safeThis]( juce::StringArray paths, juce::String err ) {
                                if( safeThis == nullptr )
                                    return;
                                if( !paths.isEmpty() )
                                    safeThis->importIncomingFiles( paths, true );
                                else if( err.isNotEmpty() )
                                    safeThis->statusBar.setBoxInfo( err );
                            } );
                            break;
                        case CloudSourcePopup::Choice::Splice:
                            safeThis->editSpliceSource();
                            break;
                        case CloudSourcePopup::Choice::WebDav:
                            safeThis->editWebDavSource();
                            break;
                        case CloudSourcePopup::Choice::None:
                            break;
                    }
                } );
            },
            bounds, false );
    }

    void FileBrowser::editSpliceSource()
    {
        juce::Component::SafePointer<FileBrowser> safeThis( this );
        juce::String currentPath = mt::getSettings().cloudSources.splicePath.get();
        if( currentPath.isEmpty() )
            currentPath = findBestSpliceDirectory().getFullPathName();

        project_actions::askText( "SPLICE FOLDER PATH (EMPTY = AUTO)", currentPath, [safeThis]( juce::String path ) {
            if( safeThis == nullptr ) return;
            path = path.trim();
            mt::getSettings().cloudSources.splicePath = path;
            mt::getSettings().cloudSources.enableSplice = true;
            if( path.isNotEmpty() )
                juce::File( path ).createDirectory();
            safeThis->refreshPlacesAndListing();
            safeThis->statusBar.setBoxInfo( "Splice source updated" );
        } );
    }

    void FileBrowser::finishGoogleDriveSourceSetup( const fileSource::SourceConfig& cfg, const std::string& secretText, const juce::String& statusMsg )
    {
        if( !hub || !hub->upsertGoogleDrive( cfg, secretText ) )
        {
            statusBar.setBoxInfo( "Could not save Google Drive connection" );
            return;
        }
        mt::getSettings().cloudSources.enableGoogleDrive = true;
        leaveSource();
        refreshSourceRows();
        int idx = -1;
        for( int i = 0; i < static_cast<int>( shownPlaces.size() ); ++i )
            if( shownPlaces[static_cast<size_t>( i )].kind == Place::Kind::Source && shownPlaces[static_cast<size_t>( i )].path == juce::String::fromUTF8( cfg.id.c_str() ) )
                idx = i;
        if( idx >= 0 )
            showPlace( idx, true );
        statusBar.setBoxInfo( statusMsg );
    }

    void FileBrowser::startGoogleOAuthSignIn( const std::string& clientId, const std::string& clientSecret, const juce::String& emailHint, const std::string& existingId )
    {
        const auto scheme = fileSource::gdrive::buildReversedClientScheme( clientId );
        if( scheme.empty() )
        {
            statusBar.setBoxInfo( "Client ID must end with .apps.googleusercontent.com" );
            return;
        }

        // Generate PKCE code_verifier & random state using system PRNG.
        auto makeRandomToken = []( int len ) -> std::string {
            static const char kAlpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
            auto&             rng      = juce::Random::getSystemRandom();
            std::string       out;
            out.reserve( static_cast<size_t>( len ) );
            for( int i = 0; i < len; ++i )
                out += kAlpha[rng.nextInt( static_cast<int>( sizeof( kAlpha ) - 1 ) )];
            return out;
        };

        const std::string verifier    = makeRandomToken( 64 );
        const std::string state       = makeRandomToken( 32 );
        const std::string challenge   = fileSource::gdrive::pkceS256Challenge( verifier );
        const std::string redirectUri = scheme + ":/oauth2redirect";
        const std::string authUrl     = fileSource::gdrive::buildAuthUrl( clientId, redirectUri, challenge, state, emailHint.trim().toStdString() );

        statusBar.setBoxInfo( "Sign in to Google Drive in browser..." );
        juce::Component::SafePointer<FileBrowser> safeThis( this );
        kick_apple_cloud::startWebAuthSession(
            juce::String::fromUTF8( authUrl.c_str() ),
            juce::String::fromUTF8( scheme.c_str() ),
            [safeThis, clientId, clientSecret, redirectUri, verifier, state, emailHint, existingId]( bool ok, juce::String urlOrError ) {
                if( safeThis == nullptr )
                    return;
                if( !ok )
                {
                    safeThis->statusBar.setBoxInfo( "Google sign-in cancelled: " + urlOrError );
                    return;
                }
                const auto cb = fileSource::gdrive::parseOAuthCallbackUrl( urlOrError.toStdString() );
                if( !cb.ok() || cb.state != state )
                {
                    const auto errMsg = cb.errorDescription.empty() ? ( cb.error.empty() ? std::string( "state mismatch" ) : cb.error ) : cb.errorDescription;
                    safeThis->statusBar.setBoxInfo( "Google sign-in failed: " + juce::String::fromUTF8( errMsg.c_str() ) );
                    return;
                }

                safeThis->statusBar.setBoxInfo( "Completing Google Drive sign-in..." );
                safeThis->listPool.addJob( [safeThis, clientId, clientSecret, redirectUri, cb, verifier, emailHint, existingId]() {
                    fileSource::gdrive::Credentials creds;
                    const auto res = fileSource::GoogleDriveSource::exchangeAuthCode( clientId, clientSecret, redirectUri, cb.code, verifier, creds );
                    juce::MessageManager::callAsync( [safeThis, res, creds, emailHint, existingId]() mutable {
                        if( safeThis == nullptr )
                            return;
                        if( !res.ok() )
                        {
                            safeThis->statusBar.setBoxInfo( "Google token exchange failed: " + juce::String::fromUTF8( res.message.c_str() ) );
                            return;
                        }
                        if( creds.email.empty() && emailHint.trim().isNotEmpty() )
                            creds.email = emailHint.trim().toStdString();
                        fileSource::SourceConfig cfg;
                        const auto               userKey = !creds.email.empty() ? creds.email : std::string( "default" );
                        cfg.type                         = "gdrive";
                        cfg.id                           = !existingId.empty() ? existingId : ( "gdrive-" + fileSource::SourceCache::makeKey( "gd", userKey ).substr( 0, 8 ) );
                        cfg.name                         = !creds.email.empty() ? ( "Google Drive (" + creds.email.substr( 0, 22 ) + ")" ) : std::string( "Google Drive" );
                        cfg.location                     = "gdrive://root";
                        cfg.user                         = creds.email;
                        cfg.auth                         = "oauth";
                        if( !creds.email.empty() )
                            mt::getSettings().cloudSources.googleUserEmail = juce::String::fromUTF8( creds.email.c_str() );
                        safeThis->finishGoogleDriveSourceSetup( cfg, fileSource::gdrive::serializeCredentials( creds ), "Connected: " + juce::String::fromUTF8( cfg.name.c_str() ) );
                    } );
                } );
            } );
    }

    void FileBrowser::editGoogleDriveSource()
    {
        fileSource::SourceConfig existingCfg;
        bool                     editing  = false;
        const auto*              selPlace = currentPlaceRow();
        if( selPlace != nullptr && selPlace->kind == Place::Kind::Source && hub )
        {
            const auto id = selPlace->path.toStdString();
            for( auto& c : hub->allConfigs() )
            {
                if( c.id == id && c.type == "gdrive" )
                {
                    existingCfg = c;
                    editing     = true;
                    break;
                }
            }
        }

        auto&        cs = mt::getSettings().cloudSources;
        juce::String initial;
        if( editing )
            initial = existingCfg.user.empty() ? juce::String::fromUTF8( existingCfg.location.c_str() ) : juce::String::fromUTF8( existingCfg.user.c_str() );
        else if( cs.googleUserEmail.get().isNotEmpty() )
            initial = cs.googleUserEmail.get();
        else if( cs.googleDrivePath.get().isNotEmpty() )
            initial = cs.googleDrivePath.get();

        juce::Component::SafePointer<FileBrowser> safeThis( this );
#if JUCE_IOS
        const juce::String promptTitle = editing
                                             ? "GOOGLE EMAIL / LINK (EMPTY = REMOVE)"
                                             : "GOOGLE EMAIL, DRIVE LINK, OR 'FILES'";
#else
        const juce::String promptTitle = editing
                                             ? "GOOGLE EMAIL / LINK (EMPTY = REMOVE)"
                                             : "GOOGLE EMAIL, DRIVE FOLDER LINK, OR LOCAL PATH";
#endif
        project_actions::askText( promptTitle, initial, [safeThis, existingCfg, editing]( juce::String input ) {
            if( safeThis == nullptr )
                return;
            input = input.trim();

            if( editing && input.isEmpty() )
            {
                if( safeThis->hub )
                    safeThis->hub->removeSource( existingCfg.id );
                safeThis->leaveSource();
                safeThis->refreshSourceRows();
                safeThis->showPlace( 0, true );
                safeThis->statusBar.setBoxInfo( "Google Drive removed" );
                return;
            }

            if( input.equalsIgnoreCase( "FILES" ) )
            {
                safeThis->statusBar.setBoxInfo( "Choose audio files from Files / Google Drive..." );
                kick_apple_cloud::pickFilesFromIosDocumentProvider( [safeThis]( juce::StringArray paths, juce::String err ) {
                    if( safeThis == nullptr )
                        return;
                    if( !paths.isEmpty() )
                        safeThis->importIncomingFiles( paths, true );
                    else if( err.isNotEmpty() )
                        safeThis->statusBar.setBoxInfo( err );
                } );
                return;
            }

            // Local synced folder path (desktop Google Drive for Desktop or explicit folder)
            if( input.startsWithChar( '/' ) || juce::File( input ).isDirectory() )
            {
                mt::getSettings().cloudSources.googleDrivePath   = input;
                mt::getSettings().cloudSources.enableGoogleDrive = true;
                safeThis->refreshPlacesAndListing();
                safeThis->statusBar.setBoxInfo( "Google Drive local folder updated" );
                return;
            }

            // Shared folder link / folder ID + API key (or bearer access token)
            const auto stdIn      = input.toStdString();
            const bool looksLink  = stdIn.find( "drive.google.com" ) != std::string::npos || stdIn.rfind( "gdrive://folder/", 0 ) == 0 || ( !stdIn.empty() && stdIn.find( '@' ) == std::string::npos && fileSource::gdrive::parseFolderTarget( stdIn ).ok && stdIn != "root" );
            if( looksLink )
            {
                const auto target = fileSource::gdrive::parseFolderTarget( stdIn );
                if( !target.ok )
                {
                    safeThis->statusBar.setBoxInfo( "Invalid Google Drive folder link or ID" );
                    return;
                }
                project_actions::askText( "GOOGLE API KEY OR ACCESS TOKEN", "", [safeThis, target, existingCfg, editing]( juce::String keyOrTok ) {
                    if( safeThis == nullptr )
                        return;
                    keyOrTok = keyOrTok.trim();
                    if( keyOrTok.isEmpty() )
                        return;
                    fileSource::gdrive::Credentials creds;
                    const bool                      isBearer = keyOrTok.startsWith( "ya29." ) || keyOrTok.length() > 80;
                    creds.mode                               = isBearer ? "bearer" : "apikey";
                    if( isBearer )
                        creds.accessToken = keyOrTok.toStdString();
                    else
                        creds.apiKey = keyOrTok.toStdString();
                    creds.resourceKey = target.resourceKey;

                    fileSource::SourceConfig cfg;
                    cfg.type     = "gdrive";
                    cfg.id       = editing ? existingCfg.id : ( "gdrive-" + fileSource::SourceCache::makeKey( "gd", target.folderId ).substr( 0, 8 ) );
                    cfg.name     = editing ? existingCfg.name : ( "Drive: " + target.folderId.substr( 0, 14 ) );
                    cfg.location = "gdrive://folder/" + target.folderId;
                    cfg.auth     = creds.mode;
                    safeThis->finishGoogleDriveSourceSetup( cfg, fileSource::gdrive::serializeCredentials( creds ), "Google Drive folder connected" );
                } );
                return;
            }

            // Google Account OAuth flow (specified email hint or account chooser in Safari)
            const auto emailHint = input;
            if( emailHint.containsChar( '@' ) )
                mt::getSettings().cloudSources.googleUserEmail = emailHint;

            juce::String defaultClientId = mt::getSettings().cloudSources.googleClientId.get();
            if( defaultClientId.isEmpty() )
            {
#if defined( KICK_GOOGLE_CLIENT_ID )
                defaultClientId = KICK_GOOGLE_CLIENT_ID;
#endif
                if( defaultClientId.isEmpty() )
                    defaultClientId = juce::SystemStats::getEnvironmentVariable( "KICK_GOOGLE_CLIENT_ID", {} );
            }

            project_actions::askText( "GOOGLE OAUTH CLIENT ID (.apps.googleusercontent.com) OR TOKEN", defaultClientId, [safeThis, emailHint, existingCfg, editing]( juce::String credInput ) {
                if( safeThis == nullptr )
                    return;
                credInput = credInput.trim();
                if( credInput.isEmpty() )
                    return;

                // Direct access token ("ya29....") or OAuth refresh token ("1//...")
                if( credInput.startsWith( "ya29." ) || credInput.startsWith( "1//" ) )
                {
                    auto setupToken = [safeThis, emailHint, existingCfg, editing]( fileSource::gdrive::Credentials creds ) {
                        safeThis->statusBar.setBoxInfo( "Verifying Google Drive token..." );
                        safeThis->listPool.addJob( [safeThis, creds, emailHint, existingCfg, editing]() mutable {
                            if( !creds.accessToken.empty() )
                            {
                                std::string em, nm;
                                if( fileSource::GoogleDriveSource::fetchUserAbout( creds.accessToken, em, nm ).ok() && !em.empty() )
                                    creds.email = em;
                            }
                            if( creds.email.empty() && emailHint.isNotEmpty() )
                                creds.email = emailHint.toStdString();
                            juce::MessageManager::callAsync( [safeThis, creds, existingCfg, editing]() {
                                if( safeThis == nullptr )
                                    return;
                                fileSource::SourceConfig cfg;
                                const auto               userKey = !creds.email.empty() ? creds.email : std::string( "token" );
                                cfg.type                         = "gdrive";
                                cfg.id                           = editing ? existingCfg.id : ( "gdrive-" + fileSource::SourceCache::makeKey( "gd", userKey ).substr( 0, 8 ) );
                                cfg.name                         = !creds.email.empty() ? ( "Google Drive (" + creds.email.substr( 0, 22 ) + ")" ) : std::string( "Google Drive" );
                                cfg.location                     = "gdrive://root";
                                cfg.user                         = creds.email;
                                cfg.auth                         = creds.mode;
                                safeThis->finishGoogleDriveSourceSetup( cfg, fileSource::gdrive::serializeCredentials( creds ), "Connected: " + juce::String::fromUTF8( cfg.name.c_str() ) );
                            } );
                        } );
                    };

                    if( credInput.startsWith( "ya29." ) )
                    {
                        fileSource::gdrive::Credentials creds;
                        creds.mode        = "bearer";
                        creds.accessToken = credInput.toStdString();
                        setupToken( creds );
                    }
                    else
                    {
                        const juce::String savedId = mt::getSettings().cloudSources.googleClientId.get();
                        project_actions::askText( "CLIENT ID FOR REFRESH TOKEN", savedId, [safeThis, credInput, setupToken]( juce::String cid ) {
                            if( safeThis == nullptr || cid.trim().isEmpty() )
                                return;
                            mt::getSettings().cloudSources.googleClientId = cid.trim();
                            fileSource::gdrive::Credentials creds;
                            creds.mode         = "oauth";
                            creds.clientId     = cid.trim().toStdString();
                            creds.refreshToken = credInput.toStdString();
                            setupToken( creds );
                        } );
                    }
                    return;
                }

                mt::getSettings().cloudSources.googleClientId = credInput;
                safeThis->startGoogleOAuthSignIn( credInput.toStdString(), {}, emailHint, editing ? existingCfg.id : std::string() );
            } );
        } );
    }

    // Add a WebDAV location, or (when a source row is selected) edit it; an empty URL removes the edited one.
    void FileBrowser::editWebDavSource()
    {
        if( !hub )
            return;
        fileSource::SourceConfig cfg;
        bool                     editing = false;
        const auto*              selPlace = currentPlaceRow();
        if( selPlace != nullptr && selPlace->kind == Place::Kind::Source )
        {
            const auto id = selPlace->path.toStdString();
            for( auto& c : hub->allConfigs() )
                if( c.id == id && c.type == "webdav" )
                {
                    cfg     = c;
                    editing = true;
                }
        }
        juce::Component::SafePointer<FileBrowser> safeThis( this );
        project_actions::askText( editing ? "WEBDAV URL (EMPTY = REMOVE)" : "WEBDAV URL (http://host/path)", editing ? juce::String::fromUTF8( cfg.location.c_str() ) : juce::String( "http://" ), [safeThis, cfg, editing]( juce::String url ) mutable {
            if( safeThis == nullptr )
                return;
            url = url.trim();
            if( editing && ( url.isEmpty() || url == "http://" ) )
            {
                safeThis->hub->removeSource( cfg.id );
                safeThis->leaveSource();
                safeThis->refreshSourceRows();
                safeThis->showPlace( 0, true );
                return;
            }
            if( url.isEmpty() || !fileSource::webdav::parseUrl( url.toStdString() ).ok )
            {
                safeThis->statusBar.setBoxInfo( "Invalid WebDAV URL" );
                return;
            }
            cfg.location = url.toStdString();
            project_actions::askText( "USER (EMPTY = NONE)", juce::String::fromUTF8( cfg.user.c_str() ), [safeThis, cfg, editing]( juce::String user ) mutable {
                if( safeThis == nullptr )
                    return;
                cfg.user = user.trim().toStdString();
                project_actions::askText( editing ? "PASSWORD (EMPTY = KEEP)" : "PASSWORD", "", [safeThis, cfg, editing]( juce::String pass ) mutable {
                    if( safeThis == nullptr )
                        return;
                    if( !editing )
                    {
                        const auto u = fileSource::webdav::parseUrl( cfg.location );
                        std::string id;
                        for( char c : u.host )
                            id += ( std::isalnum( static_cast<unsigned char>( c ) ) ? static_cast<char>( std::tolower( static_cast<unsigned char>( c ) ) ) : '-' );
                        auto key = fileSource::SourceCache::makeKey( "cfg", cfg.location + "\n" + cfg.user );
                        cfg.id   = ( id.size() > 20 ? id.substr( 0, 20 ) : id ) + "-" + key.substr( 0, 6 );
                        cfg.name = u.host.size() > 24 ? u.host.substr( 0, 24 ) : u.host;
                    }
                    cfg.type = "webdav";
                    cfg.auth = "basic";
                    mt::getSettings().cloudSources.enableWebDav = true;
                    if( !safeThis->hub->upsertWebDav( cfg, pass.toStdString() ) )
                    {
                        safeThis->statusBar.setBoxInfo( "Could not save the WebDAV location" );
                        return;
                    }
                    safeThis->leaveSource();
                    safeThis->refreshSourceRows();
                    int idx = -1;
                    for( int i = 0; i < static_cast<int>( safeThis->shownPlaces.size() ); ++i )
                        if( safeThis->shownPlaces[static_cast<size_t>( i )].kind == Place::Kind::Source && safeThis->shownPlaces[static_cast<size_t>( i )].path == juce::String::fromUTF8( cfg.id.c_str() ) )
                            idx = i;
                    if( idx >= 0 )
                        safeThis->showPlace( idx, true );
                } );
            } );
        } );
    }

    void FileBrowser::scanNetworkSources()
    {
        if( !hub )
            return;
        statusBar.setBoxInfo( "Scanning local network for Mac / NAS shares..." );
        juce::Component::SafePointer<FileBrowser> safeThis( this );
        listPool.addJob( [safeThis]() {
            auto found = fileSource::discovery::scanLocalWebDavServers( 300 );
            juce::MessageManager::callAsync( [safeThis, found]() mutable {
                if( safeThis == nullptr )
                    return;
                if( found.empty() )
                {
                    safeThis->statusBar.setBoxInfo( "No network shares found. Enter URL manually." );
                    safeThis->editWebDavSource();
                    return;
                }

                const auto ds = found.front();
                fileSource::SourceConfig cfg;
                cfg.type     = "webdav";
                cfg.id       = "mac-share";
                cfg.name     = ds.name.empty() ? "Mac Share" : ds.name;
                cfg.location = ds.url;
                cfg.auth     = "basic";
                mt::getSettings().cloudSources.enableWebDav = true;

                if( !safeThis->hub->upsertWebDav( cfg, "" ) )
                {
                    safeThis->statusBar.setBoxInfo( "Could not map network share" );
                    return;
                }
                safeThis->statusBar.setBoxInfo( "Connected to " + juce::String::fromUTF8( cfg.name.c_str() ) );
                safeThis->leaveSource();
                safeThis->refreshSourceRows();
                int idx = -1;
                for( int i = 0; i < static_cast<int>( safeThis->shownPlaces.size() ); ++i )
                    if( safeThis->shownPlaces[static_cast<size_t>( i )].kind == Place::Kind::Source && safeThis->shownPlaces[static_cast<size_t>( i )].path == juce::String::fromUTF8( cfg.id.c_str() ) )
                        idx = i;
                if( idx >= 0 )
                    safeThis->showPlace( idx, true );
            } );
        } );
    }

    // Add a local folder as an extra location (the Splice folder of a desktop, a folder on a drive), or edit / remove the selected one.
    void FileBrowser::editLocalSource()
    {
        if( !hub )
            return;
        fileSource::SourceConfig cfg;
        bool                     editing  = false;
        const auto*              selPlace = currentPlaceRow();
        if( selPlace != nullptr && selPlace->kind == Place::Kind::Source )
            for( auto& c : hub->allConfigs() )
                if( c.id == selPlace->path.toStdString() && c.type == "local" )
                {
                    cfg     = c;
                    editing = true;
                }
        juce::Component::SafePointer<FileBrowser> safeThis( this );
#if JUCE_IOS
        const auto                                initial = editing ? juce::String::fromUTF8( cfg.location.c_str() ) : findBestSpliceDirectory().getFullPathName();
#else
        const auto                                initial = editing ? juce::String::fromUTF8( cfg.location.c_str() ) : juce::File::getSpecialLocation( juce::File::userHomeDirectory ).getChildFile( "Splice" ).getFullPathName();
#endif
        project_actions::askText( editing ? "FOLDER PATH (EMPTY = REMOVE)" : "FOLDER PATH", initial, [safeThis, cfg, editing]( juce::String path ) mutable {
            if( safeThis == nullptr )
                return;
            path = path.trim();
            if( editing && path.isEmpty() )
            {
                safeThis->hub->removeSource( cfg.id );
                safeThis->leaveSource();
                safeThis->refreshSourceRows();
                safeThis->showPlace( 0, true );
                return;
            }
            if( !juce::File( path ).isDirectory() )
            {
                safeThis->statusBar.setBoxInfo( "This folder does not exist" );
                return;
            }
            cfg.type     = "local";
            cfg.location = path.toStdString();
            if( !editing )
            {
                const auto key = fileSource::SourceCache::makeKey( "cfg", cfg.location );
                cfg.id         = "local-" + key.substr( 0, 8 );
                cfg.name       = juce::File( path ).getFileName().substring( 0, 24 ).toStdString();
                if( cfg.name.empty() )
                    cfg.name = "Folder";
            }
            if( !safeThis->hub->upsertLocal( cfg ) )
            {
                safeThis->statusBar.setBoxInfo( "Could not save the folder location" );
                return;
            }
            safeThis->leaveSource();
            safeThis->refreshSourceRows();
            for( int i = 0; i < static_cast<int>( safeThis->shownPlaces.size() ); ++i )
                if( safeThis->shownPlaces[static_cast<size_t>( i )].kind == Place::Kind::Source && safeThis->shownPlaces[static_cast<size_t>( i )].path == juce::String::fromUTF8( cfg.id.c_str() ) )
                    safeThis->showPlace( i, true );
        } );
    }

    void FileBrowser::refreshPlacesAndListing()
    {
        rebuildShortcuts();
        refreshSourceRows();
        rebuildShown();
        if( left.table )
            left.table->refresh();
        if( !remoteActive && ( isShowing() || isVisible() ) )
        {
            const auto keep = pendingImportSelectName.isNotEmpty()
                                  ? pendingImportSelectName
                                  : ( selectedFile.isEmpty() ? juce::String() : selectedFile.getLast().getFileName() );
            pendingImportSelectName.clear();
            navigate( pwd, keep );
        }
    }

    void FileBrowser::importIncomingFiles( const juce::StringArray& paths, bool removeSourceOnSuccess )
    {
        if( paths.isEmpty() )
            return;

        const std::string samplesDir  = ae.getSoundsDir().getFullPathName().toStdString();
        const std::string projectsDir = ae.getInternalDir().getFullPathName().toStdString();
        juce::StringArray importedSamples;
        juce::String      importedProject;
        int               unsupported = 0;

        for( const auto& rawPath : paths )
        {
            if( rawPath.trim().isEmpty() )
                continue;
            std::string                   destPath, err;
            kick_incoming::IncomingKind   kind = kick_incoming::IncomingKind::Unsupported;
            if( kick_incoming::importIncomingFile( rawPath.toStdString(), samplesDir, projectsDir, removeSourceOnSuccess, destPath, kind, err ) )
            {
                kick_log( "INCOMING", "imported %s -> %s", rawPath.toRawUTF8(), destPath.c_str() );
                if( kind == kick_incoming::IncomingKind::Project )
                    importedProject = juce::String::fromUTF8( destPath.c_str() );
                else
                    importedSamples.add( juce::String::fromUTF8( destPath.c_str() ) );
            }
            else if( kind == kick_incoming::IncomingKind::Unsupported )
            {
                ++unsupported;
            }
        }

        if( importedSamples.isEmpty() && importedProject.isEmpty() )
        {
            if( unsupported > 0 )
                statusBar.setBoxInfo( "Unsupported file type (expected WAV/AIFF/FLAC/MP3/OGG/M4A/MID/kickPrj)" );
            return;
        }

        fileUtils::invalidateFolderSizes( ae.getSoundsDir() );
        fileUtils::invalidateFolderSizes( ae.getInternalDir() );
        rebuildShortcuts();
        refreshSourceRows();
        rebuildShown();

        if( !importedSamples.isEmpty() )
        {
            const juce::File lastSample( importedSamples[importedSamples.size() - 1] );
            const int        samplesRow = rowOfPlaceContaining( ae.getSoundsDir().getFullPathName() );
            leaveSource();
            if( leftTab != LeftTab::Places )
                setLeftTab( LeftTab::Places );
            if( samplesRow >= 0 )
                showPlace( samplesRow, false );
            placeRoot = ae.getSoundsDir().getFullPathName();
            updateEditButtons();
            pendingImportSelectName = lastSample.getFileName();
            pendingImportBanner     = importedSamples.size() == 1
                                          ? ( "Saved to Samples: " + lastSample.getFileName() )
                                          : ( "Saved " + juce::String( importedSamples.size() ) + " file(s) to Samples" );
            navigate( ae.getSoundsDir().getFullPathName(), lastSample.getFileName() );
            statusBar.setBoxInfo( pendingImportBanner );
        }
        else if( importedProject.isNotEmpty() )
        {
            const juce::File projFile( importedProject );
            statusBar.setBoxInfo( "Imported project: " + projFile.getParentDirectory().getFileName() );
            loadProjectFolder( projFile.getParentDirectory() );
        }
    }

    ///////////////////////////////////////
    // Class FileBrowserLeftPanel
    ///////////////////////////////////////
    FileBrowserLeftPanel::FileBrowserLeftPanel()
    {
        setLookAndFeel( KickLooknFeel::getLnf() );
        mt::utils::addAndMakeVisible( this, places, content, expansion, location, sourceButton, folderButton );
        for( auto* b : { &sourceButton, &folderButton } )
        {
            b->setValueColour( ui::Theme::textColour( 1 ) );
            b->setValueFontHeight( 13 );
        }

        location.setPadding( labelLeftMargin );
        location.setFontHeight( labelFontHeight );
        location.setValueJustification( juce::Justification::centredLeft );
        location.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        table.reset( new KickTableView( "List", provider ) );
        addAndMakeVisible( *table );
        table->setRowHeight( leftTableRowHeight );
        table->setRowBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        table->setSelectedRowBackgroundColour( ui::Theme::backgroundColour( 0 ) );
        table->setSelectedRowOutlineColour( ui::Theme::outlineColour( 0 ) );

        table->setRowSpacing( 0 );
        table->enableScrollBar( false );

        themeChanged.connect( [&]() {
            places.setImages( "Places" );
            content.setImages( "Content" );
            expansion.setImages( "Expansion" );
        } );
    }

    void FileBrowserLeftPanel::paint( juce::Graphics& g )
    {
        auto b = getLocalBounds();
        //Top Rectangle
        mt::utils::fillRoundedRect( g, leftTopBounds, ui::Theme::backgroundColour( 0 ), 6.0f, true, true, false, false );

        //Centre Rectangle
        mt::utils::fillRoundedRect( g, leftCentreBounds, ui::Theme::backgroundColour( 1 ), 6.0f, false, false, true, true );

        //Draw Border
        g.setColour( ui::Theme::outlineColour( 3 ) );
        g.drawRoundedRectangle( b.getX(), b.getY(), b.getWidth(), b.getHeight(), cornerSizeX, lineThickness );

        //Draw Centre Line
        g.setColour( ui::Theme::outlineColour( 3 ) );
        g.drawRect( { lineX, lineY, lineWidth, lineHeight }, lineThickness );
    }

    void FileBrowserLeftPanel::resized()
    {
        auto b = getLocalBounds();
        table->setBoundsRelative( 0.0f, 0.16f, 1.0f, 0.8421f );
        location.setBoundsRelative( 0.0292f, 0.012f, 0.30f, 0.1294f );
        sourceButton.setBoundsRelative( 0.35f, 0.016f, 0.30f, 0.1222f );
        folderButton.setBoundsRelative( 0.67f, 0.016f, 0.30f, 0.1222f );
    }

    //////////////////////////////////
    // Class FileBrowserRightPanel
    //////////////////////////////////
    FileBrowserRightPanel::FileBrowserRightPanel()
    {
        table.reset( new KickTableView( "List", provider ) );
        mt::utils::addAndMakeVisible( this, *table, name, date, timeStamp, size, backButton );

        table->setRowHeight( rightTableRowHeight );
        table->setRowSpacing( 0 );
        table->setRowBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        table->setSelectedRowBackgroundColour( ui::Theme::backgroundColour( 0 ) );
        table->setSelectedRowOutlineColour( ui::Theme::outlineColour( 0 ) );
        table->setAutoScrollToSelection( true );

        name.setPadding( labelLeftMargin );
        name.setFontHeight( labelFontHeight );
        date.setFontHeight( labelFontHeight );
        timeStamp.setFontHeight( labelFontHeight );
        size.setFontHeight( labelFontHeight );

        name.setValueJustification( juce::Justification::centredLeft );
        date.setValueJustification( juce::Justification::centred );
        timeStamp.setValueJustification( juce::Justification::centred );
        size.setValueJustification( juce::Justification::centred );

        name.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        date.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        timeStamp.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        size.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        themeChanged.connect( [&]() {
            backButton.setImages( "Back_Browser" );
        } );
    }

    void FileBrowserRightPanel::setSortIndicator( int key, bool ascending )
    {
        const juce::String arrow = ascending ? " ^" : " v"; // ascending / descending (plain characters: every font has them)
        name.setText( juce::String( "Name" ) + ( key == 0 ? arrow : juce::String() ), juce::dontSendNotification );
        date.setText( juce::String( "Date" ) + ( key == 1 ? arrow : juce::String() ), juce::dontSendNotification );
        size.setText( juce::String( "Size" ) + ( key == 2 ? arrow : juce::String() ), juce::dontSendNotification );
    }

    void FileBrowserRightPanel::setColumns( bool showDate, bool showTime, bool showSize )
    {
        date.setVisible( showDate );
        timeStamp.setVisible( showTime );
        size.setVisible( showSize );
    }

    // A tap on a column title sorts by it (again = the other way round).
    void FileBrowserRightPanel::mouseDown( const juce::MouseEvent& e )
    {
        const auto p = e.getEventRelativeTo( this ).getPosition();
        if( p.y > 54 || !onHeaderClicked )
            return;
        if( name.getBounds().contains( p ) )
            onHeaderClicked( 0 );
        else if( date.getBounds().contains( p ) || timeStamp.getBounds().contains( p ) )
            onHeaderClicked( 1 );
        else if( size.getBounds().contains( p ) )
            onHeaderClicked( 2 );
    }

    void FileBrowserRightPanel::paint( juce::Graphics& g )
    {
        auto b = getLocalBounds();
        //Top Rectangle
        mt::utils::fillRoundedRect( g, rightTopBounds, ui::Theme::backgroundColour( 1 ), 6.0f, true, true, false, false );

        //Centre Rectangle
        mt::utils::fillRoundedRect( g, rightCentreBounds, ui::Theme::backgroundColour( 3 ), 6.0f, false, false, true, true );

        //Draw Border
        g.setColour( ui::Theme::outlineColour( 3 ) );
        g.drawRoundedRectangle( b.getX(), b.getY(), b.getWidth(), b.getHeight(), cornerSizeX, lineThickness );

        //Draw Centre Line
        g.setColour( ui::Theme::outlineColour( 3 ) );
        g.drawRect( { lineX, lineY, 652, lineHeight }, lineThickness );
    }

    void FileBrowserRightPanel::resized()
    {
        table->setBoundsRelative( 0.012f, 0.16f, 0.987f, 0.842f );
        backButton.setBoundsRelative( 0.0153f, 0.0292f, 0.0521f, 0.0994f );
        name.setBoundsRelative( 0.09227f, 0.0146f, 0.2622f, 0.1286f );
        date.setBoundsRelative( 0.3857f, 0.0146f, 0.1840f, 0.1286f );
        timeStamp.setBoundsRelative( 0.5991f, 0.0146f, 0.1840f, 0.1286f );
        size.setBoundsRelative( 0.81780f, 0.0146f, 0.1840f, 0.1286f );
    }

    //////////////////////////////////
    // Class FileBrowserTopControls
    ///////////////////////////////////

    FileBrowserTopControls::FileBrowserTopControls()
    {
        mt::utils::addAndMakeVisible( this, searchLabel, searchButton, deleteBtn, selectBtn, renameBtn, cutBtn, copyBtn, pasteBtn, selectAllBtn );
        selectAllBtn.setValueColour( ui::Theme::textColour( 1 ) );
        selectAllBtn.setValueFontHeight( 14 );
        searchLabel.setTextColour( ui::Theme::textColour( 1 ) );
        searchLabel.setEditable( false, true );
        searchLabel.enableKeyboardEmptyText( true );
        searchLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 13 ) );
        searchLabel.setPadding( 10 );
        searchLabel.setFontHeight( 18.0f );

        onElementChangedCallback = [&]() {
            repaint();
            resized();
        };

        themeChanged.connect( [&]() {
            searchButton.setImages( "Search_Browser", "Search_BrowserActive" );
            backButton.setImages( "Back_Browser", "Back_Browser", "Back_BrowserActive", "Back_Browser_Disabled" );
            projectButton.setImages( "Project_Browser", "Project_BrowserActive" );
            sampleButton.setImages( "Sample_Browser", "Sample_BrowserActive" );
            allButton.setImages( "All_Browser", "All_BrowserActive" );
            cutBtn.setImages( "Cut Button", "Cut Button_Active", "Cut Button", "Cut Disabled" );
            copyBtn.setImages( "Copy Button", "Copy Button", "Copy Button_Active" );
            pasteBtn.setImages( "Paste Button", "Paste Button", "Paste Button", "Paste Disabled" );
            deleteBtn.setImages( "Trash", "Trash_Active", "Trash", "Trash Disabled" );
            renameBtn.setImages( "Rename", "Rename_Active", "Rename", "Rename Disabled" );
            selectBtn.setImages( "Select_Normal", "Select_Active" );
        } );
    }

    void FileBrowserTopControls::paint( juce::Graphics& g )
    {
        juce::Rectangle<int> bounds;
        bounds = { 10, 0, 904, 43 };

        mt::utils::renderRoundedRect( g, bounds, ui::Theme::outlineColour( 3 ), 1, 6.0f );
        mt::utils::fillRoundedRect( g, bounds, ui::Theme::backgroundColour( 3 ), 6.0f );
    }

    void FileBrowserTopControls::resized()
    {
        if( !selectBtn.getToggleState() )
        {
            searchLabel.setBoundsRelative( .0097f, .0f, .8828f, .9772f );
            searchButton.setBoundsRelative( .8515f, .0f, .0429f, 1.0f );
            selectBtn.setBoundsRelative( .8984f, .0f, .0429f, 1.0f );
            deleteBtn.setBoundsRelative( .9453f, .0f, .0429f, 1.0f );
        }
        else
        {
            // Select mode: All, Select, Cut, Copy, Paste, Rename and Delete (Delete stays visible: it acts on what is marked).
            searchLabel.setBoundsRelative( .0097f, .0f, .5500f, .9772f );
            searchButton.setBoundsRelative( .5650f, .0f, .0429f, 1.0f );
            selectAllBtn.setBoundsRelative( .6120f, .0f, .0529f, .9f );
            selectBtn.setBoundsRelative( .6700f, .0f, .0429f, 1.0f );
            cutBtn.setBoundsRelative( .7170f, .0f, .0429f, 1.0f );
            copyBtn.setBoundsRelative( .7640f, .0f, .0429f, 1.0f );
            pasteBtn.setBoundsRelative( .8110f, .0f, .0429f, 1.0f );
            renameBtn.setBoundsRelative( .8580f, .0f, .0429f, 1.0f );
            deleteBtn.setBoundsRelative( .9453f, .0f, .0429f, 1.0f );
        }
        repaint();
    }

    ///////////////////////////////////
    // Class FileBrowserBottomControls
    ///////////////////////////////////
    FileBrowserBottomControls::FileBrowserBottomControls()
        : TabbedComponent( juce::TabbedButtonBar::Orientation::TabsAtBottom )
    {
        mt::utils::setRadioGroupID( (int)kick_ui::RadioGroupId::FileBrowserFilter, allButton, projectButton, sampleButton );

        auditionButton.incrementSelection();

        sampleAssignButton.setValueColour( ui::Theme::textColour( 1 ) );
        sampleAssignButton.setValueFontHeight( 14 );

        loadButton.setValueColour( ui::Theme::textColour( 1 ) );
        loadButton.setValueFontHeight( 16 );

        loadAllButton.setValueColour( ui::Theme::textColour( 1 ) );
        loadAllButton.setValueFontHeight( 16 );

        openButton.setValueColour( ui::Theme::textColour( 1 ) );
        openButton.setValueFontHeight( 14 );
        optionsButton.setValueColour( ui::Theme::textColour( 1 ) );
        optionsButton.setValueFontHeight( 14 );
        mt::utils::addAndMakeVisible( this, sampleAssignButton, playIconButton, auditionButton, loadButton, loadAllButton, projectButton, sampleButton, allButton, volumeBtn, volumeMuteBtn, openButton, optionsButton );
        volumeMuteBtn.setVisible( false );
        sampleAssignButton.onClick = [&]() {
            kick_ui::signals::sigScreenOpen( kick_ui::Screen::MenuAssignSample );
        };

        themeChanged.connect( [&]() {
            projectButton.setImages( "Project_Browser", "Project_BrowserActive" );
            sampleButton.setImages( "Sample_Browser", "Sample_BrowserActive" );
            allButton.setImages( "All_Browser", "All_BrowserActive" );
            playIconButton.setImages( "Play_browser", "Play_browser", "Play_browser_Active" );
            volumeBtn.setImages( "Volume_Browser", "Volume_BrowserActive" );
            volumeMuteBtn.setImages( "Button_VolumeMute", "Button_VolumeMute" );
        } );
    }

    void FileBrowserBottomControls::paint( juce::Graphics& g )
    {}

    void FileBrowserBottomControls::resized()
    {
        auto b = getLocalBounds();

        projectButton.setBounds( 10, 0, 84, 34 );
        sampleButton.setBounds( 98, 0, 84, 34 );
        allButton.setBounds( 186, 0, 84, 34 );
        openButton.setBounds( 276, 0, 56, 34 );
        optionsButton.setBounds( 336, 0, 54, 34 );
        sampleAssignButton.setBounds( 394, 0, 100, 34 );
        loadButton.setBounds( 498, 0, 100, 34 );
        loadAllButton.setBounds( 602, 0, 100, 34 );
        playIconButton.setBounds( 706, 0, 100, 34 );
        auditionButton.setBounds( 810, 0, 100, 34 );
        volumeBtn.setBounds( 914, 0, 100, 34 );
        volumeMuteBtn.setBounds( 914, 0, 100, 34 );
    }

    ///////////////////////////////////////
    // Class FileLeftEntry
    ///////////////////////////////////////
    FileLeftEntry::FileLeftEntry( int rowId )
    {
        row = rowId;
        file.setTextColour( ui::Theme::textColour( 1 ) );
        file.setFontHeight( 14.0f );
        file.setPadding( 10 );
        file.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        imageButton.setInterceptsMouseClicks( false, false );
        file.setInterceptsMouseClicks( false, false );
        ejectButton.setInterceptsMouseClicks( true, false );
        ejectButton.onClick = [&]() {
            auto p = new FilePopup{ "Do you want to Eject this device?" };
            mt::utils::showDialog(
                "EJECT", p, [p, this]( int R ) {
                    if(R==UI::APPLY)
                    {
                        if( ejectCallback )
                            ejectCallback( row );
                    }
                    delete p; },
                ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
        };

        // The two confirmations and the work are in FileBrowser::formatDrive.
        formatButton.onClick = [&]() {
            if( formatCallback )
                formatCallback( row );
        };

        themeChanged.connect( [&]() {
            ejectButton.setImages( "Eject", "Eject", "Eject_Active" );
            formatButton.setImages( "Format", "Format", "Format_Active" );
        } );
    }

    void FileLeftEntry::setHighlight( const bool state )
    {
        imageButton.setToggleState( state, juce::dontSendNotification );
        state ? file.setTextColour( ui::Theme::textColour( 2 ) ) : file.setTextColour( ui::Theme::textColour( 1 ) );
        repaint();
    }

    void FileLeftEntry::resized()
    {
        auto b = getLocalBounds();
        imageButton.setBoundsRelative( 0.06f, 0.3396f, width / b.getWidth(), height / b.getHeight() );
        file.setBoundsRelative( 0.126f, 0.188f, 0.671f, 0.622f );
        ejectButton.setBoundsRelative( 0.867f, 0.188f, 0.102f, 0.641f );
        formatButton.setBoundsRelative( 0.73f, 0.188f, 0.102f, 0.641f );
    }

    void FileLeftEntry::paint( juce::Graphics& g )
    {}
    void FileLeftEntry::setProperties( const Place& place )
    {
        height = place.height;
        width  = place.width;
        file.setText( place.label, juce::dontSendNotification );
        imageButton.setImages( place.icon, place.iconActive );
        mt::utils::addAndMakeVisible( this, imageButton, file );
        // A drive can be ejected and formatted; the other places have neither button (a recycled row hides them again).
        ejectButton.setVisible( place.isDrive() );
        formatButton.setVisible( place.isDrive() );
        if( place.isDrive() )
            mt::utils::addAndMakeVisible( this, ejectButton, formatButton );
        resized();
    }
    ///////////////////////////////////////
    // Class FileLeftEntryPopup
    ///////////////////////////////////////
    FileLeftEntryPopup::FileLeftEntryPopup( juce::String Info, juce::String image, int folderW, int folderH, juce::String FileName, juce::String Date )
        : info( Info ), width( folderW ), height( folderH )
    {
        mt::utils::addAndMakeVisible( this, button1, button2, info, folder, fileName, date );
        folder.setImages( image );
        fileName.setText( FileName, juce::dontSendNotification );
        date.setText( Date, juce::dontSendNotification );

        info.setFontHeight( 18.0f );
        info.setTextColour( ui::Theme::textColour( 1 ) );
        info.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        info.setValueJustification( juce::Justification::centred );

        fileName.setTextColour( ui::Theme::titleColour( 3 ) );
        fileName.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        fileName.setFontHeight( 18.0f );

        date.setTextColour( ui::Theme::titleColour( 3 ) );
        date.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        date.setFontHeight( 18.0f );
        image == "" ? y = 123 : y = 48;
        themeChanged.connect( [&]() {
            button1.setImages( "No" );
            button2.setImages( "Yes" );
            folder.setImages( "Vector-1" );
        } );
    }

    void FileLeftEntryPopup::close()
    {
        if( auto dw = getCurrentlyModalComponent() )
            dw->exitModalState( 0 );
    }

    void FileLeftEntryPopup::resized()
    {
        auto b = getLocalBounds();

        info.setBounds( x, y, 403, 22 );
        folder.setBounds( 172, 125, width, height );
        fileName.setBounds( 239, 123, 180, 22 );
        date.setBounds( 239, 145, 180, 22 );
        button1.setBoundsRelative( 0.05f, 0.86f, 0.438f, 0.106f );
        button2.setBoundsRelative( 0.51f, 0.86f, 0.438f, 0.106f );
    }

    void FileLeftEntryPopup::paint( juce::Graphics& g )
    {}

    FileRightEntry::FileRightEntry()
    {
        file.setTextColour( ui::Theme::textColour( 1 ) );
        file.setFontHeight( 14.0f );
        file.setPadding( 10 );
        file.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        date.setValueJustification( juce::Justification::centred );
        date.setTextColour( ui::Theme::textColour( 1 ) );
        date.setFontHeight( 14.0f );
        date.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        time.setValueJustification( juce::Justification::centred );
        time.setTextColour( ui::Theme::textColour( 1 ) );
        time.setFontHeight( 14.0f );
        time.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        size.setValueJustification( juce::Justification::centred );
        size.setTextColour( ui::Theme::textColour( 1 ) );
        size.setFontHeight( 14.0f );
        size.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        mt::utils::addAndMakeVisible( this, file, date, time, size );
    }

    void FileRightEntry::resized()
    {
        auto  b = getLocalBounds();
        float x;
        isFolder ? x = 0.028f : x = 0.034f;
        imageBtn.setBoundsRelative( x, 0.33f, width / b.getWidth(), height / b.getHeight() );
        file.setBoundsRelative( 0.08f, 0.185f, 0.33f, 0.611f );
        date.setBoundsRelative( 0.383f, 0.203f, 0.19f, 0.574f );
        time.setBoundsRelative( 0.6f, 0.203f, 0.19f, 0.574f );
        size.setBoundsRelative( 0.826f, 0.203f, 0.19f, 0.574f );
    }
    void FileRightEntry::paint( juce::Graphics& g )
    {}

    void FileRightEntry::setProperties( const juce::String& imageButtonFile, bool bFolder, const float Width, const float Height, const juce::String& File, const juce::String& Date, const juce::String& Time, const juce::String& Size )
    {
        isFolder = bFolder;
        imageBtn.setImages( imageButtonFile );
        width  = Width;
        height = Height;
        file.setText( File, juce::dontSendNotification );
        date.setText( Date, juce::dontSendNotification );
        time.setText( Time, juce::dontSendNotification );
        size.setText( Size, juce::dontSendNotification );
        mt::utils::addAndMakeVisible( this, imageBtn, file );
        if( !addedLabels )
        {
            mt::utils::addAndMakeVisible( this, date, time, size );
            addedLabels = true;
        }
        resized();
    }

    void FileRightEntry::setColumns( bool showDate, bool showTime, bool showSize )
    {
        date.setVisible( showDate );
        time.setVisible( showTime );
        size.setVisible( showSize );
    }

    void FileRightEntry::setSizeText( const juce::String& Size )
    {
        size.setText( Size, juce::dontSendNotification );
    }

    AuditionComponent::AuditionComponent()
    {
        mt::utils::addAndMakeVisible( this, slider, volume );

        slider.setRange( MIDI::MIN, MIDI::MAX, MIDI::STEP );
        slider.setValue( settings.preview.level.get() );
        slider.setValueProportion( .10 );

        slider.onValueChanged = [&]( double value ) {
            settings.preview.level = value;
            if( !value )
                volume.setToggleState( true, juce::sendNotification );
            else
                volume.setToggleState( false, juce::sendNotification );

            onGainChanged( getVolume() );
        };

        volume.onClick = [&]() {
            onMuteChanged( volume.getToggleState() );
        };
    }

    void AuditionComponent::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, getLocalBounds(), ui::Theme::backgroundColour( 1 ), 6.0f );
    }

    void AuditionComponent::resized()
    {
        slider.setBoundsRelative( .3523f, .0667f, .2857f, .7428f );
        volume.setBoundsRelative( .019f, .8164f, .9523f, .1914f );
    }

}

#if ! JUCE_IOS && ! JUCE_MAC
namespace kick_apple_cloud
{
    void startWebAuthSession( const juce::String&, const juce::String&, std::function<void( bool, juce::String )> onDone )
    {
        if( onDone )
            onDone( false, "Browser OAuth session requires iOS or macOS" );
    }

    void pickFilesFromIosDocumentProvider( std::function<void( juce::StringArray, juce::String )> onDone )
    {
        if( onDone )
            onDone( {}, "Files picker is only available on iOS" );
    }
}
#endif
