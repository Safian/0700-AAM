// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test: the inserts of a bus are heard. A constant source is sent to a bus; the bus' DawInserts run as an insert of the
// bus chain (DawInsertsHost, as DawBus does it) and add a constant. Without the host the output is the bare source (INS-01).
// Build: see CMakeLists.txt (links the AudIO core library).
#include "Design.h"
#include "DawInserts.h"
#include "TouchFx.h"
#include "AudIOBuffer.h"
#include "MemoryPool.h"

#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace
{
    int g_fail = 0;
#define CHECK( a )                                                                       \
    do {                                                                                 \
        if( !( a ) ) { std::printf( "FAIL %s:%d  %s\n", __FILE__, __LINE__, #a ); ++g_fail; } \
    } while( 0 )

    class ConstSource : public AudIO::Object
    {
    public:
        explicit ConstSource( float V ) : Object( 0, 2, 0 ), v( V ) {}
        void process( AudIO::Buffer<float>& b ) override
        {
            auto** out = b.getOutputs();
            for( uint32_t c = 0; c < 2; ++c )
                for( uint32_t n = 0; n < b.getBlockSize(); ++n )
                    out[c][n] = v;
        }
        float v;
    };

    // +v, -v, +v ... : the highest frequency there is
    class NyquistSource : public AudIO::Object
    {
    public:
        explicit NyquistSource( float V ) : Object( 0, 2, 0 ), v( V ) {}
        void process( AudIO::Buffer<float>& b ) override
        {
            auto** out = b.getOutputs();
            for( uint32_t c = 0; c < 2; ++c )
                for( uint32_t n = 0; n < b.getBlockSize(); ++n )
                    out[c][n] = ( n & 1u ) ? -v : v;
        }
        float v;
    };

    class AddFx : public AudIO::Object
    {
    public:
        explicit AddFx( float A ) : Object( 2, 2, 0 ), a( A ) { setName( "AddFx" ); }
        void process( AudIO::Buffer<float>& b ) override
        {
            auto** out = b.getOutputs();
            for( uint32_t c = 0; c < 2; ++c )
                for( uint32_t n = 0; n < b.getBlockSize(); ++n )
                    out[c][n] += a;
        }
        float a;
    };

    constexpr uint32_t kBlock = 64;
    float              g_pool[kBlock * 64];

    // Renders blocks until the bus gain / bypass ramps have settled and returns the last sample of the left output.
    float renderOnce( AudIO::Design& d )
    {
        AudIO::Buffer<float> out( 0, false, 2, true );
        for( int i = 0; i < 400; ++i )
            d.process( out );
        return out.getOutputs()[0][kBlock - 1];
    }
}

int main()
{
    AudIO::MemoryPool<float>::get().initialize( g_pool, 64, kBlock );

    AudIO::Design design( 0, 2 );
    const auto    src = design.createObject( std::make_unique<ConstSource>( 0.25f ) );
    const auto    bus = design.createBus();
    design.sendToBus( bus, src, 1.f );
    design.setDesignOutput( bus );

    AudIO::DawInserts inserts;

    // The bus before its inserts are hooked up: the bare source.
    design.prepareToPlay( 48000, kBlock );
    design.setBypass( false );
    const float bare = renderOnce( design );
    CHECK( std::fabs( bare - 0.25f ) < 1e-5f );

    // Hook the inserts up the way DawBus does it.
    const auto hostId = design.createInsert( bus, std::make_unique<AudIO::DawInsertsHost>( inserts ) );
    CHECK( hostId != 0 );
    CHECK( design.addInsert( bus, hostId, 0 ) );
    design.prepareToPlay( 48000, kBlock );

    // No effect loaded: the signal passes unchanged.
    CHECK( std::fabs( renderOnce( design ) - 0.25f ) < 1e-5f );

    // An effect in slot 0: heard.
    CHECK( 0 == inserts.addEffect( std::make_unique<AddFx>( 0.5f ), 0 ) );
    CHECK( std::fabs( renderOnce( design ) - 0.75f ) < 1e-5f );

    // A second one in slot 2: both run, in order.
    CHECK( 0 == inserts.addEffect( std::make_unique<AddFx>( 0.125f ), 2 ) );
    CHECK( std::fabs( renderOnce( design ) - 0.875f ) < 1e-5f );

    // Bypassed: not heard.
    inserts.setEffectBypass( 0, true );
    CHECK( std::fabs( renderOnce( design ) - 0.375f ) < 1e-5f );

    // Removed: gone.
    inserts.removeAllEffects();
    CHECK( std::fabs( renderOnce( design ) - 0.25f ) < 1e-5f );

    // After remove-all a new effect is heard (the master bypass used to stay on).
    CHECK( 0 == inserts.addEffect( std::make_unique<AddFx>( 0.5f ), 1 ) );
    CHECK( std::fabs( renderOnce( design ) - 0.75f ) < 1e-5f );

    // ---- Touch FX of the chain (the XY screen). A second source adds the highest frequency there is (+0.5, -0.5 ...): the last
    // sample of a block (odd index) is 0.25 - 0.5 = -0.25 until a low pass removes the alternating part and leaves the 0.25 DC.
    {
        inserts.removeAllEffects();
        const auto nyq = design.createObject( std::make_unique<NyquistSource>( 0.5f ) );
        design.sendToBus( bus, nyq, 1.f );
        CHECK( std::fabs( renderOnce( design ) - ( -0.25f ) ) < 1e-4f );

        AudIO::TouchFx::Settings fx;
        fx.mode = AudIO::TouchFx::Mode::FilterManual; fx.filter = AudIO::TouchFx::FilterType::LowPass;
        fx.x = 0.3f; fx.y = 0.f; fx.attackMs = 1.f; fx.releaseMs = 1.f; fx.wet = 1.f;
        fx.gate = false;
        inserts.touchFx().set( fx );
        CHECK( std::fabs( renderOnce( design ) - ( -0.25f ) ) < 1e-4f ); // gate closed: untouched

        fx.gate = true;
        inserts.touchFx().set( fx );
        CHECK( std::fabs( renderOnce( design ) - 0.25f ) < 0.03f ); // open: only the 0.25 DC is left

        fx.gate = false;
        inserts.touchFx().set( fx );
        CHECK( std::fabs( renderOnce( design ) - ( -0.25f ) ) < 1e-3f ); // released: back
        design.removeFromBus( bus, nyq );
    }

    // ---- Pre-fader (default) vs Post-fader option
    // Source = 0.25, Insert = +0.5, Bus Gain = 0.5
    // Pre-fader: (0.25 + 0.5) * 0.5 = 0.375
    // Post-fader: (0.25 * 0.5) + 0.5 = 0.625
    {
        inserts.removeAllEffects();
        design.setBypass( false );
        design.setParameter( bus, AudIO::BusOrChain::kParamGain, 0.5f );
        CHECK( 0 == inserts.addEffect( std::make_unique<AddFx>( 0.5f ), 0 ) );

        // Default: pre-fader
        CHECK( std::fabs( design.getParameter( bus, AudIO::BusOrChain::kParamPostFader ) - 0.f ) < 1e-5f );
        CHECK( std::fabs( renderOnce( design ) - 0.375f ) < 1e-4f );

        // Selectable post-fader option
        design.setParameter( bus, AudIO::BusOrChain::kParamPostFader, 1.0f );
        CHECK( std::fabs( design.getParameter( bus, AudIO::BusOrChain::kParamPostFader ) - 1.f ) < 1e-5f );
        CHECK( std::fabs( renderOnce( design ) - 0.625f ) < 1e-4f );

        // Toggle back to pre-fader (default)
        design.setParameter( bus, AudIO::BusOrChain::kParamPostFader, 0.0f );
        CHECK( std::fabs( renderOnce( design ) - 0.375f ) < 1e-4f );

        // Reset gain and remove effect
        design.setParameter( bus, AudIO::BusOrChain::kParamGain, 1.0f );
        inserts.removeAllEffects();
    }

    if( g_fail == 0 )
        std::printf( "BusInsertsTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
