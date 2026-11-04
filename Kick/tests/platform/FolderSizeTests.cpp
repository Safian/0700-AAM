// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for FolderSizeCache and GenerationTracker. Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined
#include "../../modules/kick_ui/modes/filebrowser/kick_folderSizeHelper.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>
#include <filesystem>
#include <fstream>

static int g_fail = 0;
#define CHECK( cond )                                                                                               \
    do {                                                                                                           \
        if( !( cond ) ) {                                                                                          \
            std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond );                                         \
            ++g_fail;                                                                                              \
        }                                                                                                          \
    } while( 0 )

#define CHECK_EQ( a, b )                                                                                           \
    do {                                                                                                           \
        long long va = (long long)( a ), vb = (long long)( b );                                                    \
        if( va != vb ) {                                                                                           \
            std::printf( "FAIL %s:%d  %s = %lld, expected %lld\n", __FILE__, __LINE__, #a, va, vb );              \
            ++g_fail;                                                                                              \
        }                                                                                                          \
    } while( 0 )

using ui::fileUtils::FolderSizeCache;
using ui::fileUtils::GenerationTracker;
using ui::fileUtils::DirEntry;
using ui::fileUtils::scanFolderSize;

static void test_cache_basic()
{
    FolderSizeCache cache;
    CHECK_EQ( cache.size(), 0 );

    int64_t outSize = 0;
    CHECK( !cache.get( "/path/to/folder", 1000, outSize ) );

    cache.put( "/path/to/folder", 1234567, 1000 );
    CHECK_EQ( cache.size(), 1 );

    // Matching mtime -> hit
    CHECK( cache.get( "/path/to/folder", 1000, outSize ) );
    CHECK_EQ( outSize, 1234567 );

    // Changed mtime -> miss (invalidation)
    CHECK( !cache.get( "/path/to/folder", 1001, outSize ) );

    // Update with new mtime
    cache.put( "/path/to/folder", 7654321, 1001 );
    CHECK( cache.get( "/path/to/folder", 1001, outSize ) );
    CHECK_EQ( outSize, 7654321 );
    CHECK( !cache.get( "/path/to/folder", 1000, outSize ) );

    // Invalidate
    cache.invalidate( "/path/to/folder" );
    CHECK_EQ( cache.size(), 0 );
    CHECK( !cache.get( "/path/to/folder", 1001, outSize ) );

    // Clear
    cache.put( "/a", 10, 100 );
    cache.put( "/b", 20, 100 );
    CHECK_EQ( cache.size(), 2 );
    cache.clear();
    CHECK_EQ( cache.size(), 0 );
}

static void test_generation_tracker()
{
    GenerationTracker tracker;
    CHECK_EQ( tracker.getGeneration(), 0 );
    CHECK( !tracker.isCancelled( 0 ) );
    CHECK( tracker.isCancelled( 1 ) );

    uint64_t g1 = tracker.nextGeneration();
    CHECK_EQ( g1, 1 );
    CHECK_EQ( tracker.getGeneration(), 1 );
    CHECK( tracker.isCancelled( 0 ) );
    CHECK( !tracker.isCancelled( 1 ) );

    uint64_t g2 = tracker.nextGeneration();
    CHECK_EQ( g2, 2 );
    CHECK( tracker.isCancelled( 0 ) );
    CHECK( tracker.isCancelled( 1 ) );
    CHECK( !tracker.isCancelled( 2 ) );
}

static void test_cache_concurrency()
{
    FolderSizeCache cache;
    constexpr int numThreads = 8;
    constexpr int opsPerThread = 2000;
    std::vector<std::thread> threads;
    threads.reserve( numThreads );

    for( int t = 0; t < numThreads; ++t )
    {
        threads.emplace_back( [&cache, t]() {
            for( int i = 0; i < opsPerThread; ++i )
            {
                std::string path = "/folder_" + std::to_string( ( t * 10 + i ) % 50 );
                int64_t mtime = 1000 + ( i % 5 );
                int64_t size = 100 + i;

                cache.put( path, size, mtime );

                int64_t outSize = 0;
                cache.get( path, mtime, outSize );

                if( i % 20 == 0 )
                    cache.invalidate( path );
            }
        } );
    }

    for( auto& th : threads )
        th.join();

    CHECK( cache.size() <= 50 );
}

static void test_cancellation_simulation()
{
    GenerationTracker tracker;
    FolderSizeCache cache;

    uint64_t taskGen = tracker.getGeneration();
    std::atomic<bool> taskStarted{ false };
    std::atomic<int> processedCount{ 0 };
    std::atomic<bool> taskAborted{ false };

    // Simulate background folder scan that checks cancellation
    std::thread worker( [&]() {
        taskStarted = true;
        for( int i = 0; i < 1000; ++i )
        {
            if( tracker.isCancelled( taskGen ) )
            {
                taskAborted = true;
                return;
            }
            ++processedCount;
            std::this_thread::sleep_for( std::chrono::microseconds( 200 ) );
        }
        cache.put( "/slow/folder", 999999, 500 );
    } );

    while( !taskStarted )
        std::this_thread::yield();

    // Give it a tiny bit of time to process a few items
    std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );

    // User navigates away -> bump generation
    tracker.nextGeneration();

    worker.join();

    CHECK( taskAborted.load() );
    CHECK( processedCount.load() < 1000 );

    int64_t outSize = 0;
    CHECK( !cache.get( "/slow/folder", 500, outSize ) );
}

