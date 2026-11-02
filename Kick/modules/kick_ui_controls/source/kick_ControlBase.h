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

#include "contracts/dcl/dcl_contracts.h"
#include "contracts/dcl/dcl_contracts_helper.h"

namespace ui
{
    struct KickControlsProcessor
    {
        virtual ~KickControlsProcessor() = default;

        // Deriving class should implement these methods...
        virtual float  getNormalized() { return 0; }
        virtual double getIncrement() { return 1.0; }
        virtual void   setNormalized( float NormalziedValue, juce::NotificationType notification ){};
        virtual void   sliderMoved( float )  = 0;
        virtual void   knobRotated( double ) = 0;
        virtual void   knobClicked()         = 0;
        virtual void   bankPressed( int /*BankIndex*/ ){};
        virtual void   padPressed( int /*PadNo*/, int /*State*/, int /*Velocity*/, int /*Aftertouch*/ ){};

        virtual void handleCommand( CPktMgr::HWMsg_t& f_hwMsg )
        {
#if defined( DEBUG_PRINTS ) && ( DEBUG_PRINTS > 0 )
            DBG( "KickControlsProcessor Handle command" );
#endif
            if( contracts::DclContractsHelper::isButtonMessage( f_hwMsg ) )
            {
                int BtnNum = contracts::DclContractsHelper::getButtonCode( f_hwMsg );
                // A button sends a press and a release; the control steps on the press only (the release used to step it a
                // second time).
                const bool pressed = contracts::DclContractsHelper::getButtonState( f_hwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED;
                switch( BtnNum )
                {
                    case contracts::DeviceControlContracts::BtnLedSequence::EN_PLUS:
                    case contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_CW:
                        if( pressed )
                            knobRotated( getIncrement() );
                        break;

                    case contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_CCW:
                    case contracts::DeviceControlContracts::BtnLedSequence::EN_MINUS:
                        if( pressed )
                            knobRotated( -( getIncrement() ) );
                        break;

                    case contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_ENTER:
                        if( f_hwMsg.data[1] )
                            knobClicked();
                        break;

                    case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_A:
                    case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_B:
                    case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_C:
                    case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_D:
                    case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_E:
                    case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_F:
                    case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_G:
                    case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_H:
                        bankPressed( ( BtnNum ) );
                        break;
                }
            }
            else if( contracts::DclContractsHelper::isSliderMessage( f_hwMsg ) )
            {
                auto sliderValue     = (float)contracts::DclContractsHelper::getSliderValue( f_hwMsg ) / 100.f;
                auto normalizedValue = juce::jlimit( 0.f, 1.0f, sliderValue );
                sliderMoved( normalizedValue );
            }
            else if( contracts::DclContractsHelper::isPadButtonMessage( f_hwMsg ) )
            {
                padPressed( contracts::DclContractsHelper::getPadNumber( f_hwMsg ),
                            contracts::DclContractsHelper::getPadState( f_hwMsg ),
                            contracts::DclContractsHelper::getPadVelocity( f_hwMsg ),
                            contracts::DclContractsHelper::getPadAftertouch( f_hwMsg ) );
            }
            else if( contracts::DclContractsHelper::isKnobMessage( f_hwMsg ) )
            {
                auto knob = contracts::DclContractsHelper::getKnobCode( f_hwMsg );
                switch( knob )
                {
                    default:
                    case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_1:
                    case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_2:
                    case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_3:
                    case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_4:
                    case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_5:
                    case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_6:
                    case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_7:
                    case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_8:
                    case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_9:
                    case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_10:
                    case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_11:
                    case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_12:
                    case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_13:
                    case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_14:
                    case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_15:
                    case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_16:
                        // Nothing to be done for link knobs...
                        break;
                    case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_CENTER:
                        // One data dial tick = one increment of the control, exactly like a +/- press (the MPC manual:
                        // "use the data dial or -/+ buttons to adjust the value"). The raw tick count alone would move a
                        // 0..1 slider with step 0.01 by a whole unit.
                        knobRotated( contracts::DclContractsHelper::getKnobValue( f_hwMsg ) * getIncrement() );
                        break;
                }
            }
        }
    };

    template<typename ComponentType = juce::Component>
    struct KickControlBase
        : public ComponentType
        , public KickControlsProcessor
    {
    public:
        KickControlBase()
        {
            ComponentType::setWantsKeyboardFocus( true );
            ComponentType::setHasFocusOutline( true );
            ComponentType::getProperties().set( "CtrlBase", true );
        }

        // Class Methods...
        inline bool hasSelectionFocus() const { return ComponentType::hasKeyboardFocus(); }
        inline void setSelectionEnabled( bool enabled ) noexcept { ComponentType::setWantsKeyboardFocus( enabled ); }
        inline bool isSelectionEnabled() const noexcept { return ComponentType::getWantsKeyboardFocus(); }

        void setFocus( bool focus = true )
        {
            if( focus && ComponentType::isShowing() )
                ComponentType::grabKeyboardFocus();
            else
                ComponentType::giveAwayKeyboardFocus();
        }
    };

    // The control the data dial / +/- / dial press act on. JUCE keyboard focus is the normal way to select one, but it
    // cannot exist when the window is hidden (no X display on the MPC: Component::isShowing() is false, so
    // grabKeyboardFocus() does nothing) and the touch is delivered straight to mouseDown(). So the last touched control is
    // remembered here and getComponentInFocus() falls back to it. One slot for the whole app (inline function, one
    // definition across translation units).
    inline juce::Component::SafePointer<juce::Component>& selectedControlSlot()
    {
        static juce::Component::SafePointer<juce::Component> slot;
        return slot;
    }

    // True if the component and all its parents up to (not including) the top-level window are visible. isShowing() is
    // false for the whole tree of a hidden window, and the MPC runs with an intentionally hidden top-level window (no X
    // display), so the window's own visibility is not part of the test.
    inline bool isVisibleInTree( const juce::Component* c )
    {
        for( ; c != nullptr && c->getParentComponent() != nullptr; c = c->getParentComponent() )
            if( !c->isVisible() )
                return false;
        return c != nullptr;
    }

    // Select the control under a touch: the nearest parent (or the component itself) that is a selectable Kick control.
    // A touch that lands on no control clears the selection, so the screens keep their own dial behaviour then.
    inline void selectControlFromTouch( juce::Component* touched )
    {
        for( auto* c = touched; c != nullptr; c = c->getParentComponent() )
        {
            if( c->getProperties().contains( "CtrlBase" ) && c->getWantsKeyboardFocus() && dynamic_cast<KickControlsProcessor*>( c ) )
            {
                selectedControlSlot() = c;
                return;
            }
        }
        selectedControlSlot() = nullptr;
    }

    inline void clearSelectedControl() { selectedControlSlot() = nullptr; }

    inline juce::Component* getSelectedControl()
    {
        auto* c = selectedControlSlot().getComponent();
        return ( c != nullptr && isVisibleInTree( c ) ) ? c : nullptr;
    }

    static inline KickControlsProcessor* getComponentInFocus()
    {
        if( auto* f = dynamic_cast<KickControlsProcessor*>( juce::Component::getCurrentlyFocusedComponent() ) )
            return f;
        return dynamic_cast<KickControlsProcessor*>( getSelectedControl() );
    };

    static inline void clearFocusComponent()
    {
        juce::Component::unfocusAllComponents();
        clearSelectedControl();
    }
}
