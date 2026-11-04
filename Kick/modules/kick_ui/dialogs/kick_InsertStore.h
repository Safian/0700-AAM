// SPDX-License-Identifier: AGPL-3.0-or-later
// Insert favourites, one user preset per effect and the A/B compare state of a slot: pure (JUCE-free) logic and the text form the
// Inserts screen keeps in the preferences folder. Host test: Kick/tests/platform/InsertStoreTests.cpp.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace kick_ui::insert_store
{
    constexpr std::size_t kMaxParams  = 64;
    constexpr std::size_t kMaxEntries = 2000;

    class Store
    {
    public:
        bool isFavourite( const std::string& Effect ) const { return favourites_.count( Effect ) != 0; }

        // Returns the new state.
        bool toggleFavourite( const std::string& Effect )
        {
            if( Effect.empty() )
                return false;
            if( favourites_.erase( Effect ) != 0 )
                return false;
            if( favourites_.size() < kMaxEntries )
                favourites_.insert( Effect );
            return isFavourite( Effect );
        }

        const std::set<std::string>& favourites() const { return favourites_; }

        void savePreset( const std::string& Effect, const std::vector<float>& Values )
        {
            if( Effect.empty() || Values.empty() || Values.size() > kMaxParams || ( presets_.count( Effect ) == 0 && presets_.size() >= kMaxEntries ) )
                return;
            auto& v = presets_[Effect];
            v.clear();
            for( float f : Values )
                v.push_back( clean( f ) );
        }

        const std::vector<float>* preset( const std::string& Effect ) const
        {
            const auto it = presets_.find( Effect );
            return it == presets_.end() ? nullptr : &it->second;
        }

        std::size_t numPresets() const { return presets_.size(); }

        // One line per entry, tab separated: "fav\t<effect>" and "preset\t<effect>\t<v>\t<v>...". Names never contain a tab or newline.
        std::string serialize() const
        {
            std::ostringstream out;
            for( const auto& f : favourites_ )
                out << "fav\t" << f << '\n';
            for( const auto& p : presets_ )
            {
                out << "preset\t" << p.first;
                for( float v : p.second )
                    out << '\t' << v;
                out << '\n';
            }
            return out.str();
        }

        // Unknown or malformed lines are skipped; values are clamped to 0..1, NaN becomes 0.
        static Store parse( const std::string& Text )
        {
            Store         s;
            std::istringstream in( Text );
            std::string   line;
            while( std::getline( in, line ) )
            {
                if( !line.empty() && line.back() == '\r' )
                    line.pop_back();
                const auto fields = split( line );
                if( fields.size() == 2 && fields[0] == "fav" && !fields[1].empty() && s.favourites_.size() < kMaxEntries )
                    s.favourites_.insert( fields[1] );
                else if( fields.size() >= 3 && fields[0] == "preset" && !fields[1].empty() && fields.size() - 2 <= kMaxParams )
                {
                    std::vector<float> values;
                    bool               ok = true;
                    for( std::size_t i = 2; i < fields.size() && ok; ++i )
                    {
                        char*       end = nullptr;
                        const float v   = std::strtof( fields[i].c_str(), &end );
                        ok              = end != fields[i].c_str() && *end == '\0';
                        values.push_back( clean( v ) );
                    }
                    if( ok )
                        s.savePreset( fields[1], values );
                }
            }
            return s;
        }

    private:
        static float clean( float V ) noexcept { return V != V ? 0.f : std::clamp( V, 0.f, 1.f ); }

        static std::vector<std::string> split( const std::string& Line )
        {
            std::vector<std::string> out;
            std::size_t              from = 0;
            for( ;; )
            {
                const auto tab = Line.find( '\t', from );
                out.push_back( Line.substr( from, tab == std::string::npos ? std::string::npos : tab - from ) );
                if( tab == std::string::npos )
                    break;
                from = tab + 1;
            }
            return out;
        }

        std::set<std::string>                     favourites_;
        std::map<std::string, std::vector<float>> presets_;
    };

    // A/B compare of one insert slot: two parameter sets of the same effect. A toggle keeps the current values on the side that is
    // left and recalls the other side; the first time a side is entered it starts as a copy of what is there.
    struct AbSlot
    {
        std::string        effect;
        std::vector<float> a, b;
        bool               onB{ false };

        // Returns the values to set on the effect, or null when none have to change. A different effect starts over.
        const std::vector<float>* toggle( const std::string& Effect, const std::vector<float>& Current )
        {
            if( Effect != effect )
            {
                effect = Effect;
                a.clear();
                b.clear();
                onB = false;
            }
            auto& leaving = onB ? b : a;
            auto& entering = onB ? a : b;
            leaving = Current;
            onB     = !onB;
            if( entering.empty() )
            {
                entering = Current;
                return nullptr;
            }
            return &entering;
        }

        void reset()
        {
            effect.clear();
            a.clear();
            b.clear();
            onB = false;
        }
    };
}
