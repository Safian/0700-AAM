// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for AudIO::DawInserts: registration key naming (NC-17), parameter range guard, swap / remove, remove-all
// bypass state, and slot replacement while another thread runs process() (use-after-free guard; run with ASan).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -pthread DawInsertsTests.cpp ../../../AudIO/AudIO/Daw/DawInserts.cpp
#include "../../../AudIO/AudIO/Daw/DawInserts.h"
#include "../../../AudIO/AudIO/Includes/ObjectFactory.h"
#include "../../../AudIO/AudIO/Includes/AudIOBuffer.h"
#include <stdexcept>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

namespace AudIO
{
    const std::string _emptyStr{ "" }; // defined in DawEngine.cpp in the app
}

namespace
{
    int g_fail = 0;
#define CHECK( a )                                                                       \
    do {                                                                                 \
        if( !( a ) ) { std::printf( "FAIL %s:%d  %s\n", __FILE__, __LINE__, #a ); ++g_fail; } \
    } while( 0 )

    std::atomic<int> g_alive{ 0 };

    // Adds `gain` to every sample; reports a name different from its registration key (like NCSeventeen -> "NC-17").
    class AddObject : public AudIO::Object
    {
    public:
        explicit AddObject( float Add = 1.f, int NumParams = 2 )
            : Object( 2, 2, NumParams ), add( Add )
        {
            setName( "NC-17" );
            ++g_alive;
        }
        ~AddObject() override
        {
            alive = false; // makes a use after delete visible even without ASan
            --g_alive;
        }
        float getParameter( int i ) override { return values[i]; }
        void  setParameter( int i, float v ) override
        {
            if( i < 0 || i >= (int)_numParams ) throw std::runtime_error( "bad index" ); // like Airwindows
            values[i] = v;
        }
        void process( AudIO::Buffer<float>& b ) override
        {
            if( !alive ) ++useAfterDelete;
            auto** out = b.getOutputs();
            for( uint32_t c = 0; c < 2; ++c )
                for( uint32_t n = 0; n < b.getBlockSize(); ++n )
                    out[c][n] += add;
        }
        float       add;
        float       values[8]{};
        bool        alive{ true };
        static inline std::atomic<int> useAfterDelete{ 0 };
    };

    AudIO::ObjectFactoryRegistration<AddObject> reg( "NCSeventeenTest" );
}

// Minimal buffer over two arrays: DawInserts only needs getOutputs() / getBlockSize().
int main()
{
    using namespace AudIO;

    static float pool[64 * 64];
    AudIO::MemoryPool<float>::get().initialize( pool, 64, 64 );

    DawInserts ins;
    ins.prepareInserts( 48000, 64 );

    // ---- name = registration key, not Object::getName()
    CHECK( 0 == ins.addEffect( "NCSeventeenTest", 0 ) );
    CHECK( ins.getEffectName( 0 ) == "NCSeventeenTest" );
    CHECK( ins.getEffectsNames()[0] == "NCSeventeenTest" );
    CHECK( ins.getEffectsNames()[1].empty() );
    CHECK( 0 == ins.addEffect( "NCSeventeenTest", 0 ) ); // same effect, same slot: nothing replaced
    CHECK( g_alive == 1 );
    // the saved name creates the same effect again
    CHECK( 0 == ins.addEffect( ins.getEffectName( 0 ), 1 ) );
    CHECK( g_alive == 2 );

    // ---- parameter range guard (an out-of-range index used to reach a throwing setParameter)
    ins.setEffectParameter( 0, 1, 0.5f );
    CHECK( ins.getEffectParameter( 0, 1 ) == 0.5f );
    ins.setEffectParameter( 0, 99, 0.5f ); // ignored, no throw
    ins.setEffectParameter( 3, 0, 0.5f );  // empty slot, ignored

    // ---- remove all: the master bypass is not left on
    ins.removeAllEffects();
    CHECK( g_alive == 0 );
    CHECK( !ins.isAllEffectsBypassed() );
    CHECK( 0 == ins.addEffect( "NCSeventeenTest", 2 ) );
    CHECK( !ins.isEffectBypassed( 2 ) );

    // ---- swap keeps names and bypass with their objects
    ins.setEffectBypass( 2, true );
    CHECK( 0 == ins.addEffect( "NCSeventeenTest", 0 ) );
    CHECK( 0 == ins.swapEffects( 0, 2 ) );
    CHECK( ins.isEffectBypassed( 0 ) );
    CHECK( !ins.isEffectBypassed( 2 ) );
    CHECK( ins.getEffectName( 0 ) == "NCSeventeenTest" );
    CHECK( -1 == ins.swapEffects( 0, 9 ) );
    ins.removeAllEffects();

    // ---- slot replacement while the audio thread runs process(): no use after delete
    CHECK( 0 == ins.addEffect( "NCSeventeenTest", 0 ) );
    std::atomic<bool> run{ true };
    std::atomic<long> blocks{ 0 };
    std::thread audio( [&]() {
        AudIO::Buffer<float> buf( 2, false, 2, true );
        while( run.load() )
        {
            ins.processInserts( buf );
            ++blocks;
        }
    } );
    for( int i = 0; i < 300; ++i )
    {
        ins.addEffect( std::make_unique<AddObject>( 0.1f ), 0 ); // replace
        if( i % 3 == 0 ) ins.removeEffect( 0u );
        if( i % 5 == 0 ) ins.swapEffects( 0, 1 );
        std::this_thread::sleep_for( std::chrono::microseconds( 200 ) );
    }
    run = false;
    audio.join();
    CHECK( AddObject::useAfterDelete.load() == 0 );
    CHECK( blocks.load() > 0 );
    ins.removeAllEffects();
    CHECK( g_alive == 0 );

    if( g_fail == 0 )
        std::printf( "DawInsertsTests: all passed (%ld blocks processed)\n", blocks.load() );
    return g_fail == 0 ? 0 : 1;
}
