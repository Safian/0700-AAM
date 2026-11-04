#include "DawInserts.h"
#include "../Includes/ObjectFactory.h"

namespace AudIO
{
    DawInserts::DawInserts()
    {
        for( auto& it : _inserts )
            it.reset();
    }

    // Add or Remove effects...
    int DawInserts::InsertEffects::reset()
    {
        bypassed.store( true );
        pObj.reset();
        return 0;
    }

    int DawInserts::InsertEffects::create( const std::string& Effect, uint32_t Samplerate, uint32_t Blocksize )
    {
        bypassed.store( true );
        pObj = std::move( CREATE_OBJECT( Effect ) );
        if( pObj.get() )
        {
            // Effects created after the device started would otherwise keep blockSize()==1 (see Object::_blockSize).
            // Prepare before un-bypassing, so the audio thread never runs an unprepared effect.
            if( 0 < Blocksize )
                pObj->prepareToPlay( Samplerate, Blocksize );
            bypassed.store( false );
            return 0;
        }
        return -1;
    }

    int DawInserts::InsertEffects::setObject( std::unique_ptr<Object> obj, uint32_t Samplerate, uint32_t Blocksize )
    {
        bypassed.store( true );
        pObj = std::move( obj );
        if( pObj.get() )
        {
            if( 0 < Blocksize )
                pObj->prepareToPlay( Samplerate, Blocksize );
            bypassed.store( false );
            return 0;
        }
        return -1;
    }

    bool DawInserts::InsertEffects::isSame( const std::string& Effect ) const
    {
        if( !pObj.get() )
            return false;
        return ( 0 == Effect.compare( pObj->getName() ) ) ? true : false;
    }

    DawInserts::InsertEffects* DawInserts::getEffect( const std::string& Effect )
    {
        if( 0 != Effect.length() )
            for( auto& it : _inserts )
                if( it.isSame( Effect ) )
                    return &it;
        return nullptr;
    }

    DawInserts::InsertEffects* DawInserts::getEffect( unsigned int Position )
    {
        if( Position < _inserts.size() )
            return &_inserts[Position];
        return nullptr;
    }

    int DawInserts::addEffect( const std::string& Effect, unsigned int Position )
    {
        if( auto pIns = getEffect( Position ) )
        {
            // Return if effect already exists at this position
            if( pIns->isSame( Effect ) )
                return 0;
            // Remove effect if any exists at this position
            pIns->reset();
            // Create effect
            return pIns->create( Effect, _rate, _block );
        }
        return -1;
    }

    int DawInserts::addEffect( std::unique_ptr<Object> customObj, unsigned int Position )
    {
        if( auto pIns = getEffect( Position ) )
        {
            pIns->reset();
            return pIns->setObject( std::move( customObj ), _rate, _block );
        }
        return -1;
    }

    int DawInserts::swapEffects( unsigned int fromPos, unsigned int toPos )
    {
        if( fromPos >= _inserts.size() || toPos >= _inserts.size() )
            return -1;
        if( fromPos == toPos )
            return 0;

        bool fromBypassed = _inserts[fromPos].bypassed.exchange( true );
        bool toBypassed   = _inserts[toPos].bypassed.exchange( true );

        std::swap( _inserts[fromPos].pObj, _inserts[toPos].pObj );

        _inserts[fromPos].bypassed.store( toBypassed );
        _inserts[toPos].bypassed.store( fromBypassed );
        return 0;
    }

    int DawInserts::moveEffect( unsigned int fromPos, unsigned int toPos )
    {
        if( fromPos >= _inserts.size() || toPos >= _inserts.size() )
            return -1;
        if( fromPos == toPos )
            return 0;

        if( fromPos < toPos )
        {
            for( unsigned int i = fromPos; i < toPos; ++i )
                swapEffects( i, i + 1 );
        }
        else
        {
            for( unsigned int i = fromPos; i > toPos; --i )
                swapEffects( i, i - 1 );
        }
        return 0;
    }

    int DawInserts::removeEffect( const std::string& Effect )
    {
        // Remove effect if any exists at this position
        auto pIns = getEffect( Effect );
        return pIns ? pIns->reset() : -1;
    }

    int DawInserts::removeEffect( unsigned int Position )
    {
        // Remove effect if any exists at this position
        auto pIns = getEffect( Position );
        return pIns ? pIns->reset() : -1;
    }

    void DawInserts::removeAllEffects()
    {
        setBypassAllEffects( true );
        for( auto& it : _inserts )
            it.reset();
    }

