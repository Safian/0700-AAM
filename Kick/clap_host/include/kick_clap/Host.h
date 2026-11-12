// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <clap/clap.h>
#include <bitmap_gui_ext.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace kick::clap {

struct Descriptor {
    std::string path, id, name, vendor, version, description;
    std::vector<std::string> features;
};

struct Parameter {
    clap_id id = CLAP_INVALID_ID;
    std::string name, module;
    double minimum = 0, maximum = 1, defaultValue = 0;
    uint32_t flags = 0;
};

// One page of the plug-in's clap.remote-controls extension: up to 8 parameters for the 8 knobs of a controller (CLAP_REMOTE_CONTROLS_COUNT).
// Unused slots hold CLAP_INVALID_ID. `index` is the position in remotePages(); `id` is the plug-in's page_id.
struct RemotePage {
    static constexpr size_t knobs = 8;
    uint32_t index = 0;
    clap_id id = CLAP_INVALID_ID;
    std::string section, name;
    std::array<clap_id, knobs> parameters{};
    bool forPreset = false;
    RemotePage() { parameters.fill(CLAP_INVALID_ID); }
};

struct Port {
    clap_id id = CLAP_INVALID_ID;
    std::string name;
    uint32_t channels = 0, flags = 0;
};

// Owning, fixed-capacity event. Pointer-bearing events (SysEx) are deliberately
// rejected: their data cannot safely outlive the caller in the command queue.
struct Event {
    alignas(8) std::array<uint8_t, 128> bytes{};
    const clap_event_header_t* header() const noexcept {
        return reinterpret_cast<const clap_event_header_t*>(bytes.data());
    }
    template<class T> static Event from(const T& source) noexcept {
        static_assert(sizeof(T) <= 128, "event too large");
        Event result;
        std::memcpy(result.bytes.data(), &source, sizeof(T));
        return result;
    }
    static Event midi(uint8_t status, uint8_t a, uint8_t b, uint32_t frame = 0,
                      uint16_t port = 0) noexcept;
    static Event note(bool on, int key, double velocity, uint32_t frame = 0,
                      int channel = 0, int port = 0, int noteId = -1) noexcept;
};

struct Statistics {
    uint64_t blocks = 0, droppedEvents = 0, processErrors = 0;
    double lastMicroseconds = 0, maximumMicroseconds = 0;
    // Cumulative per host. A missed DSP deadline is not an audio-device xrun.
    uint64_t deadlineMisses = 0, controlSilences = 0, idleBlocks = 0;
};

// Receives the calls a plug-in makes through the host side of the bitmap GUI extension ("com.safian.bitmap-gui.host/1", third_party/safian).
// Main thread only. A handler is installed with Host::setBitmapGuiHandler together with the list of what it can do; the functions it cannot do
// are NULL in the table the plug-in sees (the spec: NULL means "this host cannot", the plug-in degrades).
class BitmapGuiHostHandler {
public:
    struct Capabilities { bool resize = false, cursor = false, textInput = false, file = false, share = false, url = false, remotePage = false; };
    virtual ~BitmapGuiHostHandler() = default;
    virtual void requestRender(const safian_bitmap_gui_rect_t* /*dirty, logical px, may be null*/) {}
    virtual void requestTick(uint32_t /*delayMs*/) {}
    virtual bool requestResize(uint32_t, uint32_t) { return false; }
    virtual void setCursor(uint32_t) {}
    virtual void requestTextInput(bool /*show*/, const safian_bitmap_gui_rect_t*, uint32_t /*kind*/, const char* /*text*/) {}
    virtual bool requestFile(uint32_t /*requestId*/, const safian_bitmap_gui_file_request_t&) { return false; }
    virtual bool shareFile(uint32_t /*requestId*/, const char* /*path*/, const char* /*title*/) { return false; }
    virtual bool openUrl(const char*) { return false; }
    virtual void setActiveRemotePage(clap_id) {}
    // The plug-in is about to be destroyed (unload / load of another plug-in): destroy the GUI and drop every pointer into the plug-in.
    virtual void pluginUnloading() {}
};

// Control API belongs to ONE main/UI thread. process(), enqueue() and atomic
// telemetry are the only cross-thread API. Control operations exclude rendering
// with a gate: the callback NEVER waits and returns silence during a change.
// CLAP permits the exclusively held main thread to act as the symbolic audio
// thread for stop/reset/flush; thread-check reports this scope accurately.
class Host {
public:
    static constexpr uint32_t maxEvents = 2048;
    static constexpr uint32_t maxStateBytes = 64 * 1024 * 1024;
    Host();
    ~Host();
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;

