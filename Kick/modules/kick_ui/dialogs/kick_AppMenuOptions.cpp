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
        constexpr int kStripHeight = 44; // page arrows / Move / Reset under the tiles
    }

    AppMenuOptions::AppMenuOptions()
        : KickMenuComponent( "MenuOptions", 4 ), IScreen( Screen::HomeOptions )
    {
        loadLayout();
        setMinRows( 4 );
        setBottomReserved( kStripHeight );
        setSelectionEnabled( false );

        for( auto* b : { &prevPage_, &nextPage_, &moveButton_, &resetButton_ } )
        {
            b->setValueColour( ui::Theme::textColour( 2 ) );
            b->setValueFontHeight( 20 );
            b->setBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        }
        pageLabel_.setColours( ui::Theme::textColour( 2 ), juce::Colours::transparentBlack );
        pageLabel_.setFontHeight( 18 );
        pageLabel_.setValueJustification( juce::Justification::centred );
        pageLabel_.setInterceptsMouseClicks( false, false );
        mt::utils::addAndMakeVisible( this, prevPage_, nextPage_, moveButton_, resetButton_, pageLabel_ );

        prevPage_.onClick   = [this]() { showPage( ( page_ + layout_.pageCount() - 1 ) % layout_.pageCount(), 0 ); };
        nextPage_.onClick   = [this]() { showPage( ( page_ + 1 ) % layout_.pageCount(), 0 ); };
        moveButton_.onClick = [this]() { setMoving( !moving_ ); };
        resetButton_.onClick = [this]() { resetLayout(); };

        onMenuOptionClicked = [this]( const juce::String& /*Option*/, int Index, const juce::String& /*Group*/ ) { activate( Index ); };
        onDialStep          = [this]( int delta, bool shift ) { return stepDial( delta, shift ); };
        onDialClick         = [this]( bool shift ) {
            if( !shift )
                return false;
            setMoving( !moving_ ); // Shift + Data Dial press: rearrange the tiles
            return true;
        };
        onHighlightChanged = [this]( int slot ) { announce( slot ); };

        rebuildPage( 0 );
        setVisible( true );
    }

    AppMenuOptions::~AppMenuOptions()
    {
        setVisible( false );
    }

    void AppMenuOptions::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && ( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
        {
            auto  buttonCode = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            auto& ae         = mt::getAudioInterface();
            auto& aeSel      = ae.getSelected();
            switch( buttonCode )
            {
                case contracts::DeviceControlContracts::EN_PLAY: ae.setSequencePlay( aeSel.sequence, false, true ); return;
                case contracts::DeviceControlContracts::EN_PLAYSTART: ae.setSequencePlay( aeSel.sequence, true, true ); return;
                case contracts::DeviceControlContracts::EN_STOP: ae.setSequencePause(); return;
            }
        }
        KickMenuComponent::handleCommand( f_HwMsg );
    }

    bool AppMenuOptions::activateItem( int item )
    {
        const int pos = layout_.positionOf( item );
        if( pos < 0 )
            return false;
        showPage( Layout::pageOfPosition( pos ), Layout::slotOfPosition( pos ) );
        return clickItem( Layout::slotOfPosition( pos ) );
    }

    void AppMenuOptions::resized()
    {
        KickMenuComponent::resized();
        auto strip = getLocalBounds().reduced( 10 ).removeFromBottom( kStripHeight - 4 );
        const bool paged = layout_.pageCount() > 1;
        resetButton_.setBounds( strip.removeFromRight( 150 ) );
        strip.removeFromRight( 8 );
        moveButton_.setBounds( strip.removeFromRight( 150 ) );
        if( paged )
        {
            prevPage_.setBounds( strip.removeFromLeft( 70 ) );
            strip.removeFromLeft( 8 );
            nextPage_.setBounds( strip.removeFromLeft( 70 ) );
            strip.removeFromLeft( 8 );
        }
        pageLabel_.setBounds( strip );
        prevPage_.setVisible( paged );
        nextPage_.setVisible( paged );
    }

    void AppMenuOptions::visibilityChanged()
    {
        KickMenuComponent::visibilityChanged();
        if( !isVisible() )
        {
            moving_ = false;
            carry_  = -1;
            return;
        }

        // The highlight starts on the mode the Menu was opened from, so the dial and the pads work from there.
        int slot = getHighlight();
        if( const int tile = ui::menu_logic::screenTile( KickHome::Instance().getLastNonMenuScreen() ); tile >= 0 )
        {
            if( const int pos = layout_.positionOf( tile ); pos >= 0 )
            {
                showPage( Layout::pageOfPosition( pos ), Layout::slotOfPosition( pos ) );
                return;
            }
        }
        refreshStates();
        setHighlight( slot );
    }

    void AppMenuOptions::rebuildPage( int slot )
    {
        clearOptions();
        page_ = juce::jlimit( 0, layout_.pageCount() - 1, page_ );
        for( int s = 0, n = layout_.pageItems( page_ ); s < n; ++s )
        {
            const auto& def = ui::menu_logic::tileDef<Screen>( layout_.itemAt( page_ * Layout::kPageSize + s ) );
            addOption( { def.name, def.icon }, "" );
        }
        refreshStates();
        resized();
        setHighlight( juce::jlimit( 0, juce::jmax( 0, layout_.pageItems( page_ ) - 1 ), slot ) );
    }

    void AppMenuOptions::rebuildPageAsync( int slot )
    {
        // Called from a tile's own onClick: the tile must not be deleted inside its click handler.
        juce::MessageManager::callAsync( [safe = juce::Component::SafePointer<AppMenuOptions>( this ), slot]() {
            if( safe ) safe->rebuildPage( slot );
        } );
    }

    void AppMenuOptions::refreshStates()
    {
        // Tiles of the screens that need a loaded project are grayed out (and ignore touch, dial and pad) without one.
        // While rearranging every tile is usable.
        const bool hasProject = mt::getAudioInterface().isProjectLoaded();
        for( int s = 0, n = layout_.pageItems( page_ ); s < n; ++s )
        {
            const int item = layout_.itemAt( page_ * Layout::kPageSize + s );
            setItemEnabled( s, moving_ || hasProject || !ui::menu_logic::itemNeedsProject<Screen>( item ) );
        }
        const int pages = layout_.pageCount();
        pageLabel_.setText( moving_ ? juce::String( carry_ >= 0 ? "Choose the new place" : "Choose a tile to move" )
                                    : ( pages > 1 ? "Page " + juce::String( page_ + 1 ) + " / " + juce::String( pages ) : juce::String() ),
                            juce::dontSendNotification );
        moveButton_.setButtonText( moving_ ? "Done" : "Move tiles" );
        moveButton_.setBackgroundColour( moving_ ? ui::Theme::selectionColour( 0 ) : ui::Theme::backgroundColour( 9 ) );
        resetButton_.setEnabled( !layout_.isDefault() );
        resetButton_.setAlpha( layout_.isDefault() ? 0.35f : 1.0f );
    }

    void AppMenuOptions::showPage( int page, int slot )
    {
        page_ = juce::jlimit( 0, layout_.pageCount() - 1, page );
        rebuildPage( slot );
    }

    void AppMenuOptions::announce( int slot )
    {
        const int item = layout_.itemAt( page_ * Layout::kPageSize + slot );
        if( item < 0 )
            return;
        juce::String text = ui::menu_logic::tileDef<Screen>( item ).label;
        if( moving_ )
            text = ( carry_ >= 0 ? "Moving: " : "Move: " ) + text;
        AppStatusbar::Instance().setBoxInfo( text );
    }

    void AppMenuOptions::setMoving( bool on )
    {
        moving_ = on;
        carry_  = -1;
        refreshStates();
        if( moving_ )
            AppStatusbar::Instance().setBoxInfo( "Move tiles: touch or press a tile, then its new place" );
        else
            AppStatusbar::Instance().setBoxInfo( "" );
    }

    bool AppMenuOptions::stepDial( int delta, bool shift )
    {
        const int cur = ( moving_ && carry_ >= 0 ) ? carry_ : page_ * Layout::kPageSize + getHighlight();
        const int pos = ui::menu_logic::stepTile( cur, delta, shift, layout_.itemCount(), 4 );
        if( moving_ && carry_ >= 0 )
        {
            // The picked-up tile travels with the dial (also over the page edge).
            if( layout_.move( carry_, pos ) )
            {
                carry_ = pos;
                saveLayout();
                showPage( Layout::pageOfPosition( pos ), Layout::slotOfPosition( pos ) );
            }
            return true;
        }
        if( Layout::pageOfPosition( pos ) != page_ )
            showPage( Layout::pageOfPosition( pos ), Layout::slotOfPosition( pos ) );
        else
            setHighlight( Layout::slotOfPosition( pos ) );
        return true;
    }

    void AppMenuOptions::activate( int slot )
    {
        const int position = page_ * Layout::kPageSize + slot;
        if( moving_ )
        {
            pickOrDrop( position );
            return;
        }
        if( const int item = layout_.itemAt( position ); item >= 0 )
            runItem( item );
    }

    void AppMenuOptions::pickOrDrop( int position )
    {
        if( carry_ < 0 )
        {
            carry_ = position;
            setHighlight( Layout::slotOfPosition( position ) );
            refreshStates();
            announce( Layout::slotOfPosition( position ) );
            return;
        }
        const int from = carry_;
        carry_         = -1;
        if( layout_.move( from, position ) )
            saveLayout();
        page_ = Layout::pageOfPosition( position );
        refreshStates();
        rebuildPageAsync( Layout::slotOfPosition( position ) );
    }

    void AppMenuOptions::resetLayout()
    {
        layout_.reset();
        saveLayout();
        carry_ = -1;
        page_  = 0;
        rebuildPage( 0 );
        AppStatusbar::Instance().setBoxInfo( "Menu order reset" );
    }

    juce::File AppMenuOptions::layoutFile()
    {
        return getAppDirs( MTPrefs ).getChildFile( "menu_layout.txt" );
    }

    void AppMenuOptions::loadLayout()
    {
        const auto f = layoutFile();
        if( f.existsAsFile() )
            layout_.deserialize( f.loadFileAsString().toStdString() );
    }

    void AppMenuOptions::saveLayout()
    {
        const auto f = layoutFile();
        if( layout_.isDefault() )
        {
            f.deleteFile();
            return;
        }
        f.getParentDirectory().createDirectory();
        if( !f.replaceWithText( juce::String( layout_.serialize() ) ) )
            kick_log( "warn", "[WARN] [MENU] could not store the menu order in %s\n", f.getFullPathName().toRawUTF8() );
    }

    void AppMenuOptions::runItem( int item )
    {
        switch( item )
        {
            case ui::menu_logic::kCommandSave:
            {
                ui::project_actions::saveProject(); // a demo / template opens Save As; a save writes the thumbnail too
                return;
            }
            case ui::menu_logic::kCommandResources: KickModeHeader::showSystemResources(); return;
            case ui::menu_logic::kCommandKickUpdate: signals::sigScreenOpen( Screen::KickUpdate ); return;
            default: break;
        }
        const auto target = ui::menu_logic::tileScreen<Screen>( item );
        if( target != Screen::Invalid )
            signals::sigScreenOpen( target );
    }
}
