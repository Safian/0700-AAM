// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test of the power button discovery (/proc/bus/input/devices parser + selection), the input_event decoding and the watcher
// thread (read from a pipe instead of /dev/input/eventN; failing open is logged and harmless).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -pthread -o /tmp/t PowerKeyTests.cpp
// The fixtures are in the real /proc/bus/input/devices format; the MPC's own device list is NOT measured yet (see the R-12 report).
#include "../../apps/kick/hardware/power_key_watcher.h"
#include <cstdio>
#include <cstdlib>
#include <string>

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

using namespace kick_input;

// A PC (64-bit kernel): ACPI Power Button (x2: PNP0C0C and LNXPWRBN), Sleep Button, a USB keyboard with its consumer control
// (which also reports KEY_POWER), a mouse.
static const char* kPc = R"(I: Bus=0019 Vendor=0000 Product=0001 Version=0000
N: Name="Power Button"
P: Phys=PNP0C0C/button/input0
S: Sysfs=/devices/LNXSYSTM:00/LNXSYBUS:00/PNP0C0C:00/input/input0
U: Uniq=
H: Handlers=kbd event0 
B: PROP=0
B: EV=3
B: KEY=10000000000000 0

I: Bus=0019 Vendor=0000 Product=0003 Version=0000
N: Name="Sleep Button"
P: Phys=PNP0C0E/button/input0
S: Sysfs=/devices/LNXSYSTM:00/LNXSYBUS:00/PNP0C0E:00/input/input1
U: Uniq=
H: Handlers=kbd event1 
B: PROP=0
B: EV=3
B: KEY=4000 0 0

I: Bus=0003 Vendor=046d Product=c31c Version=0110
N: Name="Logitech USB Keyboard"
P: Phys=usb-0000:00:14.0-2/input0
S: Sysfs=/devices/pci0000:00/0000:00:14.0/usb1/1-2/1-2:1.0/0003:046D:C31C.0001/input/input5
U: Uniq=
H: Handlers=sysrq kbd leds event2 
B: PROP=0
B: EV=120013
B: KEY=1000000000007 ff9f207ac14057ff febeffdfffefffff fffffffffffffffe
B: MSC=10
B: LED=7

I: Bus=0003 Vendor=046d Product=c31c Version=0110
N: Name="Logitech USB Keyboard Consumer Control"
P: Phys=usb-0000:00:14.0-2/input1
S: Sysfs=/devices/pci0000:00/0000:00:14.0/usb1/1-2/1-2:1.1/0003:046D:C31C.0002/input/input6
U: Uniq=
H: Handlers=kbd event3 
B: PROP=0
B: EV=1f
B: KEY=300ff 0 0 483ffff17aff32d bfd4444600000000 1 130c730b17c000 267bfad9415fed 9e168000004400 10000002
B: REL=1040
B: ABS=100000000
B: MSC=10

I: Bus=0019 Vendor=0000 Product=0001 Version=0000
N: Name="Power Button"
P: Phys=LNXPWRBN/button/input0
S: Sysfs=/devices/LNXSYSTM:00/LNXPWRBN:00/input/input7
U: Uniq=
H: Handlers=kbd event4 
B: PROP=0
B: EV=3
B: KEY=10000000000000 0

I: Bus=0003 Vendor=046d Product=c077 Version=0111
N: Name="Logitech USB Optical Mouse"
P: Phys=usb-0000:00:14.0-3/input0
S: Sysfs=/devices/pci0000:00/0000:00:14.0/usb1/1-3/1-3:1.0/0003:046D:C077.0003/input/input8
U: Uniq=
H: Handlers=mouse0 event5 
B: PROP=0
B: EV=17
B: KEY=1f0000 0 0 0 0
B: REL=903
B: MSC=10
)";

// An i.MX style embedded board (32-bit kernel): gpio-keys with the power key, a touch screen, a USB Akai device that shows up as
// HID with a KEY bitmask that happens to contain KEY_POWER (must NOT be chosen), a codec jack.
static const char* kEmbedded = R"(I: Bus=0019 Vendor=0001 Product=0001 Version=0100
N: Name="gpio-keys"
P: Phys=gpio-keys/input0
S: Sysfs=/devices/platform/gpio-keys/input/input0
U: Uniq=
H: Handlers=kbd event0 
B: PROP=0
B: EV=3
B: KEY=100000 0 0 0

