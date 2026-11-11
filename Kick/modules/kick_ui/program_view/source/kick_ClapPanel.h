// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <PluginPanel.h>
#include <kick_ui_controls/kick_ui_controls.h>
#include <contracts/dcl/dcl_signals.h>
#include <contracts/dcl/dcl_contracts_helper.h>

namespace kick_ui {
class ClapInstrumentPanel final : public kick::clap::PluginPanel {
public:
    explicit ClapInstrumentPanel(std::shared_ptr<kick::clap::PluginSession> session)
        : PluginPanel(std::move(session)) {
        setName("CLAP - SynthyBoy");
        setSize(ui::layout::screen.getWidth(), ui::layout::screen.getHeight());
        juce::Component::SafePointer<ClapInstrumentPanel> safe(this);
        // The MPC bridge feeds fingers through the main mouse source: the plug-in GUI tells them apart by the bridge's slot (multitouch gestures)
        kick::clap::BitmapGuiComponent::touchSlotProvider = [] { return ui::TouchSlot::state().slot; };
        // The plug-in's own GUI (bitmap GUI extension) asks for text: Kick's on-screen keyboard. It returns the whole edited text, which replaces
        // the content of the plug-in's text field; Enter then commits it. Files and sharing use the panel defaults (native chooser on desktop, an
        // in-panel chooser on the MPC).
        guiHooks.textInput = [safe](bool show, juce::Rectangle<int>, uint32_t kind, const juce::String& text) {
            if (!safe) return;
            auto& keyboard = ui::KickVirtualKeyBoard::getWithText(text);
            if (!show) { keyboard.close(); return; }
            keyboard.setInputText(text);
            keyboard.show("EDIT", kind == SAFIAN_BITMAP_GUI_TEXT_KIND_NUMBER ? ui::KickVirtualKeyBoard::Type::KEYS_NUMERIC : ui::KickVirtualKeyBoard::Type::KEYS_LOWERCASE_ALPHABETIC,
                [safe, &keyboard](int result) {
                    if (!safe || !safe->pluginGui() || UI::BUTTONS::APPLY != result) return;
                    safe->pluginGui()->commitText(keyboard.getInputText(), true);
                    safe->pluginGui()->keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
                },
                ui::layout::keyBoardPopup);
        };
        guiHooks.shareFile = [](const juce::File& file, const juce::String&, std::function<void(bool)> done) {
#if JUCE_IOS || JUCE_ANDROID
            juce::ContentSharer::getInstance()->shareFiles(juce::Array<juce::URL>{juce::URL(file)}, [done](bool ok, const juce::String&) { if (done) done(ok); });
            return true;
#elif JUCE_MAC || JUCE_WINDOWS
            file.revealToUser(); if (done) done(true); return true;
#else
            juce::ignoreUnused(file, done); return false; // the MPC has no share function: the plug-in says so and offers Save as WAV
#endif
        };
        hardware = contracts::DclSignals::claimHardwareInput([safe](CPktMgr::HWMsg_t& message) {
            if (!safe || !safe->isVisible()) return false;
            auto* modal = juce::Component::getCurrentlyModalComponent();
            if (modal != nullptr && modal != safe->getTopLevelComponent() && !modal->isParentOf(safe.getComponent())) return false;
            if (message.data.size() < 3 || !contracts::DclContractsHelper::isKnobMessage(message)) return false;
            const auto code = contracts::DclContractsHelper::getKnobCode(message);
            const auto value = contracts::DclContractsHelper::getKnobValue(message);
            // Q-Link: EN_KNOB_1 .. EN_KNOB_16 (four encoders times four banks, hardware/qlink_bank.h). With clap.remote-controls pages knobs 1..8
            // drive the page of what is on screen and 9..16 the page after it; without pages knobs 1..4 are the macros (PluginPanel::adjustKnob).
            const int first = contracts::DeviceControlContracts::KnobSequence::EN_KNOB_1;
            const int index = code - first;
            if (index < 0 || index >= 16) return false;
            return safe->adjustKnob(index, value / 127.0);
        });
        showPluginGuiNow();
    }
private:
    contracts::DclSignals::HardwareInputLease hardware;
};
inline void openClapInstrument(mt::MTIProgram* program) {
    if (auto* plugin = dynamic_cast<mt::MTClapProgram*>(program)) {
        mt::utils::showDialogOwned<ClapInstrumentPanel>(
            "CLAP - " + plugin->getName(),
            std::make_unique<ClapInstrumentPanel>(plugin->getPluginSession()),
            {},
            ui::layout::screen,
            false);
    }
}

// HW EDIT on a Plugin program (Program Edit screen) opens the same panel as the Program tab's EDIT button.
inline const bool programEditPluginOpenerRegistered = ( ProgramEdit::pluginOpener() = [](mt::MTIProgram* program) { openClapInstrument(program); }, true );
} // namespace kick_ui
