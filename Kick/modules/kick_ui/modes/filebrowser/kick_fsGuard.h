// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) file browser safety rules: which paths must never be deleted / moved / renamed, "folder into itself"
// detection and collision-free names for paste. Host-tested in Kick/tests/platform/FsGuardTests.cpp.
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace kick_fsguard
{
    // Collapses "//", resolves "." and "..", removes the trailing "/" ("/" stays "/"). Relative input is returned cleaned but relative.
    inline std::string normalize( const std::string& path )
    {
        std::vector<std::string> parts;
        const bool               absolute = !path.empty() && path[0] == '/';
        size_t                   pos      = 0;
        while( pos <= path.size() )
        {
            auto slash = path.find( '/', pos );
            if( slash == std::string::npos )
                slash = path.size();
            const auto part = path.substr( pos, slash - pos );
            pos             = slash + 1;
            if( part.empty() || part == "." )
                continue;
            if( part == ".." )
            {
                if( !parts.empty() && parts.back() != ".." )
                    parts.pop_back();
                else if( !absolute )
                    parts.push_back( part );
                continue;
            }
            parts.push_back( part );
        }
        std::string out = absolute ? "/" : "";
        for( size_t i = 0; i < parts.size(); ++i )
            out += ( i ? "/" : "" ) + parts[i];
        return out.empty() ? ( absolute ? "/" : "." ) : out;
    }

    // `inner` is `outer` or lies below it (component-wise: "/data/projects2" is not inside "/data/projects").
    inline bool isInside( const std::string& inner, const std::string& outer )
    {
        const auto a = normalize( inner );
        const auto b = normalize( outer );
        if( a == b )
            return true;
        if( b == "/" )
            return !a.empty() && a[0] == '/';
        return a.size() > b.size() && a.compare( 0, b.size(), b ) == 0 && a[b.size()] == '/';
    }

    // Folders that hold the device (root, the data tree, the library roots, the mount parents).
    inline const std::vector<std::string>& systemRoots()
    {
        static const std::vector<std::string> roots = { "/", "/data", "/data/projects", "/data/sounds", "/data/demos", "/data/templates", "/data/prefs", "/data/assets",
                                                        "/sdcard", "/media", "/mnt", "/home", "/usr", "/etc", "/bin", "/lib", "/var", "/tmp", "/dev", "/proc", "/sys" };
        return roots;
    }

    // May this path be deleted / cut / renamed? Not: a system root, a root the browser shows as a Place (SD card, USB drive,
    // library roots in `placeRoots`), a top-level folder, the open project, a folder that contains the open project, or anything
    // inside the open project's folder. `currentProject` may be the project file or its folder ("" = none open).
    inline bool isProtected( const std::string& path, const std::string& currentProject, const std::vector<std::string>& placeRoots = {} )
    {
        const auto p = normalize( path );
        if( p.empty() || p == "." || p == "/" )
            return true;
        for( const auto& r : systemRoots() )
            if( normalize( r ) == p )
                return true;
        for( const auto& r : placeRoots )
            if( !r.empty() && normalize( r ) == p )
                return true;
        // "/x" is a top-level folder of the root file system.
        if( p.size() > 1 && p[0] == '/' && p.find( '/', 1 ) == std::string::npos )
            return true;
        if( !currentProject.empty() )
        {
            const auto cur = normalize( currentProject );
            if( isInside( cur, p ) )  // the project, or a folder that contains it
                return true;
            // The project's own folder: for ".../Beat/Beat.kickPrj" everything below ".../Beat".
            const auto slash = cur.rfind( '/' );
            if( slash != std::string::npos && slash > 0 )
            {
                const auto dir = cur.substr( 0, slash );
                const auto fileName = cur.substr( slash + 1 );
                if( fileName.size() > 8 && fileName.compare( fileName.size() - 8, 8, ".kickPrj" ) == 0 && isInside( p, dir ) )
                    return true;
            }
            if( isInside( p, cur ) )
                return true;
        }
        return false;
    }

    // Pasting `source` into `destinationDir` when destinationDir is the source folder itself or inside it would copy a folder
    // into itself (endless / destroying recursion).
    inline bool pasteIntoItself( const std::string& source, const std::string& destinationDir )
    {
        return isInside( destinationDir, source );
    }

    // "name" -> "name" if free, else "name (2)", "name (3)" ... ; the extension stays at the end ("a.wav" -> "a (2).wav").
    inline std::string uniqueName( const std::string& fileName, const std::function<bool( const std::string& )>& exists )
    {
        if( !exists( fileName ) )
            return fileName;
        const auto dot  = fileName.rfind( '.' );
        const bool hasExt = dot != std::string::npos && dot > 0;
        const auto stem = hasExt ? fileName.substr( 0, dot ) : fileName;
        const auto ext  = hasExt ? fileName.substr( dot ) : std::string();
        for( int n = 2; n < 10000; ++n )
        {
            const auto candidate = stem + " (" + std::to_string( n ) + ")" + ext;
            if( !exists( candidate ) )
                return candidate;
        }
        return {}; // nothing free: the caller refuses
    }
}