I: Bus=0018 Vendor=0000 Product=0000 Version=0000
N: Name="Goodix Capacitive TouchScreen"
P: Phys=input/ts
S: Sysfs=/devices/platform/soc/21a0000.i2c/i2c-0/0-005d/input/input1
U: Uniq=
H: Handlers=event1 
B: PROP=2
B: EV=b
B: KEY=400 0 0 0 0 0
B: ABS=260800000000003

I: Bus=0003 Vendor=09e8 Product=0063 Version=0100
N: Name="Akai Professional MPC Key 61"
P: Phys=usb-ci_hdrc.1-1/input0
S: Sysfs=/devices/platform/soc/2184200.usb/ci_hdrc.1/usb1/1-1/1-1:1.0/input/input2
U: Uniq=
H: Handlers=kbd event2 
B: PROP=0
B: EV=13
B: KEY=100000 0 0 0
B: MSC=10

I: Bus=0000 Vendor=0000 Product=0000 Version=0000
N: Name="imx-audio Headphone Jack"
P: ALSA
S: Sysfs=/devices/platform/sound/sound/card0/input3
U: Uniq=
H: Handlers=kbd event3 
B: PROP=0
B: EV=21
B: SW=4
)";

// A PMIC power key on a 64-bit SoC next to gpio-keys that has many keys (volume, home, ...).
static const char* kPmic = R"(I: Bus=0019 Vendor=0001 Product=0001 Version=0100
N: Name="gpio-keys"
P: Phys=gpio-keys/input0
S: Sysfs=/devices/platform/gpio-keys/input/input0
U: Uniq=
H: Handlers=kbd event0 
B: PROP=0
B: EV=3
B: KEY=10000000000000 0

I: Bus=0000 Vendor=0001 Product=0001 Version=0100
N: Name="pm8941_pwrkey"
P: pm8941_pwrkey/input0
S: Sysfs=/devices/platform/soc/pwrkey/input/input1
U: Uniq=
H: Handlers=kbd event1 
B: PROP=0
B: EV=3
B: KEY=10000000000000 0
)";

// No power button at all: a USB MIDI controller and a mouse only.
static const char* kNone = R"(I: Bus=0003 Vendor=09e8 Product=0063 Version=0100
N: Name="Akai Professional MPC Key 61 MIDI"
P: Phys=usb-0000:00:14.0-1/input0
S: Sysfs=/devices/pci0000:00/usb1/1-1/input/input2
U: Uniq=
H: Handlers=event0 
B: PROP=0
B: EV=1f
B: KEY=300ff 0 0 483ffff17aff32d bfd4444600000000 1 130c730b17c000 267bfad9415fed 9e168000004400 10000002
)";

static void put( unsigned char* b, int off, uint16_t type, uint16_t code, int32_t value, size_t es = kEventSize )
{
    std::memset( b + off, 0, es );
    std::memcpy( b + off + es - 8, &type, 2 );
    std::memcpy( b + off + es - 6, &code, 2 );
    std::memcpy( b + off + es - 4, &value, 4 );
}

