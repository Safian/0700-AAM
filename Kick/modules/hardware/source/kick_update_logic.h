// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) logic of the Kick Update screen: VERSION file parsing, version comparison, the state of the package on
// the USB drive against the installed Kick, and the guards for a restart. The installer itself is the pendrive bootstrap
// (docs/akai_mpc_hardware/tools/tkgl_bootstrap); this only tells the user what it will do. Host-tested in
// Kick/tests/platform/KickUpdateTests.cpp.
#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace kick_update
{
    struct PackageInfo
    {
        std::string version;
        std::string commit;
        std::string buildDate;
        std::string sha256;
        bool        valid() const { return !version.empty(); }
    };

    inline std::string trimmed( const std::string& s )
    {
        const char* ws = " \t\r\n\"";
        const auto  b  = s.find_first_not_of( ws );
        if( b == std::string::npos )
            return {};
        const auto e = s.find_last_not_of( ws );
        return s.substr( b, e - b + 1 );
    }

    // KEY=VALUE lines: VERSION, GIT_COMMIT, BUILD_DATE, KICK_SHA256 (the format make_pendrive_package.sh writes).
    inline PackageInfo parseVersionText( const std::string& text )
    {
        PackageInfo out;
        size_t      pos = 0;
        while( pos <= text.size() )
        {
            auto nl = text.find( '\n', pos );
            if( nl == std::string::npos )
                nl = text.size();
            const auto line = text.substr( pos, nl - pos );
            pos             = nl + 1;
            const auto eq   = line.find( '=' );
            if( eq == std::string::npos )
                continue;
            const auto key = trimmed( line.substr( 0, eq ) );
            const auto val = trimmed( line.substr( eq + 1 ) );
            if( key == "VERSION" ) out.version = val;
            else if( key == "GIT_COMMIT" ) out.commit = val;
            else if( key == "BUILD_DATE" ) out.buildDate = val;
            else if( key == "KICK_SHA256" ) out.sha256 = val;
        }
        return out;
    }


    // Streaming SHA-256 (FIPS 180-4) so a 70 MB package binary is hashed in chunks, cancellable, without holding it in memory.
    class Sha256
    {
    public:
        Sha256() { reset(); }
        void reset()
        {
            static const uint32_t init[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
            std::memcpy( h_, init, sizeof( h_ ) );
            len_ = 0;
            fill_ = 0;
        }
        void update( const void* data, size_t n )
        {
            const auto* p = static_cast<const uint8_t*>( data );
            len_ += n;
            while( n > 0 )
            {
                const size_t take = ( 64 - fill_ ) < n ? ( 64 - fill_ ) : n;
                std::memcpy( buf_ + fill_, p, take );
                fill_ += take;
                p += take;
                n -= take;
                if( fill_ == 64 )
                {
                    block( buf_ );
                    fill_ = 0;
                }
            }
        }
        std::string hex()
        {
            const uint64_t bits = len_ * 8;
            const uint8_t  pad  = 0x80;
            update( &pad, 1 );
            const uint8_t zero = 0;
            while( fill_ != 56 )
                update( &zero, 1 );
            uint8_t lenBytes[8];
            for( int i = 0; i < 8; ++i )
                lenBytes[i] = (uint8_t)( bits >> ( 56 - 8 * i ) );
            update( lenBytes, 8 );
            static const char* digits = "0123456789abcdef";
            std::string        out;
            for( uint32_t w : h_ )
                for( int i = 3; i >= 0; --i )
                {
                    const uint8_t byte = (uint8_t)( w >> ( 8 * i ) );
                    out += digits[byte >> 4];
                    out += digits[byte & 15];
                }
            return out;
        }

    private:
        static uint32_t rotr( uint32_t x, int n ) { return ( x >> n ) | ( x << ( 32 - n ) ); }
        void            block( const uint8_t* p )
        {
            static const uint32_t k[64] = {
                0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
                0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
                0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
                0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };
            uint32_t w[64];
            for( int i = 0; i < 16; ++i )
                w[i] = ( (uint32_t)p[4 * i] << 24 ) | ( (uint32_t)p[4 * i + 1] << 16 ) | ( (uint32_t)p[4 * i + 2] << 8 ) | (uint32_t)p[4 * i + 3];
            for( int i = 16; i < 64; ++i )
            {
                const uint32_t s0 = rotr( w[i - 15], 7 ) ^ rotr( w[i - 15], 18 ) ^ ( w[i - 15] >> 3 );
                const uint32_t s1 = rotr( w[i - 2], 17 ) ^ rotr( w[i - 2], 19 ) ^ ( w[i - 2] >> 10 );
                w[i]              = w[i - 16] + s0 + w[i - 7] + s1;
            }
            uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4], f = h_[5], g = h_[6], hh = h_[7];
            for( int i = 0; i < 64; ++i )
            {
                const uint32_t t1 = hh + ( rotr( e, 6 ) ^ rotr( e, 11 ) ^ rotr( e, 25 ) ) + ( ( e & f ) ^ ( ~e & g ) ) + k[i] + w[i];
                const uint32_t t2 = ( rotr( a, 2 ) ^ rotr( a, 13 ) ^ rotr( a, 22 ) ) + ( ( a & b ) ^ ( a & c ) ^ ( b & c ) );
                hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
            }
            h_[0] += a; h_[1] += b; h_[2] += c; h_[3] += d; h_[4] += e; h_[5] += f; h_[6] += g; h_[7] += hh;
        }

        uint32_t h_[8];
        uint8_t  buf_[64];
        size_t   fill_ = 0;
        uint64_t len_  = 0;
    };

    // Dotted numbers compared field by field; a pre-release suffix ("1.2.0-beta") sorts before the release ("1.2.0").
    // Returns <0, 0, >0.
    inline int compareVersions( const std::string& a, const std::string& b )
    {
        auto split = []( const std::string& v, std::vector<long>& nums, std::string& pre ) {
            std::string core = v;
            const auto  dash = v.find( '-' );
            if( dash != std::string::npos )
            {
                core = v.substr( 0, dash );
                pre  = v.substr( dash + 1 );
            }
            size_t pos = 0;
            while( pos <= core.size() )
            {
                auto dot = core.find( '.', pos );
                if( dot == std::string::npos )
                    dot = core.size();
                const auto part = core.substr( pos, dot - pos );
                long       n    = 0;
                for( char c : part )
                {
                    if( c < '0' || c > '9' )
                        break;
                    n = n * 10 + ( c - '0' );
                    if( n > 1000000000L )
                        break;
                }
                nums.push_back( n );
                pos = dot + 1;
            }
        };
        std::vector<long> na, nb;
        std::string       pa, pb;
        split( a, na, pa );
        split( b, nb, pb );
        const size_t n = na.size() > nb.size() ? na.size() : nb.size();
        for( size_t i = 0; i < n; ++i )
        {
            const long x = i < na.size() ? na[i] : 0;
            const long y = i < nb.size() ? nb[i] : 0;
            if( x != y )
                return x < y ? -1 : 1;
        }
        if( pa.empty() != pb.empty() )
            return pa.empty() ? 1 : -1; // the release is newer than its pre-release
        return pa < pb ? -1 : ( pa > pb ? 1 : 0 );
    }

    enum class State
    {
        NoDrive,       // no USB drive mounted
        NoPackage,     // a drive, but no Kick package on it
        PackageBroken, // VERSION unreadable / binary missing / SHA-256 mismatch: the bootstrap refuses it
        UpToDate,      // the package equals the installed Kick
        Newer,         // the package is newer (or Kick is not installed yet)
        Older,         // the package is older: installing it is a downgrade
        Different      // same version number, other build
    };

    struct Inputs
    {
        bool        driveFound       = false;
        bool        versionFileFound = false;
        std::string installedText;     // /data/Kick.version, "" when missing
        std::string packageText;       // <drive>/tkgl_bootstrap_[ProjectData]/VERSION
        bool        binaryFound = false; // <drive>/.../bin/Kick exists and is not empty
        bool        shaChecked  = false; // the SHA-256 of the binary was computed
        bool        shaMatches  = false;
    };

    // The bootstrap installs whenever the VERSION files differ (cmp), upward and downward alike; this mirrors it.
    inline State classify( const Inputs& in, std::string* reason = nullptr )
    {
        auto why = [&]( const char* s ) { if( reason ) *reason = s; };
        if( !in.driveFound ) return State::NoDrive;
        if( !in.versionFileFound ) return State::NoPackage;
        const auto pkg = parseVersionText( in.packageText );
        if( !pkg.valid() ) { why( "the VERSION file of the package is unreadable" ); return State::PackageBroken; }
        if( !in.binaryFound ) { why( "the Kick binary is missing from the package" ); return State::PackageBroken; }
        if( pkg.sha256.empty() ) { why( "the package has no SHA-256 (the installer would not verify it)" ); return State::PackageBroken; }
        if( in.shaChecked && !in.shaMatches ) { why( "the SHA-256 of the Kick binary does not match (damaged copy)" ); return State::PackageBroken; }
        if( in.installedText == in.packageText ) return State::UpToDate;
        const auto inst = parseVersionText( in.installedText );
        if( !inst.valid() ) return State::Newer;
        const int c = compareVersions( pkg.version, inst.version );
        return c > 0 ? State::Newer : ( c < 0 ? State::Older : State::Different );
    }

    inline bool installable( State s ) { return s == State::Newer || s == State::Older || s == State::Different; }

    inline std::string describe( State s, const PackageInfo& installed, const PackageInfo& pkg, const std::string& reason = {} )
    {
        switch( s )
        {
            case State::NoDrive: return "No USB drive found";
            case State::NoPackage: return "No Kick package on the USB drive";
            case State::PackageBroken: return "The package on the USB drive cannot be installed: " + reason;
            case State::UpToDate: return "Up to date (" + pkg.version + ")";
            case State::Newer: return installed.valid() ? "Update available: " + installed.version + " -> " + pkg.version : "Install available: " + pkg.version;
            case State::Older: return "Older package on the USB drive: " + installed.version + " -> " + pkg.version + " (downgrade)";
            case State::Different: return "Other build of " + pkg.version + " on the USB drive (" + installed.commit + " -> " + pkg.commit + ")";
        }
        return {};
    }

    // Why a restart to install must not start now, "" when it may. The project is saved by the caller before it quits.
    inline std::string restartBlockReason( bool playing, bool recording, bool projectLoading )
    {
        if( projectLoading ) return "A project is loading";
        if( recording ) return "Recording is running";
        if( playing ) return "Playback is running";
        return {};
    }
}
