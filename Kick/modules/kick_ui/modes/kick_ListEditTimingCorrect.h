// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <kick_ui_components/kick_ui_components.h>
#include <interface/interface.h>

namespace kick_ui
{
    class ListEditTimingCorrect : public juce::Component
    {
    public:
        ListEditTimingCorrect();
        ~ListEditTimingCorrect();

        // Explicit "dialog is open" state for the hardware bridge (the dialog is modal and has touch-only buttons).
        // Message thread only. closeFromHardware() ends the modal state with APPLY / CANCEL (same as the buttons);
        // false (and nothing closed) when no dialog is open or a nested popup is the foremost modal component.
        static bool isOpen();
        static bool closeFromHardware( bool apply );

        // Current dialog values as engine params (Apply -> MTAudioInterface::timingCorrect).
        mt::track::timingCorrectInfo getData();
        // Global TC Off/On choice; the caller commits it to the engine on Apply only.
        bool getGlobalEnabled();

    private:
        ui::KickToggleOptions globalTC{ "GLOBAL TIMING CORRECT", { "Off", "On" } };
        ui::KickLabel         timeDivisionLabel{ "TIME DIVISION" };
        ui::KickListBox       timeDivList{ ui::KickListBox::withoutTitleBar, "", CommonUtils::getDefaultTimeDivisions(), 3 };
        ui::KickImageButton   triplet{ "TRIPLET" };
        ui::KickListBox       type{ "TYPE", { "Start", "End", "Length", "Legato" } };
        ui::KickListBox       events{ "EVENTS", { "All", "Selected", "Range" } };
        ui::KickNumber        startBar{ "START BAR", 1, 999, 1, 1 };
        ui::KickNumber        endBar{ "END BAR", 1, 999, 2, 1 };
        ui::KickSlider        swing{ "SWING", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
        ui::KickSlider        shiftTiming{ "SHIFT  TIMING", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
        ui::KickSlider        window{ "WINDOW", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
        ui::KickSlider        strength{ "STRENGTH", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };

        void resized() override;
    };
}
