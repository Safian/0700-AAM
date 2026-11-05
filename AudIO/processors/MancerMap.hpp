// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * File: AudIO/processors/MancerMap.hpp
 * Project: Behringer KICK (0700-AAM) — Standalone CLAP Synth Workstation Host
 * Author: Safian Szabolcs (@Safian) & Dream Maker 5 (@FYD5IVE)
 * Copyright (C) 2026 Safian & Dream Maker 5. All Rights Reserved.
 *
 * Description:
 * User-space hardware-to-parameter macro mapping schema parser for CLAP
 * synthesizers on the 0700-AAM platform. Decouples physical controller
 * layouts (16 pads, 4 Q-Links, 61-key polyphonic aftertouch keybed)
 * from the AudIO DSP engine using declarative .mancer / .json profiles
 * stored in user space (/mnt/sdcard/Plugins/CLAP/ or /data/plugins/).
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace AudIO {

struct MancerMacroBinding {
    uint8_t macroIndex{ 0 };     // 0..7 (Bank A: 0..3, Bank B: 4..7)
    uint32_t paramId{ 0 };       // CLAP parameter ID
    std::string name;            // Human-readable parameter label (e.g. "Cutoff", "Morph X")
    float minVal{ 0.0f };        // Minimum normalized value
    float maxVal{ 1.0f };        // Maximum normalized value
    float defaultVal{ 0.0f };    // Default initialized value
};

struct MancerRibbonBinding {
    uint32_t paramId{ 0 };       // CLAP parameter ID bound to ribbon controller
    bool isBipolar{ true };      // true = -1.0..+1.0 (pitch), false = 0.0..1.0 (mod)
};

struct MancerMap {
    std::string pluginId;        // Target CLAP plugin ID (e.g. "com.u-he.zebralette3")
    std::string profileName;     // Profile title (e.g. "Polykeys 61 Performance")
    std::string author;          // Creator attribution
    std::vector<MancerMacroBinding> macros;
    MancerRibbonBinding ribbon;
    bool enablePolyAftertouch{ true };

    // Helper: find macro binding by index (0..7)
    const MancerMacroBinding* findMacro( uint8_t index ) const noexcept
    {
        for( const auto& m : macros )
        {
            if( m.macroIndex == index )
                return &m;
        }
        return nullptr;
    }

    // Helper: find macro binding by name
    const MancerMacroBinding* findMacroByName( const std::string& nameQuery ) const noexcept
    {
        for( const auto& m : macros )
        {
            if( m.name == nameQuery )
                return &m;
        }
        return nullptr;
    }

    // Lightweight parser for .mancer / .json macro maps (zero external dependencies)
    static bool parseJson( const std::string& json, MancerMap& outMap )
    {
        outMap = MancerMap{};

        // Extract plugin_id
        outMap.pluginId = extractStringField( json, "plugin_id" );
        if( outMap.pluginId.empty() )
            outMap.pluginId = extractStringField( json, "pluginId" );

        outMap.profileName = extractStringField( json, "name" );
        if( outMap.profileName.empty() )
            outMap.profileName = extractStringField( json, "profileName" );

        outMap.author = extractStringField( json, "author" );

        // Extract macros array
        size_t pos = json.find( "\"macros\"" );
        if( pos != std::string::npos )
        {
            size_t startBracket = json.find( '[', pos );
            size_t endBracket = ( startBracket != std::string::npos ) ? json.find( ']', startBracket ) : std::string::npos;
            if( startBracket != std::string::npos && endBracket != std::string::npos )
            {
                std::string macrosBlock = json.substr( startBracket + 1, endBracket - startBracket - 1 );
                size_t objStart = 0;
                while( ( objStart = macrosBlock.find( '{', objStart ) ) != std::string::npos )
                {
                    size_t objEnd = macrosBlock.find( '}', objStart );
                    if( objEnd == std::string::npos ) break;

                    std::string objStr = macrosBlock.substr( objStart, objEnd - objStart + 1 );
                    MancerMacroBinding binding;
                    binding.macroIndex = static_cast<uint8_t>( extractIntField( objStr, "index" ) );
                    binding.paramId = static_cast<uint32_t>( extractIntField( objStr, "param_id" ) );
                    binding.name = extractStringField( objStr, "name" );
                    binding.minVal = extractFloatField( objStr, "min", 0.0f );
                    binding.maxVal = extractFloatField( objStr, "max", 1.0f );
                    binding.defaultVal = extractFloatField( objStr, "default", 0.0f );

                    if( binding.paramId != 0 || !binding.name.empty() )
                    {
                        outMap.macros.push_back( binding );
                    }
                    objStart = objEnd + 1;
                }
            }
        }

        return !outMap.pluginId.empty() || !outMap.macros.empty();
    }

    // Loads a .mancer / .json file from disk
    static bool loadFromFile( const std::string& filePath, MancerMap& outMap )
    {
        std::ifstream f( filePath );
        if( !f.is_open() )
            return false;
        std::stringstream ss;
        ss << f.rdbuf();
        return parseJson( ss.str(), outMap );
    }

private:
    static std::string extractStringField( const std::string& src, const std::string& key )
    {
        std::string pattern = "\"" + key + "\"";
        size_t p = src.find( pattern );
        if( p == std::string::npos ) return "";
        size_t colon = src.find( ':', p + pattern.length() );
        if( colon == std::string::npos ) return "";
        size_t q1 = src.find( '"', colon );
        if( q1 == std::string::npos ) return "";
        size_t q2 = src.find( '"', q1 + 1 );
        if( q2 == std::string::npos ) return "";
        return src.substr( q1 + 1, q2 - q1 - 1 );
    }

    static int extractIntField( const std::string& src, const std::string& key, int defaultVal = 0 )
    {
        std::string pattern = "\"" + key + "\"";
        size_t p = src.find( pattern );
        if( p == std::string::npos ) return defaultVal;
        size_t colon = src.find( ':', p + pattern.length() );
        if( colon == std::string::npos ) return defaultVal;
        size_t valStart = src.find_first_of( "-0123456789", colon );
        if( valStart == std::string::npos ) return defaultVal;
        return std::stoi( src.substr( valStart ) );
    }

    static float extractFloatField( const std::string& src, const std::string& key, float defaultVal = 0.0f )
    {
        std::string pattern = "\"" + key + "\"";
        size_t p = src.find( pattern );
        if( p == std::string::npos ) return defaultVal;
        size_t colon = src.find( ':', p + pattern.length() );
        if( colon == std::string::npos ) return defaultVal;
        size_t valStart = src.find_first_of( "-0123456789.", colon );
        if( valStart == std::string::npos ) return defaultVal;
        return std::stof( src.substr( valStart ) );
    }
};

} // namespace AudIO