int main()
{
    // bitmasks
    CHECK( hasBit( "10000000000000 0", 116, 64 ) );
    CHECK( hasBit( "10000000000000 0", 116 ) ); // 14 digits: detected as 64 bit
    CHECK( !hasBit( "10000000000000 0", 115, 64 ) && !hasBit( "10000000000000 0", 0, 64 ) );
    CHECK( hasBit( "100000 0 0 0", 116, 32 ) && !hasBit( "100000 0 0 0", 117, 32 ) );
    CHECK( !hasBit( "100000 0 0 0", 116, 64 ) ); // read as 64 bit words it is a different key
    CHECK( hasBit( "3", 1, 32 ) && hasBit( "3", 0, 64 ) && !hasBit( "3", 2, 64 ) );
    CHECK( !hasBit( "", 1 ) && !hasBit( "zz", 1 ) && !hasBit( "3", -1 ) && !hasBit( "3", 900 ) );

    // parser
    {
        const auto d = parseProcDevices( kPc );
        CHECK( d.size() == 6 );
        CHECK( d[0].name == "Power Button" && d[0].bus == 0x19 && d[0].events == std::vector<int>{ 0 } && d[0].ev == "3" );
        CHECK( d[2].handlers == "sysrq kbd leds event2" && d[2].events == std::vector<int>{ 2 } && d[2].bus == 3 && d[2].vendor == 0x46d );
        CHECK( d[5].events == std::vector<int>{ 5 } ); // "mouse0 event5": mouse0 is not an event node
        CHECK( d[1].phys == "PNP0C0E/button/input0" );
        CHECK( parseProcDevices( "" ).empty() );
        CHECK( parseProcDevices( "garbage\n\nmore garbage" ).empty() );
        // no trailing empty line, CRLF-free, a device without handlers
        const auto e = parseProcDevices( "I: Bus=0019 Vendor=0001 Product=0001 Version=0100\nN: Name=\"x\"\nB: EV=3\nB: KEY=10000000000000 0" );
        CHECK( e.size() == 1 && e[0].events.empty() && e[0].key == "10000000000000 0" );
        CHECK( selectPowerButtons( e ).empty() ); // no event node: cannot be read
    }

    // selection: PC: both ACPI "Power Button" nodes, never the keyboard (even though it reports KEY_POWER) / sleep button / mouse
    {
        const auto c = selectPowerButtons( parseProcDevices( kPc ), 64 );
        CHECK( c.size() == 2 );
        CHECK( c.size() == 2 && c[0].path == "/dev/input/event0" && c[1].path == "/dev/input/event4" );
        CHECK( c.size() == 2 && c[0].score == 100 );
        const auto kb = parseProcDevices( kPc );
        CHECK( powerScore( kb[3], 64 ) == 0 ); // USB consumer control with KEY_POWER
        CHECK( hasBit( kb[3].key, kKeyPower, 64 ) );
        CHECK( powerScore( kb[2], 64 ) == 0 ); // USB keyboard
        CHECK( powerScore( kb[1], 64 ) == 0 ); // Sleep Button: no KEY_POWER
    }
    // embedded 32-bit kernel: gpio-keys only (not the USB Akai node that has the same bitmask, not the touch screen / jack)
    {
        const auto c = selectPowerButtons( parseProcDevices( kEmbedded ), 32 );
        CHECK( c.size() == 1 && c[0].path == "/dev/input/event0" && c[0].dev.name == "gpio-keys" );
        const auto d = parseProcDevices( kEmbedded );
        CHECK( powerScore( d[2], 32 ) == 0 ); // Akai USB (bus 3, name)
        CHECK( powerScore( d[1], 32 ) == 0 && powerScore( d[3], 32 ) == 0 );
    }
    // pwrkey ranks before gpio-keys
    {
        const auto c = selectPowerButtons( parseProcDevices( kPmic ), 64 );
        CHECK( c.size() == 2 && c[0].path == "/dev/input/event1" && c[1].path == "/dev/input/event0" );
    }
    // no candidate
    CHECK( selectPowerButtons( parseProcDevices( kNone ) ).empty() );
    CHECK( selectPowerButtons( {} ).empty() );

    // input_event decoding (both ABI sizes), only value 1 of KEY_POWER counts
    {
        unsigned char b[64 * 4];
        for( size_t es : { (size_t)16, (size_t)24 } )
        {
            std::memset( b, 0, sizeof( b ) );
            auto at = [&]( int i ) { return (int)( es * (size_t)i ); };
            put( b, at( 0 ), 4, 4, 0x70072, es );               // EV_MSC
            std::memset( b + at( 1 ), 0, es );              // EV_SYN
            put( b, at( 2 ), 1, 116, 1, es );                   // press
            put( b, at( 3 ), 1, 116, 0, es );                   // release
            put( b, at( 4 ), 1, 116, 2, es );                   // autorepeat
            put( b, at( 5 ), 1, 115, 1, es );                   // other key
            put( b, at( 6 ), 1, 116, 1, es );                   // press again
            CHECK( countPowerPresses( b, es * 7, es ) == 2 );
            CHECK( countPowerPresses( b, es * 3, es ) == 1 );
            CHECK( countPowerPresses( b, es * 3 - 1, es ) == 0 ); // short read: ignored
            CHECK( countPowerPresses( b, 0, es ) == 0 );
        }
        CHECK( kEventSize == 2 * sizeof( long ) + 8 );
    }

    // watcher: a pipe stands in for /dev/input/eventN
    {
        int p[2];
        CHECK( ::pipe( p ) == 0 );
        std::atomic<int> presses{ 0 };
        PowerKeyWatcher  w;
        const int        n = w.start( { "fake0" }, [&] { ++presses; }, [&]( const std::string& ) { return p[0]; }, 300 );
        CHECK( n == 1 && w.active() );
        unsigned char b[kEventSize * 3];
        put( b, 0, 4, 4, 1 );
        put( b, (int)kEventSize, 1, 116, 1 );
        put( b, (int)kEventSize * 2, 1, 116, 0 );
        CHECK( ::write( p[1], b, sizeof( b ) ) == (ssize_t)sizeof( b ) );
        for( int i = 0; i < 100 && presses.load() < 1; ++i ) usleep( 20000 );
        CHECK( presses.load() == 1 );
        // a second press within the debounce time is ignored
        CHECK( ::write( p[1], b, sizeof( b ) ) == (ssize_t)sizeof( b ) );
        usleep( 100000 );
        CHECK( presses.load() == 1 );
        // after it counts again
        usleep( 350000 );
        CHECK( ::write( p[1], b, sizeof( b ) ) == (ssize_t)sizeof( b ) );
        for( int i = 0; i < 100 && presses.load() < 2; ++i ) usleep( 20000 );
        CHECK( presses.load() == 2 );
        // release / other events alone do nothing
        put( b, 0, 1, 116, 0 );
        CHECK( ::write( p[1], b, kEventSize ) == (ssize_t)kEventSize );
        usleep( 100000 );
        CHECK( presses.load() == 2 );
        w.stop(); // joins the thread, closes the read end
        CHECK( !w.active() );
        ::close( p[1] );
        w.stop(); // idempotent
    }
    // watcher: nothing can be opened -> 0 devices, no thread, error logged (kick_log), no crash
    {
        PowerKeyWatcher w;
        std::atomic<int> presses{ 0 };
        const int n = w.start( { "/dev/input/event-does-not-exist-12345" }, [&] { ++presses; } );
        CHECK( n == 0 && !w.active() );
        const int m = w.start( { "a", "b" }, [&] { ++presses; }, []( const std::string& ) { errno = EACCES; return -1; } );
        CHECK( m == 0 && !w.active() );
        w.stop();
    }
    // watcher: discovery from a fixture file through the env override, the opened device is the chosen one
    {
        const std::string path = "/tmp/kick_power_key_test_devices.txt";
        FILE*             f    = std::fopen( path.c_str(), "w" );
        CHECK( f != nullptr );
        if( f ) { std::fputs( kPmic, f ); std::fclose( f ); }
        ::setenv( "KICK_PROC_INPUT_DEVICES", path.c_str(), 1 );
        ::unsetenv( "KICK_POWER_KEY_DEVICE" );
        PowerKeyWatcher w;
        const int       n = startPowerKeyWatcher( w, [] {} ); // /dev/input/event1 and 0 do not exist on a Mac: error logged, 0 opened
        CHECK( n >= 0 );
        w.stop();
        ::setenv( "KICK_PROC_INPUT_DEVICES", "/tmp/kick_power_key_test_missing.txt", 1 );
        CHECK( startPowerKeyWatcher( w, [] {} ) == 0 );
        ::unsetenv( "KICK_PROC_INPUT_DEVICES" );
        std::remove( path.c_str() );
    }
    if( g_fail == 0 ) std::printf( "PowerKeyTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
