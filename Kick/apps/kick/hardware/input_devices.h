// SPDX-License-Identifier: AGPL-3.0-or-later
// Linux input device discovery for the power button (JUCE / Linux-header free, host-tested: PowerKeyTests.cpp).
//
// parseProcDevices() reads the text of /proc/bus/input/devices ("I: / N: / P: / S: / U: / H: / B:" blocks separated by an empty line);
// selectPowerButtons() picks the devices that can be the power button: an event node (Handlers: ... eventN) with EV_KEY and
// KEY_POWER (116) in the key bitmask, that is not a USB / Bluetooth device (a keyboard's own power key must not shut the
// MPC down), not a keyboard (letter keys), and not a MIDI / audio device. Best candidate first. Nothing is opened here.
//
// The bitmask lines are hex words, highest word first, space separated, each word an unsigned long of the KERNEL (32 bit on the
// MPC's ARMv7, 64 bit on a 64-bit kernel): "B: KEY=10000000000000 0" is KEY_POWER on a 64-bit kernel. The word size is taken
// from the text when a word is longer than 8 digits (then 64), else from the host `unsigned long` (the same kernel as the one
// that wrote /proc on the device), and can be given explicitly.
#pragma once

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace kick_input
{
    constexpr int kEvKey    = 1;
    constexpr int kKeyPower = 116;

    struct Device
    {
        std::string      name, phys, sysfs, handlers;
        int              bus{ 0 }, vendor{ 0 }, product{ 0 };
        std::vector<int> events; // N of every "eventN" handler
        std::string      ev, key; // the raw bitmask text ("3", "10000000000000 0")
    };

    struct Candidate
    {
        Device      dev;
        std::string path;  // /dev/input/eventN
        int         score; // higher = more likely the power button
    };

    namespace detail
    {
        inline std::string trim( std::string s )
        {
            size_t a = 0, b = s.size();
            while( a < b && std::isspace( (unsigned char)s[a] ) ) ++a;
            while( b > a && std::isspace( (unsigned char)s[b - 1] ) ) --b;
            return s.substr( a, b - a );
        }
        inline std::string lower( std::string s )
        {
            for( auto& c : s ) c = (char)std::tolower( (unsigned char)c );
            return s;
        }
        inline bool contains( const std::string& hay, const char* needle ) { return hay.find( needle ) != std::string::npos; }
        inline bool startsWith( const std::string& s, const char* p ) { return s.compare( 0, std::char_traits<char>::length( p ), p ) == 0; }

        inline std::vector<std::string> split( const std::string& s )
        {
            std::vector<std::string> out;
            size_t                   i = 0;
            while( i < s.size() )
            {
                while( i < s.size() && std::isspace( (unsigned char)s[i] ) ) ++i;
                size_t j = i;
                while( j < s.size() && !std::isspace( (unsigned char)s[j] ) ) ++j;
                if( j > i ) out.push_back( s.substr( i, j - i ) );
                i = j;
            }
            return out;
        }
        // "Key=value Key=value ..." of the I: line.
        inline int hexField( const std::string& line, const char* key )
        {
            const auto pos = line.find( key );
            if( pos == std::string::npos ) return 0;
            return (int)std::strtol( line.c_str() + pos + std::char_traits<char>::length( key ), nullptr, 16 );
        }
    }

    inline std::vector<Device> parseProcDevices( const std::string& text )
    {
        std::vector<Device> out;
        Device              cur;
        bool                any = false;
        const auto          flush = [&] {
            if( any ) out.push_back( cur );
            cur = Device();
            any = false;
        };
        size_t pos = 0;
        while( pos <= text.size() )
        {
            auto nl = text.find( '\n', pos );
            if( nl == std::string::npos ) nl = text.size();
            std::string line = detail::trim( text.substr( pos, nl - pos ) );
            pos              = nl + 1;
            if( line.empty() )
            {
                flush();
                continue;
            }
            if( line.size() < 3 || line[1] != ':' ) continue;
            const std::string body = detail::trim( line.substr( 2 ) );
            switch( line[0] )
            {
                case 'I':
                    cur.bus     = detail::hexField( body, "Bus=" );
                    cur.vendor  = detail::hexField( body, "Vendor=" );
                    cur.product = detail::hexField( body, "Product=" );
                    any         = true;
                    break;
                case 'N':
                {
                    const auto q1 = body.find( '"' );
                    const auto q2 = body.rfind( '"' );
                    if( detail::startsWith( body, "Name=" ) )
                        cur.name = ( q1 != std::string::npos && q2 > q1 ) ? body.substr( q1 + 1, q2 - q1 - 1 ) : detail::trim( body.substr( 5 ) );
                    any = true;
                    break;
                }
                case 'P':
                    if( detail::startsWith( body, "Phys=" ) ) cur.phys = body.substr( 5 );
                    break;
                case 'S':
                    if( detail::startsWith( body, "Sysfs=" ) ) cur.sysfs = body.substr( 6 );
                    break;
                case 'H':
                    if( detail::startsWith( body, "Handlers=" ) )
                    {
                        cur.handlers = detail::trim( body.substr( 9 ) );
                        for( const auto& t : detail::split( cur.handlers ) )
                            if( detail::startsWith( t, "event" ) && t.size() > 5 && std::isdigit( (unsigned char)t[5] ) )
                                cur.events.push_back( std::atoi( t.c_str() + 5 ) );
                    }
                    break;
                case 'B':
                    if( detail::startsWith( body, "EV=" ) ) cur.ev = detail::trim( body.substr( 3 ) );
                    else if( detail::startsWith( body, "KEY=" ) ) cur.key = detail::trim( body.substr( 4 ) );
                    break;
                default: break;
            }
        }
        flush();
        return out;
    }

    // Is `bit` set in the bitmask text. wordBits 0 = detect (see the header comment). Malformed words count as zero bits.
    inline bool hasBit( const std::string& words, int bit, int wordBits = 0 )
    {
        if( bit < 0 ) return false;
        const auto tokens = detail::split( words );
        if( tokens.empty() ) return false;
        if( wordBits != 32 && wordBits != 64 )
        {
            wordBits = (int)sizeof( unsigned long ) * 8;
            for( const auto& t : tokens )
                if( t.size() > 8 ) wordBits = 64;
        }
        const size_t word = (size_t)bit / (size_t)wordBits; // 0 = the last token
        if( word >= tokens.size() ) return false;
        const std::string& t = tokens[tokens.size() - 1 - word];
        for( char c : t )
            if( !std::isxdigit( (unsigned char)c ) ) return false;
        const unsigned long long v = std::strtoull( t.c_str(), nullptr, 16 );
        return ( ( v >> ( (size_t)bit % (size_t)wordBits ) ) & 1ULL ) != 0;
    }

    inline int countBits( const std::string& words )
    {
        int n = 0;
        for( const auto& t : detail::split( words ) )
            for( char c : t )
                if( std::isxdigit( (unsigned char)c ) )
                {
                    int v = std::isdigit( (unsigned char)c ) ? c - '0' : std::tolower( (unsigned char)c ) - 'a' + 10;
                    for( ; v; v >>= 1 ) n += v & 1;
                }
        return n;
    }

    // Letter keys set = a keyboard.
    inline bool looksLikeKeyboard( const Device& d, int wordBits = 0 )
    {
        int letters = 0;
        for( int k = 16; k <= 50; ++k )
            if( ( k <= 25 || ( k >= 30 && k <= 38 ) || k >= 44 ) && hasBit( d.key, k, wordBits ) ) ++letters;
        return letters >= 8;
    }

    // Name based score of a device that can send KEY_POWER; <= 0 = not a candidate.
    inline int powerScore( const Device& d, int wordBits = 0 )
    {
        using namespace detail;
        if( d.events.empty() ) return 0;
        if( !hasBit( d.ev, kEvKey, wordBits ) || !hasBit( d.key, kKeyPower, wordBits ) ) return 0;
        if( d.bus == 0x03 || d.bus == 0x05 ) return 0; // USB / Bluetooth: external keyboards, consumer-control and MIDI devices
        const std::string n = lower( d.name ), p = lower( d.phys );
        if( startsWith( p, "usb" ) ) return 0;
        for( const char* bad : { "midi", "audio", "akai", "mpc", "keyboard", "mouse", "touch", "pad", "headphone", "jack" } )
            if( contains( n, bad ) ) return 0;
        if( looksLikeKeyboard( d, wordBits ) ) return 0;
        int score = 10;
        if( n == "power button" ) score = 100;
        else if( contains( n, "pwrkey" ) || contains( n, "pwrbutton" ) || contains( n, "power" ) ) score = 80;
        else if( contains( n, "gpio-keys" ) || contains( n, "gpio_keys" ) || contains( n, "gpio keys" ) ) score = 60;
        else if( contains( n, "pmic" ) || contains( n, "onkey" ) || contains( n, "rtc" ) ) score = 50;
        else if( contains( p, "button" ) || contains( p, "pwr" ) ) score = 40;
        // few keys: a dedicated button is better than a many-key matrix
        const int keys = countBits( d.key );
        score -= keys > 60 ? 9 : 0;
        return score > 0 ? score : 1;
    }

    // Every candidate (best first, then the lower event number); one path per eventN of a device.
    inline std::vector<Candidate> selectPowerButtons( const std::vector<Device>& devices, int wordBits = 0 )
    {
        std::vector<Candidate> out;
        for( const auto& d : devices )
        {
            const int s = powerScore( d, wordBits );
            if( s <= 0 ) continue;
            for( int e : d.events ) out.push_back( { d, "/dev/input/event" + std::to_string( e ), s } );
        }
        for( size_t i = 1; i < out.size(); ++i ) // stable insertion sort, the list is tiny
            for( size_t j = i; j > 0 && out[j].score > out[j - 1].score; --j ) std::swap( out[j], out[j - 1] );
        return out;
    }

    // struct input_event = struct timeval (two longs) + __u16 type + __u16 code + __s32 value on every Linux ABI.
    constexpr size_t kEventSize = 2 * sizeof( long ) + 8;

    // Number of power-key PRESSES (EV_KEY, KEY_POWER, value 1; 0 = release and 2 = autorepeat do not count) in a read() buffer of
    // whole events. Short trailing bytes are ignored.
    inline int countPowerPresses( const unsigned char* buf, size_t len, size_t eventSize = kEventSize )
    {
        if( eventSize < 8 ) return 0;
        int n = 0;
        for( size_t off = 0; off + eventSize <= len; off += eventSize )
        {
            const unsigned char* e = buf + off + eventSize - 8;
            uint16_t             type, code;
            int32_t              value;
            std::memcpy( &type, e, 2 );
            std::memcpy( &code, e + 2, 2 );
            std::memcpy( &value, e + 4, 4 );
            if( type == kEvKey && code == kKeyPower && value == 1 ) ++n;
        }
        return n;
    }
}
