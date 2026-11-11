// SPDX-License-Identifier: AGPL-3.0-or-later
// KICK - generic client of the CLAP extension "com.safian.bitmap-gui/1" (third_party/safian/bitmap_gui_ext.h, spec: SynthyBoyVST
// docs/BITMAP-GUI-EXTENSION.md): the plug-in draws its own GUI into a pixel buffer, the host only copies dirty rectangles and forwards input.
//
// No JUCE, no windowing, no knowledge of any plug-in. Main thread only (like the Host control API). The client installs itself as the
// Host's BitmapGuiHostHandler; the std::function sinks below are what the plug-in's host-side calls turn into. A sink that is empty means
// "this host cannot": the matching function is NULL in the table the plug-in sees and it degrades (dims file entries, no virtual keyboard ...).
#pragma once
#include "Host.h"
#include <functional>
#include <string>

namespace kick::clap {

using BitmapGuiFrame = safian_bitmap_gui_frame_t;   // pixels / dirty are valid until the next render, resize, scale change or close
using BitmapGuiRect = safian_bitmap_gui_rect_t;

struct BitmapGuiFileRequest {
    uint32_t mode = SAFIAN_BITMAP_GUI_FILE_OPEN;
    std::string title, wildcards, suggestedName, purpose;
};

class BitmapGuiClient final : public BitmapGuiHostHandler {
public:
    // What the plug-in's host-side calls turn into. All optional; set them before open().
    struct Sinks {
        std::function<void(const BitmapGuiRect*)> requestRender;                // first change after a render; rect in logical px or null
        std::function<void(uint32_t)> requestTick;                              // the GUI wants a tick() in about this many ms
        std::function<bool(uint32_t, uint32_t)> requestResize;                  // true: the host will call resize()
        std::function<void(uint32_t)> setCursor;                                // SAFIAN_BITMAP_GUI_CURSOR_*
        std::function<void(bool, const BitmapGuiRect*, uint32_t, const std::string&)> requestTextInput; // show, field (logical px), kind, current text
        std::function<bool(uint32_t, const BitmapGuiFileRequest&)> requestFile; // true: accepted, the answer goes to requestDone() exactly once
        std::function<bool(uint32_t, const std::string&, const std::string&)> shareFile; // id, path, title
        std::function<bool(const std::string&)> openUrl;
        std::function<void(clap_id)> setActiveRemotePage;                       // the controller bank should show this clap.remote-controls page
        std::function<void()> detached;                                         // the plug-in is being unloaded: the GUI is gone
    };
    Sinks sinks;

    explicit BitmapGuiClient(Host& host);
    ~BitmapGuiClient() override;
    BitmapGuiClient(const BitmapGuiClient&) = delete;
    BitmapGuiClient& operator=(const BitmapGuiClient&) = delete;

    // True when the loaded plug-in offers the extension and every function a host needs is there.
    static bool supported(const Host& host);

    // ---- life cycle ----
    // Installs the handler (capabilities from the sinks) and calls create(). False: not supported or the plug-in refused.
    bool open(uint32_t width, uint32_t height, double scale = 1.0, uint32_t flags = 0);
    void close();
    bool isOpen() const noexcept { return opened; }

    // ---- size (valid before open() for info / preferred / limits) ----
    bool info(safian_bitmap_gui_info_t& out) const;
    bool preferredSize(uint32_t& width, uint32_t& height) const;
    bool canResize() const;
    bool sizeLimits(safian_bitmap_gui_size_limits_t& out) const;
    bool size(uint32_t& width, uint32_t& height) const;
    bool resize(uint32_t width, uint32_t height);
    bool setScale(double scale);

    // ---- frames ----
    bool tick();            // runs due work; true when render() would produce a frame
    bool isDirty() const;
    bool render(BitmapGuiFrame& frame);

    // ---- input (logical pixels); true when the GUI used the event ----
    bool pointer(const safian_bitmap_gui_pointer_event_t& event);
    bool wheel(const safian_bitmap_gui_wheel_event_t& event);
    bool key(const safian_bitmap_gui_key_event_t& event);
    bool text(const std::string& utf8);
    void setFocus(bool focused);
    // The answer to a requestFile / shareFile sink call. `path` is the chosen file for an OK file request.
    void requestDone(uint32_t requestId, uint32_t result, const std::string& path = {});
    // The controller bank moved to remote page `pageId` (clap.remote-controls page_id): the GUI shows the panel that holds it.
    bool showRemotePage(clap_id pageId);

private:
    // BitmapGuiHostHandler
    void requestRender(const BitmapGuiRect* dirty) override { if (sinks.requestRender) sinks.requestRender(dirty); }
    void requestTick(uint32_t ms) override { if (sinks.requestTick) sinks.requestTick(ms); }
    bool requestResize(uint32_t w, uint32_t h) override { return sinks.requestResize && sinks.requestResize(w, h); }
    void setCursor(uint32_t cursor) override { if (sinks.setCursor) sinks.setCursor(cursor); }
    void requestTextInput(bool show, const BitmapGuiRect* field, uint32_t kind, const char* text) override;
    bool requestFile(uint32_t id, const safian_bitmap_gui_file_request_t& request) override;
    bool shareFile(uint32_t id, const char* path, const char* title) override { return sinks.shareFile && sinks.shareFile(id, path, title); }
    bool openUrl(const char* url) override { return sinks.openUrl && sinks.openUrl(url); }
    void setActiveRemotePage(clap_id page) override { if (sinks.setActiveRemotePage) sinks.setActiveRemotePage(page); }
    void pluginUnloading() override;

    Host& host;
    const safian_plugin_bitmap_gui_t* api = nullptr;
    const clap_plugin_t* plugin = nullptr;
    bool opened = false;
};

} // namespace kick::clap
