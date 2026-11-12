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
    std::string schemaVersion;   // Schema version (e.g. "0.1-open-interop")
    std::string architect;       // Architect attribution (e.g. "Dream Maker 5 (@FYD5IVE)")
    std::string licenseNotice;   // License or dual-licensing notice
    std::string pluginId;        // Target CLAP plugin ID (e.g. "com.u-he.Zebra3")
    std::string profileName;     // Profile title (e.g. "Polykeys 61 Performance")
    std::string author;          // Creator attribution
    std::vector<MancerMacroBinding> macros;
    MancerRibbonBinding ribbon;
    uint32_t touchStripParamId{ 0 };     // Expressive bus touch strip parameter ID
    uint32_t polyAftertouchParamId{ 0 }; // Expressive bus poly aftertouch parameter ID
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
    // Supports both v0 legacy schemas and Open Interop .mancer.json v0.1 stub schemas
    static bool parseJson( const std::string& json, MancerMap& outMap )
    {
        outMap = MancerMap{};

        // Extract metadata fields
        outMap.schemaVersion = extractStringField( json, "schema_version" );
        outMap.architect = extractStringField( json, "architect" );
        outMap.licenseNotice = extractStringField( json, "license_notice" );

        outMap.pluginId = extractStringField( json, "plugin_id" );
        if( outMap.pluginId.empty() )
            outMap.pluginId = extractStringField( json, "pluginId" );

        outMap.profileName = extractStringField( json, "name" );
        if( outMap.profileName.empty() )
            outMap.profileName = extractStringField( json, "profileName" );

        outMap.author = extractStringField( json, "author" );

        // Support Open Interop .mancer.json v0.1 stub schema: audiom_matrix_bindings
        if( json.find( "\"audiom_matrix_bindings\"" ) != std::string::npos )
        {
            // Parse Bank A encoders (Macros 1-4 / X1, Y1, X2, Y2 -> index 0..3)
            parseEncoderArray( json, "bank_a_encoders", 0, outMap.macros );

            // Parse Bank B encoders (Macros 5-8 / X3, Y3, X4, Y4 -> index 4..7)
            parseEncoderArray( json, "bank_b_encoders", 4, outMap.macros );

            // Parse expressive bus (touch strip, poly-aftertouch)
            size_t expPos = json.find( "\"expressive_bus\"" );
            if( expPos != std::string::npos )
            {
                size_t expStart = json.find( '{', expPos );
                size_t expEnd = ( expStart != std::string::npos ) ? json.find( '}', expStart ) : std::string::npos;
                if( expStart != std::string::npos && expEnd != std::string::npos )
                {
                    std::string expBlock = json.substr( expStart, expEnd - expStart + 1 );
                    outMap.touchStripParamId = static_cast<uint32_t>( extractIntField( expBlock, "touch_strip_param_id", 0 ) );
                    outMap.polyAftertouchParamId = static_cast<uint32_t>( extractIntField( expBlock, "poly_aftertouch_mod_id", 0 ) );
                    if( outMap.touchStripParamId != 0 )
                    {
                        outMap.ribbon.paramId = outMap.touchStripParamId;
                    }
                    if( outMap.polyAftertouchParamId != 0 )
                    {
                        outMap.enablePolyAftertouch = true;
                    }
                }
            }
        }

        // Support legacy schema: macros array
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

    static bool extractRange( const std::string& src, float& outMin, float& outMax )
    {
        size_t pos = src.find( "\"range\"" );
        if( pos == std::string::npos ) return false;
        size_t start = src.find( '[', pos );
        size_t end = ( start != std::string::npos ) ? src.find( ']', start ) : std::string::npos;
        if( start == std::string::npos || end == std::string::npos ) return false;
        std::string inner = src.substr( start + 1, end - start - 1 );
        size_t comma = inner.find( ',' );
        if( comma != std::string::npos ) {
            try {
                outMin = std::stof( inner.substr( 0, comma ) );
                outMax = std::stof( inner.substr( comma + 1 ) );
                return true;
            } catch(...) {}
        }
        return false;
    }

    static void parseEncoderArray( const std::string& json, const std::string& arrayKey, uint8_t baseIndex, std::vector<MancerMacroBinding>& outMacros )
    {
        std::string pattern = "\"" + arrayKey + "\"";
        size_t pos = json.find( pattern );
        if( pos == std::string::npos ) return;
        size_t startBracket = json.find( '[', pos );
        if( startBracket == std::string::npos ) return;

        size_t depth = 0;
        size_t endBracket = std::string::npos;
        for( size_t i = startBracket; i < json.length(); ++i )
        {
            if( json[i] == '[' ) ++depth;
            else if( json[i] == ']' )
            {
                --depth;
                if( depth == 0 )
                {
                    endBracket = i;
                    break;
                }
            }
        }
        if( endBracket == std::string::npos ) return;

        std::string block = json.substr( startBracket + 1, endBracket - startBracket - 1 );
        size_t objStart = 0;
        int count = 0;
        while( ( objStart = block.find( '{', objStart ) ) != std::string::npos )
        {
            size_t objEnd = block.find( '}', objStart );
            if( objEnd == std::string::npos ) break;

            std::string objStr = block.substr( objStart, objEnd - objStart + 1 );
            MancerMacroBinding binding;
            int encIdx = extractIntField( objStr, "encoder_idx", -1 );
            if( encIdx < 0 ) {
                encIdx = count;
            }
            binding.macroIndex = static_cast<uint8_t>( baseIndex + encIdx );
            binding.paramId = static_cast<uint32_t>( extractIntField( objStr, "clap_param_id", 0 ) );
            if( binding.paramId == 0 ) {
                binding.paramId = static_cast<uint32_t>( extractIntField( objStr, "param_id", 0 ) );
            }
            binding.name = extractStringField( objStr, "slot" );
            if( binding.name.empty() ) {
                binding.name = extractStringField( objStr, "name" );
            }
            float rMin = 0.0f, rMax = 1.0f;
            if( extractRange( objStr, rMin, rMax ) ) {
                binding.minVal = rMin;
                binding.maxVal = rMax;
            } else {
                binding.minVal = extractFloatField( objStr, "min", 0.0f );
                binding.maxVal = extractFloatField( objStr, "max", 1.0f );
            }
            binding.defaultVal = extractFloatField( objStr, "default", binding.minVal );

            if( binding.paramId != 0 || !binding.name.empty() ) {
                outMacros.push_back( binding );
            }
            objStart = objEnd + 1;
            count++;
        }
    }
};

} // namespace AudIO
