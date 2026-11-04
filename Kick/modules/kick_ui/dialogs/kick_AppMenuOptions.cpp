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
    AppMenuOptions::AppMenuOptions()
        : KickMenuComponent( "MenuOptions", 4 ), IScreen( Screen::HomeOptions )
    {
        addOption( { "Browser", "Browser" }, "" );
        addOption( { "Track Mixer", "Track Mixer" }, "" );
        addOption( { "Track Mutes", "Track Mutes" }, "" );
        addOption( { "Sequences", "Sequences" }, "" );

        addOption( { "listedit", "listedit" }, "" );
        addOption( { "Padmixer", "Padmixer" }, "" );
        addOption( { "Padmutes", "Padmutes" }, "" );
        addOption( { "songs", "songs" }, "" );

        addOption( { "Coloreditor", "Coloreditor" }, "" );
        addOption( { "Sampler", "Sampler" }, "" );
        addOption( { "Sample editor", "Sample editor" }, "" );
        addOption( { "Program edit", "Program edit" }, "" );

        addOption( { "Padnotes", "Padnotes" }, "" );
        addOption( { "Looper", "Looper" }, "" );
        addOption( { "Qlinks", "Qlinks" }, "" );
        addOption( { "Midicontrol", "Midicontrol" }, "" );

        addOption( { "Keyboard", "Keyboard" }, "" );

        // Commands without a theme icon: text tiles.
        addOption( { "Save", "" }, "" );
        addOption( { "Resources", "" }, "" );
        addOption( { "Kick Update", "" }, "" );

        setVisible( true );
        setSelectionEnabled( false );

        onMenuOptionClicked = [this]( const juce::String& /*Option*/, int Index, const juce::String& /*Group*/ ) {
            loadScreen( Index );
        };
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

    void AppMenuOptions::visibilityChanged()
    {
        KickMenuComponent::visibilityChanged();
        if( !isVisible() )
            return;

        // Tiles of the screens that need a loaded project are grayed out (and ignore touch, dial and pad) without one.
        const bool hasProject = mt::getAudioInterface().isProjectLoaded();
        for( int i = 0; i < ui::menu_logic::kTileCount; ++i )
            setItemEnabled( i, hasProject || !ui::menu_logic::screenNeedsProject( ui::menu_logic::tileScreen<Screen>( i ) ) );
        setItemEnabled( ui::menu_logic::kCommandSave, hasProject );

        // The highlight starts on the mode the Menu was opened from, so the dial and the pads work from there.
        if( const int tile = ui::menu_logic::screenTile( KickHome::Instance().getLastNonMenuScreen() ); tile >= 0 )
            setHighlight( tile );
    }

    void AppMenuOptions::loadScreen( int index )
    {
        switch( index )
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
        const auto target = ui::menu_logic::tileScreen<Screen>( index );
        if( target != Screen::Invalid )
            signals::sigScreenOpen( target );
    }
}
