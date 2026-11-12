// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <vector>
#include <string>
#include <cstring>
#include <cctype>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <algorithm>

namespace fileSource::discovery
{
    struct DiscoveredServer
    {
        std::string name; // e.g. "Mac Share (192.168.0.136)"
        std::string url;  // e.g. "http://192.168.0.136:8080"
    };

    inline std::string getLocalIp()
    {
        int s = ::socket( AF_INET, SOCK_DGRAM, 0 );
        if( s < 0 ) return "";
        sockaddr_in loop;
        std::memset( &loop, 0, sizeof( loop ) );
        loop.sin_family      = AF_INET;
        loop.sin_addr.s_addr = inet_addr( "8.8.8.8" );
        loop.sin_port        = htons( 53 );
        if( ::connect( s, reinterpret_cast<sockaddr*>( &loop ), sizeof( loop ) ) != 0 )
        {
            ::close( s );
            return "";
        }
        sockaddr_in name;
        socklen_t namelen = sizeof( name );
        ::getsockname( s, reinterpret_cast<sockaddr*>( &name ), &namelen );
        ::close( s );
        char buf[INET_ADDRSTRLEN] = { 0 };
        ::inet_ntop( AF_INET, &name.sin_addr, buf, sizeof( buf ) );
        return std::string( buf );
    }

    inline bool probeEndpoint( const std::string& ip, int port, int timeoutMs, DiscoveredServer& out )
    {
        int fd = ::socket( AF_INET, SOCK_STREAM, 0 );
        if( fd < 0 ) return false;
        int flags = ::fcntl( fd, F_GETFL, 0 );
        ::fcntl( fd, F_SETFL, flags | O_NONBLOCK );

        sockaddr_in addr;
        std::memset( &addr, 0, sizeof( addr ) );
        addr.sin_family = AF_INET;
        addr.sin_port   = htons( static_cast<uint16_t>( port ) );
        if( ::inet_pton( AF_INET, ip.c_str(), &addr.sin_addr ) <= 0 )
        {
            ::close( fd );
            return false;
        }

        int rc = ::connect( fd, reinterpret_cast<sockaddr*>( &addr ), sizeof( addr ) );
        if( rc < 0 && errno != EINPROGRESS )
        {
            ::close( fd );
            return false;
        }

        pollfd pfd;
        pfd.fd     = fd;
        pfd.events = POLLOUT;
        int prc    = ::poll( &pfd, 1, timeoutMs );
        if( prc <= 0 || !( pfd.revents & POLLOUT ) )
        {
            ::close( fd );
            return false;
        }

        int err = 0;
        socklen_t elen = sizeof( err );
        if( ::getsockopt( fd, SOL_SOCKET, SO_ERROR, &err, &elen ) != 0 || err != 0 )
        {
            ::close( fd );
            return false;
        }

        // Send HEAD request
        const std::string req = "HEAD / HTTP/1.1\r\nHost: " + ip + "\r\nUser-Agent: Kick/1.0\r\nConnection: close\r\n\r\n";
        ::send( fd, req.data(), req.size(), 0 );

        pfd.events = POLLIN;
        prc        = ::poll( &pfd, 1, timeoutMs );
        if( prc <= 0 || !( pfd.revents & POLLIN ) )
        {
            ::close( fd );
            return false;
        }

        char buf[512] = { 0 };
        ssize_t n     = ::recv( fd, buf, sizeof( buf ) - 1, 0 );
        ::close( fd );
        if( n <= 0 ) return false;

        const std::string resp( buf, static_cast<size_t>( n ) );
        if( resp.find( "HTTP/1." ) == std::string::npos ) return false;

        // Check for WebDAV / HTTP server markers
        if( resp.find( " 200 " ) != std::string::npos ||
            resp.find( " 207 " ) != std::string::npos ||
            resp.find( " 401 " ) != std::string::npos ||
            resp.find( "Server: rclone" ) != std::string::npos ||
            resp.find( "DAV:" ) != std::string::npos )
        {
            out.url  = "http://" + ip + ":" + std::to_string( port );
            out.name = "Mac (" + ip + ")";
            if( resp.find( "Server: rclone" ) != std::string::npos )
                out.name = "Mac Share (" + ip + ")";
            return true;
        }
        return false;
    }

    inline std::vector<DiscoveredServer> scanLocalWebDavServers( int timeoutMs = 250 )
    {
        std::vector<DiscoveredServer> found;
        const std::string localIp = getLocalIp();

        // 1. Check known candidate IPs first (quick hit: Mac IP, default gateway, local subnet peers)
        std::vector<std::string> candidates;
        if( !localIp.empty() )
        {
            auto lastDot = localIp.rfind( '.' );
            if( lastDot != std::string::npos )
            {
                const std::string prefix = localIp.substr( 0, lastDot + 1 );
                // Add common hosts on the subnet
                candidates.push_back( prefix + "136" ); // known Mac dev host
                candidates.push_back( prefix + "1" );   // router / gateway
                candidates.push_back( prefix + "2" );
                candidates.push_back( prefix + "100" );
                candidates.push_back( prefix + "10" );
                // Add other IPs on the subnet
                for( int i = 1; i <= 254; ++i )
                {
                    std::string ip = prefix + std::to_string( i );
                    if( ip != localIp && std::find( candidates.begin(), candidates.end(), ip ) == candidates.end() )
                        candidates.push_back( ip );
                }
            }
        }
        if( candidates.empty() )
        {
            candidates.push_back( "192.168.0.136" );
            candidates.push_back( "127.0.0.1" );
        }

        const int ports[] = { 8080, 8000, 8888 };

        // Test priority candidates on common ports
        size_t priorityCount = std::min( candidates.size(), static_cast<size_t>( 8 ) );
        for( size_t i = 0; i < priorityCount; ++i )
        {
            for( int p : ports )
            {
                DiscoveredServer ds;
                if( probeEndpoint( candidates[i], p, timeoutMs, ds ) )
                {
                    found.push_back( ds );
                    return found; // If priority found, return immediately without waiting for full scan!
                }
            }
        }

        // If not found in priority list, probe the rest on standard port 8080
        for( size_t i = priorityCount; i < candidates.size(); ++i )
        {
            DiscoveredServer ds;
            if( probeEndpoint( candidates[i], 8080, 50, ds ) )
            {
                found.push_back( ds );
                return found;
            }
        }

        return found;
    }
}
