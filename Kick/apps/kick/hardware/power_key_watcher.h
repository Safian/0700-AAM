// SPDX-License-Identifier: AGPL-3.0-or-later
// Power button watcher: a thread reads the power button's /dev/input/eventN (struct input_event, EV_KEY, KEY_POWER, value 1 =
// press) and calls `onPress` on that thread; the caller hands it to the message thread. JUCE-free, POSIX only, host-tested with
// pipes: PowerKeyTests.cpp. The device is only read (no EVIOCGRAB): logind still sees the key, which is why the bootstrap starts
// Kick under `systemd-inhibit --what=handle-power-key` (docs/akai_mpc_hardware/tools/tkgl_bootstrap), else the system would
// handle the same press (poweroff) as well.
//
// If no device is found or none can be opened, an error is logged and the watcher is simply inactive: the Menu "Power" tile
// always works.
#pragma once

#include "input_devices.h"
#include "../../../modules/common/source/kick_logger.h"
#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <poll.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace kick_input
{
    class PowerKeyWatcher
    {
    public:
        using OpenFn = std::function<int( const std::string& )>; // returns a readable fd or -1 (errno set)

        PowerKeyWatcher() = default;
        ~PowerKeyWatcher() { stop(); }
        PowerKeyWatcher( const PowerKeyWatcher& )            = delete;
        PowerKeyWatcher& operator=( const PowerKeyWatcher& ) = delete;

        static int openDevice( const std::string& path ) { return ::open( path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK ); }

        // Opens every path; returns how many could be opened (each failure is logged as an error). With none, no thread starts.
        // `debounceMs`: presses closer than this to the previous one are ignored (two nodes of one button, a bouncing contact).
        int start( const std::vector<std::string>& paths, std::function<void()> onPress, OpenFn open = &PowerKeyWatcher::openDevice, int debounceMs = 800 )
        {
            stop();
            for( const auto& p : paths )
            {
                errno    = 0;
                const int fd = open( p );
                if( fd < 0 )
                {
                    kick_log( "error", "[ERROR] [POWERKEY] cannot open %s: %s\n", p.c_str(), std::strerror( errno ) );
                    continue;
                }
                kick_log( "info", "[INFO] [POWERKEY] watching %s\n", p.c_str() );
                fds_.push_back( fd );
            }
            if( fds_.empty() ) return 0;
            if( ::pipe( wake_ ) != 0 )
            {
                kick_log( "error", "[ERROR] [POWERKEY] pipe failed: %s\n", std::strerror( errno ) );
                closeAll();
                return 0;
            }
            running_.store( true );
            thread_ = std::thread( [this, onPress = std::move( onPress ), debounceMs] { loop( onPress, debounceMs ); } );
            return (int)fds_.size();
        }

        void stop()
        {
            if( thread_.joinable() )
            {
                running_.store( false );
                const char b = 1;
                (void)!::write( wake_[1], &b, 1 );
                thread_.join();
            }
            closeAll();
        }

        bool active() const { return running_.load(); }

    private:
        void closeAll()
        {
            for( int fd : fds_ ) ::close( fd );
            fds_.clear();
            for( int& w : wake_ )
                if( w >= 0 ) { ::close( w ); w = -1; }
            running_.store( false );
        }

        void loop( const std::function<void()>& onPress, int debounceMs )
        {
            using clock = std::chrono::steady_clock;
            std::vector<pollfd> pfds;
            for( int fd : fds_ ) pfds.push_back( { fd, POLLIN, 0 } );
            pfds.push_back( { wake_[0], POLLIN, 0 } );
            clock::time_point last{};
            bool              haveLast = false;
            unsigned char     buf[kEventSize * 32];
            while( running_.load() )
            {
                const int r = ::poll( pfds.data(), (nfds_t)pfds.size(), -1 );
                if( r < 0 )
                {
                    if( errno == EINTR ) continue;
                    kick_log( "error", "[ERROR] [POWERKEY] poll failed: %s\n", std::strerror( errno ) );
                    break;
                }
                if( pfds.back().revents ) break; // stop()
                for( size_t i = 0; i + 1 < pfds.size(); ++i )
                {
                    if( pfds[i].revents & ( POLLERR | POLLHUP | POLLNVAL ) )
                    {
                        kick_log( "error", "[ERROR] [POWERKEY] device %zu closed or failed (revents 0x%x), no longer watched\n", i, (unsigned)pfds[i].revents );
                        pfds[i].fd = -1; // poll ignores negative fds
                        continue;
                    }
                    if( !( pfds[i].revents & POLLIN ) ) continue;
                    const ssize_t n = ::read( pfds[i].fd, buf, sizeof( buf ) );
                    if( n <= 0 ) continue; // EAGAIN
                    if( countPowerPresses( buf, (size_t)n ) <= 0 ) continue;
                    const auto now = clock::now();
                    if( haveLast && now - last < std::chrono::milliseconds( debounceMs ) ) continue;
                    last     = now;
                    haveLast = true;
                    kick_log( "info", "[INFO] [POWERKEY] KEY_POWER pressed\n" );
                    onPress();
                }
            }
            running_.store( false );
        }

        std::vector<int>  fds_;
        int               wake_[2]{ -1, -1 };
        std::thread       thread_;
        std::atomic<bool> running_{ false };
    };

    inline std::string readFile( const std::string& path )
    {
        std::string out;
        const int   fd = ::open( path.c_str(), O_RDONLY | O_CLOEXEC );
        if( fd < 0 ) return out;
        char    b[1024];
        ssize_t n;
        while( ( n = ::read( fd, b, sizeof( b ) ) ) > 0 ) out.append( b, (size_t)n );
        ::close( fd );
        return out;
    }

    // Finds the power button from /proc/bus/input/devices (KICK_PROC_INPUT_DEVICES overrides the path for tests) and starts the
    // watcher on it. KICK_POWER_KEY_DEVICE=/dev/input/eventN forces the device list (comma separated). Logs every input device it
    // saw (name, handlers, whether it can send KEY_POWER) so the choice can be checked from kick.log. Returns the opened devices.
    inline int startPowerKeyWatcher( PowerKeyWatcher& w, std::function<void()> onPress )
    {
        std::vector<std::string> paths;
        if( const char* forced = std::getenv( "KICK_POWER_KEY_DEVICE" ); forced && *forced )
        {
            for( auto& t : detail::split( [&] { std::string s = forced; for( auto& c : s ) if( c == ',' ) c = ' '; return s; }() ) ) paths.push_back( t );
            kick_log( "info", "[INFO] [POWERKEY] KICK_POWER_KEY_DEVICE forces %zu device(s)\n", paths.size() );
        }
        else
        {
            const char* procEnv = std::getenv( "KICK_PROC_INPUT_DEVICES" );
            const std::string procPath = procEnv && *procEnv ? procEnv : "/proc/bus/input/devices";
            const std::string text = readFile( procPath );
            if( text.empty() )
            {
                kick_log( "error", "[ERROR] [POWERKEY] cannot read %s: the power button is not watched (Menu > Power still works)\n", procPath.c_str() );
                return 0;
            }
            const auto devices = parseProcDevices( text );
            for( const auto& d : devices )
                kick_log( "info", "[INFO] [POWERKEY] input '%s' bus=%04x handlers='%s' ev=%s key-power=%d score=%d\n", d.name.c_str(), d.bus, d.handlers.c_str(), d.ev.c_str(),
                          hasBit( d.key, kKeyPower ) ? 1 : 0, powerScore( d ) );
            for( const auto& c : selectPowerButtons( devices ) ) paths.push_back( c.path );
            if( paths.empty() )
            {
                kick_log( "error", "[ERROR] [POWERKEY] no power button device found in %s (Menu > Power still works)\n", procPath.c_str() );
                return 0;
            }
        }
        const int opened = w.start( paths, std::move( onPress ) );
        if( opened == 0 ) kick_log( "error", "[ERROR] [POWERKEY] no power button device could be opened (Menu > Power still works)\n" );
        return opened;
    }
}