    bool load(const Descriptor&, double sampleRate, uint32_t maxFrames,
              const std::vector<uint8_t>& state = {});
    void unload();
    bool reconfigure(double sampleRate, uint32_t maxFrames);
    void service(); // main-loop callbacks, restart, timers, fd polling, rescans
    bool saveState(std::vector<uint8_t>& destination);
    bool loadState(const std::vector<uint8_t>& source);
    bool supportsState() const noexcept;
    bool loaded() const noexcept;
    bool faulted() const noexcept;
    const std::string& error() const noexcept;
    const Descriptor& descriptor() const noexcept;
    const std::vector<Parameter>& parameters() const noexcept;
    uint64_t metadataRevision() const noexcept;
    // clap.remote-controls pages of the loaded plug-in (empty when it has none). Read on load and whenever the plug-in calls
    // clap_host_remote_controls.changed; remotePagesRevision() changes with it. Main thread.
    const std::vector<RemotePage>& remotePages() const noexcept;
    uint64_t remotePagesRevision() const noexcept;
    // The page the plug-in suggested last (clap_host_remote_controls.suggest_page) or CLAP_INVALID_ID; the call clears it. Main thread.
    clap_id takeSuggestedRemotePage() noexcept;
    // Raw plug-in access for host-side adapters that speak a plug-in specific extension (for example synthyboy.content). Main thread only,
    // valid until unload()/load(); null when nothing is loaded.
    const clap_plugin_t* plugin() const noexcept;

    // Bitmap GUI extension ("com.safian.bitmap-gui/1"): the plug-in's side of it, null when the loaded plug-in has none. Main thread.
    const safian_plugin_bitmap_gui_t* bitmapGui() const noexcept;
    // Installs the receiver of the plug-in's host-side calls (null removes it). Install BEFORE the plug-in's create(): the plug-in reads the
    // table there. The handler must outlive the loaded plug-in or remove itself (pluginUnloading() is called before the plug-in is destroyed).
    void setBitmapGuiHandler(BitmapGuiHostHandler* handler, const BitmapGuiHostHandler::Capabilities& capabilities = {}) noexcept;
    const std::vector<Port>& inputPorts() const noexcept;
    const std::vector<Port>& outputPorts() const noexcept;
    double parameterValue(clap_id) const noexcept;
    std::string parameterText(clap_id, double value) const;
    bool setParameter(clap_id, double plainValue) noexcept;
    bool parameterGesture(clap_id, bool begin) noexcept;
    bool stateDirty() const noexcept;
    uint32_t latency() const noexcept;
    uint32_t tail() const noexcept;
    Statistics statistics() const noexcept;

    // CLAP GUI extension (clap.gui)
    bool supportsGui() const noexcept;
    bool isGuiApiSupported(const char* api, bool isFloating = false) const noexcept;
    bool getPreferredGuiApi(const char** outApi, bool* outIsFloating) const noexcept;
    bool createGui(const char* api, bool isFloating = false);
    void destroyGui();
    bool setGuiScale(double scale);
    bool getGuiSize(uint32_t* width, uint32_t* height) const;
    bool canResizeGui() const;
    bool adjustGuiSize(uint32_t* width, uint32_t* height) const;
    bool setGuiSize(uint32_t width, uint32_t height);
    bool setGuiParent(const clap_window_t* window);
    bool setGuiTransient(const clap_window_t* window);
    void suggestGuiTitle(const char* title);
    bool showGui();
    bool hideGui();
    bool isGuiVisible() const noexcept;
    bool isGuiCreated() const noexcept;

    bool enqueue(const Event&) noexcept; // bounded multi-producer queue
    bool popOutput(Event&) noexcept; // main/control-thread consumer
    void panic() noexcept;
    void reset(); // main-thread, exclusive audio scope

    // Planar channels are flattened in port order. Missing inputs are silent;
    // surplus plugin outputs render into preallocated scratch storage. All
    // supplied outputs are overwritten. Frames may exceed negotiated maxFrames:
    // the host splits them and rebases event and transport timestamps.
    bool process(const float* const* inputs, uint32_t inputChannels,
                 float* const* outputs, uint32_t outputChannels, uint32_t frames,
                 const Event* events = nullptr, uint32_t count = 0,
                 const clap_event_transport_t* transport = nullptr) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace kick::clap
