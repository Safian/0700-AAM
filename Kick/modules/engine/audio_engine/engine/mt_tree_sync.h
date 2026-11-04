/*
 * Kick - Behringer KICK drum machine software.
 * Licensed under AGPL-3.0-or-later; see the repository LICENSE.
 */
#pragma once

#include <algorithm>
#include <memory>
#include <vector>

// JUCE-free: reconciliation of an engine object list (sequences, tracks, programs) with the child nodes of the project
// ValueTree. Undo / redo of a create or delete changes the tree only; the engine vector has to follow.
namespace mt::treesync
{
    // Makes Objs match Nodes: objects without a node are handed to BeforeDestroy and destroyed, nodes without an object
    // get one from Create (a null result skips the node), and the surviving objects end up in tree order.
    //   Same(const Obj&, const Node&)          -> true if the object was built from the node
    //   Create(const Node&)                    -> std::unique_ptr<Obj>
    //   BeforeDestroy(Obj&)                    -> called for every object about to be destroyed
    // Returns true if anything was removed, created or moved. Objects that stay keep their address.
    template <class Obj, class Node, class Same, class Create, class BeforeDestroy>
    bool reconcile( std::vector<std::unique_ptr<Obj>>& Objs, const std::vector<Node>& Nodes, Same same, Create create, BeforeDestroy beforeDestroy )
    {
        bool changed = false;

        // 1. destroy what the tree no longer has
        for( size_t i = Objs.size(); i-- > 0; )
        {
            const bool kept = std::any_of( Nodes.begin(), Nodes.end(), [&]( const Node& N ) { return same( *Objs[i], N ); } );
            if( !kept )
            {
                beforeDestroy( *Objs[i] );
                Objs.erase( Objs.begin() + static_cast<std::ptrdiff_t>( i ) );
                changed = true;
            }
        }

        // 2. tree order, creating the missing ones
        std::vector<std::unique_ptr<Obj>> ordered;
        ordered.reserve( Nodes.size() );
        for( const auto& n : Nodes )
        {
            auto it = std::find_if( Objs.begin(), Objs.end(), [&]( const std::unique_ptr<Obj>& O ) { return O && same( *O, n ); } );
            if( it != Objs.end() )
            {
                if( it - Objs.begin() != static_cast<std::ptrdiff_t>( ordered.size() ) ) changed = true; // moved
                ordered.push_back( std::move( *it ) );
            }
            else if( auto fresh = create( n ) )
            {
                ordered.push_back( std::move( fresh ) );
                changed = true;
            }
        }

        Objs = std::move( ordered );
        return changed;
    }
} // namespace mt::treesync
