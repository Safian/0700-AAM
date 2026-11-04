// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) project folder operations of the Projects screen: Duplicate, Rename, Delete, Info and the folder copy behind
// Save As / Save as Template. A project is a folder <name>/ with <name>.kickPrj inside (+ samples/, exports ...). All of it works on
// the file system only, off the message thread, and is host-tested with temp folders: Kick/tests/platform/ProjectOpsTests.cpp.
#pragma once

#include "kick_project_names.h"
#include "../modes/filebrowser/kick_fileOps.h"

#include <ctime>

namespace kick_project
{
    namespace fs = std::filesystem;

    constexpr const char* kProjectExt = ".kickPrj";

    enum class OpResult { Ok, BadName, AlreadyExists, NotAProject, Protected, SourceMissing, Failed, Cancelled };

    inline const char* describe( OpResult r )
    {
        switch( r )
        {
            case OpResult::Ok: return "";
            case OpResult::BadName: return "The project name is not valid";
            case OpResult::AlreadyExists: return "A project with the same name already exists";
            case OpResult::NotAProject: return "This folder is not a project";
            case OpResult::Protected: return "The open project and the factory demos and templates cannot be changed";
            case OpResult::SourceMissing: return "The project was not found";
            case OpResult::Failed: return "The operation failed (is the drive full or read-only?)";
            case OpResult::Cancelled: return "Cancelled";
        }
        return "";
    }

    inline std::string projectFileIn( const std::string& dir )
    {
        std::error_code ec;
        const fs::path  d( dir );
        const auto      expected = d / ( d.filename().string() + kProjectExt );
        if( fs::is_regular_file( expected, ec ) )
            return expected.string();
        for( fs::directory_iterator it( d, ec ), end; !ec && it != end; it.increment( ec ) )
            if( it->path().extension() == kProjectExt && it->is_regular_file( ec ) )
                return it->path().string();
        return {};
    }

    inline bool isProjectFolder( const std::string& dir )
    {
        std::error_code ec;
        return fs::is_directory( dir, ec ) && !projectFileIn( dir ).empty();
    }

    // Backups and recovery copies are not part of a copy of a project.
    inline bool isBackupFile( const std::string& fileName )
    {
        return ( fileName.size() > 4 && fileName.compare( fileName.size() - 4, 4, ".bak" ) == 0 ) || fileName.find( "_corrupt_" ) != std::string::npos;
    }

    // Renames the project file inside a freshly copied folder to the new name and drops the backups of the original.
    inline bool finishCopy( const std::string& newDir, const std::string& oldName, const std::string& newName )
    {
        std::error_code ec;
        std::vector<fs::path> backups;
        for( fs::directory_iterator it( newDir, ec ), end; !ec && it != end; it.increment( ec ) )
            if( it->is_regular_file( ec ) && isBackupFile( it->path().filename().string() ) )
                backups.push_back( it->path() );
        for( const auto& b : backups )
            fs::remove( b, ec );
        const fs::path from = fs::path( newDir ) / ( oldName + kProjectExt );
        const fs::path to   = fs::path( newDir ) / ( newName + kProjectExt );
        if( from == to )
            return true;
        if( !fs::exists( from, ec ) )
            return !projectFileIn( newDir ).empty() ? ( fs::rename( projectFileIn( newDir ), to, ec ), !ec ) : false;
        fs::rename( from, to, ec );
        return !ec;
    }

    // Duplicate: <root>/<newName> becomes a full copy of the project folder (samples included), the project file renamed.
    inline OpResult duplicate( const std::string& srcDir, const std::string& root, const std::string& rawName, const std::atomic<bool>& cancel, const kick_fileops::ProgressFn& progress = {}, std::string* newDirOut = nullptr )
    {
        std::error_code ec;
        const auto      name = trimName( rawName );
        if( checkName( name ) != NameProblem::Ok )
            return OpResult::BadName;
        if( !fs::is_directory( srcDir, ec ) )
            return OpResult::SourceMissing;
        if( !isProjectFolder( srcDir ) )
            return OpResult::NotAProject;
        const auto dst = ( fs::path( root ) / name ).string();
        if( fs::exists( fs::symlink_status( dst, ec ) ) )
            return OpResult::AlreadyExists;
        const auto res = kick_fileops::copyFolderTo( srcDir, dst, cancel, progress );
        if( !res.ok )
            return res.cancelled ? OpResult::Cancelled : OpResult::Failed;
        if( !finishCopy( dst, fs::path( srcDir ).filename().string(), name ) )
        {
            fs::remove_all( dst, ec );
            return OpResult::Failed;
        }
        if( newDirOut )
            *newDirOut = dst;
        return OpResult::Ok;
    }

