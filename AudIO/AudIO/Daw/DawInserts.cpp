#include "DawInserts.h"
#include "../Includes/ObjectFactory.h"

#include <chrono>
#include <thread>

namespace AudIO
{
    DawInserts::DawInserts()
    {
        for( auto& it : _inserts )
        {
            it.owner = this;
            it.reset();
        }
    }

    // Waits (bounded) until the audio thread has left the process() call it was in when this was called. Not inside process():
    // returns at once (device stopped, or between blocks).
    void DawInserts::waitForAudio() const
    {
        const auto e = _epoch.load( std::memory_order_acquire );
        if( 0 == ( e & 1u ) )
            return;
        for( int i = 0; i < 2000 && _epoch.load( std::memory_order_acquire ) == e; ++i )
            std::this_thread::sleep_for( std::chrono::microseconds( 250 ) );
    }

    // Add or Remove effects...
    int DawInserts::InsertEffects::reset()
    {
        bypassed.store( true );
        live.store( nullptr, std::memory_order_release );
        if( owner )
            owner->waitForAudio();
        pObj.reset();
        key.clear();
        return 0;
    }

    int DawInserts::InsertEffects::create( const std::string& Effect, uint32_t Samplerate, uint32_t Blocksize )
    {
        reset();
        auto obj = CREATE_OBJECT( Effect );
        if( obj.get() )
        {
            // Effects created after the device started would otherwise keep blockSize()==1 (see Object::_blockSize).
            // Prepare before it goes into service, so the audio thread never runs an unprepared effect.
            if( 0 < Blocksize )
                obj->prepareToPlay( Samplerate, Blocksize );
            pObj = std::move( obj );
            key  = Effect;
            live.store( pObj.get(), std::memory_order_release );
            bypassed.store( false );
            return 0;
        }
        return -1;
    }

    int DawInserts::InsertEffects::setObject( std::unique_ptr<Object> obj, uint32_t Samplerate, uint32_t Blocksize )
    {
        reset();
        if( obj.get() )
        {
            if( 0 < Blocksize )
                obj->prepareToPlay( Samplerate, Blocksize );
            pObj = std::move( obj );
            key  = pObj->getName();
            live.store( pObj.get(), std::memory_order_release );
            bypassed.store( false );
            return 0;
        }
        return -1;
    }

    bool DawInserts::InsertEffects::isSame( const std::string& Effect ) const
    {
        if( !pObj.get() )
            return false;
        return 0 == Effect.compare( key );
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

        auto& from = _inserts[fromPos];
        auto& to   = _inserts[toPos];

        const bool fromBypassed = from.bypassed.exchange( true );
        const bool toBypassed   = to.bypassed.exchange( true );
        from.live.store( nullptr, std::memory_order_release );
        to.live.store( nullptr, std::memory_order_release );
        waitForAudio(); // nobody is inside either object any more

        std::swap( from.pObj, to.pObj );
        std::swap( from.key, to.key );

        from.live.store( from.pObj.get(), std::memory_order_release );
        to.live.store( to.pObj.get(), std::memory_order_release );
        from.bypassed.store( toBypassed );
        to.bypassed.store( fromBypassed );
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
        setBypassAllEffects( false ); // nothing is left to bypass; an effect added later must run (it stayed bypassed for good)
    }

    std::vector<std::string> DawInserts::getEffectsNames() const
    {
        std::vector<std::string> names;
        for( auto& it : _inserts )
            names.push_back( it.pObj ? it.key : std::string() );
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
        return ( pIns && pIns->pObj.get() ) ? pIns->key : _emptyStr;
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
        // The Airwindows setParameter throws (std::terminate) for an index that does not exist.
        if( pIns && pIns->pObj && static_cast<int>( Parameter ) < pIns->pObj->getNumParameters() )
            pIns->pObj->setParameter( static_cast<int>( Parameter ), Value );
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
        // Marks "inside process()" for waitForAudio(): odd while running, even outside.
        struct EpochScope
        {
            explicit EpochScope( std::atomic<uint32_t>& E ) : e( E ) { e.fetch_add( 1, std::memory_order_acq_rel ); }
            ~EpochScope() { e.fetch_add( 1, std::memory_order_acq_rel ); }
            std::atomic<uint32_t>& e;
        } scope( _epoch );

        if( _bypassAll.load() )
            return;
        // Defensive: never run effects on an empty or unprepared block (processReplacing(sampleFrames=0), HW test error 7), nor on
        // a block larger than the one the effects were prepared for (they would read / write past their buffers).
        if( 0 == _block || 0 == Buffer.getBlockSize() || Buffer.getBlockSize() > _block )
            return;

        // Run loop to process all inserts...
        for( auto& it : _inserts )
        {
            if( it.bypassed.load() )
                continue;
            if( auto* obj = it.live.load( std::memory_order_acquire ) )
                obj->process( Buffer );
        }
    }
};