    std::vector<std::string> DawInserts::getEffectsNames() const
    {
        std::vector<std::string> names;
        for( auto& it : _inserts )
            names.push_back( it.pObj ? it.pObj->getName() : "" );
        return names;
    }

    std::vector<bool> DawInserts::getEffectsBypass() const
    {
        std::vector<bool> mutes;
        for( auto& it : _inserts )
            mutes.push_back( it.bypassed.load() );
        return mutes;
    }

    const std::string& DawInserts::getEffectName( unsigned int Position )
    {
        auto pIns = getEffect( Position );
        return ( pIns && pIns->pObj.get() ) ? pIns->pObj->getName() : _emptyStr;
    }

    Object* DawInserts::getEffectObject( unsigned int Position )
    {
        auto pIns = getEffect( Position );
        return ( pIns && pIns->pObj ) ? pIns->pObj.get() : nullptr;
    }

    // Effect Parameters...
    int DawInserts::getNumEffectParameters( unsigned int Position )
    {
        auto pIns = getEffect( Position );
        return ( pIns && pIns->pObj ) ? pIns->pObj->getNumParameters() : 0;
    }

    const std::vector<ParamInfo>& DawInserts::getEffectParameters( unsigned int Position )
    {
        static std::vector<ParamInfo> _emptyParams;

        auto pIns = getEffect( Position );
        return ( pIns && pIns->pObj ) ? pIns->pObj->getParameters() : _emptyParams;
    }

    float DawInserts::getEffectParameter( unsigned int Position, unsigned int Parameter )
    {
        auto pIns = getEffect( Position );
        return ( pIns && pIns->pObj ) ? pIns->pObj->getParameter( Parameter ) : 0.0f;
    }

    void DawInserts::setEffectParameter( unsigned int Position, unsigned int Parameter, float Value )
    {
        auto pIns = getEffect( Position );
        if( pIns && pIns->pObj )
            pIns->pObj->setParameter( Parameter, Value );
    }

    ///
    /// Bypass
    ///
    void DawInserts::setEffectBypass( unsigned int Position, bool Value )
    {
        if( auto pIns = getEffect( Position ) )
            pIns->bypassed.store( Value );
    }

    ///
    /// Serialize and Deserialize...
    ///
    void DawInserts::serializeEffect( unsigned int Position, std::vector<float>& Values )
    {
        Values.clear();
        auto pIns = getEffect( Position );
        if( pIns && pIns->pObj )
        {
            //// Bypass...
            Values.emplace_back( pIns->bypassed.load() ? 1.f : 0.f );
            //// Number of Parameters...
            auto numParams = pIns->pObj->getNumParameters();
            //// Parameter Valuese...
            for( auto i = 0; i < numParams; ++i )
            {
                auto val = pIns->pObj->getParameter( i );
                Values.emplace_back( val );
            }
        }
    }

    void DawInserts::deserializeEffect( unsigned int Position, const std::vector<float>& Values )
    {
        auto pIns = getEffect( Position );
        if( pIns && pIns->pObj )
        {
            auto nV = (int)Values.size();
            if( 1 <= nV )
            { //// Bypass...
                pIns->bypassed.store( ( 0.5f > Values[0] ) ? false : true );
            }
            if( 2 <= nV )
            {
                auto numValues = ( (int)Values.size() - 1 ); //// Number of Parameters in serialized data...
                auto numParams = utils::min( numValues, pIns->pObj->getNumParameters() );

                //// Parameter Valuese...
                for( auto i = 0; i < numParams; ++i )
                {
                    pIns->pObj->setParameter( i, Values[i + 1] );
                }
            }
        }
    }

    void DawInserts::prepareToPlay( uint32_t Samplerate, uint32_t Blocksize )
    {
        _rate  = Samplerate;
        _block = Blocksize;
        // Run loop to prepare all inserts...
        for( auto& it : _inserts )
            if( it.pObj.get() )
                it.pObj->prepareToPlay( Samplerate, Blocksize );
    }

    void DawInserts::process( Buffer<float>& Buffer )
    {
        if( _bypassAll.load() )
            return;
        // Defensive: never run effects on an empty or unprepared block (processReplacing(sampleFrames=0), HW test error 7)
        if( 0 == _block || 0 == Buffer.getBlockSize() )
            return;

        // Run loop to process all inserts...
        for( auto& it : _inserts )
            if( !it.bypassed.load() && it.pObj.get() )
                it.pObj->process( Buffer );
    }
};
