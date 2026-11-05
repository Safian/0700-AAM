// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../../../AudIO/processors/MancerMap.hpp"
#include "../../../AudIO/processors/ClapInstrumentProcessor.hpp"
#include <cassert>
#include <iostream>

int main()
{
    using namespace AudIO;

    const std::string sampleJson = R"({
        "plugin_id": "com.u-he.zebralette3",
        "name": "Polykeys 61 Performance Map",
        "author": "@Safian",
        "macros": [
            { "index": 0, "param_id": 1001, "name": "Cutoff", "min": 20.0, "max": 20000.0, "default": 1000.0 },
            { "index": 1, "param_id": 1002, "name": "Resonance", "min": 0.0, "max": 1.0, "default": 0.2 },
            { "index": 2, "param_id": 1003, "name": "Morph X", "min": -1.0, "max": 1.0, "default": 0.0 },
            { "index": 3, "param_id": 1004, "name": "Morph Y", "min": -1.0, "max": 1.0, "default": 0.0 }
        ]
    })";

    MancerMap map;
    bool ok = MancerMap::parseJson( sampleJson, map );
    assert( ok );
    assert( map.pluginId == "com.u-he.zebralette3" );
    assert( map.profileName == "Polykeys 61 Performance Map" );
    assert( map.author == "@Safian" );
    assert( map.macros.size() == 4 );

    const auto* m0 = map.findMacro( 0 );
    assert( m0 != nullptr );
    assert( m0->paramId == 1001 );
    assert( m0->name == "Cutoff" );
    assert( m0->minVal == 20.0f );
    assert( m0->maxVal == 20000.0f );

    const auto* mRes = map.findMacroByName( "Resonance" );
    assert( mRes != nullptr );
    assert( mRes->paramId == 1002 );
    assert( mRes->defaultVal == 0.2f );

    assert( map.findMacro( 7 ) == nullptr );
    assert( map.findMacroByName( "NonExistent" ) == nullptr );

    // Test processor binding
    ClapInstrumentProcessor proc;
    assert( proc.getActiveMancerMap() == nullptr );
    assert( std::string( proc.getMacroName( 0 ) ).empty() );

    bool applied = proc.applyMancerMap( map );
    assert( applied );
    assert( proc.getActiveMancerMap() != nullptr );
    assert( proc.getActiveMancerMap()->pluginId == "com.u-he.zebralette3" );
    assert( std::string( proc.getMacroName( 0 ) ) == "Cutoff" );
    assert( std::string( proc.getMacroName( 1 ) ) == "Resonance" );
    assert( std::string( proc.getMacroName( 2 ) ) == "Morph X" );
    assert( std::string( proc.getMacroName( 3 ) ) == "Morph Y" );
    assert( std::string( proc.getMacroName( 4 ) ).empty() );

    std::cout << "MancerMapTests OK" << std::endl;
    return 0;
}
