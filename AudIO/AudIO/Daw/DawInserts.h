#pragma once

#include "../Includes/AudioDataTypes.h"
#include "../Includes/AudioObject.h"

#include <string>
#include <array>

namespace AudIO
{
    class Design;
    class DawInserts
    {
    public:
        const static int MAX_INSERTS{ 4 };

        explicit DawInserts();
        ~DawInserts() = default;

        // Add or Remove inserts...
        int  addEffect( const std::string& Effect, unsigned int Position );
        int  addEffect( std::unique_ptr<Object> customObj, unsigned int Position );
        int  removeEffect( const std::string& Effect );
        int  removeEffect( unsigned int Position );
        void removeAllEffects();
        int  swapEffects( unsigned int fromPos, unsigned int toPos );
        int  moveEffect( unsigned int fromPos, unsigned int toPos );

        // Effects helpers...
        inline static const std::map<const std::string, ObjectInfo>& getEffectsMap() { return ObjectFactory::Get().GetObjectsInfo(); };

        inline int               getNumEffects() const { return (int)_inserts.size(); }
        std::vector<std::string> getEffectsNames() const;
        std::vector<bool>        getEffectsBypass() const;

        const std::string& getEffectName( unsigned int Position );
        Object*            getEffectObject( unsigned int Position );

        // Effect Parameters...
        int                           getNumEffectParameters( unsigned int Position );
        const std::vector<ParamInfo>& getEffectParameters( unsigned int Position );

        float getEffectParameter( unsigned int Position, unsigned int Parameter );
        void  setEffectParameter( unsigned int Position, unsigned int Parameter, float Value );

        // Bypass...
        inline bool isAllEffectsBypassed() const { return _bypassAll.load(); };
        inline void setBypassAllEffects( bool Value ) { _bypassAll.store( Value ); };

        inline bool isEffectBypassed( unsigned int Position ) const { return ( Position < MAX_INSERTS ) ? _inserts[Position].bypassed.load() : false; };
        void        setEffectBypass( unsigned int Position, bool Value );

        // Serialize and Deserialize...
        void serializeEffect( unsigned int Position, std::vector<float>& Values );
        void deserializeEffect( unsigned int Position, const std::vector<float>& Values );

        // For hosts that run the inserts from outside (a bus runs them as a chain insert, see DawBus.cpp).
        inline void prepareInserts( uint32_t Samplerate, uint32_t Blocksize ) { prepareToPlay( Samplerate, Blocksize ); }
        inline void processInserts( Buffer<float>& Buffer ) { process( Buffer ); }

        AUD_IO_MAKE_NON_COPYABLE( DawInserts );

    protected:
        void prepareToPlay( uint32_t Samplerate, uint32_t Blocksize );
        void process( Buffer<float>& Buffer );

    private:
        // Slot. The UI thread owns pObj (create, replace, delete). The audio thread only reads `live`: a slot is taken out of
        // service (live = nullptr), the audio thread is waited for (waitForAudio) and only then is the object replaced or
        // deleted, so process() can never run on an object that is being destroyed.
        struct InsertEffects
        {
            std::atomic<bool>       bypassed{ true };
            std::atomic<Object*>    live{ nullptr };
            std::unique_ptr<Object> pObj;
            std::string             key;           // registration key the effect was created with (the name Object::getName() reports can differ: "NC-17")
            DawInserts*             owner{ nullptr };

            int  reset();
            int  create( const std::string& Effect, uint32_t Samplerate, uint32_t Blocksize );
            int  setObject( std::unique_ptr<Object> obj, uint32_t Samplerate, uint32_t Blocksize );
            bool isSame( const std::string& Effect ) const;
        };

        // Odd while the audio thread is inside process(): the UI thread waits for it to change before it deletes an object.
        mutable std::atomic<uint32_t>          _epoch{ 0 };
        void                                   waitForAudio() const;

        std::atomic<bool>                      _bypassAll{ false };
        uint32_t                               _rate{ 0 };  // set in prepareToPlay, used for effects created later
        uint32_t                               _block{ 0 };
        std::array<InsertEffects, MAX_INSERTS> _inserts;

        InsertEffects* getEffect( const std::string& Effect );
        InsertEffects* getEffect( unsigned int Position );
    };
};
