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

    std::cout << "Legacy MancerMap format OK" << std::endl;

    // Test Open Interop .mancer.json v0.1 Stub Schema (PR #27 & Layer 2 alignment)
    const std::string v01InteropJson = R"json({
        "schema_version": "0.1-open-interop",
        "architect": "Dream Maker 5 (@FYD5IVE)",
        "license_notice": "Open Layer-2 UI Interop Stub. Full MancerMaps Compiler & Alch-Loop CV Matrices Copyright (c) 2026 Dream Maker 5 (@FYD5IVE).",
        "plugin_id": "com.u-he.Zebra3",
        "audiom_matrix_bindings": {
            "bank_a_encoders": [
                { "slot": "X1", "encoder_idx": 0, "clap_param_id": 1001, "range": [0.0, 1.0] },
                { "slot": "Y1", "encoder_idx": 1, "clap_param_id": 1002, "range": [0.0, 1.0] },
                { "slot": "X2", "encoder_idx": 2, "clap_param_id": 1003, "range": [0.0, 1.0] },
                { "slot": "Y2", "encoder_idx": 3, "clap_param_id": 1004, "range": [0.0, 1.0] }
            ],
            "bank_b_encoders": [
                { "slot": "X3", "encoder_idx": 0, "clap_param_id": 1005, "range": [0.0, 1.0] },
                { "slot": "Y3", "encoder_idx": 1, "clap_param_id": 1006, "range": [0.0, 1.0] },
                { "slot": "X4", "encoder_idx": 2, "clap_param_id": 1007, "range": [0.0, 1.0] },
                { "slot": "Y4", "encoder_idx": 3, "clap_param_id": 1008, "range": [0.0, 1.0] }
            ],
            "expressive_bus": {
                "touch_strip_param_id": 2001,
                "poly_aftertouch_mod_id": 2002
            }
        }
    })json";

    MancerMap v01Map;
    bool v01Ok = MancerMap::parseJson( v01InteropJson, v01Map );
    assert( v01Ok );
    assert( v01Map.schemaVersion == "0.1-open-interop" );
    assert( v01Map.architect == "Dream Maker 5 (@FYD5IVE)" );
    assert( v01Map.pluginId == "com.u-he.Zebra3" );
    assert( v01Map.macros.size() == 8 );
    assert( v01Map.touchStripParamId == 2001 );
    assert( v01Map.ribbon.paramId == 2001 );
    assert( v01Map.polyAftertouchParamId == 2002 );
    assert( v01Map.enablePolyAftertouch == true );

    // Verify Bank A bindings (0..3)
    const auto* slotX1 = v01Map.findMacro( 0 );
    assert( slotX1 != nullptr );
    assert( slotX1->name == "X1" );
    assert( slotX1->paramId == 1001 );

    const auto* slotY2 = v01Map.findMacro( 3 );
    assert( slotY2 != nullptr );
    assert( slotY2->name == "Y2" );
    assert( slotY2->paramId == 1004 );

    // Verify Bank B bindings (4..7)
    const auto* slotX3 = v01Map.findMacro( 4 );
    assert( slotX3 != nullptr );
    assert( slotX3->name == "X3" );
    assert( slotX3->paramId == 1005 );

    const auto* slotY4 = v01Map.findMacro( 7 );
    assert( slotY4 != nullptr );
    assert( slotY4->name == "Y4" );
    assert( slotY4->paramId == 1008 );

    // Test binding into processor
    ClapInstrumentProcessor procV01;
    bool v01Applied = procV01.applyMancerMap( v01Map );
    assert( v01Applied );
    assert( std::string( procV01.getMacroName( 0 ) ) == "X1" );
    assert( std::string( procV01.getMacroName( 1 ) ) == "Y1" );
    assert( std::string( procV01.getMacroName( 2 ) ) == "X2" );
    assert( std::string( procV01.getMacroName( 3 ) ) == "Y2" );
    assert( std::string( procV01.getMacroName( 4 ) ) == "X3" );
    assert( std::string( procV01.getMacroName( 5 ) ) == "Y3" );
    assert( std::string( procV01.getMacroName( 6 ) ) == "X4" );
    assert( std::string( procV01.getMacroName( 7 ) ) == "Y4" );

    std::cout << "Open Interop v0.1 format OK" << std::endl;
    std::cout << "MancerMapTests OK" << std::endl;
    return 0;
}
