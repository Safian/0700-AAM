// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free, C++17) helpers for importing shared / Open-In files (from iOS Share Sheet / Inbox / Files)
// into Kick's Samples or Projects directory, plus auto-discovery of user Splice folders across platforms.
// Host-tested by Kick/tests/platform/IncomingFilesTests.cpp.
#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace kick_incoming
{
    namespace fs = std::filesystem;

    enum class IncomingKind
    {
        Sample,
        Midi,
        Project,
        Unsupported
    };

    inline std::string toLower( std::string s )
    {
        for( auto& c : s )
            c = static_cast<char>( std::tolower( static_cast<unsigned char>( c ) ) );
        return s;
    }

    inline std::string extensionLower( const std::string& filenameOrPath )
    {
        fs::path p( filenameOrPath );
        auto     ext = toLower( p.extension().string() );
        if( !ext.empty() && ext[0] == '.' )
            ext.erase( 0, 1 );
        return ext;
    }

    inline IncomingKind classifyExtension( const std::string& filenameOrPath )
    {
        const auto ext = extensionLower( filenameOrPath );
        if( ext.empty() )
            return IncomingKind::Unsupported;

        if( ext == "kickprj" )
            return IncomingKind::Project;
        if( ext == "mid" || ext == "midi" )
            return IncomingKind::Midi;

        static const char* const sampleExts[] = {
            "wav", "bwf", "aif", "aiff", "flac", "ogg", "mp3", "wmv", "asf", "wm", "wma",
            "m4a", "aac", "caf"
        };
        for( const char* s : sampleExts )
            if( ext == s )
                return IncomingKind::Sample;

        return IncomingKind::Unsupported;
    }

    inline bool isSupportedFile( const std::string& filenameOrPath )
    {
        return classifyExtension( filenameOrPath ) != IncomingKind::Unsupported;
    }

    // Strips directory components and unsafe characters, returning a single clean filename.
    inline std::string sanitiseFileName( const std::string& nameOrPath )
    {
        auto raw = fs::path( nameOrPath ).filename().string();
        std::string clean;
        clean.reserve( raw.size() );
        for( char ch : raw )
        {
            const auto c = static_cast<unsigned char>( ch );
            if( c < 0x20 || c == '/' || c == '\\' || c == ':' || c == 0x7f )
                clean.push_back( '_' );
            else
                clean.push_back( ch );
        }
        if( clean.empty() || clean == "." || clean == ".." )
            clean = "imported_file";
        return clean;
    }

    // Picks a non-colliding path inside `destDir`: `name.wav`, `name (2).wav`, `name (3).wav`, ...
    inline std::string uniqueDestinationPath( const std::string& destDir, const std::string& originalName )
    {
        const auto     cleanName = sanitiseFileName( originalName );
        const fs::path dir( destDir );
        const fs::path file( cleanName );
        const auto     stem = file.stem().string();
        const auto     ext  = file.extension().string();

        std::error_code ec;
        fs::path        candidate = dir / file;
        if( !fs::exists( candidate, ec ) )
            return candidate.string();

        for( int idx = 2; idx <= 9999; ++idx )
        {
            const std::string alt = ( stem.empty() ? std::string( "imported" ) : stem ) + " (" + std::to_string( idx ) + ")" + ext;
            candidate             = dir / alt;
            if( !fs::exists( candidate, ec ) )
                return candidate.string();
        }
        return ( dir / ( cleanName + ".new" ) ).string();
    }

    // Copies (or moves when `removeSourceOnSuccess` is true) a Kick-compatible file or .kickPrj folder
    // into `samplesDir` (Sample/Midi) or `projectsDir` (Project). Writes regular files via a temp file
    // + rename so partial copies never appear.
    inline bool importIncomingFile( const std::string& srcPath,
                                    const std::string& samplesDir,
                                    const std::string& projectsDir,
                                    bool               removeSourceOnSuccess,
                                    std::string&       outDestPath,
                                    IncomingKind&      outKind,
                                    std::string&       outError )
    {
        outDestPath.clear();
        outError.clear();
        outKind = classifyExtension( srcPath );

        std::error_code ec;
        const fs::path  src( srcPath );
        if( !fs::exists( src, ec ) )
        {
            outError = "Source file does not exist";
            return false;
        }
        if( outKind == IncomingKind::Unsupported )
        {
            outError = "Unsupported file type";
            if( removeSourceOnSuccess )
                fs::remove_all( src, ec );
            return false;
        }

        const bool     isProjectDir = ( outKind == IncomingKind::Project && fs::is_directory( src, ec ) );
        const fs::path baseDir      = ( outKind == IncomingKind::Project ) ? fs::path( projectsDir ) : fs::path( samplesDir );
        if( baseDir.empty() )
        {
            outError = "Destination folder not configured";
            return false;
        }

        fs::create_directories( baseDir, ec );
        if( ec )
        {
            outError = "Could not create destination folder";
            return false;
        }

        // For a single .kickPrj file, keep it inside a matching project folder `<projectsDir>/<stem>/<stem>.kickPrj`.
        if( outKind == IncomingKind::Project && !isProjectDir )
        {
            const auto     baseName  = sanitiseFileName( src.stem().string() );
            const auto     projDir   = uniqueDestinationPath( baseDir.string(), baseName );
            fs::create_directories( projDir, ec );
            if( ec )
            {
                outError = "Could not create project folder";
                return false;
            }
            const auto destFile = ( fs::path( projDir ) / ( fs::path( projDir ).filename().string() + ".kickPrj" ) ).string();
            const auto tmpFile  = destFile + ".kicktmp";
            fs::copy_file( src, tmpFile, fs::copy_options::overwrite_existing, ec );
            if( ec )
            {
                fs::remove( tmpFile, ec );
                outError = "Copy failed";
                return false;
            }
            fs::rename( tmpFile, destFile, ec );
            if( ec )
            {
                fs::remove( tmpFile, ec );
                outError = "Finalise failed";
                return false;
            }
            if( removeSourceOnSuccess )
                fs::remove( src, ec );
            outDestPath = destFile;
            return true;
        }

        const std::string destPath = uniqueDestinationPath( baseDir.string(), src.filename().string() );
        if( isProjectDir )
        {
            fs::copy( src, destPath, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec );
            if( ec )
            {
                fs::remove_all( destPath, ec );
                outError = "Project folder copy failed";
                return false;
            }
            if( removeSourceOnSuccess )
                fs::remove_all( src, ec );
            outDestPath = destPath;
            return true;
        }

        const std::string tmpPath = destPath + ".kicktmp";
        fs::remove( tmpPath, ec );
        fs::copy_file( src, tmpPath, fs::copy_options::overwrite_existing, ec );
        if( ec )
        {
            fs::remove( tmpPath, ec );
            outError = "File copy failed";
            return false;
        }
        fs::rename( tmpPath, destPath, ec );
        if( ec )
        {
            fs::remove( tmpPath, ec );
            outError = "File rename failed";
            return false;
        }
        if( removeSourceOnSuccess )
            fs::remove( src, ec );
        outDestPath = destPath;
        return true;
    }

    struct ImportSummary
    {
        std::vector<std::string> importedPaths;
        int                      skippedUnsupported{ 0 };
        int                      failed{ 0 };
    };

    // Sweeps a directory (such as iOS Documents/Inbox) and imports every supported entry inside it,
    // removing consumed entries on success.
    inline ImportSummary sweepInboxDirectory( const std::string& inboxDir,
                                              const std::string& samplesDir,
                                              const std::string& projectsDir )
    {
        ImportSummary   summary;
        std::error_code ec;
        const fs::path  inbox( inboxDir );
        if( !fs::exists( inbox, ec ) || !fs::is_directory( inbox, ec ) )
            return summary;

        std::vector<fs::path> items;
        for( auto& entry : fs::directory_iterator( inbox, ec ) )
            items.push_back( entry.path() );
        std::sort( items.begin(), items.end() );

        for( const auto& item : items )
        {
            const auto name = item.filename().string();
            if( !name.empty() && name[0] == '.' )
                continue;
            std::string  dest, err;
            IncomingKind kind = IncomingKind::Unsupported;
            if( importIncomingFile( item.string(), samplesDir, projectsDir, true, dest, kind, err ) )
                summary.importedPaths.push_back( std::move( dest ) );
            else if( kind == IncomingKind::Unsupported )
                ++summary.skippedUnsupported;
            else
                ++summary.failed;
        }
        return summary;
    }

    // Resolves the Splice folder path from an optional explicit override or candidate base folders.
    // Returns the first existing `Splice` directory (check exact match or `<root>/Splice`), or `fallback` if none exist.
    inline std::string resolveSpliceDir( const std::string&              explicitPath,
                                         const std::vector<std::string>& candidateRoots,
                                         const std::string&              fallback = {} )
    {
        std::error_code ec;
        auto isMisleadingRoot = []( const std::string& path ) {
            return path == "/root/Splice" || path == "/root" || path.rfind( "/root/", 0 ) == 0;
        };

        if( !explicitPath.empty() && !isMisleadingRoot( explicitPath ) )
        {
            const fs::path p( explicitPath );
            if( fs::exists( p, ec ) && fs::is_directory( p, ec ) )
                return p.string();
        }

        std::string firstExisting;
        for( const auto& root : candidateRoots )
        {
            if( root.empty() || isMisleadingRoot( root ) )
                continue;
            fs::path p = fs::path( root );
            if( p.filename() != "Splice" )
                p /= "Splice";

            if( fs::exists( p, ec ) && fs::is_directory( p, ec ) )
            {
                if( !fs::is_empty( p, ec ) )
                    return p.string();
                if( firstExisting.empty() )
                    firstExisting = p.string();
            }
        }
        if( !firstExisting.empty() )
            return firstExisting;

        if( !explicitPath.empty() && !isMisleadingRoot( explicitPath ) )
            return explicitPath;

#if JUCE_LINUX || __linux__
        if( fs::exists( "/sdcard/Splice", ec ) && fs::is_directory( "/sdcard/Splice", ec ) )
            return "/sdcard/Splice";
        if( fs::exists( "/sdcard", ec ) && fs::is_directory( "/sdcard", ec ) )
            return "/sdcard/Splice";
#endif

        return fallback;
    }
} // namespace kick_incoming
