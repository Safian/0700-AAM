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

#pragma once

namespace kick_ui
{
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////  MasterRouteDisplay
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    class MasterRouteDisplay : public juce::Component
    {
    public:
        ~MasterRouteDisplay() = default;

        MasterRouteDisplay()
        {
            std::vector<juce::String> busNames;
            auto&                     outBusNames = busMgr.getOutputBusNames();
            for( auto& n : outBusNames )
                busNames.push_back( n );
            ioListBox.setListOptions( busNames );

            mt::utils::addChildComponents( this, master, ioListBox, masterInserts, limiter, levelSlider, panSlider, mute, levelMeter, rmsValue, peakValue );
            // The engine has no limiter on the output bus: the button does nothing, so it is not shown.
            limiter.setVisible( false );

            mt::utils::setTitleFontHeight( 16, masterInserts, levelSlider, panSlider, limiter, rmsValue, peakValue );
            mt::utils::setValueFontHeight( 18, masterInserts, levelSlider, panSlider, limiter );
            mt::utils::setValueJustification( juce::Justification::centred, rmsValue, peakValue, ioListBox, master );
            mt::utils::setValueColour( ui::Theme::textColour( 1 ), masterInserts, limiter );
            mt::utils::setValueBackgroundColour( ui::Theme::backgroundColour( 9 ), rmsValue, peakValue );

            master.setValueBackgroundColour( ui::Theme::backgroundColour( 6 ) );
            master.setValueColour( ui::Theme::textColour( 0 ) );
            master.setValueFontHeight( 26 );
            master.setBorderCurves( false, false, false, false );

            ioListBox.setBackGroundColours( ui::Theme::textColour( 4 ), ui::Theme::backgroundColour( 9 ) );

            panSlider.onValueChanged   = [&]( double Value ) { if( pBus) pBus->pan = Value; };
            levelSlider.onValueChanged = [&]( double Value ) { if( pBus) pBus->gain = CommonUtils::midiToGain(Value); };
            mute.onClick               = [&]() { if( pBus) pBus->mute = mute.getToggleState() ; };
            ioListBox.onValueChanged   = [&]( const juce::String& value ) { updateData(); };

            masterInserts.onClick = [&]() {
                if( pBus )
                {
                    kick_ui::signals::onScreenData( Screen::Inserts, (void*)( pBus->getInsertsInterface() ) );
                    kick_ui::signals::sigScreenOpen( kick_ui::Screen::Inserts );
                }
            };

            ui::themeChanged.connect( [&]() {
                mute.setImages( "Mute_normal", "Mute_active" );
            } );
        };

        void meterUpdate()
        {
            //auto& in  = pBus ? pBus->readInMeter() : AudIO::_emptyMeterData;
            auto& out = pBus ? pBus->readOutMeter() : AudIO::_emptyMeterData;
            levelMeter.setSignalStrength( out[0].rms, out[1].rms, out[0].peak, out[1].peak );

            // RMS / PEAK read-out in dB (the labels were never updated).
            const auto rmsText  = "RMS : " + juce::String( mt::mixlogic::formatGainDb( std::max( out[0].rms, out[1].rms ) ) );
            const auto peakText = "PEAK : " + juce::String( mt::mixlogic::formatGainDb( std::max( out[0].peak, out[1].peak ) ) );
            if( rmsText != lastRmsText )
            {
                lastRmsText = rmsText;
                rmsValue.setText( rmsText, juce::dontSendNotification );
            }
            if( peakText != lastPeakText )
            {
                lastPeakText = peakText;
                peakValue.setText( peakText, juce::dontSendNotification );
            }
        }

        // Q-Link 13..16 (simulator) or Shift + Q-Link 1..4 (MPC has four): output, level, pan, mute of the master strip.
        void knobCommand( int knob, int value )
        {
            using K = contracts::DeviceControlContracts::KnobSequence;
            switch( knob )
            {
                case K::EN_KNOB_13: ioListBox.knobRotated( value ); break;
                case K::EN_KNOB_14: levelSlider.knobRotated( mt::mixlogic::qlinkDelta( value, levelSlider.getIncrement() ) ); break;
                case K::EN_KNOB_15: panSlider.knobRotated( mt::mixlogic::qlinkDelta( value, panSlider.getIncrement() ) ); break;
                case K::EN_KNOB_16: qlinkSwitch( mute, value ); break;
                default: break;
            }
        }

    private:
        mt::MTBusManager& busMgr{ mt::getAudioInterface().getBusManager() };
        mt::MTIBus*       pBus{ nullptr };

        ui::KickLabel                    master{ "M" };
        ui::KickLabel                    rmsValue{ "RMS :" };
        ui::KickLabel                    peakValue{ "PEAK :" };
        ui::KickListBox                  ioListBox{ ui::KickListBox::withTitleBar, "OUT", {} };
        ui::KickButton                   masterInserts{ "Inserts", "Inserts" };
        ui::KickButton                   limiter{ "Limiter", "Limiter" };
        ui::KickLevelSlider              levelSlider{ "LEVEL" };
        ui::KickPanRotary                panSlider{ "PAN" };
        ui::KickImageButton              mute{ "MUTE", "Mute_normal", "Mute_active" };
        ui::KickMeterHorizantalWithMarks levelMeter;

        std::shared_ptr<mt::Kick::ScopeTracker> scope;
        juce::String                            lastRmsText, lastPeakText;

        void paint( juce::Graphics& g ) override
        {
            g.fillAll( ui::Theme::backgroundColour( 12 ) );
        }

        void resized() override
        {
            master.setBoundsRelative( .0f, .0f, .0544f, 1.0f );
            ioListBox.setBoundsRelative( .4295f, .1339f, .1634f, .5535f );
            masterInserts.setBoundsRelative( .2563f, .3035f, .0978f, .3928f );
            limiter.setBoundsRelative( .3581f, .3035f, .0489f, .3928f );
            levelSlider.setBoundsRelative( .6125f, .0892f, .1682f, .8214f );
            panSlider.setBoundsRelative( .7954f, .0892f, .0782f, .8214f );
            mute.setBoundsRelative( .9001f, .3035f, .0587f, .3928f );
            levelMeter.setBoundsRelative( .0733f, .3035f, .1584f, .3928f );
            rmsValue.setBoundsRelative( .065f, .7857f, .095f, .1428f );
            peakValue.setBoundsRelative( .162f, .7857f, .095f, .1428f );
        }

        void updateData()
        {
            if( !isVisible() )
                return;

            auto pos = ioListBox.getSelectedIndex();
            pBus     = busMgr.getOutput( pos );

            scope.reset();
            if( pBus )
            {
                pBus->enableOutMetering( true );
                scope = std::make_shared<mt::Kick::ScopeTracker>();

                auto muteValueHandler  = [&]() { mute.setToggleState( pBus ? pBus->mute.get() : false, juce::dontSendNotification ); };
                auto panValueHandler   = [&]() { panSlider.setValue( pBus ? (double)pBus->pan.get() : 0.0, juce::dontSendNotification ); };
                auto levelValueHandler = [&]() { levelSlider.setValue( CommonUtils::gainToMidi( pBus ? (double)pBus->gain.get() : 0.0 ), juce::dontSendNotification ); };

                pBus->mute.onValueChange.connect( muteValueHandler, scope );
                pBus->pan.onValueChange.connect( panValueHandler, scope );
                pBus->gain.onValueChange.connect( levelValueHandler, scope );

                // Force data update.
                pBus->mute.onValueChange();
                pBus->pan.onValueChange();
                pBus->gain.onValueChange();
            }
            else
                clear();
        }

        void clear()
        {
            levelSlider.setValue( 0.0, juce::dontSendNotification );
            panSlider.setValue( 0.0, juce::dontSendNotification );
            mute.setToggleState( false, juce::dontSendNotification );
            ioListBox.setSelected( 0, juce::dontSendNotification );
        }

        void visibilityChanged() override
        {
            bool visible = isVisible();
            for( auto it : getChildren() )
            {
                it->setVisible( visible );
            }
            limiter.setVisible( false );

            updateData();
            if( pBus )
                pBus->enableOutMetering( visible );
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( MasterRouteDisplay )
    };
}
