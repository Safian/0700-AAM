/*
 * Copyright (c) 2021 MUSIC Tribe
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

#include "kick_Resorces.h"

#if JUCE_MAC
    #include <mach/mach.h>
    #include <sys/sysctl.h>
#endif
#if JUCE_LINUX || defined( __linux__ )
    #include <fstream>
    #include <sstream>
#endif

namespace mt
{
    namespace
    {
#if JUCE_LINUX || defined( __linux__ )
        std::string readWholeFile( const char* path )
        {
            std::ifstream     f( path );
            std::stringstream ss;
            ss << f.rdbuf();
            return ss.str();
        }
#endif
    }

    bool SystemResource::cpuInfo( double& cpuUsage )
    {
        cpuUsage = 0.0;
        kick_sysinfo::CpuTimes cur;
#if JUCE_LINUX || defined( __linux__ )
        cur = kick_sysinfo::parseProcStatCpu( readWholeFile( "/proc/stat" ) );
#elif JUCE_MAC
        host_cpu_load_info_data_t load;
        mach_msg_type_number_t    count = HOST_CPU_LOAD_INFO_COUNT;
        if( host_statistics( mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&load, &count ) == KERN_SUCCESS )
        {
            cur.total = (unsigned long long)load.cpu_ticks[CPU_STATE_USER] + load.cpu_ticks[CPU_STATE_SYSTEM] + load.cpu_ticks[CPU_STATE_IDLE] + load.cpu_ticks[CPU_STATE_NICE];
            cur.busy  = cur.total - load.cpu_ticks[CPU_STATE_IDLE];
            cur.valid = true;
        }
#endif
        const double pct = kick_sysinfo::cpuPercent( prevCpu_, cur );
        prevCpu_         = cur;
        if( pct < 0.0 )
            return false;
        cpuUsage = pct;
        return true;
    }

    void SystemResource::sizeInfo( const std::string& path, sysSizeInfo& info )
    {
        info          = sysSizeInfo{};
        const auto du = kick_sysinfo::diskUsage( path );
        if( !du.ok )
            return;
        info.status    = true;
        info.totalSize = du.totalMb;
        info.usedSize  = du.usedMb;
        info.avaSize   = du.availMb;
        info.perSize   = du.percent;
    }

    void SystemResource::ramInfo( sysRamInfo& info )
    {
        info = sysRamInfo{};
#if JUCE_LINUX || defined( __linux__ )
        const auto mem = kick_sysinfo::parseMemInfo( readWholeFile( "/proc/meminfo" ) );
        if( mem.valid )
        {
            info.status   = true;
            info.totalRam = (double)mem.totalKb / 1024.0;
            info.freeRam  = (double)mem.availableKb / 1024.0;
            info.usedRam  = info.totalRam - info.freeRam;
            info.perRam   = kick_sysinfo::percentUsed( info.usedRam, info.totalRam );
        }
        // Resident set of this process (VmRSS, kB).
        const auto status = readWholeFile( "/proc/self/status" );
        const auto pos    = status.find( "VmRSS:" );
        if( pos != std::string::npos )
            info.processRam = std::atof( status.c_str() + pos + 6 ) / 1024.0;
#elif JUCE_MAC
        int64_t                memsize = 0;
        size_t                 len     = sizeof( memsize );
        vm_statistics64_data_t vm;
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
        if( sysctlbyname( "hw.memsize", &memsize, &len, nullptr, 0 ) == 0 && host_statistics64( mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm, &count ) == KERN_SUCCESS )
        {
            const double page = (double)vm_page_size;
            info.status       = true;
            info.totalRam     = (double)memsize / ( 1024.0 * 1024.0 );
            info.freeRam      = ( (double)vm.free_count + (double)vm.inactive_count + (double)vm.purgeable_count ) * page / ( 1024.0 * 1024.0 );
            info.freeRam      = std::min( info.freeRam, info.totalRam );
            info.usedRam      = info.totalRam - info.freeRam;
            info.perRam       = kick_sysinfo::percentUsed( info.usedRam, info.totalRam );
        }
        mach_task_basic_info_data_t task;
        count = MACH_TASK_BASIC_INFO_COUNT;
        if( task_info( mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&task, &count ) == KERN_SUCCESS )
            info.processRam = (double)task.resident_size / ( 1024.0 * 1024.0 );
#endif
    }

    std::string SystemResource::volumePath( kick_sysinfo::Volume volume )
    {
#if JUCE_LINUX || defined( __linux__ )
        const auto mounts = kick_sysinfo::parseMounts( readWholeFile( "/proc/mounts" ) );
        auto       found  = kick_sysinfo::findVolumeMount( mounts, volume, []( const std::string& p ) { return juce::File( juce::String( p ) ).isDirectory(); } );
        if( found.empty() && volume == kick_sysinfo::Volume::Internal && juce::File( "/data" ).isDirectory() )
            found = "/data";
        return found;
#else
        if( volume == kick_sysinfo::Volume::Internal )
            return juce::File::getSpecialLocation( juce::File::userHomeDirectory ).getFullPathName().toStdString();
        return {};
#endif
    }
}
