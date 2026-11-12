// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "PluginSession.h"
#include <kick_clap/Discovery.h>
#include <kick_clap/BitmapGui.h>
#include "BitmapGuiComponent.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <thread>

namespace kick::clap {
class PluginPanel : public juce::Component, private juce::Timer, private juce::ListBoxModel {
public:
    explicit PluginPanel(std::shared_ptr<PluginSession>, bool automaticallyScan = true);
    ~PluginPanel() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void adjustMacro(int index, double delta) { session->adjustMacro(index, delta); }
    // A Q-Link encoder turn. `knobOffset` 0..15 is the DCL knob (EN_KNOB_1 + offset). With clap.remote-controls pages knobs 1..8 drive the
    // focused page and 9..16 the page after it (see kick_clap/RemotePages.h); without pages knobs 1..4 are the four macros. Returns true when used.
    bool adjustKnob(int knobOffset, double delta);
    /** Hardware +/- and cursor left/right: the previous / next Q-Link page (false: the plug-in has no pages). */
    bool stepRemotePage(int direction);
    bool hasRemotePages() const { return !session->host().remotePages().empty(); }
    /** Hardware Undo / Shift+Undo: sent to the plug-in's GUI as Ctrl+Z / Ctrl+Shift+Z (false: no plug-in GUI is showing). */
    bool pluginUndoRedo(bool redo);
    /** Hardware jog wheel: scrolls the plug-in's GUI (steps > 0 = clockwise = reveal what is below). False: no plug-in GUI is showing. */
    bool scrollGui(int steps);
    // The remote-controls page the knobs follow (index into the plug-in's pages, -1 none), and a title for the display.
    int remoteFocus() const noexcept { return remoteFocusIndex; }
    juce::String remoteTitle() const;
    void setRemoteFocus(int index, bool tellPlugin);
    // The plug-in's own GUI (bitmap GUI extension) while it is shown, else null.
    BitmapGuiComponent* pluginGui() noexcept { return gui.get(); }
    bool showsPluginGui() const noexcept { return gui != nullptr && gui->isVisible(); }
    // The strip's Parameters button: switches between the plug-in's GUI and the generic parameter list (no effect without a plug-in GUI).
    void toggleGenericView() { if (gui) { showGeneric = !showGeneric; updatePluginGui(); resized(); } }
    bool showsGenericList() const noexcept { return parameterList.isVisible(); }
    // Creates the plug-in GUI now instead of at the next timer tick (call after guiHooks are set).
    void showPluginGuiNow() { updatePluginGui(); resized(); }
    // Hooks the application fills in BEFORE the plug-in GUI is created (Kick's on-screen keyboard, its file browser, share).
    BitmapGuiComponent::Hooks guiHooks;
    // Parent size of the GUI when it is shown: the whole panel below the strip.
    juce::Rectangle<int> guiBounds() const;
    static std::filesystem::path scannerPath();
private:
    struct ParameterRow;
    struct ScanJob {
        std::atomic<bool> cancelled{false}, complete{false};
        ScanResult result;
    };
    void scanPlugins();
    void rebuildParameters();
    void updatePluginGui();
    void layoutStrip(juce::Rectangle<int>& bounds);
    bool hasStrip() const;
    void chooseGuiFile(const BitmapGuiFileRequest&, std::function<void(uint32_t, const juce::String&)>);
    void timerCallback() override;
    void chooseFolder();
    int getNumRows() override { return static_cast<int>(visibleParameters.size()); }
    void paintListBoxItem(int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow(int, bool, juce::Component*) override;
    std::shared_ptr<PluginSession> session;
    std::shared_ptr<ScanJob> scanJob;
    std::vector<Descriptor> catalog;
    std::vector<std::filesystem::path> roots;
    juce::TextButton scanButton{"Scan"}, folderButton{"Add folder"}, loadButton{"Load"}, removeButton{"Remove"}, panicButton{"Panic"}, errorsButton{"Scan errors"};
    juce::ComboBox pluginList;
    juce::TextEditor search;
    juce::Label statusLabel, title;
    std::array<juce::ComboBox, 4> macros;
    juce::ListBox parameterList{"Parameters", this};
    std::vector<Parameter> visibleParameters;
    std::unique_ptr<juce::FileChooser> chooser;
    uint64_t parameterRevision = 0;
    juce::String scanErrors;
    // Q-Link strip (remote-controls page, page stepping) and the switch between the plug-in's GUI and the generic list
    juce::TextButton viewToggle{"Parameters"}, pagePrev{"<"}, pageNext{">"};
    juce::Label pageTitle;
    std::unique_ptr<BitmapGuiComponent> gui;
    std::unique_ptr<juce::Component> filePicker;
    bool showGeneric = false, guiRefused = false;
    int remoteFocusIndex = -1;
    uint64_t remoteRevision = ~uint64_t{0};
    juce::String guiPluginId;
};
} // namespace kick::clap
