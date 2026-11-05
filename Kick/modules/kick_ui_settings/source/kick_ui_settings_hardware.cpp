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

#include "kick_ui_settings_hardware.h"

namespace kick_ui
{

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////  SettingsHardware
    ///////////////////////////////////////////////////////////////////////////////////////////////////////

    SettingsHardware::SettingsHardware()
        : SettingsInterface( 5, 3, true )
    {
        ui::KickGrid::setBackgroundColour( juce::Colours::transparentBlack );
        setRowTitles( { "Pads", "Pad Monitor", "Screen", "Date", "Time" } );

        addComponent( padThreshold, 0, 0 );
        addComponent( padSensitivity, 0, 1 );
        addComponent( padCurve, 0, 2 );

        velocity.setRange( 1, 127, 1.0 );
        velocity.showTitle( true, juce::Justification::centred );
        addComponent( velocity, 1, 0 );

        afterTouch.setRange( 1, 127, 1.0 );
        afterTouch.showTitle( true, juce::Justification::centred );
        addComponent( afterTouch, 1, 2 );

        brightness.showTitle( true, juce::Justification::centred );
        brightness.setRange( 0, 100, 1.0 );
        addComponent( brightness, 2, 1 );

        addComponent( date_DD, 3, 0 );
        addComponent( date_MM, 3, 1 );
        addComponent( date_YYYY, 3, 2 );

        addComponent( time_HH, 4, 0 );
        addComponent( time_MM, 4, 1 );
        addComponent( timezone, 4, 2 );

        updateDateTime();
        timezone.setListOptions( CONSTANTS::timeZoneList );

        padThreshold.onSelectionChanged = [this]( int index, const juce::String& str ) {
            settings.hardware.threshold = ( str );
            SendPadSettings();
        };
        padSensitivity.onSelectionChanged = [this]( int index, const juce::String& str ) {
            settings.hardware.sensitivity = ( str );
            SendPadSettings();
        };
        padCurve.onSelectionChanged = [this]( int index, const juce::String& str ) {
            settings.hardware.curve = ( str );
            SendPadSettings();
        };

        brightness.onValueChanged = [this]( double value ) {
            settings.hardware.brightness = ( value );
            // The slider fires on every step of a drag: only the settled value is logged (400 ms after the last change).
            static int logToken = 0;
            const int  token    = ++logToken;
            juce::Timer::callAfterDelay( 400, [token, value = (int)value] {
                if( token == logToken ) kick_log( "info", "[INFO] [BACKLIGHT] %d%% set with the Settings slider\n", value );
            } );
            mt::HwPref::getHwPref().setBrightness( settings.hardware.brightness() );
        };

        date_DD.onValueChanged = [&]( int Value ) {
            setDate();
        };
        date_MM.onValueChanged = [&]( int Value ) {
            int days = mt::HwPref::getHwPref().getDaysInMonth( Value, date_YYYY.getValue() );
            date_DD.setRange( 1, days, 1 );
            if( date_DD.getValue() > days )
                date_DD.setValue( days, juce::dontSendNotification );
            setDate();
        };
        date_YYYY.onValueChanged = [&]( int Value ) {
            int days = mt::HwPref::getHwPref().getDaysInMonth( date_MM.getValue(), Value );
            date_DD.setRange( 1, days, 1 );
            if( date_DD.getValue() > days )
                date_DD.setValue( days, juce::dontSendNotification );
            setDate();
        };
        time_HH.onValueChanged = [&]( int Value ) {
            setTime();
        };
        time_MM.onValueChanged = [&]( int Value ) {
            setTime();
        };
        timezone.onSelectionChanged = [&]( int index, juce::String str ) {
            settings.hardware.timezone = ( str );
            settings.hardware.timeZoneUserSet = true;
            mt::HwPref::getHwPref().setTimeZone( str );
        };

        // Send hardware brightness and timezone at startup
        int value = settings.hardware.brightness();
        kick_log( "info", "[INFO] [BACKLIGHT] %d%% re-applied when the Settings screen was built\n", value );
        mt::HwPref::getHwPref().setBrightness( value );
        juce::String zone = settings.hardware.timezone();
        mt::HwPref::getHwPref().setTimeZone( zone );
        SendPadSettings();
    }

    void SettingsHardware::SendPadSettings( void )
    {
        std::vector<int> padSettingsData;
        padSettingsData.push_back( 16 );
        padSettingsData.push_back( settings.hardware.threshold().getIntValue() );
        padSettingsData.push_back( settings.hardware.sensitivity().getIntValue() );
        padSettingsData.push_back( settings.hardware.curve().getIntValue() );

        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setPadSettingsMessage( padSettingsData ) );
    }

    void SettingsHardware::refresh()
    {
        updateDateTime();
        padThreshold.setSelected( settings.hardware.threshold(), juce::dontSendNotification );
        padSensitivity.setSelected( settings.hardware.sensitivity(), juce::dontSendNotification );
        padCurve.setSelected( settings.hardware.curve(), juce::dontSendNotification );

        brightness.setValue( settings.hardware.brightness(), juce::dontSendNotification );
        timezone.setSelected( settings.hardware.timezone(), juce::dontSendNotification );
    }

    void SettingsHardware::setDate()
    {
        mt::HwPref::getHwPref().setDate( date_DD.getValue(), date_MM.getValue(), date_YYYY.getValue() );
    }
    void SettingsHardware::setTime()
    {
        mt::HwPref::getHwPref().setTime( time_HH.getValue(), time_MM.getValue() );
    }

    void SettingsHardware::updateDateTime()
    {
        mt::timeInfo time;
        mt::dateInfo date;
        mt::HwPref::getHwPref().getDateTime( date, time, settings.hardware.timezone() );
        date_DD.setValue( date.DD, juce::dontSendNotification );
        date_MM.setValue( date.MM, juce::dontSendNotification );
        date_YYYY.setValue( date.YYYY, juce::dontSendNotification );
        time_HH.setValue( time.HH, juce::dontSendNotification );
        time_MM.setValue( time.MM, juce::dontSendNotification );
    }

    void SettingsHardware::setupHeader( kick_ui::KickModeHeader& Hdr )
    {
        Hdr.setTitle( "Preferences - Hardware & Clock" );
    }

    void SettingsHardware::UpdatePadInfo( uint8_t f_velocity, uint8_t f_afterTouch )
    {
        velocity.setValue( f_velocity, juce::dontSendNotification );
        afterTouch.setValue( f_afterTouch, juce::dontSendNotification );
    }

    void SettingsHardware::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            int padNo       = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            int padState    = contracts::DclContractsHelper::getPadState( f_HwMsg );
            int velocity_   = contracts::DclContractsHelper::getPadVelocity( f_HwMsg );
            int afterTouch_ = contracts::DclContractsHelper::getPadAftertouch( f_HwMsg );

            if( padState )
            {
                UpdatePadInfo( velocity_, afterTouch_ );
                Initialvelocity_ = velocity_;
            }
            else
            {
                UpdatePadInfo( Initialvelocity_, 0 );
            }
        }
    }

    void SettingsHardware::visibilityChanged()
    {
        if( isVisible() )
        {
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::enableAfterTouchMessage( true ) );
        }
        else
        {
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::enableAfterTouchMessage( false ) );
        }
    }
}
