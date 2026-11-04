// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) logic of the recent-projects list (KickProjectsDefault): which entries go when a project is opened again,
// when it is renamed or removed, and how the list is cut to its maximum. The list is stored oldest first, newest last.
// The ValueTree wrapper in kick_settings.cpp applies these plans. Host-tested in Kick/tests/platform/RecentListTests.cpp.
#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace kick_recent
{
    // Longest list that is kept (a maximum below 1 counts as 1: the project that is open is always the newest entry).
    inline size_t effectiveMax( int maxRecent ) { return maxRecent < 1 ? 1 : (size_t)maxRecent; }

    // Indexes of every entry equal to `path`, highest first (so removing them one by one never shifts a later index).
    inline std::vector<int> indicesOf( const std::vector<std::string>& list, const std::string& path )
    {
        std::vector<int> out;
        for( int i = (int)list.size() - 1; i >= 0; --i )
            if( list[(size_t)i] == path )
                out.push_back( i );
        return out;
    }

    // Opening a project: the entries to remove (highest index first) and whether the project is added as the newest entry.
    //  - an existing entry for the project is removed (it comes back at the end),
    //  - the oldest entries are removed until there is room for the new one,
    //  - a project whose file does not exist is not added and changes nothing.
    struct Plan
    {
        std::vector<int> remove; // highest first
        bool             add{ false };
    };

    inline Plan planOpen( const std::vector<std::string>& list, const std::string& path, bool projectExists, int maxRecent )
    {
        Plan plan;
        if( !projectExists || path.empty() )
            return plan;
        plan.add = true;
        plan.remove = indicesOf( list, path );
        size_t remaining = list.size() - plan.remove.size();
        const size_t max = effectiveMax( maxRecent );
        // the oldest entries that are not already removed
        int idx = 0;
        while( remaining >= max && idx < (int)list.size() )
        {
            if( std::find( plan.remove.begin(), plan.remove.end(), idx ) == plan.remove.end() )
            {
                plan.remove.push_back( idx );
                --remaining;
            }
            ++idx;
        }
        std::sort( plan.remove.begin(), plan.remove.end(), std::greater<int>() );
        return plan;
    }

    // Number of oldest entries to drop so that the list fits `maxRecent` (after the maximum was lowered).
    inline int excess( size_t count, int maxRecent )
    {
        const size_t max = effectiveMax( maxRecent );
        return count > max ? (int)( count - max ) : 0;
    }

    // Renaming a project folder: entries that name it get the new path. When the new path is already in the list the older of the
    // two entries is dropped (one project, one entry). Returns the new list.
    inline std::vector<std::string> renamed( const std::vector<std::string>& list, const std::string& from, const std::string& to )
    {
        std::vector<std::string> out;
        for( const auto& e : list )
            out.push_back( e == from ? to : e );
        // keep the newest of equal entries
        std::vector<std::string> unique;
        for( int i = (int)out.size() - 1; i >= 0; --i )
            if( std::find( unique.begin(), unique.end(), out[(size_t)i] ) == unique.end() )
                unique.push_back( out[(size_t)i] );
        std::reverse( unique.begin(), unique.end() );
        return unique;
    }

    // Applies a plan to a plain list (the tests, and a check for the ValueTree wrapper).
    inline void apply( std::vector<std::string>& list, const Plan& plan, const std::string& path )
    {
        for( int i : plan.remove )
            if( i >= 0 && i < (int)list.size() )
                list.erase( list.begin() + i );
        if( plan.add )
            list.push_back( path );
    }
}
