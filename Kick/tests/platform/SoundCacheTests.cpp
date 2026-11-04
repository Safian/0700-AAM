// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test of the sample cache (SR-21): stats, "unused" = only the cache references the sound, Purge Unused drops exactly those,
// a held sound keeps its data, a dropped sound is decoded again by the next getOrLoad, clear().
// Links the real ASoundCache (module audio_file_manager) with juce_audio_formats; writes two small WAV files to a temp folder.
#include <audio_file_manager/audio_file_manager.h>
#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

static bool writeWav( const juce::File& file, int samples )
{
    juce::WavAudioFormat fmt;
    file.deleteFile();
    std::unique_ptr<juce::FileOutputStream> out( file.createOutputStream() );
    if( !out )
        return false;
    std::unique_ptr<juce::AudioFormatWriter> writer( fmt.createWriterFor( out.get(), 44100.0, 2, 16, {}, 0 ) );
    if( !writer )
        return false;
    out.release(); // the writer owns the stream
    juce::AudioBuffer<float> buf( 2, samples );
    for( int c = 0; c < 2; ++c )
        for( int i = 0; i < samples; ++i )
            buf.setSample( c, i, 0.5f * std::sin( 0.05f * (float)i ) );
    return writer->writeFromAudioSampleBuffer( buf, 0, samples );
}

int main()
{
    const auto dir = juce::File::getSpecialLocation( juce::File::tempDirectory ).getChildFile( "kick_sound_cache_test" );
    dir.deleteRecursively();
    dir.createDirectory();
    const auto fa = dir.getChildFile( "a.wav" ), fb = dir.getChildFile( "b.wav" );
    CHECK( writeWav( fa, 4410 ) && writeWav( fb, 8820 ) );

    mt::ASoundCache cache;
    CHECK( cache.getStats().count == 0 && cache.getStats().bytes == 0 );

    mt::ASound::Ptr a = cache.getOrLoad( fa.getFullPathName() );
    juce::int64     bBytes = 0;
    {
        mt::ASound::Ptr b = cache.getOrLoad( fb.getFullPathName() );
        CHECK( a != nullptr && b != nullptr && a->isValid() && b->isValid() );
        bBytes = b->getMemoryBytes();
        const auto st = cache.getStats();
        CHECK( st.count == 2 && st.unusedCount == 0 ); // both are held by a pointer here
        CHECK( st.bytes == a->getMemoryBytes() + bBytes && bBytes > 0 );
    } // b released: only the cache holds it
    {
        const auto st = cache.getStats();
        CHECK( st.count == 2 && st.unusedCount == 1 && st.unusedBytes == bBytes );
    }

    // Purge Unused drops b and only b; a keeps its data.
    const auto aBytes = a->getMemoryBytes();
    const auto freed  = cache.purgeUnused();
    CHECK( freed.count == 1 && freed.bytes == bBytes && freed.unusedCount == 1 );
    {
        const auto st = cache.getStats();
        CHECK( st.count == 1 && st.bytes == aBytes && st.unusedCount == 0 );
    }
    CHECK( a->isValid() && a->getMemoryBytes() == aBytes && a->getData().getNumSamples() > 0 );
    CHECK( cache.exists( fb.getFullPathName() ) == nullptr && cache.exists( fa.getFullPathName() ) != nullptr );

    // Nothing unused: a second purge frees nothing.
    {
        const auto again = cache.purgeUnused();
        CHECK( again.count == 0 && again.bytes == 0 && cache.getStats().count == 1 );
    }

    // A dropped sound is decoded again by the next getOrLoad (same size), and the held one is not duplicated.
    {
        mt::ASound::Ptr b2 = cache.getOrLoad( fb.getFullPathName() );
        CHECK( b2 != nullptr && b2->isValid() && b2->getMemoryBytes() == bBytes );
        CHECK( cache.getOrLoad( fa.getFullPathName() ).get() == a.get() );
        CHECK( cache.getStats().count == 2 );
    }

    // After the last holder lets go, a purge empties the cache.
    a = nullptr;
    {
        const auto last = cache.purgeUnused();
        CHECK( last.count == 2 && cache.getStats().count == 0 && cache.getStats().bytes == 0 );
    }

    // clear() drops everything, held or not (the held pointer stays valid: reference counted).
    mt::ASound::Ptr keep = cache.getOrLoad( fa.getFullPathName() );
    cache.clear();
    CHECK( cache.getStats().count == 0 && keep != nullptr && keep->isValid() );

    dir.deleteRecursively();
    if( g_fail == 0 ) std::printf( "SoundCacheTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
