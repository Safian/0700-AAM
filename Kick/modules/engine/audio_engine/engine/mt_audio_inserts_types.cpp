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

#include "mt_clap_effect.h"

namespace mt
{

    // The tree is the serialized state of the engine, not an editing model: it is written without an UndoManager (an undo of
    // an insert edit used to change the tree and leave the sound as it was; the next write then overwrote the undo).
    static void insertsSerializeSlot( juce::ValueTree& vt, AudIO::DawInserts* synth, unsigned int i )
    {
        juce::UndoManager* um{ nullptr };
        auto*              obj  = synth->getEffectObject( i );
        auto*              clap = dynamic_cast<ClapEffect*>( obj );
        const auto&        name = synth->getEffectName( i ); // the registration key (never empty for a loaded effect)

        auto childVT = vt.getChild( static_cast<int>( i ) );
        if( !childVT.isValid() )
        {
            childVT = createVTree( IDs::Insert, IDs::name, juce::String( name ) );
            vt.addChild( childVT, static_cast<int>( i ), um );
        }
        else
        {
            childVT.setProperty( IDs::name, juce::String( name ), um );
        }

        if( clap != nullptr )
        {
            childVT.setProperty( "isClap", true, um );
            childVT.setProperty( "pluginId", juce::String( clap->getPluginId() ), um );
            childVT.setProperty( "version", juce::String( clap->getPluginVersion() ), um );
            childVT.setProperty( "path", juce::String( clap->getPluginPath() ), um );
            childVT.setProperty( "missing", clap->isMissing(), um );

            std::vector<uint8_t> blob;
            clap->saveState( blob );
            if( !blob.empty() )
            {
                juce::MemoryBlock mb( blob.data(), blob.size() );
                childVT.setProperty( "stateBlob", mb.toBase64Encoding(), um );
            }
            else
            {
                childVT.removeProperty( "stateBlob", um );
            }
        }
        else
        {
            childVT.setProperty( "isClap", false, um );
            childVT.removeProperty( "pluginId", um );
            childVT.removeProperty( "version", um );
            childVT.removeProperty( "path", um );
            childVT.removeProperty( "missing", um );
            childVT.removeProperty( "stateBlob", um );

            std::vector<float> vals;
            synth->serializeEffect( i, vals );
            childVT.setProperty( IDs::state, VectorToJuceVar<float>( vals ), um );
        }
    }

    static void insertsSerialize( juce::ValueTree& vt, AudIO::DawInserts* synth )
    {
        juce::UndoManager* um{ nullptr };
        if( synth && vt.hasType( IDs::INSERTS ) )
        {
            const auto numIns = static_cast<unsigned int>( synth->getEffectsNames().size() );

            vt.setProperty( IDs::bypass, synth->isAllEffectsBypassed(), um );
            for( unsigned int i = 0; i < numIns; ++i )
                insertsSerializeSlot( vt, synth, i );

            // Remove any excess children in ValueTree
            while( vt.getNumChildren() > static_cast<int>( numIns ) )
                vt.removeChild( vt.getNumChildren() - 1, um );
        }
    }

    static void insertsDeserialize( const juce::ValueTree& vt, AudIO::DawInserts* synth )
    {
        if( synth && vt.hasType( IDs::INSERTS ) )
        {
            auto numIns = vt.getNumChildren();

            synth->setBypassAllEffects( vt.getProperty( IDs::bypass, false ) );

            for( auto i = 0; i < numIns; ++i )
            {
                auto insVT = vt.getChild( i );
                if( insVT.isValid() )
                {
                    auto name  = insVT.getProperty( IDs::name, "" ).toString();
                    bool isClap = insVT.getProperty( "isClap", false );
                    auto pluginId = insVT.getProperty( "pluginId", "" ).toString();

                    if( isClap || pluginId.isNotEmpty() )
                    {
                        auto version = insVT.getProperty( "version", "" ).toString();
                        auto path = insVT.getProperty( "path", "" ).toString();
                        auto stateStr = insVT.getProperty( "stateBlob", "" ).toString();
                        juce::MemoryBlock mb;
                        if( stateStr.isNotEmpty() )
                            mb.fromBase64Encoding( stateStr );
                        std::vector<uint8_t> stateBytes( mb.getSize() );
                        if( !stateBytes.empty() )
                            std::memcpy( stateBytes.data(), mb.getData(), stateBytes.size() );

                        auto clapObj = std::make_unique<ClapEffect>();
                        bool loaded = clapObj->loadPluginById( pluginId.toStdString(), path.toStdString(), 48000.0, 512, stateBytes );
                        if( !loaded )
                        {
                            clapObj->markMissing( pluginId.toStdString(), name.toStdString(), version.toStdString(), path.toStdString(), stateBytes );
                        }
                        synth->addEffect( std::move( clapObj ), static_cast<unsigned int>( i ) );
                    }
                    else
                    {
                        auto state = insVT.getProperty( IDs::state );
                        auto vals  = JuceVarToVector<float>( state );

                        synth->addEffect( name.toStdString(), static_cast<unsigned int>( i ) );
                        synth->deserializeEffect( static_cast<unsigned int>( i ), vals );
                    }
                }
            }
        }
    }