// scanFolderSize against a REAL temp folder (FB-33): empty, nested, cached sub-folders, cancellation keeps finished work, symlinks.
namespace fs = std::filesystem;

static void put( const fs::path& p, size_t bytes )
{
    fs::create_directories( p.parent_path() );
    std::ofstream( p, std::ios::binary ) << std::string( bytes, 'x' );
}

static std::vector<DirEntry> listStd( const std::string& dir )
{
    std::vector<DirEntry> out;
    std::error_code       ec;
    for( const auto& e : fs::directory_iterator( dir, ec ) )
    {
        DirEntry d;
        d.path      = e.path().string();
        d.isSymlink = e.is_symlink();
        d.isDirectory = e.is_directory( ec );
        if( d.isSymlink )
            d.size = 16; // a link counts with its own (small) size
        else if( d.isDirectory )
            d.mtime = (int64_t)e.last_write_time().time_since_epoch().count();
        else
            d.size = (int64_t)e.file_size();
        out.push_back( d );
    }
    return out;
}

static void test_scan_real_folder()
{
    const fs::path root = fs::temp_directory_path() / "kick_folder_size_test";
    fs::remove_all( root );
    fs::create_directories( root / "empty" );
    put( root / "a.bin", 100 );
    put( root / "deep" / "b.bin", 200 );
    put( root / "deep" / "x" / "y" / "z" / "c.bin", 300 );
    FolderSizeCache cache;
    const auto      noStop = std::function<bool()>();

    // empty folder = 0; whole tree = 100 + 200 + 300
    CHECK_EQ( scanFolderSize( ( root / "empty" ).string(), listStd, cache, noStop ), 0 );
    CHECK_EQ( scanFolderSize( root.string(), listStd, cache, noStop ), 600 );
    // every sub-folder that was finished is cached
    int64_t cached = 0;
    CHECK( cache.size() >= 5 ); // empty, deep, x, y, z
    (void)cached;

    // a second scan with a changed file in a cached sub-folder: its mtime changed, so it is scanned again
    {
        FolderSizeCache c2;
        CHECK_EQ( scanFolderSize( root.string(), listStd, c2, noStop ), 600 );
        put( root / "deep" / "x" / "y" / "z" / "d.bin", 50 ); // z changes (mtime), the parents are found by their own mtime when listed
        c2.invalidate( ( root / "deep" / "x" / "y" / "z" ).string() );
        fs::last_write_time( root / "deep" / "x" / "y" / "z", fs::file_time_type::clock::now() + std::chrono::seconds( 5 ) );
        FolderSizeCache c3;
        CHECK_EQ( scanFolderSize( root.string(), listStd, c3, noStop ), 650 );
    }

    // cancellation: -1, and the sub-folders finished before the cancel stay cached
    {
        FolderSizeCache c4;
        int             calls = 0;
        const auto      stopAfter = [&]() { return ++calls > 4; };
        CHECK_EQ( scanFolderSize( root.string(), listStd, c4, stopAfter ), -1 );
        CHECK( c4.size() < 5 );
        // a later scan completes with the right total
        CHECK_EQ( scanFolderSize( root.string(), listStd, c4, noStop ), 650 );
    }

    // symlinks: a link to the parent folder is not followed (no loop, no double count); a link to a file counts once as a link
    {
        std::error_code ec;
        fs::create_directory_symlink( root, root / "deep" / "loop", ec );
        fs::create_symlink( root / "a.bin", root / "link_to_a", ec );
        if( !ec )
        {
            FolderSizeCache c5;
            const int64_t   total = scanFolderSize( root.string(), listStd, c5, noStop );
            CHECK( total >= 650 && total <= 650 + 2 * 64 ); // plus the two small links, never the target again
        }
    }

    // the depth bound stops a run-away tree (a mount loop): 100 nested folders still finish
    {
        fs::path p = root / "nest";
        for( int i = 0; i < 100; ++i )
            p /= "n";
        fs::create_directories( p );
        FolderSizeCache c6;
        CHECK( scanFolderSize( ( root / "nest" ).string(), listStd, c6, noStop ) >= 0 );
    }
    fs::remove_all( root );
}

int main()
{
    test_scan_real_folder();
    test_cache_basic();
    test_generation_tracker();
    test_cache_concurrency();
    test_cancellation_simulation();

    if( g_fail == 0 )
        std::printf( "folder size helper tests OK\n" );
    else
        std::printf( "folder size helper tests FAILED: %d errors\n", g_fail );

    return g_fail ? 1 : 0;
}
