/*
 * Copyright (c) 2021 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

namespace kick_ui
{
    namespace
    {
        // Controls without a function behind them are not shown (they used to look usable and did nothing).
        constexpr bool kSnapAvailable   = false; // Snap button: no handler
    }

    bool ListEntry::useNoteNames = false;

    /////////////////////////////////////////////////////////////////////////////////////
    ///////////////               List Edit Mode
    /////////////////////////////////////////////////////////////////////////////////////

    ListEditMode::ListEditMode()
        : IScreen( Screen::MenuListEdit )
    {
        setLookAndFeel( ui::KickLooknFeel::getLnf() );
        tableView.reset( new ui::KickTableView( "List", provider ) );

        tableView->setRowHeight( 52 );
        tableView->setRowSpacing( 0 );
        tableView->setSelectedRowBackgroundColour( ui::Theme::backgroundColour( 0 ) );
        tableView->setRowBackgroundColour( ui::Theme::backgroundColour( 3 ) );
        tableView->setMargin( 0, 0, 4, 0 );
        tableView->enableSelectMode( false );
        // Only the rows in (and just around) the visible area exist as components: thousands of events open at once.
        tableView->enableVirtualRows( true );
        tableView->addMouseListener( this, true ); // a touch pauses auto scroll
        for( int i = 0; i < headerTitles.size(); ++i )
        {
            tableHeaderData.emplace_back( std::make_unique<ui::KickLabel>( headerTitles[i] ) );
            mt::utils::addChildComponents( this, tableHeaderData[i].get() );
            tableHeaderData[i]->setValueJustification( juce::Justification::centred );
            tableHeaderData[i]->setFontHeight( 16 );
            tableHeaderData[i]->setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        }

        mt::utils::addChildComponents( this, header, tableView.get(), filterListBox, snapButton, insertButton, deleteButton, trackSelector, tcButton,
                                       selectButton, velDownButton, velUpButton, nudgeLeftButton, nudgeRightButton, playheadButton, playButton );

        header.onBackButtonPressed = [this]( bool ) { closeScreen(); };

        setInterceptsMouseClicks( true, true );

        header.showSettingsButton( true );
        header.onSettingsClick = [&]() {
            gridSettings.setScreen( GridSettings::ListEdit );
            mt::utils::showDialog( "SETTINGS", &gridSettings, [this]( int R ) {

            },
                                   { 0, 0, 650, 520 } );
        };

        filterListBox.setJustificationType( juce::Justification::centredTop, juce::Justification::centred );
        filterListBox.setTitleFontHeight( 16 );
        filterListBox.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
        filterListBox.setBackGroundColours( ui::Theme::backgroundColour( 13 ), ui::Theme::backgroundColour( 2 ) );
        trackSelector.tracks.setBackGroundColours( ui::Theme::backgroundColour( 13 ), ui::Theme::backgroundColour( 2 ) );

        snapButton.setValueColour( ui::Theme::textColour( 1 ) );
        snapButton.setValueFontHeight( 18 );
        deleteButton.setValueColour( ui::Theme::textColour( 1 ) );
        deleteButton.setValueFontHeight( 18 );
        insertButton.setValueColour( ui::Theme::textColour( 1 ) );
        insertButton.setValueFontHeight( 18 );
        tcButton.setValueColour( ui::Theme::textColour( 1 ) );
        tcButton.setValueFontHeight( 18 );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), selectButton, velDownButton, velUpButton, nudgeLeftButton, nudgeRightButton, playheadButton, playButton );
        mt::utils::setValueFontHeight( 18, selectButton, velDownButton, velUpButton, nudgeLeftButton, nudgeRightButton, playheadButton, playButton );

        provider.numRows = [&]() {
            return (int)rows.size();
        };

        // A touch / the Data Dial selects a row. Touch Select (or the hardware Shift) adds / removes rows (group edit);
        // otherwise the touched row is the only selected one.
        provider.onRowSelected = [&]( int Row ) {
            if( !juce::isPositiveAndBelow( Row, (int)rows.size() ) )
                return;
            const bool group = selectButton.getToggleState() || contracts::DclContractsHelper::isShiftPressed();
            setSelection( group ? list_edit_logic::toggleRow( selectedRows, Row ) : std::vector<int>{ Row } );
        };

        provider.cellComponent = [&]( int Row, juce::Component* Existing ) -> juce::Component* {
            if( !juce::isPositiveAndBelow( Row, (int)rows.size() ) )
                return nullptr;
            const auto& r = rows[(size_t)Row];
            if( !r.automation )
            {
                auto vt = noteAt( Row );
                if( !vt.isValid() )
                    return nullptr;
                // An existing component of the other kind is replaced (and deleted) by the table.
                auto* comp = dynamic_cast<ListEntry*>( Existing );
                if( comp == nullptr )
                    comp = new ListEntry( Row, vt );
                comp->setInterceptsMouseClicks( true, true );
                comp->setValues( Row, vt, r.sameTime );
                comp->setPlayhead( Row == playheadRow );
                comp->onPadTapped  = [this]( ListEntry* E ) { editPad( E ); };
                comp->onTimeTapped = [this]( ListEntry* E ) { editTime( E ); };
                return comp;
            }
            if( !juce::isPositiveAndBelow( r.index, (int)autoEvents.size() ) )
                return nullptr;
            auto* comp = dynamic_cast<AutomationEntry*>( Existing );
            if( comp == nullptr )
                comp = new AutomationEntry();
            comp->setInterceptsMouseClicks( true, true );
            const auto& e = autoEvents[(size_t)r.index];
            comp->setValues( Row, e.param, e.tick, e.value, r.sameTime );
            comp->setPlayhead( Row == playheadRow );
            comp->onTimeTapped = [this]( AutomationEntry* E ) { editAutomationTime( E ); };
            comp->onValueEdited = [this]( AutomationEntry* E, int LaneValue ) {
                mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Edit automation value" );
                ae.setAutomationEvent( sel, E->getParam(), E->getTick(), E->getTick(), LaneValue );
            };
            return comp;
        };

        filterListBox.onSelectionChanged = [&]( int Index, const juce::String& ) {
            const auto next = Index == 1 ? list_edit_logic::EventFilter::Notes : Index == 2 ? list_edit_logic::EventFilter::Automation :
                                                                                          list_edit_logic::EventFilter::All;
            if( next == filter )
                return;
            filter = next;
            updateHeaders();
            setSelection( {}, true );
            reloadEvents();
        };

        insertButton.onClick = [&]() {
            auto p = new ListEditInsertDialog( filter == list_edit_logic::EventFilter::Automation );
            mt::utils::showOptions(
                p->getName(), p, [&, p]( int R ) {
                    if( R == UI::APPLY )
                    {
                        auto& data     = p->getData();
                        auto& gridData = GridData::Instance();
                        // BAR / BEAT are 1-based, TICK 0-based: the grid position 1:1:0 is tick 0 of the bar.
                        const int bar0      = juce::jmax( 0, p->getStartBar() - 1 );
                        const int barStart  = (int)gridData.getTicksUptoBar( (unsigned int)bar0 );
                        const int beats     = juce::jmax( 1, gridData.getBeatsPerBar( bar0 ) );
                        const int tpb       = juce::jmax( 1, gridData.getTicksPerBar( bar0 ) / beats );
                        const int start     = list_edit_logic::bbtToTicks( barStart, tpb, { p->getStartBar(), p->getStartBeat(), p->getStartTick() } );
                        const int length    = juce::jmax( 1, gridData.getTicksPerQuantization( bar0 ) );
                        if( data.automation )
                        {
                            bool ok = false;
                            {
                                mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Insert automation" );
                                ok = ae.setAutomationEvent( sel, data.autoParam, -1, start, list_edit_logic::autoFromDisplay( data.autoParam, data.autoShown ) ) == 0;
                            }
                            reloadEvents();
                            if( ok )
                            {
                                for( int i = 0; i < (int)rows.size(); ++i )
                                    if( rows[(size_t)i].automation && autoEvents[(size_t)rows[(size_t)i].index].param == data.autoParam && rows[(size_t)i].tick == start )
                                    {
                                        selectOnly( i );
                                        scrollRowIntoView( i );
                                        break;
                                    }
                                statusBar.offerUndo( "Automation point inserted", [this]() { ae.getUndoManager().undo(); reloadEvents(); } );
                            }
                            else
                                mt::utils::showNotification( "The automation point could not be added (the Pan lane belongs to another program, or the lane is full)" );
                        }
                        else
                        {
                            {
                                mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Insert note" );
                                ae.addOrModifyNote( data.padIndex, start, length );
                            }
                            reloadEvents();
                            // select the inserted note
                            for( int i = 0; i < (int)rows.size(); ++i )
                            {
                                if( rows[(size_t)i].automation )
                                    continue;
                                auto vt = noteAt( i );
                                mt::MTNoteRO n( vt );
                                if( n.number() == data.padIndex && n.start() == start )
                                {
                                    selectOnly( i );
                                    scrollRowIntoView( i );
                                    break;
                                }
                            }
                            statusBar.offerUndo( "Note inserted", [this]() { ae.getUndoManager().undo(); reloadEvents(); } );
                        }
                    }
                    insertButton.setToggleState( false, juce::dontSendNotification );
                    if( p ) delete p;
                } );
        };

        deleteButton.onClick = [&]() {
            deleteButton.setToggleState( false, juce::dontSendNotification ); // always, also when nothing is deleted
            deleteSelection();
        };

        selectButton.onClick  = [&]() { /* the toggle state is read when a row is touched */ };
        velDownButton.onClick = [&]() { adjustSelectionVelocity( -5 ); };
        velUpButton.onClick   = [&]() { adjustSelectionVelocity( 5 ); };
        nudgeLeftButton.onClick  = [&]() { nudgeSelection( -1 ); };
        nudgeRightButton.onClick = [&]() { nudgeSelection( 1 ); };
        playheadButton.onClick   = [&]() { goToPlayhead(); };
        playButton.onClick       = [&]() { playSelectedNote(); };

        trackSelector.tracks.onSelectionChanged = [&]( int Sel, const juce::String& ) {
            ae.setSelectedTrack( Sel );
            noteWatcher.rebind();
            setSelection( {}, false );
            reloadEvents();
        };

        // Undo / redo, live recording and edits made on other screens show up while this screen is open
        // (notes and the track's automation lanes).
        noteWatcher.setWatchAutomation( true );
        noteWatcher.onChanged = [this]() { externalNotesChanged(); };

        tcButton.onClick = [&]() {
            // Only one Timing Correct window at a time: a second one would overwrite the open-window
            // pointer and the first could no longer be closed from the hardware.
            if( ListEditTimingCorrect::isOpen() )
                return;
            ListEditTimingCorrect* pTc = new ListEditTimingCorrect();
            mt::utils::showOptions( "TIMING CORRECT", pTc, [pTc, this]( int R ) {
                if( R == UI::APPLY )
                {
                    ae.setTimingCorrectEnabled( pTc->getGlobalEnabled() );
                    auto tcData = pTc->getData();
                    ae.setTimingCorrectParams( tcData );
                    if( ae.timingCorrect( ae.getSelected(), tcData ) < 0 )
                        mt::utils::showNotification( "Timing correct failed" );
                }
                delete pTc;
            },
                                    { 0, 0, 650, 520 } );
        };
        gridData.gridPlayHeadChanged.connect( [&]() {
            updatePlayheadRow();
            updateViewPosition();
        } );

        ui::themeChanged.connect( [&]() {
            snapButton.setToggleButtonText( "Snap:On", "Snap:Off" );
            deleteButton.setToggleButtonText( "Delete", "Delete" );
            insertButton.setToggleButtonText( "Insert", "Insert" );
            selectButton.setToggleButtonText( "Select:On", "Select:Off" );
            header.setIcon( "Icon_listEdit" );
        } );
    }

    ListEditMode::~ListEditMode() {}

    void ListEditMode::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, tableView->getBounds(), ui::Theme::backgroundColour( 3 ), 5.0f, false, false, true, true );
        mt::utils::fillRoundedRect( g, { 10, 86, 1002, 54 }, ui::Theme::backgroundColour( 1 ), 5.0f, true, true, false, false );
        mt::utils::renderRoundedRect( g, tableView->getBounds(), ui::Theme::backgroundColour( 0 ), 1, 6.f, false, false );
        mt::utils::renderRoundedRect( g, { 10, 86, 1002, 54 }, ui::Theme::backgroundColour( 0 ), 1, 6.f, true, true, false, false );
    }

    void ListEditMode::resized()
    {
        auto b = getLocalBounds();
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );

        auto cntlButtons = b.removeFromBottom( 54 );
        cntlButtons.removeFromRight( 10 ); // For Right Space

        tableView->setBounds( b.withTrimmedTop( 64 ).reduced( 10, 0 ) );

        trackSelector.setBoundsRelative( 0.225f, 0.0267f, 0.1956f, 0.0735f );

        tcButton.setBoundsRelative( 0.7f, 0.031f, 0.048f, 0.0792f );
        filterListBox.setBoundsRelative( 0.624f, 0.029f, 0.0724f, 0.0735f );

        snapButton.setBounds( {} ); // hidden (no function behind it): no space is reserved
        deleteButton.setBounds( cntlButtons.removeFromRight( 128 ).reduced( 4, 10 ) );
        insertButton.setBounds( cntlButtons.removeFromRight( 122 ).reduced( 0, 10 ) );
        // Group edit buttons, from the left.
        cntlButtons.removeFromLeft( 10 );
        selectButton.setBounds( cntlButtons.removeFromLeft( 112 ).reduced( 2, 10 ) );
        velDownButton.setBounds( cntlButtons.removeFromLeft( 82 ).reduced( 2, 10 ) );
        velUpButton.setBounds( cntlButtons.removeFromLeft( 82 ).reduced( 2, 10 ) );
        nudgeLeftButton.setBounds( cntlButtons.removeFromLeft( 106 ).reduced( 2, 10 ) );
        nudgeRightButton.setBounds( cntlButtons.removeFromLeft( 106 ).reduced( 2, 10 ) );
        playheadButton.setBounds( cntlButtons.removeFromLeft( 118 ).reduced( 2, 10 ) );
        playButton.setBounds( cntlButtons.removeFromLeft( 82 ).reduced( 2, 10 ) );

        tableHeaderData[0]->setBoundsRelative( 0.0190f, 0.172f, 0.0587f, 0.0735f );
        tableHeaderData[1]->setBoundsRelative( 0.0975f, 0.172f, 0.1448f, 0.0735f );
        tableHeaderData[2]->setBoundsRelative( 0.2550f, 0.172f, 0.1174f, 0.0735f );
        tableHeaderData[3]->setBoundsRelative( 0.3980f, 0.172f, 0.0782f, 0.0735f );
        tableHeaderData[4]->setBoundsRelative( 0.493f, 0.172f, 0.0782f, 0.0735f );
        tableHeaderData[5]->setBoundsRelative( 0.590f, 0.172f, 0.1448f, 0.0735f );
        tableHeaderData[6]->setBoundsRelative( 0.755f, 0.172f, 0.0587f, 0.0735f );
        tableHeaderData[7]->setBoundsRelative( 0.5879f, 0.172f, 0.0645f, 0.0735f ); // Prob (Mod Type / Value columns are gone)
        tableHeaderData[8]->setBoundsRelative( 0.6640f, 0.172f, 0.0586f, 0.0735f ); // Rat
    }

    // Header texts follow the view: automation rows have a parameter and a value instead of a pad and a velocity.
    void ListEditMode::updateHeaders()
    {
        const bool shown = isVisible();
        using F          = list_edit_logic::EventFilter;
        tableHeaderData[2]->setText( filter == F::Automation ? "Parameter" : filter == F::All ? "Pad / Param" : "Pad/Note", juce::dontSendNotification );
        tableHeaderData[4]->setText( filter == F::Automation ? "Value" : filter == F::All ? "Vel / Value" : "Velocity", juce::dontSendNotification );
        tableHeaderData[3]->setVisible( shown && filter != F::Automation ); // Length
        tableHeaderData[7]->setVisible( shown && filter != F::Automation ); // Prob
        tableHeaderData[8]->setVisible( shown && filter != F::Automation ); // Rat
        tableHeaderData[5]->setVisible( false );                            // Mod Type
        tableHeaderData[6]->setVisible( false );                            // Value (note modifier)
    }

    void ListEditMode::visibilityChanged()
    {
        for( auto it : getChildren() )
            it->setVisible( isVisible() );
        trackSelector.setVisible( isVisible() );
        filterListBox.setVisible( isVisible() );
        snapButton.setVisible( isVisible() && kSnapAvailable );
        updateHeaders();

        if( isVisible() )
        {
            trackSelector.tracks.setSelected( ae.getSelectedTrack() );
            markSelectedRows();
            noteWatcher.setEnabled( true );
        }
        else
        {
            noteWatcher.setEnabled( false );
            noteList = {};
            autoEvents.clear();
            noteChild.clear();
            rows.clear();
            selectedRows.clear();
            playheadRow = -1;
        }
    }

    void ListEditMode::refreshData()
    {
        if( isVisible() )
            tableView->refresh();
    }

    void ListEditMode::updateNoteNaming()
    {
        auto* prog = ae.isProjectLoaded() ? ae.getProgramDataForTrack( sel.sequence, sel.track ) : nullptr;
        ListEntry::useNoteNames = prog != nullptr && prog->type.get() != mt::MTProgramType::Drum && prog->type.get() != mt::MTProgramType::Clip;
    }

    juce::ValueTree ListEditMode::noteAt( int Row ) const
    {
        if( !juce::isPositiveAndBelow( Row, (int)rows.size() ) || rows[(size_t)Row].automation || !noteList.isValid() )
            return {};
        const int idx = rows[(size_t)Row].index;
        if( !juce::isPositiveAndBelow( idx, (int)noteChild.size() ) || !juce::isPositiveAndBelow( noteChild[(size_t)idx], noteList.getNumChildren() ) )
            return {};
        return noteList.getChild( noteChild[(size_t)idx] );
    }

    // Read the track's notes and automation breakpoints and merge them into the rows of the list.
    void ListEditMode::rebuildRows()
    {
        const bool loaded = ae.isProjectLoaded();
        noteList          = loaded ? ae.getNoteList() : juce::ValueTree{};
        autoEvents        = loaded ? ae.getAutomationEvents( ae.getSelected() ) : std::vector<mt::MTAutomationEvent>{};

        std::vector<std::pair<int, int>> byStart; // (start, child index)
        if( noteList.isValid() )
            for( int i = 0; i < noteList.getNumChildren(); ++i )
                byStart.emplace_back( (int)noteList.getChild( i ).getProperty( mt::IDs::start ), i );
        std::stable_sort( byStart.begin(), byStart.end(), []( const auto& a, const auto& b ) { return a.first < b.first; } );
        noteChild.clear();
        std::vector<int> noteTicks, autoTicks;
        for( auto& n : byStart )
        {
            noteChild.push_back( n.second );
            noteTicks.push_back( n.first );
        }
        for( auto& e : autoEvents )
            autoTicks.push_back( e.tick );
        rows = list_edit_logic::mergeEvents( noteTicks, autoTicks, filter );
        updateNoteNaming();
        playheadRow = list_edit_logic::rowAtPlayhead( rows, ae.getSequenceSeekTick( sel.sequence ) );
    }

    // Rebuild the rows and the table; the selected events stay selected (matched by identity, the rows may have moved).
    void ListEditMode::reloadEvents()
    {
        const auto keys = captureSelection();
        rebuildRows();
        restoreSelection( keys );
        if( isVisible() )
            tableView->refresh();
    }

    void ListEditMode::externalNotesChanged()
    {
        if( !isVisible() )
            return;
        // Structure unchanged (a value was edited, an edit was undone): refresh the existing cells in place, no
        // rebuild (a rebuild during a finger drag on a cell would cancel the drag). Else rebuild.
        const auto oldRows = rows;
        const auto keys    = captureSelection();
        rebuildRows();
        bool same = oldRows.size() == rows.size();
        for( size_t i = 0; same && i < rows.size(); ++i )
            same = oldRows[i].automation == rows[i].automation && oldRows[i].tick == rows[i].tick;
        restoreSelection( keys );
        if( !same )
        {
            tableView->refresh();
            return;
        }
        forEachRowIndexed( [this]( int Row, juce::Component* C ) {
            const auto& r = rows[(size_t)Row];
            if( auto* e = dynamic_cast<ListEntry*>( C ) )
            {
                auto vt = noteAt( Row );
                if( vt.isValid() )
                    e->setValues( Row, vt, r.sameTime );
            }
            else if( auto* a = dynamic_cast<AutomationEntry*>( C ) )
            {
                if( r.automation && juce::isPositiveAndBelow( r.index, (int)autoEvents.size() ) )
                {
                    const auto& ev = autoEvents[(size_t)r.index];
                    a->setValues( Row, ev.param, ev.tick, ev.value, r.sameTime );
                }
            }
        } );
    }

    void ListEditMode::forEachRowIndexed( const std::function<void( int, juce::Component* )>& F )
    {
        auto& comps = tableView->getRowComponents();
        for( int i = 0; i < comps.size() && i < (int)rows.size(); ++i )
            if( auto* c = comps[i] )
                F( i, c );
    }

    int ListEditMode::getNumBuiltRows()
    {
        int n = 0;
        for( auto* c : tableView->getRowComponents() )
            n += c != nullptr ? 1 : 0;
        return n;
    }

    std::vector<ListEditMode::SelKey> ListEditMode::captureSelection() const
    {
        std::vector<SelKey> keys;
        for( int row : selectedRows )
        {
            if( !juce::isPositiveAndBelow( row, (int)rows.size() ) )
                continue;
            SelKey k;
            k.automation = rows[(size_t)row].automation;
            if( k.automation )
            {
                if( !juce::isPositiveAndBelow( rows[(size_t)row].index, (int)autoEvents.size() ) )
                    continue;
                k.param = autoEvents[(size_t)rows[(size_t)row].index].param;
                k.tick  = rows[(size_t)row].tick;
            }
            else
            {
                auto vt = noteAt( row );
                if( !vt.isValid() )
                    continue;
                k.id = vt.getProperty( mt::IDs::id );
            }
            keys.push_back( k );
        }
        return keys;
    }

    void ListEditMode::restoreSelection( const std::vector<SelKey>& Keys )
    {
        std::vector<int> found;
        if( !Keys.empty() )
        {
            for( int i = 0; i < (int)rows.size(); ++i )
            {
                const auto& r = rows[(size_t)i];
                for( auto& k : Keys )
                {
                    if( k.automation != r.automation )
                        continue;
                    bool match = false;
                    if( r.automation )
                        match = juce::isPositiveAndBelow( r.index, (int)autoEvents.size() ) && autoEvents[(size_t)r.index].param == k.param && r.tick == k.tick;
                    else
                    {
                        auto vt = noteAt( i );
                        match   = vt.isValid() && (int)vt.getProperty( mt::IDs::id ) == k.id;
                    }
                    if( match )
                    {
                        found.push_back( i );
                        break;
                    }
                }
            }
        }
        setSelection( found, false ); // the engine selection already holds these notes
    }

    // The selection as a whole (sorted rows). SyncEngine: the engine's selected notes follow (Timing Correct "Selected").
    void ListEditMode::setSelection( std::vector<int> Rows, bool SyncEngine )
    {
        std::sort( Rows.begin(), Rows.end() );
        Rows.erase( std::unique( Rows.begin(), Rows.end() ), Rows.end() );
        Rows.erase( std::remove_if( Rows.begin(), Rows.end(), [this]( int r ) { return r < 0 || r >= (int)rows.size(); } ), Rows.end() );
        selectedRows = Rows;
        juce::Array<int> arr;
        for( int r : selectedRows )
            arr.add( r );
        tableView->setSelectedRows( arr );
        if( SyncEngine )
            syncEngineSelection();
    }

    void ListEditMode::selectOnly( int Row )
    {
        setSelection( Row >= 0 ? std::vector<int>{ Row } : std::vector<int>{} );
    }

    // The notes among the selected rows are the engine's selected notes (Timing Correct "Selected", Grid selection).
    void ListEditMode::syncEngineSelection()
    {
        if( !ae.isProjectLoaded() )
            return;
        ae.selectNote( sel, false );
        std::vector<int> ids;
        for( int row : selectedRows )
        {
            auto vt = noteAt( row );
            if( vt.isValid() )
                ids.push_back( (int)vt.getProperty( mt::IDs::id ) );
        }
        if( !ids.empty() )
            ae.selectNote( sel, ids, true );
    }

    void ListEditMode::scrollRowIntoView( int Row, bool Centre )
    {
        if( !juce::isPositiveAndBelow( Row, (int)rows.size() ) )
            return;
        auto&      vp = tableView->getTableViewPort();
        const auto rb = tableView->getRowBounds( Row );
        const auto va = vp.getViewArea();
        if( Centre )
            vp.setViewPosition( va.getX(), juce::jmax( 0, rb.getCentreY() - vp.getHeight() / 2 ) );
        else if( !va.contains( rb ) )
            vp.setViewPosition( va.getX(), rb.getY() < va.getY() ? rb.getY() : juce::jmax( 0, rb.getBottom() - vp.getHeight() ) );
    }

    // The red marker follows the playhead row (the manual's arrow on the left of the list).
    void ListEditMode::updatePlayheadRow()
    {
        if( !isVisible() || rows.empty() )
            return;
        const int now = list_edit_logic::rowAtPlayhead( rows, ae.getSequenceSeekTick( sel.sequence ) );
        if( now == playheadRow )
            return;
        playheadRow = now;
        forEachRowIndexed( [this]( int Row, juce::Component* C ) {
            if( auto* e = dynamic_cast<ListEntry*>( C ) )
                e->setPlayhead( Row == playheadRow );
            else if( auto* a = dynamic_cast<AutomationEntry*>( C ) )
                a->setPlayhead( Row == playheadRow );
        } );
    }

    // "Go to playhead": bring the row at the playhead into view (centred) and mark it.
    void ListEditMode::goToPlayhead()
    {
        const int tick = ae.getSequenceSeekTick( sel.sequence );
        const int row  = list_edit_logic::goToPlayheadRow( rows, tick );
        if( row < 0 )
            return;
        lastTouchMs = 0; // auto scroll may follow again
        updatePlayheadRow();
        scrollRowIntoView( row, true );
    }

    void ListEditMode::deleteSelection()
    {
        const int before = (int)rows.size();
        const auto toDelete = list_edit_logic::rowsToDelete( selectedRows, before ); // highest row first
        if( toDelete.empty() )
            return; // nothing selected: nothing is deleted
        // What each row stands for, read before anything is removed (the rows shift while deleting).
        struct Item { bool automation; int a, b, c; };
        std::vector<Item> items;
        for( int row : toDelete )
        {
            const auto& r = rows[(size_t)row];
            if( r.automation && juce::isPositiveAndBelow( r.index, (int)autoEvents.size() ) )
                items.push_back( { true, autoEvents[(size_t)r.index].param, r.tick, 0 } );
            else if( !r.automation )
            {
                auto vt = noteAt( row );
                if( vt.isValid() )
                {
                    mt::MTNoteRO n( vt );
                    items.push_back( { false, n.number(), n.start(), n.length() } );
                }
            }
        }
        {
            // The selected events, matched exactly (start AND length for notes): never a longer note of an earlier step.
            mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), items.size() > 1 ? "Delete events" : "Delete event" );
            for( auto& it : items )
            {
                if( it.automation )
                    ae.deleteAutomationEvent( sel, it.a, it.b );
                else
                    ae.eraseNote( it.a, it.b, it.c );
            }
        }
        setSelection( {}, false );
        rebuildRows();
        const int next = list_edit_logic::selectionAfterGroupDelete( before, toDelete );
        if( isVisible() )
            tableView->refresh();
        if( next >= 0 )
            selectOnly( next );
        else
            syncEngineSelection();
        statusBar.offerUndo( juce::String( (int)items.size() ) + " event(s) deleted", [this]() { ae.getUndoManager().undo(); reloadEvents(); } );
    }

    // Vel - / Vel +: every selected note's velocity, every selected breakpoint's value; one undo step.
    void ListEditMode::adjustSelectionVelocity( int Delta )
    {
        if( selectedRows.empty() )
            return;
        const auto keys = captureSelection();
        {
            mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Group velocity" );
            for( int row : selectedRows )
            {
                if( !juce::isPositiveAndBelow( row, (int)rows.size() ) )
                    continue;
                const auto& r = rows[(size_t)row];
                if( r.automation )
                {
                    if( !juce::isPositiveAndBelow( r.index, (int)autoEvents.size() ) )
                        continue;
                    const auto& e   = autoEvents[(size_t)r.index];
                    const int   now = list_edit_logic::autoDisplay( e.param, e.value );
                    ae.setAutomationEvent( sel, e.param, e.tick, e.tick, list_edit_logic::autoFromDisplay( e.param, now + Delta ) );
                }
                else
                {
                    auto vt = noteAt( row );
                    if( vt.isValid() )
                        vt.setProperty( mt::IDs::velocity, list_edit_logic::velocityDelta( vt.getProperty( mt::IDs::velocity ), Delta ), &ae.getUndoManager() );
                }
            }
        }
        reloadEvents();
        restoreSelection( keys );
    }

    int ListEditMode::tcStepTicks()
    {
        const auto bnb = ae.toBnB( sel, (unsigned int)juce::jmax( 0, ae.getSequenceSeekTick( sel.sequence ) ) );
        return list_edit_logic::stepRecordLength( ae.getTimingCorrectParams().gridTicks, juce::jmax( 1, gridData.getTicksPerQuantization( bnb.bar ) ) );
    }

    // Play: the selected note sounds once on its track, as long as it is long (50 ms .. 2 s).
    void ListEditMode::playSelectedNote()
    {
        for( int row = selectedRows.empty() ? -1 : selectedRows.back(); row >= 0; row = -1 )
        {
            auto vt = noteAt( row );
            if( !vt.isValid() )
                return; // breakpoints are not played
            mt::MTNoteRO n( vt );
            const auto*  sd     = ae.getSequenceData( sel.sequence );
            const double bpm    = sd != nullptr ? juce::jmax( 20.0, (double)sd->tempo ) : 120.0;
            const int    ms     = juce::jlimit( 50, 2000, juce::roundToInt( n.length() * 60000.0 / ( bpm * mt::defaultPPQN ) ) );
            const int    number = n.number();
            const auto   track  = sel;
            ae.addLiveMidiNote( track, number, juce::jlimit( 1, 127, n.velocity() ) );
            juce::Timer::callAfterDelay( ms, [track, number]() { mt::getAudioInterface().stopLiveMidiNote( track, number ); } );
        }
    }

    // Nudge < / >: the selected events move by one Timing Correct division (one grid step when TC is on BAR).
    void ListEditMode::nudgeSelection( int Direction )
    {
        if( selectedRows.empty() || Direction == 0 )
            return;
        const int  step     = tcStepTicks();
        const int  seqTicks = (int)gridData.getTicksUptoBar( (unsigned int)juce::jmax( 1, ae.getSequenceLength( sel.sequence ) ) );
        auto       keys     = captureSelection();
        {
            mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Nudge events" );
            for( size_t i = 0; i < keys.size(); ++i )
            {
                auto& k = keys[i];
                if( k.automation )
                {
                    const int newTick = list_edit_logic::nudgeStart( k.tick, Direction * step, seqTicks );
                    for( auto& e : autoEvents )
                        if( e.param == k.param && e.tick == k.tick )
                        {
                            if( ae.setAutomationEvent( sel, e.param, e.tick, newTick, e.value ) == 0 )
                                k.tick = newTick;
                            break;
                        }
                }
                else
                {
                    for( int c = 0; c < noteList.getNumChildren(); ++c )
                    {
                        auto vt = noteList.getChild( c );
                        if( (int)vt.getProperty( mt::IDs::id ) == k.id )
                        {
                            const int newStart = list_edit_logic::nudgeStart( vt.getProperty( mt::IDs::start ), Direction * step, seqTicks );
                            ae.editNote( sel, k.id, newStart );
                            break;
                        }
                    }
                }
            }
        }
        rebuildRows();
        restoreSelection( keys );
        if( isVisible() )
            tableView->refresh();
        syncEngineSelection();
    }

    // Step record (the manual's REC + pad in List Edit): with the sequence REC armed and the transport stopped a pad press
    // inserts a note at the current position, one Timing Correct division long; Auto Advance moves the position on.
    bool ListEditMode::stepRecordActive()
    {
        return ae.isProjectLoaded() && ae.isSequenceRecArmed() && !ae.isSequencePlaying();
    }

    void ListEditMode::stepRecord( int Pad, int Velocity )
    {
        const int start = ae.getSequenceSeekTick( sel.sequence );
        if( start < 0 || Pad < 0 || Pad > 127 )
            return;
        const auto bnb      = ae.toBnB( sel, (unsigned int)start );
        const int  length   = list_edit_logic::stepRecordLength( ae.getTimingCorrectParams().gridTicks, juce::jmax( 1, gridData.getTicksPerQuantization( bnb.bar ) ) );
        const int  velocity = list_edit_logic::stepRecordVelocity( Velocity );
        {
            mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Step record" );
            ae.addOrModifyNote( Pad, start, length, velocity );
        }
        if( gridSettings.getAutoAdvance() )
        {
            const int seqTicks = (int)gridData.getTicksUptoBar( (unsigned int)juce::jmax( 1, ae.getSequenceLength( sel.sequence ) ) );
            ae.seekToTick( sel.sequence, list_edit_logic::stepRecordNextTick( start, length, seqTicks ) );
        }
        reloadEvents();
        for( int i = 0; i < (int)rows.size(); ++i )
        {
            if( rows[(size_t)i].automation || rows[(size_t)i].tick != start )
                continue;
            auto vt = noteAt( i );
            if( vt.isValid() && (int)vt.getProperty( mt::IDs::no ) == Pad )
            {
                selectOnly( i );
                scrollRowIntoView( i );
                break;
            }
        }
    }

    void ListEditMode::editTime( ListEntry* Entry )
    {
        if( Entry == nullptr )
            return;
        auto vt = Entry->getValueTree();
        if( !vt.isValid() )
            return;
        const int  start    = vt.getProperty( mt::IDs::start );
        const int  noteId   = vt.getProperty( mt::IDs::id );
        auto&      gd       = GridData::Instance();
        const auto bnb      = ae.toBnB( sel, (unsigned int)start );
        auto*      dlg      = new ListEditTimeDialog( bnb.bar + 1, bnb.beat + 1, bnb.tick, ae.getSequenceLength( sel.sequence ) );
        mt::utils::showOptions( dlg->getName(), dlg, [this, dlg, noteId, &gd]( int R ) {
            if( R == UI::APPLY )
            {
                const int bar0     = juce::jmax( 0, dlg->getBar() - 1 );
                const int beats    = juce::jmax( 1, gd.getBeatsPerBar( bar0 ) );
                const int tpb      = juce::jmax( 1, gd.getTicksPerBar( bar0 ) / beats );
                const int newStart = list_edit_logic::bbtToTicks( (int)gd.getTicksUptoBar( (unsigned int)bar0 ), tpb, { dlg->getBar(), dlg->getBeat(), dlg->getTick() } );
                {
                    mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Edit note time" );
                    ae.editNote( sel, noteId, newStart );
                }
                reloadEvents();
            }
            delete dlg;
        } );
    }

    void ListEditMode::editAutomationTime( AutomationEntry* Entry )
    {
        if( Entry == nullptr )
            return;
        const int  param    = Entry->getParam();
        const int  oldTick  = Entry->getTick();
        const int  value    = Entry->getLaneValue();
        auto&      gd       = GridData::Instance();
        const auto bnb      = ae.toBnB( sel, (unsigned int)oldTick );
        auto*      dlg      = new ListEditTimeDialog( bnb.bar + 1, bnb.beat + 1, bnb.tick, ae.getSequenceLength( sel.sequence ) );
        mt::utils::showOptions( dlg->getName(), dlg, [this, dlg, param, oldTick, value, &gd]( int R ) {
            if( R == UI::APPLY )
            {
                const int bar0     = juce::jmax( 0, dlg->getBar() - 1 );
                const int beats    = juce::jmax( 1, gd.getBeatsPerBar( bar0 ) );
                const int tpb      = juce::jmax( 1, gd.getTicksPerBar( bar0 ) / beats );
                const int newTick  = list_edit_logic::bbtToTicks( (int)gd.getTicksUptoBar( (unsigned int)bar0 ), tpb, { dlg->getBar(), dlg->getBeat(), dlg->getTick() } );
                {
                    mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Edit automation time" );
                    ae.setAutomationEvent( sel, param, oldTick, newTick, value );
                }
                // the moved breakpoint stays selected
                setSelection( {}, false );
                rebuildRows();
                if( isVisible() )
                    tableView->refresh();
                for( int i = 0; i < (int)rows.size(); ++i )
                    if( rows[(size_t)i].automation && autoEvents[(size_t)rows[(size_t)i].index].param == param && rows[(size_t)i].tick == newTick )
                    {
                        selectOnly( i );
                        break;
                    }
            }
            delete dlg;
        } );
    }

    void ListEditMode::editPad( ListEntry* Entry )
    {
        if( Entry == nullptr )
            return;
        auto vt = Entry->getValueTree();
        if( !vt.isValid() )
            return;
        std::vector<juce::String> names;
        for( int n = 0; n < 128; ++n )
            names.push_back( ListEntry::useNoteNames ? juce::String( list_edit_logic::noteName( n ) ) : juce::String( list_edit_logic::padName( n ) ) );
        auto* dlg = new ListEditPadDialog( names, vt.getProperty( mt::IDs::no ) );
        mt::utils::showOptions( dlg->getName(), dlg, [this, dlg, vt]( int R ) mutable {
            if( R == UI::APPLY && vt.isValid() )
            {
                mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Edit note pad" );
                vt.setProperty( mt::IDs::no, list_edit_logic::clampCell( list_edit_logic::Cell::Pad, dlg->getSelected() ), &ae.getUndoManager() );
                externalNotesChanged();
            }
            delete dlg;
        } );
    }

    // Data Dial / +/-: step the selected row (the last selected one), scroll it into view.
    void ListEditMode::stepRowBy( int Delta )
    {
        const int cur    = selectedRows.empty() ? -1 : selectedRows.back();
        const int target = list_edit_logic::stepRow( cur, Delta, (int)rows.size() );
        if( target < 0 )
            return;
        selectOnly( target );
        scrollRowIntoView( target );
    }

    void ListEditMode::mouseDown( const juce::MouseEvent& ) { lastTouchMs = juce::Time::getMillisecondCounter(); }
    void ListEditMode::mouseDrag( const juce::MouseEvent& ) { lastTouchMs = juce::Time::getMillisecondCounter(); }
    void ListEditMode::mouseWheelMove( const juce::MouseEvent&, const juce::MouseWheelDetails& ) { lastTouchMs = juce::Time::getMillisecondCounter(); }

    void ListEditMode::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            const auto padNum = contracts::DclContractsHelper::getPadNumber( f_HwMsg ) + ae.getBankOffset();
            if( contracts::DclContractsHelper::getPadState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED && stepRecordActive() )
                stepRecord( padNum, contracts::DclContractsHelper::getPadVelocity( f_HwMsg ) );
            else if( gridSettings.getHittingPadMode() )
            {
                // Hitting a pad selects all of its note events.
                std::vector<int> hit;
                for( int i = 0; i < (int)rows.size(); ++i )
                {
                    if( rows[(size_t)i].automation )
                        continue;
                    auto vt = noteAt( i );
                    if( vt.isValid() )
                    {
                        mt::MTNoteRO note( vt );
                        if( padNum == note.number() )
                            hit.push_back( i );
                    }
                }
                setSelection( hit );
                if( !hit.empty() )
                    scrollRowIntoView( hit.front() ); // the first matching row
            }
        }
        else if( contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
        {
            auto knob  = contracts::DclContractsHelper::getKnobCode( f_HwMsg );
            auto value = contracts::DclContractsHelper::getKnobValue( f_HwMsg );
            if( knob != contracts::DeviceControlContracts::EN_KNOB_CENTER )
                knobCommand( knob, value );
            else if( value != 0 && contracts::DclContractsHelper::isShiftPressed() )
                nudgeSelection( value ); // Shift + dial: the events move by Timing Correct divisions (the manual)
            else if( value != 0 )
                stepRowBy( value ); // the dial walks the rows (it used to be dropped here)
        }
        else if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && ( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
        {
            auto  buttonCode = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            auto& aeSel      = ae.getSelected();
            switch( buttonCode )
            {
                case contracts::DeviceControlContracts::EN_PLAY: ae.setSequencePlay( aeSel.sequence, false, true ); break;
                case contracts::DeviceControlContracts::EN_PLAYSTART: ae.setSequencePlay( aeSel.sequence, true, true ); break;
                case contracts::DeviceControlContracts::EN_STOP: ae.setSequencePause(); break;
                case contracts::DeviceControlContracts::EN_TC: tcButton.onClick(); break;
            }
        }
        else if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) )
        {
            // +/- and the cursor keys act on release (the bridge delivers them that way).
            switch( contracts::DclContractsHelper::getButtonCode( f_HwMsg ) )
            {
                case contracts::DeviceControlContracts::BtnLedSequence::EN_PLUS:
                case contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_CW: stepRowBy( 1 ); break;
                case contracts::DeviceControlContracts::BtnLedSequence::EN_MINUS:
                case contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_CCW: stepRowBy( -1 ); break;
                default: break;
            }
        }
    }

    // Q-Link 1-4 edit the SELECTED row: Pad, Length, Velocity, Probability (a breakpoint: Q-Link 3 = its value). No
    // selection or no rows: nothing happens.
    void ListEditMode::knobCommand( int knob, int value )
    {
        list_edit_logic::Cell cell;
        if( value == 0 || !list_edit_logic::qlinkCell( knob - contracts::DeviceControlContracts::EN_KNOB_1 + 1, cell ) )
            return;
        const int row = selectedRows.empty() ? -1 : selectedRows.back();
        if( !list_edit_logic::canDeleteRow( (int)rows.size(), row ) )
            return;
        // Shift: Pad moves by a pad bank (16), Length by a Timing Correct division (the manual: Shift + dial / Q-Link).
        if( contracts::DclContractsHelper::isShiftPressed() )
        {
            if( cell == list_edit_logic::Cell::Pad )
                value *= 16;
            else if( cell == list_edit_logic::Cell::Length )
                value *= tcStepTicks();
        }
        scrollRowIntoView( row ); // virtual rows: make sure the row has a component
        auto& rowComps = tableView->getRowComponents();
        if( !juce::isPositiveAndBelow( row, rowComps.size() ) )
            return;
        if( auto* entry = dynamic_cast<ListEntry*>( rowComps[row] ) )
            entry->adjust( cell, value );
        else if( auto* a = dynamic_cast<AutomationEntry*>( rowComps[row] ) )
        {
            if( cell == list_edit_logic::Cell::Velocity )
                a->adjust( value );
        }
    }

    // Opening the screen: the notes the engine has selected (Grid / Step Sequencer selection) are the selected rows.
    void ListEditMode::markSelectedRows()
    {
        rebuildRows();
        std::vector<int> found;
        auto             selectedNotes = ae.getSelectedNoteList();
        for( int i = 0; i < (int)rows.size(); ++i )
        {
            if( rows[(size_t)i].automation )
                continue;
            auto vt = noteAt( i );
            if( !vt.isValid() )
                continue;
            mt::MTNoteRO note( vt );
            for( const auto& it : selectedNotes )
            {
                mt::MTNoteRO selectedNote( it );
                if( selectedNote.number() == note.number() && selectedNote.start() == note.start() )
                {
                    found.push_back( i );
                    break;
                }
            }
        }
        setSelection( found, false );
        tableView->refresh();
    }

    void ListEditMode::updateViewPosition()
    {
        // Follow / Page must not fight a finger that is scrolling or editing.
        if( !list_edit_logic::autoScrollAllowed( juce::Time::getMillisecondCounter(), lastTouchMs ) )
            return;
        if( isVisible() )
        {
            // Total Number of Rows and  Height
            auto totalRows      = tableView->getNumRows();
            auto totalRowhieght = tableView->getRowHeight();
            auto totalHeight    = totalRows * totalRowhieght;

            auto& ViewPort       = tableView->getTableViewPort();
            auto  xPos           = ViewPort.getViewPositionX();
            auto  ViewPortHeight = ViewPort.getHeight();

            auto normized   = ( gridData.trackPlayHead.normized );
            auto scrollType = gridSettings.getAutoScrollType();
            if( scrollType == Follow )
            {
                int scrollY = static_cast<int>( normized * ( totalHeight - ViewPortHeight ) );
                ViewPort.setViewPosition( xPos, scrollY );
                if( normized == 0.0f )
                {
                    ViewPort.setViewPosition( 0, 0 );
                }
            }
            else if( scrollType == Page )
            {
                auto totalPages  = std::ceil( static_cast<float>( totalHeight ) / ViewPortHeight );
                auto currentPage = static_cast<int>( normized * totalPages );

                ViewPort.setViewPosition( xPos, currentPage * ViewPortHeight );

                if( normized == 0.0f )
                {
                    ViewPort.setViewPosition( 0, 0 );
                }
            }
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////
    ///////////////               List Entry
    /////////////////////////////////////////////////////////////////////////////////////

    ListEntry::ListEntry( int Row, juce::ValueTree Vt )
    {
        addMouseListener( this, true );
        mt::utils::addAndMakeVisible( this, row, time, noteLabel, length, velocity, rat, prob );
        row.setValueJustification( juce::Justification::centred );
        time.setValueJustification( juce::Justification::centred );
        length.setValueJustification( juce::Justification::centred );
        velocity.setValueJustification( juce::Justification::centred );
        rat.setValueJustification( juce::Justification::centred );
        prob.setValueJustification( juce::Justification::centred );
        noteLabel.setJustificationType( juce::Justification::centred );
        noteLabel.setEditable( false );

        row.setFontHeight( 16 );
        time.setFontHeight( 16 );
        noteLabel.setFontHeight( 16 );
        length.setValueFontHeight( 16 );
        velocity.setValueFontHeight( 16 );
        rat.setValueFontHeight( 16 );
        prob.setValueFontHeight( 16 );

        prob.setPostFix( "%" );

        row.setTextColour( ui::Theme::textColour( 1 ) );
        time.setTextColour( ui::Theme::textColour( 1 ) );
        noteLabel.setColours( ui::Theme::textColour( 1 ), ui::Theme::backgroundColour( 9 ) );
        length.setColours( ui::Theme::textColour( 1 ), juce::Colours::transparentBlack );
        velocity.setColours( ui::Theme::textColour( 1 ), juce::Colours::transparentBlack );
        prob.setColours( ui::Theme::textColour( 1 ), juce::Colours::transparentBlack );
        rat.setColours( ui::Theme::textColour( 1 ), juce::Colours::transparentBlack );

        row.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        time.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        length.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        velocity.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        prob.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        rat.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        setValues( Row, Vt );

        // Only the user's own edits write to the note (the cells are filled without notification).
        length.onValueChanged   = [&]( int V ) { writeProperty( mt::IDs::length, list_edit_logic::clampCell( list_edit_logic::Cell::Length, V ) ); };
        velocity.onValueChanged = [&]( int V ) { writeProperty( mt::IDs::velocity, list_edit_logic::clampCell( list_edit_logic::Cell::Velocity, V ) ); };
        prob.onValueChanged     = [&]( int V ) { writeProperty( mt::IDs::probability, list_edit_logic::clampCell( list_edit_logic::Cell::Probability, V ) ); };
        rat.onValueChanged      = [&]( int V ) { writeProperty( mt::IDs::ratchet, list_edit_logic::clampCell( list_edit_logic::Cell::Ratchet, V ) ); };
    }

    void ListEntry::writeProperty( const juce::Identifier& Id, int Value )
    {
        if( !vt.isValid() || (int)vt.getProperty( Id ) == Value )
            return;
        auto& um = mt::getAudioInterface().getUndoManager();
        mt::ScopedUndoTransaction undoScope( um, "List edit" );
        vt.setProperty( Id, Value, &um );
        if( onEdited )
            onEdited();
    }

    // Relative edit straight on the note (Q-Link): based on the stored value, so fast turns add up.
    void ListEntry::adjust( list_edit_logic::Cell C, int Delta )
    {
        if( !vt.isValid() || Delta == 0 )
            return;
        const juce::Identifier id = C == list_edit_logic::Cell::Pad ? mt::IDs::no : C == list_edit_logic::Cell::Length ? mt::IDs::length : C == list_edit_logic::Cell::Velocity ? mt::IDs::velocity : C == list_edit_logic::Cell::Probability ? mt::IDs::probability : mt::IDs::ratchet;
        writeProperty( id, list_edit_logic::clampCell( C, (int)vt.getProperty( id ) + Delta ) );
        refreshData();
    }

    void ListEntry::setValues( int Row, juce::ValueTree Vt, bool SameTime )
    {
        vt = Vt;
        row.setText( juce::String( Row + 1 ), juce::dontSendNotification );
        time.setTextColour( ui::Theme::textColour( SameTime ? 2 : 1 ) ); // the same position as the row above: greyed
        refreshData();
    }

    void ListEntry::setPlayhead( bool On )
    {
        if( atPlayhead == On )
            return;
        atPlayhead = On;
        repaint();
    }

    // The manual's red arrow on the left of the list marks the playhead row.
    void ListEntry::paint( juce::Graphics& g )
    {
        if( atPlayhead )
        {
            juce::Path arrow;
            const float cy = getHeight() * 0.5f;
            arrow.addTriangle( 3.0f, cy - 8.0f, 3.0f, cy + 8.0f, 14.0f, cy );
            g.setColour( juce::Colours::red );
            g.fillPath( arrow );
        }
    }

    void ListEntry::setSelected( bool Select )
    {
        isSelected = Select;
        repaint();
    }

    void ListEntry::refreshData()
    {
        if( vt.isValid() )
        {
            auto& ae  = mt::getAudioInterface();
            auto& sel = ae.getSelected();
            using list_edit_logic::Cell;

            int n = vt.getProperty( mt::IDs::no );
            int t = vt.getProperty( mt::IDs::start );
            int l = vt.getProperty( mt::IDs::length );
            int v = vt.getProperty( mt::IDs::velocity );
            int p = vt.getProperty( mt::IDs::probability );
            int r = vt.getProperty( mt::IDs::ratchet );

            auto bnb      = ae.toBnB( sel, (unsigned int)t );
            time.setText( juce::String( list_edit_logic::formatTime( { bnb.bar + 1, bnb.beat + 1, bnb.tick } ) ), juce::dontSendNotification );
            noteLabel.setText( juce::String( useNoteNames ? list_edit_logic::noteName( n ) : list_edit_logic::padName( n ) ), juce::dontSendNotification );
            // No notification: showing a note must never write to it (a stored ratchet 0 used to be rewritten).
            length.setValue( list_edit_logic::displayValue( Cell::Length, l ), juce::dontSendNotification );
            velocity.setValue( list_edit_logic::displayValue( Cell::Velocity, v ), juce::dontSendNotification );
            prob.setValue( list_edit_logic::displayValue( Cell::Probability, p ), juce::dontSendNotification );
            rat.setValue( list_edit_logic::displayValue( Cell::Ratchet, r ), juce::dontSendNotification );
        }
    }

    void ListEntry::mouseUp( const juce::MouseEvent& e )
    {
        if( e.mouseWasDraggedSinceMouseDown() )
            return;
        const auto p = e.getEventRelativeTo( this ).getPosition();
        if( noteLabel.getBounds().contains( p ) && onPadTapped )
            onPadTapped( this );
        else if( time.getBounds().contains( p ) && onTimeTapped )
            onTimeTapped( this );
    }

    juce::Rectangle<int> ListEntry::getElementBounds( int state )
    {
        switch( state )
        {
            case 1: return noteLabel.getBounds(); break;
            case 2: return length.getBounds(); break;
            case 3: return velocity.getBounds(); break;
            case 4: return prob.getBounds(); break;
            default: return noteLabel.getBounds(); break;
        }
    }

    void ListEntry::resized()
    {
        auto b = getLocalBounds();
        row.setBounds( b.removeFromLeft( 69 ).reduced( 8 ) );
        b.removeFromLeft( 10 );
        time.setBounds( b.removeFromLeft( 167 ).reduced( 8 ) );
        noteLabel.setBounds( b.removeFromLeft( 139 ).reduced( 8 ) );
        length.setBounds( b.removeFromLeft( 99 ).reduced( 8 ) );
        velocity.setBounds( b.removeFromLeft( 99 ).reduced( 8 ) );
        prob.setBounds( b.removeFromLeft( 79 ).reduced( 8 ) );
        rat.setBounds( b.removeFromLeft( 70 ).reduced( 8 ) );
    }

    void ListEntry::linkRotated( int ID, int Value )
    {
        using list_edit_logic::Cell;
        switch( ID )
        {
            case 0: adjust( Cell::Pad, Value ); break;
            case 1: adjust( Cell::Length, Value ); break;
            case 2: adjust( Cell::Velocity, Value ); break;
            case 3: adjust( Cell::Probability, Value ); break;
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////
    ///////////////               Automation Entry
    /////////////////////////////////////////////////////////////////////////////////////

    AutomationEntry::AutomationEntry()
    {
        addMouseListener( this, true );
        mt::utils::addAndMakeVisible( this, row, time, paramLabel, value );
        row.setValueJustification( juce::Justification::centred );
        time.setValueJustification( juce::Justification::centred );
        value.setValueJustification( juce::Justification::centred );
        paramLabel.setJustificationType( juce::Justification::centred );
        paramLabel.setEditable( false );

        row.setFontHeight( 16 );
        time.setFontHeight( 16 );
        paramLabel.setFontHeight( 16 );
        value.setValueFontHeight( 16 );

        row.setTextColour( ui::Theme::textColour( 1 ) );
        time.setTextColour( ui::Theme::textColour( 1 ) );
        paramLabel.setColours( ui::Theme::textColour( 1 ), ui::Theme::backgroundColour( 9 ) );
        value.setColours( ui::Theme::textColour( 1 ), juce::Colours::transparentBlack );

        row.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        time.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        value.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        // Only the user's own edit writes (the cell is filled without notification).
        value.onValueChanged = [this]( int Shown ) {
            if( settingValue )
                return;
            const int lane = list_edit_logic::autoFromDisplay( param, Shown );
            if( lane != laneValue && onValueEdited )
                onValueEdited( this, lane );
        };
    }

    void AutomationEntry::setValues( int Row, int Param, int Tick, int LaneValue, bool SameTime )
    {
        auto& ae  = mt::getAudioInterface();
        param     = Param;
        tick      = Tick;
        laneValue = LaneValue;
        row.setText( juce::String( Row + 1 ), juce::dontSendNotification );
        const auto bnb = ae.toBnB( ae.getSelected(), (unsigned int)Tick );
        time.setText( juce::String( list_edit_logic::formatTime( { bnb.bar + 1, bnb.beat + 1, bnb.tick } ) ), juce::dontSendNotification );
        time.setTextColour( ui::Theme::textColour( SameTime ? 2 : 1 ) );
        paramLabel.setText( "TRK " + juce::String( list_edit_logic::autoName( Param ) ), juce::dontSendNotification );
        settingValue = true;
        value.setRange( list_edit_logic::autoMinShown( Param ), list_edit_logic::autoMaxShown( Param ), 1 );
        value.setValue( list_edit_logic::autoDisplay( Param, LaneValue ), juce::dontSendNotification );
        settingValue = false;
    }

    void AutomationEntry::setPlayhead( bool On )
    {
        if( atPlayhead == On )
            return;
        atPlayhead = On;
        repaint();
    }

    void AutomationEntry::adjust( int DeltaShown )
    {
        if( DeltaShown == 0 || !onValueEdited )
            return;
        const int shown = list_edit_logic::autoDisplay( param, laneValue ) + DeltaShown;
        const int lane  = list_edit_logic::autoFromDisplay( param, shown );
        if( lane != laneValue )
            onValueEdited( this, lane );
    }

    void AutomationEntry::paint( juce::Graphics& g )
    {
        if( atPlayhead )
        {
            juce::Path arrow;
            const float cy = getHeight() * 0.5f;
            arrow.addTriangle( 3.0f, cy - 8.0f, 3.0f, cy + 8.0f, 14.0f, cy );
            g.setColour( juce::Colours::red );
            g.fillPath( arrow );
        }
    }

    void AutomationEntry::mouseUp( const juce::MouseEvent& e )
    {
        if( e.mouseWasDraggedSinceMouseDown() )
            return;
        const auto p = e.getEventRelativeTo( this ).getPosition();
        if( time.getBounds().contains( p ) && onTimeTapped )
            onTimeTapped( this );
    }

    void AutomationEntry::resized()
    {
        auto b = getLocalBounds();
        row.setBounds( b.removeFromLeft( 69 ).reduced( 8 ) );
        b.removeFromLeft( 10 );
        time.setBounds( b.removeFromLeft( 167 ).reduced( 8 ) );
        paramLabel.setBounds( b.removeFromLeft( 139 ).reduced( 8 ) );
        b.removeFromLeft( 99 ); // the Length column has no meaning for a breakpoint
        value.setBounds( b.removeFromLeft( 99 ).reduced( 8 ) );
    }

    // "BAR" -> 0 (engine: one bar), "1/N" -> a whole note (4 * defaultPPQN ticks) / N, "1/NT" -> two thirds of that.
    static int timeDivToTicks( const juce::String& Div )
    {
        if( !Div.startsWith( "1/" ) )
            return 0;
        const int n = Div.substring( 2 ).getIntValue();
        if( n <= 0 )
            return 0;
        const int ticks = 4 * mt::defaultPPQN / n;
        return Div.endsWithChar( 'T' ) ? ticks * 2 / 3 : ticks;
    }

    bool ListEditTimingCorrect::getGlobalEnabled()
    {
        return globalTC.getSelected() == 1;
    }

    mt::track::timingCorrectInfo ListEditTimingCorrect::getData()
    {
        mt::track::timingCorrectInfo d;
        d.type        = (mt::track::timingCorrectInfo::Type)juce::jlimit( 0, 3, type.getSelectedIndex() );
        d.events      = (mt::track::timingCorrectInfo::Events)juce::jlimit( 0, 2, events.getSelectedIndex() );
        d.gridTicks   = timeDivToTicks( timeDivList.getSelectedValue() );
        d.swingPct    = (int)swing.getValue();
        d.shiftTicks  = (int)shiftTiming.getValue();
        d.windowPct   = (int)window.getValue();
        d.strengthPct = (int)strength.getValue();
        d.startBar    = (int)startBar.getValue();
        d.endBar      = (int)endBar.getValue();

        auto& ae = mt::getAudioInterface();
        const int seq = ae.getSelectedSequence();
        int num = 4, den = 4;
        ae.getTimeSignature( seq, num, den );
        const int ticksPerBar = juce::jmax( 1, num * 4 * mt::defaultPPQN / juce::jmax( 1, den ) );
        d.rangeStartTicks = juce::jmax( 0, ( d.startBar - 1 ) * ticksPerBar );
        d.rangeEndTicks   = d.endBar * ticksPerBar;
        return d;
    }

    namespace
    {
        ListEditTimingCorrect* g_openTimingCorrect = nullptr; // message thread only
    }

    bool ListEditTimingCorrect::isOpen() { return g_openTimingCorrect != nullptr; }

    bool ListEditTimingCorrect::closeFromHardware( bool apply )
    {
        if( !g_openTimingCorrect ) return false;
        // Close only when the TC's own modal popup is the foremost modal component. A nested list popup (TYPE /
        // EVENTS) is modal on top and re-parents a KickList owned by this dialog; ending the lower modal then would
        // delete the dialog under it (use-after-free when the list popup closes). Then do nothing and return false.
        juce::Component* modalAncestor = nullptr;
        for( auto* c = g_openTimingCorrect->getParentComponent(); c != nullptr; c = c->getParentComponent() )
            if( c->isCurrentlyModal( false ) ) { modalAncestor = c; break; }
        if( modalAncestor == nullptr )
            return false;
        if( !modalAncestor->isCurrentlyModal( true ) )
        {
            // A nested list popup (TYPE / EVENTS / time division) is the foremost modal component: close just that popup
            // (its own selection stays); the Timing Correct dialog underneath stays open for the next press.
            if( auto* top = juce::Component::getCurrentlyModalComponent( 0 ) )
                if( top != modalAncestor && !top->isParentOf( g_openTimingCorrect ) && !g_openTimingCorrect->isParentOf( top ) )
                {
                    top->exitModalState( UI::CANCEL );
                    return true;
                }
            return false;
        }
        mt::utils::exitDialog( g_openTimingCorrect, apply ? UI::APPLY : UI::CANCEL );
        return true;
    }

    ListEditTimingCorrect::~ListEditTimingCorrect()
    {
        if( g_openTimingCorrect == this ) g_openTimingCorrect = nullptr;
    }

    ListEditTimingCorrect::ListEditTimingCorrect()
    {
        g_openTimingCorrect = this;
        mt::utils::addAndMakeVisible( this, globalTC, timeDivisionLabel, timeDivList, triplet, type, events, startBar, endBar, swing, shiftTiming, window, strength );

        mt::utils::setTitleFontHeight( 16, globalTC, type, events, startBar, endBar, swing, shiftTiming, window, strength );
        mt::utils::setValueFontHeight( 16, globalTC, type, events, startBar, endBar, swing, shiftTiming, window, strength, timeDivList );

        mt::utils::setValueBackgroundColour( ui::Theme::backgroundColour( 9 ), timeDivisionLabel );
        mt::utils::setValueJustification( juce::Justification::centred, timeDivisionLabel );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), timeDivisionLabel, startBar, endBar );
        mt::utils::setValueFontHeight( 20, timeDivList, type, events );
        mt::utils::setValueFontHeight( 16, timeDivisionLabel );
        mt::utils::setJustificationType( juce::Justification::centred, timeDivList, type, events );

        mt::utils::setTitleProportion( .36f, globalTC, type, events, startBar, endBar, swing, shiftTiming, window, strength );
        mt::utils::setValueProportion( .36f, swing, shiftTiming, window, strength );
        mt::utils::setTitleJustification( juce::Justification::centredTop, globalTC, type, events, startBar, endBar, swing, shiftTiming, window, strength );
        mt::utils::setValueJustification( juce::Justification::centredBottom, swing, shiftTiming, window, strength );

        swing.showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );
        shiftTiming.showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );
        window.showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );
        strength.showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );

        swing.setRange( 50, 75, 1 );
        shiftTiming.setRange( -48, 48, 1 );
        window.setRange( 0, 100, 1 );
        strength.setRange( 0, 100, 1 );

        window.setValue( 100, juce::dontSendNotification );
        strength.setValue( 100, juce::dontSendNotification );

        // Restore the last values (kept in the engine for the session; also used by RECALL when global TC is on).
        {
            auto& ae = mt::getAudioInterface();
            auto  d  = ae.getTimingCorrectParams();
            globalTC.setSelected( ae.isTimingCorrectEnabled() ? 1 : 0, juce::dontSendNotification );
            type.setSelected( (int)d.type, juce::dontSendNotification );
            events.setSelected( juce::jlimit( 0, 2, (int)d.events ), juce::dontSendNotification );
            startBar.setValue( juce::jmax( 1, d.startBar ), juce::dontSendNotification );
            endBar.setValue( juce::jmax( 1, d.endBar ), juce::dontSendNotification );
            swing.setValue( d.swingPct, juce::dontSendNotification );
            shiftTiming.setValue( d.shiftTicks, juce::dontSendNotification );
            window.setValue( d.windowPct, juce::dontSendNotification );
            strength.setValue( d.strengthPct, juce::dontSendNotification );
            bool gridFound = false;
            for( bool trip : { false, true } )
            {
                auto opts = trip ? CommonUtils::getTripletTimeDivisions() : CommonUtils::getDefaultTimeDivisions();
                for( size_t i = 0; i < opts.size() && !gridFound; ++i )
                    if( timeDivToTicks( opts[i] ) == d.gridTicks )
                    {
                        triplet.setToggleState( trip, juce::dontSendNotification );
                        timeDivList.setListOptions( opts, (int)i );
                        gridFound = true; // first match wins (BAR is 0 ticks in both lists)
                    }
                if( gridFound )
                    break;
            }
        }

        // The global Off/On switch is committed by the caller on Apply only (getGlobalEnabled()), not on selection.

        timeDivList.onSelectionChanged = [&]( int Index, const juce::String& Selected ) {};

        triplet.onClick = [&]() {
            timeDivList.setListOptions( triplet.getToggleState() ? CommonUtils::getTripletTimeDivisions() : CommonUtils::getDefaultTimeDivisions(), timeDivList.getSelectedIndex() );
        };

        type.onSelectionChanged = [&]( int Index, const juce::String& Selected ) {};

        events.onSelectionChanged = [&]( int Index, const juce::String& Selected ) {
            resized();
        };

        swing.onValueChanged = [&]( int Value ) {};

        shiftTiming.onValueChanged = [&]( int Value ) {};

        window.onValueChanged = [&]( int Value ) {};

        strength.onValueChanged = [&]( int Value ) {};
    }

    void ListEditTimingCorrect::resized()
    {
        globalTC.setBoundsRelative( .1246f, .0985f, .3076f, .1674f );
        type.setBoundsRelative( .0892f, .3177f, .3876f, .1674f );
        swing.setBoundsRelative( .0892f, .5344f, .3876f, .1822f );
        window.setBoundsRelative( .0892f, .7487f, .3876f, .1822f );

        timeDivisionLabel.setBoundsRelative( .523f, .0985f, .3876f, .0394f );
        timeDivList.setBoundsRelative( .523f, .1576f, .2553f, .1083f );
        triplet.setBoundsRelative( .7876f, .1576f, .123f, .1083f );

        if( events.getSelectedIndex() == 2 ) // Range
        {
            events.setBoundsRelative( .523f, .3177f, .16f, .1822f );
            startBar.setBoundsRelative( .693f, .3177f, .105f, .1822f );
            endBar.setBoundsRelative( .808f, .3177f, .105f, .1822f );
            startBar.setVisible( true );
            endBar.setVisible( true );
        }
        else
        {
            events.setBoundsRelative( .523f, .3177f, .3876f, .1822f );
            startBar.setVisible( false );
            endBar.setVisible( false );
        }

        shiftTiming.setBoundsRelative( .523f, .5344f, .3876f, .1822f );
        strength.setBoundsRelative( .523f, .7487f, .3876f, .1822f );
    }
}