    // Rename: the folder and the project file get the new name; a failed second step puts the first one back. `isProtected` is
    // asked with the folder (the open project, demos, templates).
    inline OpResult rename( const std::string& dir, const std::string& rawName, const std::function<bool( const std::string& )>& isProtected, std::string* newDirOut = nullptr )
    {
        std::error_code ec;
        const auto      name = trimName( rawName );
        if( checkName( name ) != NameProblem::Ok )
            return OpResult::BadName;
        if( !fs::is_directory( dir, ec ) )
            return OpResult::SourceMissing;
        if( !isProjectFolder( dir ) )
            return OpResult::NotAProject;
        if( isProtected && isProtected( dir ) )
            return OpResult::Protected;
        const fs::path d( dir );
        const auto     oldName = d.filename().string();
        const auto     newDir  = d.parent_path() / name;
        if( newDir == d )
            return OpResult::Ok;
        if( fs::exists( fs::symlink_status( newDir, ec ) ) )
            return OpResult::AlreadyExists;
        const auto projFile = projectFileIn( dir );
        fs::rename( d, newDir, ec );
        if( ec )
            return OpResult::Failed;
        const auto oldFileInNew = newDir / fs::path( projFile ).filename();
        const auto target       = newDir / ( name + kProjectExt );
        fs::rename( oldFileInNew, target, ec );
        if( ec )
        {
            std::error_code back;
            fs::rename( newDir, d, back ); // undo: the project stays as it was
            return OpResult::Failed;
        }
        // The backup of the old name goes along (it is only a recovery copy of the same file).
        std::error_code ignore;
        const auto      bak = newDir / ( fs::path( projFile ).filename().string() + ".bak" );
        if( fs::exists( bak, ignore ) )
            fs::rename( bak, newDir / ( name + kProjectExt + ".bak" ), ignore );
        if( newDirOut )
            *newDirOut = newDir.string();
        return OpResult::Ok;
    }

    // Delete: only a folder that holds a project file is removed, never a protected one, and in steps that honour `cancel`.
    inline OpResult remove( const std::string& dir, const std::function<bool( const std::string& )>& isProtected, const std::atomic<bool>& cancel, const kick_fileops::ProgressFn& progress = {} )
    {
        std::error_code ec;
        if( !fs::is_directory( dir, ec ) )
            return OpResult::SourceMissing;
        if( !isProjectFolder( dir ) )
            return OpResult::NotAProject;
        if( isProtected && isProtected( dir ) )
            return OpResult::Protected;
        kick_fileops::Job job;
        job.kind    = kick_fileops::Kind::Delete;
        job.sources = { dir };
        const auto r = kick_fileops::run( job, cancel, progress, isProtected );
        if( r.cancelled )
            return OpResult::Cancelled;
        return r.ok ? OpResult::Ok : OpResult::Failed;
    }

    struct Info
    {
        bool        valid{ false };
        std::string name;
        uint64_t    bytes{ 0 };
        int64_t     modifiedSec{ 0 };
        int         programs{ 0 };
        int         sequences{ 0 };
        int         samples{ 0 }; // audio files inside the project folder
    };

    inline int countOccurrences( const std::string& text, const std::string& needle )
    {
        int n = 0;
        for( size_t p = text.find( needle ); p != std::string::npos; p = text.find( needle, p + needle.size() ) )
            ++n;
        return n;
    }

    // Programs / sequences are counted in the project XML (<PROGRAM ...> / <SEQUENCE ...> elements, not the plural containers).
    inline Info info( const std::string& dir )
    {
        Info            i;
        std::error_code ec;
        if( !isProjectFolder( dir ) )
            return i;
        i.valid = true;
        i.name  = fs::path( dir ).filename().string();
        for( fs::recursive_directory_iterator it( dir, fs::directory_options::skip_permission_denied, ec ), end; !ec && it != end; it.increment( ec ) )
        {
            std::error_code e2;
            if( !it->is_regular_file( e2 ) )
                continue;
            i.bytes += static_cast<uint64_t>( it->file_size( e2 ) );
            auto ext = it->path().extension().string();
            for( auto& c : ext )
                c = static_cast<char>( std::tolower( static_cast<unsigned char>( c ) ) );
            if( ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".ogg" || ext == ".mp3" || ext == ".bwf" )
                ++i.samples;
        }
        const auto file = projectFileIn( dir );
        std::error_code e3;
        const auto      t = fs::last_write_time( file, e3 );
        if( !e3 )
        {
            const auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>( t - fs::file_time_type::clock::now() + std::chrono::system_clock::now() );
            i.modifiedSec   = std::chrono::duration_cast<std::chrono::seconds>( sctp.time_since_epoch() ).count();
        }
        std::ifstream in( file, std::ios::binary );
        std::string   xml( ( std::istreambuf_iterator<char>( in ) ), {} );
        i.programs  = countOccurrences( xml, "<PROGRAM " ) + countOccurrences( xml, "<PROGRAM>" );
        i.sequences = countOccurrences( xml, "<SEQUENCE " ) + countOccurrences( xml, "<SEQUENCE>" );
        return i;
    }

    inline std::string formatBytes( uint64_t b )
    {
        const char* u[] = { "B", "KB", "MB", "GB", "TB" };
        double      v   = static_cast<double>( b );
        int         k   = 0;
        while( v >= 1024.0 && k < 4 )
        {
            v /= 1024.0;
            ++k;
        }
        char buf[32];
        if( k == 0 ) std::snprintf( buf, sizeof( buf ), "%llu B", static_cast<unsigned long long>( b ) );
        else std::snprintf( buf, sizeof( buf ), v < 10.0 ? "%.1f %s" : "%.0f %s", v, u[k] );
        return buf;
    }

    // A name for "Save a copy": "Beat" -> "Beat copy", "Beat copy 2", ... the first one that is free under `root`.
    inline std::string suggestCopyName( const std::string& baseName, const std::string& root )
    {
        std::error_code ec;
        const auto      base = trimName( baseName );
        for( int n = 1; n < 1000; ++n )
        {
            const auto candidate = base + ( n == 1 ? " copy" : " copy " + std::to_string( n ) );
            if( candidate.size() <= kMaxNameLength && !fs::exists( fs::symlink_status( fs::path( root ) / candidate, ec ) ) )
                return candidate;
        }
        return base;
    }
}
