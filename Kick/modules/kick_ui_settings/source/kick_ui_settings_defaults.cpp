/*
 * Copyright (c) 2024 MUSIC Tribe
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

#pragma once

#include "kick_ui_settings_defaults.h"

namespace kick_ui
{

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////  SettingsPDefault
    ///////////////////////////////////////////////////////////////////////////////////////////////////////

    SettingsPDefault::SettingsPDefault()
        : SettingsInterface( 5, 4, true )
    {
        ui::KickGrid::setBackgroundColour( juce::Colours::transparentBlack );
        setRowTitles( { "Recent Projects", "Auto Save", "Project Defaults", "Default Project Name", "Cloud & Storage Sources" } );

        asEnabled.setToggleState( settings.projects.isAutoSave(), juce::dontSendNotification );
        settings.setAutoSave( asEnabled.getToggleState() );
        asTimeout.setSelected( settings.projects.saveAfter(), juce::dontSendNotification );

        addComponent( ssRecProjects, 0, 0 );

        addComponent( asEnabled, 1, 0 );
        addComponent( asTimeout, 1, 1 );

        addComponent( pdTempo, 2, 0 );
        addComponent( pdBars, 2, 1 );
        addComponent( pdNum, 2, 2 );
        addComponent( pdDen, 2, 3 );

        addComponent( enableList, 3, 0 );
        addComponent( defaultName, 3, 1 );

        addComponent( toggleSplice, 4, 0 );
        addComponent( toggleGoogleDrive, 4, 1 );
        addComponent( toggleWebDav, 4, 2 );
        addComponent( btnConfigCloud, 4, 3 );

        toggleSplice.onClick = [this]() {
            settings.cloudSources.enableSplice = toggleSplice.getToggleState();
        };
        toggleGoogleDrive.onClick = [this]() {
            settings.cloudSources.enableGoogleDrive = toggleGoogleDrive.getToggleState();
        };
        toggleWebDav.onClick = [this]() {
            settings.cloudSources.enableWebDav = toggleWebDav.getToggleState();
        };
        btnConfigCloud.onClick = [this]() {
            if( onConfigCloudPaths )
                onConfigCloudPaths();
        };

        ssRecProjects.onValueChanged = [this]( int value ) {
            settings.projects.maxRecentProjects = ( value );
        };

        asEnabled.onClick = [this]() {
            settings.setAutoSave( asEnabled.getToggleState() );
            settings.projects.isAutoSave = ( asEnabled.getToggleState() );
        };
        asTimeout.onSelectionChanged = [this]( int index, const juce::String& str ) {
            settings.projects.saveAfter = ( str );
            if( index == 0 || index == 1 )
                settings.setAutoSaveTime( str.getIntValue() * 1000 ); // convert sec to ms
            else
                settings.setAutoSaveTime( str.getIntValue() * 1000 * 60 ); // convert min to ms
        };

        pdTempo.onValueChanged = [this]( int value ) {
            settings.projects.tempo = ( value );
        };

        pdBars.onValueChanged = [this]( int value ) {
            settings.projects.bars = ( value );
        };

        pdNum.onSelectionChanged = [this]( int index, const juce::String& str ) {
            settings.projects.numerator = ( str );
        };
        pdDen.onSelectionChanged = [this]( int index, const juce::String& str ) {
            settings.projects.denominator = ( str );
        };

        enableList.onSelectionChanged = [this]( int index, const juce::String& ) {
            if( index == 0 )
            {
                defaultName.setEditable( false, false );
                settings.projects.defProjectState = ( 0 );
                settings.projects.namingStyle     = ( "DISABLE" );
                settings.projects.namePrefix      = ( "" );
                defaultName.setText( "", juce::dontSendNotification );
            }
            else if( index == 1 )
            {
                juce::Time   currentTime = juce::Time::getCurrentTime();
                juce::String timestamp   = currentTime.formatted( "%d%b%y_%H%M%S_" ).toUpperCase();
                defaultName.setText( timestamp, juce::dontSendNotification );
                defaultName.setEditable( false, false );
                settings.projects.defProjectState = ( 1 );
                settings.projects.namingStyle     = ( "TIME STAMP" );
                settings.projects.namePrefix      = ( timestamp );
            }
            else if( index == 2 )
            {
                defaultName.setEditable( false, true );
                settings.projects.defProjectState = ( 2 );
                settings.projects.namingStyle     = ( "CUSTOM" );
                juce::String currentPrefix = settings.projects.namePrefix();
                if( currentPrefix.isEmpty() )
                {
                    currentPrefix = "KICK_";
                    settings.projects.namePrefix = ( currentPrefix );
                }
                defaultName.setText( currentPrefix, juce::dontSendNotification );
            }
        };

        defaultName.setValueJustification( juce::Justification::centred );
        defaultName.setValueFontHeight( 20 );
        defaultName.onTextChange = [&]() {
            juce::String text = defaultName.getText();
            text = text.replaceCharacter( '/', '_' ).replaceCharacter( '\\', '_' ).replaceCharacter( ':', '_' );
            settings.projects.namePrefix = ( text );
        };
    }

    void SettingsPDefault::refresh()
    {
        ssRecProjects.setValue( settings.projects.maxRecentProjects(), juce::dontSendNotification );
        asEnabled.setToggleState( settings.projects.isAutoSave(), juce::dontSendNotification );
        asTimeout.setSelected( settings.projects.saveAfter(), juce::dontSendNotification );

        pdBars.setValue( settings.projects.bars(), juce::dontSendNotification );
        pdTempo.setValue( settings.projects.tempo(), juce::dontSendNotification );

        pdNum.setSelected( settings.projects.numerator(), juce::dontSendNotification );
        pdDen.setSelected( settings.projects.denominator(), juce::dontSendNotification );

        juce::String namingStyle = settings.projects.namingStyle();
        enableList.setSelected( namingStyle, juce::dontSendNotification );
        defaultName.setText( settings.projects.namePrefix(), juce::dontSendNotification );
        defaultName.setEditable( false, namingStyle == "CUSTOM" );

        toggleSplice.setToggleState( settings.cloudSources.enableSplice.get(), juce::dontSendNotification );
        toggleGoogleDrive.setToggleState( settings.cloudSources.enableGoogleDrive.get(), juce::dontSendNotification );
        toggleWebDav.setToggleState( settings.cloudSources.enableWebDav.get(), juce::dontSendNotification );
    }

    void SettingsPDefault::setupHeader( kick_ui::KickModeHeader& Hdr )
    {
        Hdr.setTitle( "Preferences - Project Defaults" );
    }

    void SettingsPDefault::handleCommand( CPktMgr::HWMsg_t& ) {}
}
