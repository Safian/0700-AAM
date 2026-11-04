#pragma once

#include "hardware_adapter.h"

#if JUCE_LINUX || __linux__
#include "adapters/akai_mpc_key61_adapter.h"
#include "adapters/ni_kontrol_adapter.h"
#include <stdio.h>
#include <string.h>

class HardwareDeviceFactory {
public:
    static IHardwareAdapter* create() {
        // 1. Check Device Tree model
        char model_buf[128] = { 0 };
        FILE* f = fopen("/sys/firmware/devicetree/base/model", "r");
        if (f) {
            if (fgets(model_buf, sizeof(model_buf), f)) {
                // read model
            }
            fclose(f);
        }

        // 2. Check ALSA soundcards
        char cards_buf[512] = { 0 };
        f = fopen("/proc/asound/cards", "r");
        if (f) {
            fread(cards_buf, 1, sizeof(cards_buf) - 1, f);
            fclose(f);
        }

        // 3. Auto-detect matched profile
        if (strstr(model_buf, "Key 61") || strstr(cards_buf, "ACVM") || strstr(cards_buf, "MPC Key 61")) {
            printf("[HAL] Auto-Detected Hardware: AKAI MPC KEY 61 -> Loading AkaiMpcKey61Adapter\n");
            return new AkaiMpcKey61Adapter();
        } else if (strstr(cards_buf, "Komplete") || strstr(cards_buf, "Traktor") || strstr(cards_buf, "Maschine")) {
            printf("[HAL] Auto-Detected Hardware: NATIVE INSTRUMENTS KONTROL -> Loading NiKontrolAdapter\n");
            return new NiKontrolAdapter();
        }

        // Default to Akai adapter if running on the MPC Linux system
        printf("[HAL] Defaulting to AkaiMpcKey61Adapter\n");
        return new AkaiMpcKey61Adapter();
    }
};
#else
class HardwareDeviceFactory {
public:
    static IHardwareAdapter* create() {
        return nullptr;
    }
};
#endif
