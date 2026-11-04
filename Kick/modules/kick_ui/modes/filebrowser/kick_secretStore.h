// SPDX-License-Identifier: AGPL-3.0-or-later
// Secret (password / token) files: created with mode 0600 from the start (no window with looser permissions), written via a
// temp file + rename. The contents are never logged; callers must not pass them to kick_log. JUCE-free.
#pragma once

#include <cerrno>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace ui
{
    namespace fileSource
    {
        inline bool writeSecretFile( const std::string& path, const std::string& secret )
        {
            const std::string tmp = path + ".tmp";
            ::unlink( tmp.c_str() );
            int fd = ::open( tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600 );
            if( fd < 0 ) return false;
            ::fchmod( fd, 0600 );
            size_t off = 0;
            while( off < secret.size() )
            {
                auto n = ::write( fd, secret.data() + off, secret.size() - off );
                if( n < 0 && errno == EINTR ) continue;
                if( n <= 0 ) { ::close( fd ); ::unlink( tmp.c_str() ); return false; }
                off += (size_t)n;
            }
            ::fsync( fd );
            ::close( fd );
            if( ::rename( tmp.c_str(), path.c_str() ) != 0 ) { ::unlink( tmp.c_str() ); return false; }
            return true;
        }

        // Refuses files readable/writable by group or others (a loosened secret file is treated as absent).
        inline bool readSecretFile( const std::string& path, std::string& out )
        {
            struct stat st;
            if( ::stat( path.c_str(), &st ) != 0 || !S_ISREG( st.st_mode ) || ( st.st_mode & 077 ) != 0 || st.st_size > 65536 ) return false;
            int fd = ::open( path.c_str(), O_RDONLY );
            if( fd < 0 ) return false;
            out.clear();
            char buf[1024];
            for( ;; )
            {
                auto n = ::read( fd, buf, sizeof buf );
                if( n < 0 && errno == EINTR ) continue;
                if( n <= 0 ) break;
                out.append( buf, (size_t)n );
            }
            ::close( fd );
            return true;
        }

        inline void deleteSecretFile( const std::string& path ) { ::unlink( path.c_str() ); }
    }
}
