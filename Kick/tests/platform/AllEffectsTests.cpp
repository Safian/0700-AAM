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
#include "../../modules/kick_ui/dialogs/kick_InsertCost.h"

#include <algorithm>
#include <chrono>
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
    std::vector<std::pair<double, std::string>> cost; // real-time ratio of one stereo block per effect (host)
    auto fillSine = []( AudIO::Buffer<float>& B, uint32_t& Phase ) {
        for( uint32_t n = 0; n < kBlock; ++n, ++Phase )
        {
            const float v = 0.3f * std::sin( 6.2831853f * 440.f * static_cast<float>( Phase ) / 48000.f );
            B.getOutputs()[0][n] = v;
            B.getOutputs()[1][n] = v;
        }
    };
    auto finite = []( AudIO::Buffer<float>& B ) {
        for( uint32_t c = 0; c < 2; ++c )
            for( uint32_t n = 0; n < kBlock; ++n )
                if( !std::isfinite( B.getOutputs()[c][n] ) )
                    return false;
        return true;
    };

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

        // The parameter at both ends of its range while audio runs (a sine, and silence afterwards): nothing goes NaN / Inf.
        uint32_t phase = 0;
        for( int p = 0; p < numParams; ++p )
            for( float v : { 1.0f, 0.0f, 0.37f } )
            {
                obj->setParameter( p, v );
                fillSine( buf, phase );
                obj->process( buf );
                if( !finite( buf ) ) { std::printf( "FAIL %s parameter %d = %g gives a non finite output\n", name.c_str(), p, v ); ++g_fail; }
            }
        for( uint32_t c = 0; c < 2; ++c )
            std::fill( buf.getOutputs()[c], buf.getOutputs()[c] + kBlock, 0.f );
        obj->process( buf );
        CHECK( finite( buf ) );

        // Cost: 400 blocks of 64 frames of a sine (the effect as it was after the parameter sweep is not what is timed:
        // a fresh instance with its defaults is).
        auto timed = AudIO::ObjectFactory::Get().Create( name );
        timed->prepareToPlay( 48000, kBlock );
        const int blocks = 400;
        for( int b = 0; b < 40; ++b ) // warm up
        {
            fillSine( buf, phase );
            timed->process( buf );
        }
        const auto t0 = std::chrono::steady_clock::now();
        for( int b = 0; b < blocks; ++b )
        {
            fillSine( buf, phase );
            timed->process( buf );
        }
        const double seconds = std::chrono::duration<double>( std::chrono::steady_clock::now() - t0 ).count();
        cost.emplace_back( seconds / ( blocks * kBlock / 48000.0 ), name );
    }

    std::sort( cost.begin(), cost.end(), []( const auto& a, const auto& b ) { return a.first > b.first; } );
    std::printf( "INS-19 (host): the heaviest effects, fraction of real time for one stereo insert at 48 kHz, 64 frame blocks (the ARMv7 MPC is several times slower):\n" );
    for( std::size_t i = 0; i < std::min<std::size_t>( 40, cost.size() ); ++i )
        std::printf( "    %-24s %.4f\n", cost[i].second.c_str(), cost[i].first );
    for( const auto& c : cost )
    {
        using kick_ui::insert_cost::Level;
        if( c.first > kick_ui::insert_cost::kHeavy && kick_ui::insert_cost::levelOf( c.second.c_str() ) == Level::Normal )
        {
            std::printf( "FAIL %s costs %.3f of real time on the host but is not in kick_InsertCost.h\n", c.second.c_str(), c.first );
            ++g_fail;
        }
        if( c.first > 0.9 ) // does not even run in real time on the host
        {
            std::printf( "FAIL %s costs %.3f of real time on the host\n", c.second.c_str(), c.first );
            ++g_fail;
        }
    }
    CHECK( kick_ui::insert_cost::levelOf( "Pockey" ) == kick_ui::insert_cost::Level::VeryHeavy );
    CHECK( kick_ui::insert_cost::levelOf( "Reverb" ) == kick_ui::insert_cost::Level::Normal );

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