    // Ensure all parameters are correct before calling this API, this API will not check for correctness.
    static void insertsSyncInfo( unsigned int Position, AudIO::DawInserts* synth, MTInsertsInfo& Info )
    {
        Info.name   = synth->getEffectName( Position );
        Info.bypass = synth->isEffectBypassed( Position );

        Info.params.clear();
        const auto& insParams = synth->getEffectParameters( Position );
        for( const auto& it : insParams )
        {
            Info.params.emplace_back( MTInsertsParamInfo{ juce::String( it.name ), juce::String( it.unit ), juce::String( it.displayValue ), it.value, it.isStepped, it.step } );
        }

        auto* obj = synth->getEffectObject( Position );
        if( auto* clap = dynamic_cast<ClapEffect*>( obj ) )
        {
            Info.isClap = true;
            Info.pluginId = juce::String( clap->getPluginId() );
            Info.version = juce::String( clap->getPluginVersion() );
            Info.path = juce::String( clap->getPluginPath() );
            Info.latency = clap->getLatency();
            Info.tail = clap->getTail();
            Info.missing = clap->isMissing();
            Info.statusMessage = juce::String( clap->getStatusMessage() );
            Info.supportsGui = clap->getHost().supportsGui();
        }
        else
        {
            Info.isClap = false;
            Info.pluginId = "";
            Info.version = "";
            Info.path = "";
            Info.latency = 0;
            Info.tail = 0;
            Info.missing = false;
            Info.statusMessage = "";
            Info.supportsGui = false;
        }
    }

    ///
    /// Inserts interface API's
    ///

    struct MTIInsertsImpl
    {
        AudIO::DawInserts*         synth{ nullptr };
        std::vector<MTInsertsInfo> inserts;
    };

    MTIInsertsInterface::MTIInsertsInterface()
        : state( juce::ValueTree{ IDs::INSERTS } )
    {
        pImpl = std::make_unique<MTIInsertsImpl>();
        setup( state, nullptr, nullptr );
    }

    MTIInsertsInterface::~MTIInsertsInterface()
    {
        pImpl.reset();
    }

    void MTIInsertsInterface::setup( juce::ValueTree& VT, juce::UndoManager* UM, AudIO::DawInserts* Impl )
    {
        jassert( VT.hasType( IDs::INSERTS ) );

        state        = VT;
        um           = UM;
        pImpl->synth = Impl;

        if( pImpl->synth )
        {
            const auto numEffects = pImpl->synth->getNumEffects();
            pImpl->inserts.reserve( numEffects );

            pImpl->synth->removeAllEffects();
        }

        insertsDeserialize( state, pImpl->synth );
    }

    void MTIInsertsInterface::getInserts( std::vector<juce::String>& InsertNames )
    {
        InsertNames.clear();
        if( pImpl->synth )
        {
            const auto& effects = pImpl->synth->getEffectsNames();
            for( auto& it : effects )
                InsertNames.emplace_back( juce::String( it ) );
        }
    }

    std::vector<MTInsertsInfo>* MTIInsertsInterface::getInserts()
    {
        if( pImpl->synth )
        {
            const auto numEffects = pImpl->synth->getNumEffects();
            pImpl->inserts.clear();
            for( auto pos = 0; pos < numEffects; ++pos )
                pImpl->inserts.emplace_back( getInsertsInfo( pos ) );
        }
        return &pImpl->inserts;
    }

    MTInsertsInfo MTIInsertsInterface::getInsertsInfo( unsigned int Position )
    {
        MTInsertsInfo info;
        if( pImpl->synth && ( (int)Position < pImpl->synth->getNumEffects() ) )
            insertsSyncInfo( Position, pImpl->synth, info );
        return info;
    }

