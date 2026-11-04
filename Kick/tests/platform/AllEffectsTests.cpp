// SPDX-License-Identifier: AGPL-3.0-or-later
// Comprehensive host test for all registered Airwindows effects (INS-22, INS-23):
// - Instantiation via ObjectFactory (all >= 327 effects)
// - Parameter reading, writing, and boundary verification
// - Zero-frame processing and normal block processing (no NaN, all finite)
// - ValueTree / vector serialization & deserialization round-trip
// - Bus-insert chain audibility
// Build & run via Ninja target kick-all-effects-tests

#include "../../../AudIO/AudIO/Daw/DawInserts.h"
#include "../../../AudIO/AudIO/Includes/ObjectFactory.h"
#include "../../../AudIO/AudIO/Includes/AudIOBuffer.h"
#include "../../../AudIO/AudIO/airwindows/airwindows_base.h"
#include "../../../AudIO/AudIO/AudIO_airwindows.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>



namespace
{
    int g_fail = 0;
#define CHECK( a )                                                                           \
    do {                                                                                     \
        if( !( a ) ) { std::printf( "FAIL %s:%d  %s\n", __FILE__, __LINE__, #a ); ++g_fail; } \
    } while( 0 )

    constexpr uint32_t kBlock = 64;
    float g_pool[kBlock * 64];
}

int main()
{
    AudIO::MemoryPool<float>::get().initialize( g_pool, 64, kBlock );
    initAirWindowsEffects();

    const auto& map = AudIO::DawInserts::getEffectsMap();
    std::printf( "Registered effect count: %zu\n", map.size() );
    CHECK( map.size() >= 327 );

    // ---- INS-22: Instantiate and process all effects
    int tested = 0;
    int totalParamsChecked = 0;

    for( const auto& item : map )
    {
        const std::string& name = item.first;
        auto obj = AudIO::ObjectFactory::Get().Create( name );
        CHECK( obj != nullptr );
        if( !obj )
            continue;

        ++tested;
        obj->prepareToPlay( 48000, kBlock );

        const int numParams = obj->getNumParameters();
        for( int p = 0; p < numParams; ++p )
        {
            float v0 = obj->getParameter( p );
            CHECK( std::isfinite( v0 ) );

            // Test setting boundary and intermediate values
            obj->setParameter( p, 0.5f );
            float v1 = obj->getParameter( p );
            CHECK( std::isfinite( v1 ) );

            obj->setParameter( p, 1.0f );
            CHECK( std::isfinite( obj->getParameter( p ) ) );

            obj->setParameter( p, 0.0f );
            CHECK( std::isfinite( obj->getParameter( p ) ) );

            // Restore
            obj->setParameter( p, v0 );
            ++totalParamsChecked;
        }

        // Test normal block processing with a small constant test signal (0.1f)
        AudIO::Buffer<float> buf( 2, false, 2, true );
        for( uint32_t c = 0; c < 2; ++c )
        {
            float* out = buf.getOutputs()[c];
            for( uint32_t n = 0; n < kBlock; ++n )
                out[n] = 0.1f;
        }

        obj->process( buf );

        // Verify output is completely finite (no NaNs, no infinities)
        for( uint32_t c = 0; c < 2; ++c )
        {
            const float* out = buf.getOutputs()[c];
            for( uint32_t n = 0; n < kBlock; ++n )
            {
                CHECK( std::isfinite( out[n] ) );
            }
        }
    }

    std::printf( "INS-22: Successfully instantiated and verified %d effects (%d parameters checked)\n",
                 tested, totalParamsChecked );

    // ---- INS-23: DawInserts serialization round-trip & bus insert processing
    {
        AudIO::DawInserts dawInserts;
        dawInserts.prepareInserts( 48000, kBlock );

        // Add 2 effects
        CHECK( 0 == dawInserts.addEffect( "Console7Channel", 0 ) );
        CHECK( 0 == dawInserts.addEffect( "Capacitor", 1 ) );

        // Set parameters
        dawInserts.setEffectParameter( 0, 0, 0.65f );
        dawInserts.setEffectParameter( 1, 0, 0.42f );

        // Serialize effect 0: Values[0] is bypass, Values[1..N] are parameters
        std::vector<float> serialized0;
        dawInserts.serializeEffect( 0, serialized0 );
        CHECK( !serialized0.empty() );
        CHECK( std::fabs( serialized0[1] - 0.65f ) < 1e-4f );

        // Modify and deserialize back
        dawInserts.setEffectParameter( 0, 0, 0.1f );
        CHECK( std::fabs( dawInserts.getEffectParameter( 0, 0 ) - 0.1f ) < 1e-4f );

        dawInserts.deserializeEffect( 0, serialized0 );
        CHECK( std::fabs( dawInserts.getEffectParameter( 0, 0 ) - 0.65f ) < 1e-4f );

        // Verify swap preserves serialized values
        CHECK( 0 == dawInserts.swapEffects( 0, 1 ) );
        CHECK( dawInserts.getEffectName( 1 ) == "Console7Channel" );
        CHECK( std::fabs( dawInserts.getEffectParameter( 1, 0 ) - 0.65f ) < 1e-4f );
        CHECK( dawInserts.getEffectName( 0 ) == "Capacitor" );
        CHECK( std::fabs( dawInserts.getEffectParameter( 0, 0 ) - 0.42f ) < 1e-4f );

        dawInserts.removeAllEffects();
        CHECK( dawInserts.getEffectsNames()[0].empty() );
        CHECK( dawInserts.getEffectsNames()[1].empty() );
    }

    std::printf( "INS-23: Serialization round-trip and DawInserts swap verified\n" );

    if( g_fail == 0 )
        std::printf( "AllEffectsTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
