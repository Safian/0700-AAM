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

#pragma once
#include <string>
#include "kick_sysinfo.h"

namespace mt
{
    struct sysSizeInfo
    {
        bool   status    = false; // false: the volume is not there / cannot be read (the other fields are 0)
        double avaSize   = 0.0;   // MB
        double usedSize  = 0.0;   // MB
        double totalSize = 0.0;   // MB
        int    perSize   = 0;     // used, percent
    };
    struct sysRamInfo
    {
        bool   status     = false; // false: the platform could not report memory
        double totalRam   = 0.0;   // MB
        double usedRam    = 0.0;   // MB (total - available: page cache counts as available)
        double freeRam    = 0.0;   // MB available
        double processRam = 0.0;   // MB resident set of this process
        int    perRam     = 0;     // used, percent
    };
    class SystemResource
    {
    public:
        SystemResource() = default;

        // CPU load in percent since the previous call. Returns false (and leaves cpuUsage at 0) for the very first call, when
        // there is no interval to measure yet, or on a platform without a source.
        bool cpuInfo( double& cpuUsage );
        void ramInfo( sysRamInfo& info );
        // Size of the file system holding `path` (statvfs).
        void sizeInfo( const std::string& path, sysSizeInfo& info );
        // Mount point of the internal storage / SD card / USB drive from the mount table; "" when absent.
        // Off the MPC (macOS / desktop) the internal volume is the user's home directory and there is no SD / USB.
        std::string volumePath( kick_sysinfo::Volume volume );

        static SystemResource& getResourceInterface()
        {
            static SystemResource _sysResource;
            return _sysResource;
        }

    private:
        kick_sysinfo::CpuTimes prevCpu_;
    };
}
