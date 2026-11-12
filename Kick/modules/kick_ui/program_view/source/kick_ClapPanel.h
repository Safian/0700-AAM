// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <PluginPanel.h>
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
        hardware = contracts::DclSignals::claimHardwareInput([safe](CPktMgr::HWMsg_t& message) {
            if (!safe || !safe->isVisible()) return false;
            auto* modal = juce::Component::getCurrentlyModalComponent();
            if (modal != nullptr && modal != safe->getTopLevelComponent() && !modal->isParentOf(safe.getComponent())) return false;
            if (message.data.size() < 3 || !contracts::DclContractsHelper::isKnobMessage(message)) return false;
            const auto code = contracts::DclContractsHelper::getKnobCode(message);
            const auto value = contracts::DclContractsHelper::getKnobValue(message);
            int index = -1;
            switch (code) {
                case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_1: index = 0; break;
                case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_2: index = 1; break;
                case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_3: index = 2; break;
                case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_4: index = 3; break;
                default: return false;
            }
            safe->adjustMacro(index, value / 127.0);
            return true;
        });
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
