// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <PluginPanel.h>
#include <kick_clap/HardwareHooks.h>
#include <kick_ui_controls/kick_ui_controls.h>
#include <contracts/dcl/dcl_signals.h>
#include <contracts/dcl/dcl_contracts_helper.h>

namespace kick_ui {
class ClapPanelLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    ClapPanelLookAndFeel()
        : juce::LookAndFeel_V4(juce::LookAndFeel_V4::getDarkColourScheme()) {
        // Keep Kick's CLAP parameter browser dark rather than inheriting KickLooknFeel from dialogDisplayer.
        setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff12161c));
        setColour(juce::ListBox::outlineColourId, juce::Colour(0xff2a3340));
    }

    void setMenuParent(juce::Component* parent) noexcept { menuParent = parent; }

    juce::Component* getParentComponentForMenuOptions(const juce::PopupMenu::Options& options) override {
        if (auto* parent = options.getParentComponent())
            return parent;
        return menuParent.getComponent();
    }

private:
    juce::Component::SafePointer<juce::Component> menuParent;
};

struct ClapPanelLookAndFeelHolder {
    ClapPanelLookAndFeel pluginLookAndFeel;
};

class ClapInstrumentPanel final : private ClapPanelLookAndFeelHolder, public kick::clap::PluginPanel {
    struct LookAndFeelReset {
        juce::Component* target = nullptr;
        ~LookAndFeelReset() {
            if (target != nullptr)
                target->setLookAndFeel(nullptr);
        }
    } lookAndFeelReset { this };
public:
    explicit ClapInstrumentPanel(std::shared_ptr<kick::clap::PluginSession> session)
        : PluginPanel(std::move(session)) {
        setName("CLAP - SynthyBoy");
        setSize(ui::layout::screen.getWidth(), ui::layout::screen.getHeight());
        pluginLookAndFeel.setMenuParent(this);
        setLookAndFeel(&pluginLookAndFeel);
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
            // Hardware +/- and cursor left/right (the bridge sends both as EN_PLUS / EN_MINUS): the previous / next Q-Link page. Press and release
            // are both taken while the plug-in has pages, so the release finds no one else and nothing else sees a lone release.
            if (contracts::DclContractsHelper::isButtonMessage(message)) {
                const auto button = contracts::DclContractsHelper::getButtonCode(message);
                if (button != contracts::DeviceControlContracts::BtnLedSequence::EN_PLUS && button != contracts::DeviceControlContracts::BtnLedSequence::EN_MINUS) return false;
                if (contracts::DclContractsHelper::getButtonState(message) != contracts::DeviceControlContracts::BTN_PRESSED)
                    return safe->hasRemotePages();
                return safe->stepRemotePage(button == contracts::DeviceControlContracts::BtnLedSequence::EN_PLUS ? 1 : -1);
            }
            if (message.data.size() < 3 || !contracts::DclContractsHelper::isKnobMessage(message)) return false;
            const auto code = contracts::DclContractsHelper::getKnobCode(message);
            const auto value = contracts::DclContractsHelper::getKnobValue(message);
            // Q-Link: EN_KNOB_1 .. EN_KNOB_16 (four encoders times four banks, hardware/qlink_bank.h). With clap.remote-controls pages knobs 1..8
            // drive the page of what is on screen and 9..16 the page after it; without pages knobs 1..4 are the macros (PluginPanel::adjustKnob).
            // The jog / data wheel scrolls the plug-in's GUI (it is taller than the 1280x800 screen)
            if (code == contracts::DeviceControlContracts::KnobSequence::EN_KNOB_CENTER) return safe->scrollGui(value > 0 ? 1 : (value < 0 ? -1 : 0));
            const int first = contracts::DeviceControlContracts::KnobSequence::EN_KNOB_1;
            const int index = code - first;
            if (index < 0 || index >= 16) return false;
            return safe->adjustKnob(index, value / 127.0);
        });
        // Hardware Undo / Shift+Undo go to the plug-in while its panel is open (the bridge asks this hook before the project's undo)
        kick::clap::hardwareUndoHook() = [safe](bool redo) { return safe && safe->isVisible() && safe->pluginUndoRedo(redo); };
        showPluginGuiNow();
    }
    ~ClapInstrumentPanel() override { kick::clap::hardwareUndoHook() = nullptr; }
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