    void MTIInsertsInterface::setTouchFx( const MTTouchFxSettings& S ) noexcept
    {
        if( !pImpl || !pImpl->synth )
            return;
        AudIO::TouchFx::Settings fx;
        fx.mode      = static_cast<AudIO::TouchFx::Mode>( juce::jlimit( 0, 5, S.mode ) );
        fx.filter    = static_cast<AudIO::TouchFx::FilterType>( juce::jlimit( 0, 2, S.filter ) );
        fx.x         = S.x;
        fx.y         = S.y;
        fx.division  = juce::jlimit( 0, AudIO::TouchFx::kNumDivisions - 1, S.division );
        fx.gate      = S.gate;
        fx.attackMs  = juce::jmax( 0.f, S.attackMs );
        fx.releaseMs = juce::jmax( 0.f, S.releaseMs );
        fx.wet       = juce::jlimit( 0.f, 1.f, S.wet );
        fx.bpm       = S.bpm;
        pImpl->synth->touchFx().set( fx );
    }

    void MTIInsertsInterface::closeTouchFx() noexcept
    {
        if( pImpl && pImpl->synth )
            pImpl->synth->touchFx().close();
    }

    void MTIInsertsInterface::setTouchFxGesture( float X, float Y, bool Gate ) noexcept
    {
        if( pImpl && pImpl->synth )
            pImpl->synth->touchFx().setGesture( juce::jlimit( 0.f, 1.f, X ), juce::jlimit( 0.f, 1.f, Y ), Gate );
    }

    void MTIInsertsInterface::setParameterAutomated( unsigned int Position, int Parameter, float Value ) noexcept
    {
        if( pImpl && pImpl->synth && Parameter >= 0 && static_cast<int>( Position ) < pImpl->synth->getNumEffects() )
            pImpl->synth->setEffectParameter( Position, static_cast<unsigned int>( Parameter ), juce::jlimit( 0.f, 1.f, Value ) );
    }

    void MTIInsertsInterface::restoreInsertFromState( unsigned int Position )
    {
        if( !pImpl || !pImpl->synth || static_cast<int>( Position ) >= pImpl->synth->getNumEffects() || Position >= static_cast<unsigned int>( state.getNumChildren() ) )
            return;
        const auto child = state.getChild( static_cast<int>( Position ) );
        if( child.getProperty( "isClap", false ) )
            return;
        const auto saved = JuceVarToVector<float>( child.getProperty( IDs::state ) );
        if( !saved.empty() )
            pImpl->synth->deserializeEffect( Position, saved );
    }

    bool              MTIInsertsInterface::isAvailable() const noexcept { return pImpl && pImpl->synth != nullptr; }
    int               MTIInsertsInterface::getNumInserts() { return pImpl->synth ? pImpl->synth->getNumEffects() : 0; }
    int               MTIInsertsInterface::getNumParameters( unsigned int Position ) { return pImpl->synth ? pImpl->synth->getNumEffectParameters( Position ) : 0; }
    std::vector<bool> MTIInsertsInterface::getInsertBypassState() { return pImpl->synth ? pImpl->synth->getEffectsBypass() : std::vector<bool>{}; }
    bool              MTIInsertsInterface::isAllInsertsBypassed() { return pImpl->synth ? pImpl->synth->isAllEffectsBypassed() : true; }
    bool              MTIInsertsInterface::isInsertBypassed( unsigned int Position ) { return pImpl->synth ? pImpl->synth->isEffectBypassed( Position ) : true; }

    void MTIInsertsInterface::setAllInsertsBypass( bool Bypass )
    {
        if( pImpl->synth )
        {
            state.setProperty( IDs::bypass, Bypass, nullptr );
            pImpl->synth->setBypassAllEffects( Bypass );
        }
    }

    int MTIInsertsInterface::setInsertBypass( unsigned int Position, bool Bypass )
    {
        int retVal = -1;
        if( pImpl->synth && static_cast<int>( Position ) < pImpl->synth->getNumEffects() )
        {
            pImpl->synth->setEffectBypass( Position, Bypass );
            retVal = 0;
            insertsSerializeSlot( state, pImpl->synth, Position ); // Also, save the slot's latest state to the value tree.
            if( pImpl->inserts.size() != static_cast<size_t>( pImpl->synth->getNumEffects() ) )
                getInserts(); // the cached list is filled lazily: never index it before it exists
            else
                insertsSyncInfo( Position, pImpl->synth, pImpl->inserts[Position] ); // Synchronize as well.
        }
        return retVal;
    }

