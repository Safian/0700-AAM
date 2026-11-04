/*
 * Copyright (c) 2021 MUSIC Tribe / Safian
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
#include <array>
#include <vector>
#include <atomic>
#include <cstdint>
#include "kick_PlatformCommands.h"

namespace mt
{
    class I2CDevice
    {
    public:
        I2CDevice();
        ~I2CDevice();

        bool isOpen() const;

        bool writeBytes( const uint8_t* data, int length );
        bool readBytes( uint8_t* buffer, int length );

    private:
        int         file_;
        std::string devicePath_;
        int         address_;
        bool        isDeviceOpen_;

        bool openDevice();
        void closeDevice();
    };

    class CV
    {
    public:
        CV();
        ~CV() = default;

        static const int MaxCVPorts{ 8 };

        bool           setVoltage( uint8_t channel, uint8_t Note );
        bool           setCentVoltage( uint8_t channel, uint16_t centValue );
        bool           setstepVoltage( uint8_t channel, uint16_t step );
        bool           readVoltage( uint8_t channel, float& voltage );
        bool           readAllchannelVoltage( std::vector<float>& data );
        bool           writeAllChannelVoltage( const std::vector<uint16_t>& data );
        bool           writeAllChannelVoltage( const uint16_t& data );
        bool           setVoltageOff( uint8_t channel );
        bool           setVoltageOn( uint8_t channel );
        bool           calibrateCV( uint8_t index, int step );
        bool           resetCalibCV( uint8_t index );
        bool           applyCalibration( const std::array<uint16_t, 6>& index );
        bool           setChannelNoteMap( uint8_t channel, int startNote );
        uint8_t        getChannelNoteMap( uint8_t channel );

        static CV& getCVInterface()
        {
            static CV _cvInterface;
            return _cvInterface;
        }

    private:
        bool     initRead( uint8_t channel );
        uint16_t calculateValue( uint8_t note );
        int      toChannelmap( int port );

        I2CDevice                                i2c;
        std::array<std::atomic<int>, MaxCVPorts> channelNoteMap;
        uint16_t                                 claibValue[61];
        uint16_t                                 defaultCV[6] = { 0, 820, 1640, 2460, 3280, 4095 };
        uint16_t                                 calibCV[6]   = { 0, 820, 1640, 2460, 3280, 4095 };
    };
}
