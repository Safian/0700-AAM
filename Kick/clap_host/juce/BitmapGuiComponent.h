// SPDX-License-Identifier: AGPL-3.0-or-later
// KICK - JUCE view of a plug-in GUI that the plug-in draws itself (CLAP extension "com.safian.bitmap-gui/1", see include/kick_clap/BitmapGui.h).
//
// Owns a BitmapGuiClient. A message-thread timer ticks the plug-in's GUI; it renders and copies the dirty rectangles only when the plug-in
// says something changed (is_dirty) and otherwise costs one cheap call per tick. Input is forwarded in logical pixels: mouse, wheel, keys,
// and touch with the finger index of JUCE's multitouch sources as pointer id (Kick's MPC bridge and iOS deliver touches that way).
// The OS escapes of the extension (files, text input, share, URLs) are hooks the surrounding application fills in; without a hook the plug-in
// gets "this host cannot" and degrades.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <kick_clap/BitmapGui.h>
#include <functional>
#include <memory>

namespace kick::clap {
class BitmapGuiComponent final : public juce::Component, private juce::Timer {
public:
    explicit BitmapGuiComponent(Host& host);
    ~BitmapGuiComponent() override;

    // Hooks of the application (set before open()). Empty = the host cannot.
    struct Hooks {
        std::function<void(bool show, juce::Rectangle<int> field, uint32_t kind, const juce::String& text)> textInput; // virtual keyboard
        std::function<bool(const BitmapGuiFileRequest&, std::function<void(uint32_t result, const juce::String& path)> done)> chooseFile;
        std::function<bool(const juce::File&, const juce::String& title, std::function<void(bool)> done)> shareFile;
        std::function<void(clap_id)> remotePage;  // the plug-in's active remote page changed (Q-Link follows)
        std::function<void()> detached;           // the plug-in was unloaded: remove this component
    };
    Hooks hooks;
    // The parent decides the size: called (on the message thread) when the plug-in asks for another size; default is to resize this component.
    std::function<void(int width, int height)> onResizeRequest;

    // Opens the GUI at the current size (call after the component has bounds, or pass the size). False: not supported / refused.
    bool open(int logicalWidth, int logicalHeight);
    bool isOpen() const noexcept { return client.isOpen(); }
    BitmapGuiClient& guiClient() noexcept { return client; }
    // The device pixel ratio frames are rendered at: 1.0 unless the display is denser, KICK_BITMAP_GUI_SCALE overrides it (read once).
    static double renderScale(const juce::Component* forComponent);
    // Counters for measurements and tests.
    struct Statistics { uint64_t ticks = 0, renders = 0, pixelsCopied = 0; double renderMicroseconds = 0, tickMicroseconds = 0; };
    Statistics statistics() const noexcept { return stats; }
    // Kick's MPC bridge delivers a finger by calling mouseDown / mouseDrag / mouseUp itself with the main mouse source, so JUCE cannot tell the fingers
    // apart; it publishes the finger's slot in a thread local while it does. The application sets this to read it (returns the slot 0..9 or -1 when the
    // event is not a bridge touch). Without it (desktop, iOS) the input source of the event tells.
    static inline std::function<int()> touchSlotProvider;
    // The plug-in's size limits, or false (any size) when it cannot resize / has none.
    bool sizeLimits(juce::Rectangle<int>& minMax) const;
    // Q-Link: the controller bank moved to this clap.remote-controls page; the GUI shows its panel.
    bool showRemotePage(clap_id pageId) { return client.showRemotePage(pageId); }
    // Paints the latest frame into an image (screenshots, tests). Same pixels as on screen.
    juce::Image snapshot() const { return image.createCopy(); }

    void paint(juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;
    void focusGained(FocusChangeType) override;
    void focusLost(FocusChangeType) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    // Committed text of an on-screen keyboard: replaces the content of the focused text field when `replaceAll`.
    void commitText(const juce::String& text, bool replaceAll);

private:
    void timerCallback() override;
    void step();                       // tick, render when dirty, copy, repaint
    void scheduleSoon(int milliseconds);
    void sendPointer(const juce::MouseEvent&, uint32_t type);
    void wireSinks();
    void adoptFrame(const BitmapGuiFrame&);

    Host& host;
    BitmapGuiClient client;
    juce::Image image;
    double scale = 1.0, baseScale = 1.0, pendingScale = 1.0; // pendingScale: a scale change already queued from paint()
    void adoptScale(double target);
    int idleMs = 66;                   // an idle GUI is ticked at ~15 Hz (the extension asks for at least every 100 ms)
    Statistics stats;
    bool wanted = false;               // visible and open
    std::shared_ptr<bool> alive = std::make_shared<bool>(true); // asynchronous answers (file chooser) check it
    std::unique_ptr<juce::FileChooser> chooser;
};
} // namespace kick::clap