    int MTIInsertsInterface::addInsert( const juce::String& Effect, unsigned int Position )
    {
        int retVal = -1;
        if( pImpl->synth )
        {
            retVal = ( 0 == pImpl->synth->addEffect( Effect.toStdString(), Position ) ) ? 0 : -1;
            insertsSerialize( state, pImpl->synth ); // writes the slot (name = registration key) into the value tree
            getInserts();
        }
        return retVal;
    }

    int MTIInsertsInterface::removeInsert( const juce::String& Effect )
    {
        int retVal = -1;
        if( pImpl->synth )
        {
            retVal = pImpl->synth->removeEffect( Effect.toStdString() );
            insertsSerialize( state, pImpl->synth ); // Also, save effects latest state to value tree.
            getInserts();
        }
        return retVal;
    }

    int MTIInsertsInterface::removeInsert( unsigned int Position )
    {
        int retVal = -1;
        if( pImpl->synth )
        {
            retVal = pImpl->synth->removeEffect( Position );
            insertsSerialize( state, pImpl->synth ); // Also, save effects latest state to value tree.
            getInserts();
        }
        return retVal;
    }

    void MTIInsertsInterface::removeAllInserts()
    {
        if( pImpl->synth )
        {
            pImpl->synth->removeAllEffects();
            insertsSerialize( state, pImpl->synth ); // Also, save effects latest state to value tree.
            getInserts();
        }
    }

    juce::String MTIInsertsInterface::getInsertName( unsigned int Position )
    {
        return pImpl->synth ? pImpl->synth->getEffectName( Position ) : "";
    }

    juce::String MTIInsertsInterface::getParameterName( unsigned int Position, int Parameter )
    {
        if( pImpl->synth )
        {
            const auto& params = pImpl->synth->getEffectParameters( Position );
            if( Parameter < (int)params.size() )
                return params[Parameter].name;
        }
        return "";
    }

    juce::String MTIInsertsInterface::getParameterValueString( unsigned int Position, int Parameter )
    {
        if( pImpl->synth )
        {
            const auto& params = pImpl->synth->getEffectParameters( Position );
            if( Parameter < (int)params.size() )
                return params[Parameter].displayValue;
        }
        return "";
    }

    float MTIInsertsInterface::getParameterValue( unsigned int Position, int Parameter )
    {
        if( pImpl->synth )
        {
            const auto& params = pImpl->synth->getEffectParameters( Position );
            if( Parameter < (int)params.size() )
                return params[Parameter].value;
        }
        return 0.f;
    }

    void MTIInsertsInterface::setParameterValue( unsigned int Position, int Parameter, float Value )
    {
        if( pImpl->synth && static_cast<int>( Position ) < pImpl->synth->getNumEffects() )
        {
            pImpl->synth->setEffectParameter( Position, Parameter, Value );
            // Only the touched slot is serialized and refreshed (a knob turn used to rewrite all four slots and rebuild the whole list).
            insertsSerializeSlot( state, pImpl->synth, Position );
            if( pImpl->inserts.size() != static_cast<size_t>( pImpl->synth->getNumEffects() ) )
                getInserts();
            else
                insertsSyncInfo( Position, pImpl->synth, pImpl->inserts[Position] );
        }
    }

    int MTIInsertsInterface::swapInserts( unsigned int fromPosition, unsigned int toPosition )
    {
        int retVal = -1;
        if( pImpl->synth )
        {
            retVal = pImpl->synth->swapEffects( fromPosition, toPosition );
            if( retVal == 0 )
            {
                insertsSerialize( state, pImpl->synth );
                getInserts();
            }
        }
        return retVal;
    }

    int MTIInsertsInterface::moveInsert( unsigned int fromPosition, unsigned int toPosition )
    {
        int retVal = -1;
        if( pImpl->synth )
        {
            retVal = pImpl->synth->moveEffect( fromPosition, toPosition );
            if( retVal == 0 )
            {
                insertsSerialize( state, pImpl->synth );
                getInserts();
            }
        }
        return retVal;
    }

    ClapEffect* MTIInsertsInterface::getClapEffect( unsigned int Position )
    {
        if( pImpl->synth )
        {
            return dynamic_cast<ClapEffect*>( pImpl->synth->getEffectObject( Position ) );
        }
        return nullptr;
    }
}
