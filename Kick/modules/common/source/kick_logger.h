/*
 * Copyright (c) 2024 MUSIC Tribe / Safian
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cstdlib>
#include <mutex>
#include <string>
#include <unordered_set>
#include <algorithm>
#include <cctype>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#if defined(__linux__)
 #include <sys/syscall.h>
#elif defined(__APPLE__)
 #include <pthread.h>
#else
 #include <thread>
#endif

namespace kick_logger
{
    // Constants
    static constexpr size_t CRASH_LOG_LINES = 50;
    static constexpr size_t CRASH_LOG_LINE_MAX = 512;
    static constexpr size_t DEFAULT_MAX_FILE_SIZE = 2 * 1024 * 1024; // 2 MB

    // Pre-allocated static ring buffer for the last 50 log lines.
    // Dump is completely async-signal-safe (uses only open, write, close).
    struct CrashRingBuffer
    {
        char                  lines[CRASH_LOG_LINES][CRASH_LOG_LINE_MAX];
        uint16_t              lengths[CRASH_LOG_LINES];
        std::atomic<uint64_t> write_count{ 0 };

        void append( const char* str, size_t len ) noexcept
        {
            if( !str || len == 0 ) return;
            if( len >= CRASH_LOG_LINE_MAX ) len = CRASH_LOG_LINE_MAX - 1;
            const uint64_t idx = write_count.fetch_add( 1, std::memory_order_relaxed ) % CRASH_LOG_LINES;
            std::memcpy( lines[idx], str, len );
            lines[idx][len] = '\0';
            lengths[idx] = static_cast<uint16_t>( len );
        }

        void dump_async_signal_safe( int fd ) const noexcept
        {
            if( fd < 0 ) return;
            const uint64_t total = write_count.load( std::memory_order_relaxed );
            const uint64_t count = ( total < CRASH_LOG_LINES ) ? total : CRASH_LOG_LINES;
            const uint64_t start = ( total < CRASH_LOG_LINES ) ? 0 : ( total % CRASH_LOG_LINES );
            for( uint64_t i = 0; i < count; ++i )
            {
                const uint64_t idx = ( start + i ) % CRASH_LOG_LINES;
                const uint16_t len = lengths[idx];
                if( len > 0 )
                {
                    (void)!::write( fd, lines[idx], len );
                }
            }
        }

        void dump_async_signal_safe( const char* path ) const noexcept
        {
            if( !path || !*path ) return;
            const int fd = ::open( path, O_WRONLY | O_CREAT | O_TRUNC, 0644 );
            if( fd >= 0 )
            {
                dump_async_signal_safe( fd );
                ::close( fd );
            }
        }

        void clear() noexcept
        {
            write_count.store( 0, std::memory_order_relaxed );
            for( size_t i = 0; i < CRASH_LOG_LINES; ++i ) lengths[i] = 0;
        }
    };

    inline CrashRingBuffer g_crash_buffer;

    // Async-signal-safe dump of last 50 log lines to specified file or /data/kick.crash.log
    inline void dump_crash_log_async_signal_safe( const char* path = "/data/kick.crash.log" ) noexcept
    {
        g_crash_buffer.dump_async_signal_safe( path );
    }

    inline void dump_crash_log_async_signal_safe( int fd ) noexcept
    {
        g_crash_buffer.dump_async_signal_safe( fd );
    }

    // Monotonic uptime in ms
    inline uint64_t get_uptime_ms() noexcept
    {
        using namespace std::chrono;
        static const auto start_time = steady_clock::now();
        return static_cast<uint64_t>( duration_cast<milliseconds>( steady_clock::now() - start_time ).count() );
    }

    // Thread ID
    inline uint64_t get_thread_id() noexcept
    {
#if defined(__linux__)
        return static_cast<uint64_t>( ::syscall( SYS_gettid ) );
#elif defined(__APPLE__)
        uint64_t tid = 0;
        pthread_threadid_np( nullptr, &tid );
        return tid;
#else
        return static_cast<uint64_t>( std::hash<std::thread::id>{}( std::this_thread::get_id() ) );
#endif
    }

    inline std::string to_lower_trimmed( const std::string& str )
    {
        size_t start = 0;
        while( start < str.size() && std::isspace( static_cast<unsigned char>( str[start] ) ) ) ++start;
        size_t end = str.size();
        while( end > start && std::isspace( static_cast<unsigned char>( str[end - 1] ) ) ) --end;
        std::string res;
        res.reserve( end - start );
        for( size_t i = start; i < end; ++i )
            res.push_back( static_cast<char>( std::tolower( static_cast<unsigned char>( str[i] ) ) ) );
        return res;
    }

    class Logger
    {
    public:
        static Logger& instance()
        {
            static Logger inst;
            return inst;
        }

        void init( const char* custom_path = nullptr, size_t custom_max_size = 0, bool redirect_stdio = false )
        {
            std::lock_guard<std::mutex> lock( m_mutex );
            close_file_unlocked();

            load_categories_from_env();

            // Determine log file path
            if( custom_path && *custom_path )
            {
                m_log_path = custom_path;
            }
            else if( const char* env_path = std::getenv( "KICK_LOG_FILE" ) )
            {
                if( *env_path ) m_log_path = env_path;
            }
            else
            {
                // Auto-detect MPC: /data exists and is a directory
                struct stat st {};
                if( ::stat( "/data", &st ) == 0 && S_ISDIR( st.st_mode ) )
                {
                    m_log_path = "/data/kick.log";
                    if( !std::getenv( "KICK_LOG_NO_REDIRECT" ) ) redirect_stdio = true;
                }
            }

            // Max file size
            if( custom_max_size > 0 )
            {
                m_max_file_size = custom_max_size;
            }
            else if( const char* env_size = std::getenv( "KICK_LOG_MAX_SIZE" ) )
            {
                char* end = nullptr;
                unsigned long long s = std::strtoull( env_size, &end, 10 );
                if( s > 0 ) m_max_file_size = static_cast<size_t>( s );
            }
            else
            {
                m_max_file_size = DEFAULT_MAX_FILE_SIZE;
            }

            m_redirect_stdio = redirect_stdio;

            // Startup rotation: kick.log -> kick.log.1 if existing and non-empty
            if( !m_log_path.empty() )
            {
                struct stat st {};
                if( ::stat( m_log_path.c_str(), &st ) == 0 && st.st_size > 0 )
                {
                    std::string rot = m_log_path + ".1";
                    ::rename( m_log_path.c_str(), rot.c_str() );
                }
                open_file_unlocked();
            }
        }

        void shutdown()
        {
            std::lock_guard<std::mutex> lock( m_mutex );
            close_file_unlocked();
            m_log_path.clear();
        }

        void set_categories( const std::string& cat_str )
        {
            std::lock_guard<std::mutex> lock( m_mutex );
            parse_categories_unlocked( cat_str );
        }

        bool is_category_enabled( const char* cat ) const noexcept
        {
            if( !cat || !*cat ) return true;
            std::string c = to_lower_trimmed( cat );
            // Errors and warnings always enabled
            if( c == "error" || c == "err" || c == "warn" || c == "warning" ) return true;

            std::lock_guard<std::mutex> lock( m_mutex );
            if( m_enabled_categories.find( c ) != m_enabled_categories.end() ) return true;
            // Wildcard 'all' or '*' enables everything EXCEPT 'frame'
            if( m_wildcard_all && c != "frame" ) return true;
            // 'proj' is also enabled if 'info' is enabled
            if( c == "proj" && m_enabled_categories.find( "info" ) != m_enabled_categories.end() ) return true;
            return false;
        }

        void log_v( const char* category, const char* fmt, va_list args )
        {
            if( !is_category_enabled( category ) ) return;

            // Format prefix with timestamp (ms) and thread id
            char prefix[48];
            const int plen = std::snprintf( prefix, sizeof( prefix ), "[%6llums] [t:%-5llu] ",
                                            static_cast<unsigned long long>( get_uptime_ms() ),
                                            static_cast<unsigned long long>( get_thread_id() ) );

            // Format body
            char body[1024];
            va_list args_copy;
            va_copy( args_copy, args );
            const int blen = std::vsnprintf( body, sizeof( body ), fmt, args_copy );
            va_end( args_copy );

            char full[1280];
            int total_len = 0;
            if( plen > 0 && blen >= 0 )
            {
                total_len = std::snprintf( full, sizeof( full ), "%s%s", prefix, body );
            }
            if( total_len <= 0 ) return;

            // Ensure newline at the end
            if( full[total_len - 1] != '\n' && total_len < static_cast<int>( sizeof( full ) - 1 ) )
            {
                full[total_len++] = '\n';
                full[total_len] = '\0';
            }

            // Append to crash ring buffer unconditionally
            g_crash_buffer.append( full, static_cast<size_t>( total_len ) );

            // Write to file or stdout with lock
            std::lock_guard<std::mutex> lock( m_mutex );
            if( m_log_fd >= 0 )
            {
                if( m_current_file_size + static_cast<size_t>( total_len ) > m_max_file_size )
                {
                    rotate_unlocked();
                }
                (void)!::write( m_log_fd, full, static_cast<size_t>( total_len ) );
                m_current_file_size += static_cast<size_t>( total_len );
            }
            else
            {
                (void)!::write( 1, full, static_cast<size_t>( total_len ) );
            }
        }

        size_t get_current_file_size() const
        {
            std::lock_guard<std::mutex> lock( m_mutex );
            return m_current_file_size;
        }

        std::string get_log_path() const
        {
            std::lock_guard<std::mutex> lock( m_mutex );
            return m_log_path;
        }

    private:
        Logger()
        {
            load_categories_from_env();
        }

        ~Logger()
        {
            close_file_unlocked();
        }

        void open_file_unlocked()
        {
            if( m_log_path.empty() ) return;
            m_log_fd = ::open( m_log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644 );
            m_current_file_size = 0;
            if( m_log_fd >= 0 && m_redirect_stdio )
            {
                ::dup2( m_log_fd, 1 );
                ::dup2( m_log_fd, 2 );
            }
        }

        void close_file_unlocked()
        {
            if( m_log_fd >= 0 )
            {
                ::close( m_log_fd );
                m_log_fd = -1;
            }
            m_current_file_size = 0;
        }

        void rotate_unlocked()
        {
            if( m_log_path.empty() || m_log_fd < 0 ) return;
            ::close( m_log_fd );
            m_log_fd = -1;

            std::string rot = m_log_path + ".1";
            ::rename( m_log_path.c_str(), rot.c_str() );

            open_file_unlocked();
        }

        void load_categories_from_env()
        {
            const char* env = std::getenv( "KICK_LOG" );
            if( env && *env )
            {
                parse_categories_unlocked( env );
            }
            else
            {
                // Default: info, error, warn, proj
                m_enabled_categories.clear();
                m_enabled_categories.insert( "info" );
                m_enabled_categories.insert( "error" );
                m_enabled_categories.insert( "err" );
                m_enabled_categories.insert( "warn" );
                m_enabled_categories.insert( "warning" );
                m_enabled_categories.insert( "proj" );
                m_wildcard_all = false;
            }
        }

        void parse_categories_unlocked( const std::string& str )
        {
            m_enabled_categories.clear();
            m_wildcard_all = false;
            m_enabled_categories.insert( "error" );
            m_enabled_categories.insert( "err" );
            m_enabled_categories.insert( "warn" );
            m_enabled_categories.insert( "warning" );

            size_t start = 0;
            while( start < str.size() )
            {
                while( start < str.size() && ( str[start] == ',' || str[start] == ';' || str[start] == ' ' || str[start] == '\t' ) )
                    ++start;
                if( start >= str.size() ) break;
                size_t end = start;
                while( end < str.size() && str[end] != ',' && str[end] != ';' && str[end] != ' ' && str[end] != '\t' )
                    ++end;
                std::string token = to_lower_trimmed( str.substr( start, end - start ) );
                if( token == "all" || token == "*" )
                {
                    m_wildcard_all = true;
                }
                else if( !token.empty() )
                {
                    m_enabled_categories.insert( token );
                }
                start = end;
            }
        }

        mutable std::mutex           m_mutex;
        std::unordered_set<std::string> m_enabled_categories;
        bool                         m_wildcard_all{ false };
        std::string                  m_log_path;
        int                          m_log_fd{ -1 };
        size_t                       m_max_file_size{ DEFAULT_MAX_FILE_SIZE };
        size_t                       m_current_file_size{ 0 };
        bool                         m_redirect_stdio{ false };
    };

    inline void init( const char* custom_path = nullptr, size_t custom_max_size = 0, bool redirect_stdio = false )
    {
        Logger::instance().init( custom_path, custom_max_size, redirect_stdio );
    }

    inline void shutdown()
    {
        Logger::instance().shutdown();
    }

    inline bool is_category_enabled( const char* category ) noexcept
    {
        return Logger::instance().is_category_enabled( category );
    }

    inline void set_categories( const std::string& categories )
    {
        Logger::instance().set_categories( categories );
    }

    inline void log( const char* category, const char* fmt, ... )
    {
        va_list args;
        va_start( args, fmt );
        Logger::instance().log_v( category, fmt, args );
        va_end( args );
    }
} // namespace kick_logger

// Global helper function so callers can just call kick_log(...)
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
inline void kick_log( const char* category, const char* fmt, ... )
{
    va_list args;
    va_start( args, fmt );
    kick_logger::Logger::instance().log_v( category, fmt, args );
    va_end( args );
}
