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

/*******************************************************************************
  The block below describes the properties of this module, and is read by
  the Projucer to automatically generate project code that uses it.
  For details about the syntax and how to create or use a module, see the
  JUCE Module Format.txt file.

  BEGIN_JUCE_MODULE_DECLARATION

   ID:                 kick_ui
   vendor:             MusicTribe
   version:            0.1.0
   name:               MusicTribe ui
   description:        Classes aand interfaces for DAW
   website:            http://www.musictribe.com/
   license:            AGPLv3

   dependencies:       kick_ui_controls

  END_JUCE_MODULE_DECLARATION

 *******************************************************************************/

#pragma once

#include <kick_ui_controls/kick_ui_controls.h>
#include <functional>

namespace kick_ui
{
    /// System Resources dialog: CPU, RAM, internal / SD / USB storage, sample cache. The numbers are sampled once a second on
    /// a background thread (a slow USB stick cannot freeze the UI) and shown by a timer on the message thread.
    class KSystemResources
        : public juce::Component
        , private juce::Timer
        , public juce::DeletedAtShutdown
    {
    public:
        /// Sample cache numbers; the engine side is supplied by the owner of the dialog (no engine dependency here).
        struct CacheStats
        {
            int         count       = 0;
            juce::int64 bytes       = 0;
            int         unusedCount = 0;
            juce::int64 unusedBytes = 0;
        };
        std::function<CacheStats()> getCacheStats;
        std::function<CacheStats()> purgeUnused; // returns what was freed
        /// Audio callback load (0..1 of the callback budget) and the xrun count of the running device; false without a device.
        /// Read on the message thread.
        std::function<bool( double& load, int& xruns )> getAudioLoad;

        static KSystemResources& Instance();
        KSystemResources();
        ~KSystemResources() override;
        void start();
        void stop();

    private:
        class Sampler;
        struct Snapshot
        {
            bool   haveCpu = false, haveRam = false;
            double cpu = 0.0;
            int    ramPercent = 0;
            double ramUsedMb = 0.0, ramTotalMb = 0.0, processMb = 0.0;
            bool   haveInternal = false, haveSd = false, haveUsb = false;
            int    internalPercent = 0, sdPercent = 0, usbPercent = 0;
            double internalFreeMb = 0.0, sdFreeMb = 0.0, usbFreeMb = 0.0;
        };

        void timerCallback() override;
        void paint( juce::Graphics& g ) override;
        void resized() override;
        void applySnapshot( const Snapshot& s );
        void refreshCache();
        void refreshAudio();

    private:
        ui::KickLabel storage{ "STORAGE" };

        ui::KickSlider cpu{ "CPU" };
        ui::KickSlider ram{ "RAM" };
        ui::KickSlider audio{ "AUDIO" };
        ui::KickSlider flash{ "INTERNAL MEMORY" };
        ui::KickSlider sdcard{ "SDCARD" };
        ui::KickSlider usb{ "USB DRIVE" };
        ui::KickLabel  info{ "" };

        ui::KickImageButton purgeUnused_{ "Purge Unused", "purgeused" };
        ui::KickImageButton ok{ "OK", "ok_resource" };

        std::unique_ptr<Sampler> sampler;
        Snapshot                 last;
    };
}
