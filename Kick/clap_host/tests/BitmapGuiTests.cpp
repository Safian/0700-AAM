// SPDX-License-Identifier: AGPL-3.0-or-later
// Tests of the generic bitmap GUI client (include/kick_clap/BitmapGui.h) against
//  1. a fake plug-in that implements "com.safian.bitmap-gui/1" (statically linked into this test, so its counters can be read), and
//  2. the real SynthyBoy module when its path is given (argv[1] or KICK_SYNTHYBOY_CLAP; otherwise the cases print SKIPPED and the run passes).
// Also: the precedence rule of PluginManager::resolve (built-in against a binary of the same id).
#include <kick_clap/BitmapGui.h>
#include <kick_clap/Discovery.h>
#include <kick_clap/PluginManager.h>
#include <clap/ext/audio-ports.h>
#include <clap/ext/remote-controls.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
using namespace kick::clap;

static int failures = 0;
#define CHECK(cond, why) do { if (!(cond)) { std::cerr << "FAIL: " << why << "  (" #cond ")\n"; ++failures; } } while (0)

namespace fake {
constexpr uint32_t W0 = 64, H0 = 48;
const char* features[]{CLAP_PLUGIN_FEATURE_INSTRUMENT, nullptr};
const clap_plugin_descriptor_t descriptor{CLAP_VERSION, "org.kick.test.bitmap", "bitmap fake", "KICK", "", "", "", "1", "bitmap gui fake", features};

struct State {
    const clap_host_t* host = nullptr;
    const safian_host_bitmap_gui_t* hostTable = nullptr;
    bool created = false;
    int createCalls = 0, destroyCalls = 0, tickCalls = 0;
    uint32_t width = 0, height = 0;
    double scale = 1;
    std::vector<uint8_t> pixels;
    std::vector<safian_bitmap_gui_rect_t> dirty;
    bool dirtyFlag = false, full = false;
    uint64_t frameId = 0;
    std::vector<safian_bitmap_gui_pointer_event_t> pointers;
    std::vector<safian_bitmap_gui_key_event_t> keys;
    std::string text;
    bool focus = false;
    std::vector<std::pair<uint32_t, std::pair<uint32_t, std::string>>> answers; // request id -> (result, path)
    clap_id shownPage = CLAP_INVALID_ID;
    // what the table offered at create()
    bool hadResize = false, hadCursor = false, hadText = false, hadFile = false, hadShare = false, hadUrl = false, hadRemote = false;
} state;

void reset() { state = State{}; }
void markDirty(bool full) { state.dirtyFlag = true; state.full = state.full || full; }

const safian_plugin_bitmap_gui_t api{
    /*create*/ [](const clap_plugin_t* p, uint32_t w, uint32_t h, double s, uint32_t) -> bool {
        if (state.created) return false;
        state.hostTable = static_cast<const safian_host_bitmap_gui_t*>(state.host->get_extension(state.host, SAFIAN_EXT_BITMAP_GUI_HOST));
        auto* t = state.hostTable;
        state.hadResize = t && t->request_resize; state.hadCursor = t && t->set_cursor; state.hadText = t && t->request_text_input;
        state.hadFile = t && t->request_file; state.hadShare = t && t->share_file; state.hadUrl = t && t->open_url;
        state.hadRemote = t && t->set_active_remote_page;
        (void)p;
        state.created = true; ++state.createCalls; state.width = w; state.height = h; state.scale = s;
        state.pixels.assign(static_cast<size_t>(w * s) * static_cast<size_t>(h * s) * 4, 0x20);
        markDirty(true);
        return true;
    },
    /*destroy*/ [](const clap_plugin_t*) { if (state.created) ++state.destroyCalls; state.created = false; },
    /*get_info*/ [](const clap_plugin_t*, safian_bitmap_gui_info_t* i) -> bool {
        *i = {}; i->flags = SAFIAN_BITMAP_GUI_CAP_KEYBOARD | SAFIAN_BITMAP_GUI_CAP_TEXT_INPUT; i->pixel_format = SAFIAN_BITMAP_GUI_PIXEL_BGRA8888_PREMUL;
        i->max_dirty_rects = 4; i->min_scale = 1; i->max_scale = 2; return true; },
    /*get_preferred_size*/ [](const clap_plugin_t*, uint32_t* w, uint32_t* h) -> bool { *w = W0; *h = H0; return true; },
    /*can_resize*/ [](const clap_plugin_t*) -> bool { return true; },
    /*get_size_limits*/ [](const clap_plugin_t*, safian_bitmap_gui_size_limits_t* l) -> bool { *l = {16, 16, 256, 256, 0, 0}; return true; },
    /*get_size*/ [](const clap_plugin_t*, uint32_t* w, uint32_t* h) -> bool { if (!state.created) return false; *w = state.width; *h = state.height; return true; },
    /*set_size*/ [](const clap_plugin_t*, uint32_t w, uint32_t h) -> bool {
        if (!state.created) return false;
        state.width = w < 16 ? 16 : (w > 256 ? 256 : w); state.height = h < 16 ? 16 : (h > 256 ? 256 : h);
        state.pixels.assign(static_cast<size_t>(state.width * state.scale) * static_cast<size_t>(state.height * state.scale) * 4, 0x20);
        markDirty(true); return true; },
    /*set_scale*/ [](const clap_plugin_t*, double s) -> bool {
        if (!state.created) return false; state.scale = s;
        state.pixels.assign(static_cast<size_t>(state.width * s) * static_cast<size_t>(state.height * s) * 4, 0x20); markDirty(true); return true; },
    /*tick*/ [](const clap_plugin_t*) -> bool { ++state.tickCalls; return state.dirtyFlag; },
    /*is_dirty*/ [](const clap_plugin_t*) -> bool { return state.dirtyFlag; },
    /*render*/ [](const clap_plugin_t*, safian_bitmap_gui_frame_t* f) -> bool {
        if (!state.created) return false;
        const uint32_t w = static_cast<uint32_t>(state.width * state.scale), h = static_cast<uint32_t>(state.height * state.scale);
        f->pixels = state.pixels.data(); f->width = w; f->height = h; f->stride_bytes = w * 4; f->pixel_format = SAFIAN_BITMAP_GUI_PIXEL_BGRA8888_PREMUL;
        if (state.dirtyFlag) {
            ++state.frameId;
            state.dirty.clear();
            if (state.full) state.dirty.push_back({0, 0, w, h}); else state.dirty.push_back({2, 3, 4, 5});
            f->dirty_count = static_cast<uint32_t>(state.dirty.size()); f->dirty = state.dirty.data();
            f->flags = state.full ? SAFIAN_BITMAP_GUI_FRAME_FULL : 0;
        }
        f->frame_id = state.frameId;
        state.dirtyFlag = false; state.full = false;
        return true; },
    /*pointer_event*/ [](const clap_plugin_t*, const safian_bitmap_gui_pointer_event_t* e) -> bool {
        state.pointers.push_back(*e);
        if (e->type == SAFIAN_BITMAP_GUI_POINTER_DOWN) {
            const bool first = !state.dirtyFlag;
            markDirty(false);
            if (first && state.hostTable && state.hostTable->request_render) state.hostTable->request_render(state.host, nullptr);
        }
        return true; },
    /*wheel_event*/ [](const clap_plugin_t*, const safian_bitmap_gui_wheel_event_t*) -> bool { return false; },
    /*key_event*/ [](const clap_plugin_t*, const safian_bitmap_gui_key_event_t* e) -> bool { state.keys.push_back(*e); return true; },
    /*text_input*/ [](const clap_plugin_t*, const char* t) -> bool { state.text += t; return true; },
    /*set_focus*/ [](const clap_plugin_t*, bool f) { state.focus = f; },
    /*request_done*/ [](const clap_plugin_t*, uint32_t id, uint32_t result, const char* path) { state.answers.push_back({id, {result, path ? path : "<null>"}}); },
    /*show_remote_page*/ [](const clap_plugin_t*, clap_id page) -> bool { state.shownPage = page; return page < 3; },
};

const clap_plugin_audio_ports_t audioPorts{
    [](const clap_plugin_t*, bool input) -> uint32_t { return input ? 0 : 1; },
    [](const clap_plugin_t*, uint32_t, bool input, clap_audio_port_info_t* info) -> bool {
        if (input) return false;
        *info = {}; info->id = 0; std::strcpy(info->name, "out"); info->channel_count = 2; info->flags = CLAP_AUDIO_PORT_IS_MAIN; info->port_type = CLAP_PORT_STEREO;
        return true; }};
const clap_plugin_remote_controls_t remotes{
    [](const clap_plugin_t*) -> uint32_t { return 3; },
    [](const clap_plugin_t*, uint32_t i, clap_remote_controls_page_t* page) -> bool {
        if (i >= 3) return false;
        *page = {}; page->page_id = 100 + i; std::strcpy(page->section_name, "Fake"); std::strcpy(page->page_name, i == 0 ? "First" : (i == 1 ? "Second" : "Third"));
        for (uint32_t k = 0; k < CLAP_REMOTE_CONTROLS_COUNT; ++k) page->param_ids[k] = CLAP_INVALID_ID;
        page->param_ids[0] = 1000 + i;
        return true; }};

clap_plugin_t plugin{};
bool withExtension = true;
const void* getExtension(const clap_plugin_t*, const char* id) {
    if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS)) return &audioPorts;
    if (!std::strcmp(id, CLAP_EXT_REMOTE_CONTROLS)) return &remotes;
    if (withExtension && !std::strcmp(id, SAFIAN_EXT_BITMAP_GUI)) return &api;
    return nullptr;
}
const clap_plugin_t* create(const clap_plugin_factory_t*, const clap_host_t* host, const char* id) {
    if (std::strcmp(id, descriptor.id)) return nullptr;
    state.host = host;
    plugin.desc = &descriptor;
    plugin.init = [](const clap_plugin_t*) { return true; };
    plugin.destroy = [](const clap_plugin_t*) { if (state.created) ++state.destroyCalls; state.created = false; /* a forgotten GUI dies with the plug-in */ };
    plugin.activate = [](const clap_plugin_t*, double, uint32_t, uint32_t) { return true; };
    plugin.deactivate = [](const clap_plugin_t*) {};
    plugin.start_processing = [](const clap_plugin_t*) { return true; };
    plugin.stop_processing = [](const clap_plugin_t*) {};
    plugin.reset = [](const clap_plugin_t*) {};
    plugin.process = [](const clap_plugin_t*, const clap_process_t*) -> clap_process_status { return CLAP_PROCESS_CONTINUE; };
    plugin.get_extension = getExtension;
    plugin.on_main_thread = [](const clap_plugin_t*) {};
    return &plugin;
}
const clap_plugin_factory_t factory{[](const clap_plugin_factory_t*) -> uint32_t { return 1; },
                                    [](const clap_plugin_factory_t*, uint32_t i) -> const clap_plugin_descriptor_t* { return i == 0 ? &descriptor : nullptr; }, create};
const clap_plugin_entry_t entry{CLAP_VERSION, [](const char*) { return true; }, []() {},
                                [](const char* id) -> const void* { return std::strcmp(id, CLAP_PLUGIN_FACTORY_ID) ? nullptr : &factory; }};
} // namespace fake

static Descriptor fakeDescriptor() { Descriptor d; d.path = "internal:fake-bitmap"; d.id = fake::descriptor.id; d.name = "bitmap fake"; return d; }

static BitmapGuiClient::Sinks allSinks(std::vector<std::string>& log, BitmapGuiClient*& clientRef) {
    BitmapGuiClient::Sinks s;
    s.requestRender = [&](const BitmapGuiRect*) { log.push_back("render"); };
    s.requestTick = [&](uint32_t ms) { log.push_back("tick" + std::to_string(ms)); };
    s.requestResize = [&](uint32_t w, uint32_t h) { log.push_back("resize" + std::to_string(w) + "x" + std::to_string(h)); return true; };
    s.setCursor = [&](uint32_t c) { log.push_back("cursor" + std::to_string(c)); };
    s.requestTextInput = [&](bool show, const BitmapGuiRect*, uint32_t kind, const std::string& t) { log.push_back(std::string("text") + (show ? "+" : "-") + std::to_string(kind) + t); };
    s.requestFile = [&](uint32_t id, const BitmapGuiFileRequest& r) { log.push_back("file" + std::to_string(id) + r.title + "|" + r.wildcards + "|" + r.purpose); return true; };
    s.shareFile = [&](uint32_t id, const std::string& path, const std::string&) { log.push_back("share" + std::to_string(id) + path); return true; };
    s.openUrl = [&](const std::string& u) { log.push_back("url" + u); return true; };
    s.setActiveRemotePage = [&](clap_id p) { log.push_back("page" + std::to_string(p)); };
    s.detached = [&] { log.push_back("detached"); clientRef = nullptr; };
    return s;
}

static void testFake() {
    PluginManifest manifest; manifest.id = fake::descriptor.id; manifest.name = "bitmap fake"; manifest.kind = PluginKind::Instrument;
    PluginManager::getInstance().registerStaticPlugin(std::make_shared<StaticPluginLoader>(manifest, &fake::entry));

    // ---- a plug-in without the extension: not supported, open() refuses ----
    fake::reset(); fake::withExtension = false;
    {
        Host host;
        CHECK(host.load(fakeDescriptor(), 48000, 128), "fake loads without the extension");
        CHECK(host.bitmapGui() == nullptr && !BitmapGuiClient::supported(host), "no extension -> not supported");
        BitmapGuiClient client(host);
        CHECK(!client.open(64, 48), "open() refuses a plug-in without the extension");
    }
    fake::withExtension = true;

    // ---- host table: functions without a sink are NULL for the plug-in ----
    fake::reset();
    {
        Host host;
        CHECK(host.load(fakeDescriptor(), 48000, 128), "fake loads");
        CHECK(BitmapGuiClient::supported(host), "extension supported");
        BitmapGuiClient client(host);
        client.sinks.requestRender = [](const BitmapGuiRect*) {};
        client.sinks.openUrl = [](const std::string&) { return true; };
        CHECK(client.open(64, 48, 1.0), "open with two sinks");
        CHECK(fake::state.hostTable != nullptr && fake::state.hostTable->request_render && fake::state.hostTable->request_tick, "render and tick requests are always offered");
        CHECK(fake::state.hadUrl && !fake::state.hadFile && !fake::state.hadShare && !fake::state.hadText && !fake::state.hadResize && !fake::state.hadCursor && !fake::state.hadRemote,
              "capabilities follow the sinks (NULL = this host cannot)");
        CHECK(!client.open(64, 48), "a second open() without close() fails");
        client.close();
        CHECK(fake::state.destroyCalls == 1 && !client.isOpen(), "close() destroys the GUI exactly once");
        client.close();
        CHECK(fake::state.destroyCalls == 1, "close() twice is harmless");
    }

    // ---- everything wired ----
    fake::reset();
    {
        std::vector<std::string> log;
        Host host;
        CHECK(host.load(fakeDescriptor(), 48000, 128), "fake loads");
        BitmapGuiClient client(host);
        BitmapGuiClient* clientPointer = &client;
        client.sinks = allSinks(log, clientPointer);
        BitmapGuiClient::Sinks keep = client.sinks;
        safian_bitmap_gui_info_t info{};
        uint32_t pw = 0, ph = 0;
        CHECK(client.info(info) && info.max_dirty_rects == 4 && (info.flags & SAFIAN_BITMAP_GUI_CAP_TEXT_INPUT), "info before create");
        CHECK(client.preferredSize(pw, ph) && pw == fake::W0 && ph == fake::H0, "preferred size before create");
        safian_bitmap_gui_size_limits_t limits{};
        CHECK(client.canResize() && client.sizeLimits(limits) && limits.max_width == 256, "limits before create");
        CHECK(client.open(64, 48, 2.0), "open at scale 2");
        CHECK(fake::state.hadResize && fake::state.hadCursor && fake::state.hadText && fake::state.hadFile && fake::state.hadShare && fake::state.hadUrl && fake::state.hadRemote, "every capability offered");

        // frames
        CHECK(client.isDirty(), "first frame pending");
        BitmapGuiFrame frame{};
        CHECK(client.render(frame) && frame.width == 128 && frame.height == 96 && frame.stride_bytes == 512 && (frame.flags & SAFIAN_BITMAP_GUI_FRAME_FULL), "first frame is full, device pixels = logical * scale");
        CHECK(frame.dirty_count == 1 && frame.dirty[0].width == 128 && frame.frame_id == 1, "full frame lists the whole frame once");
        CHECK(!client.isDirty() && !client.tick(), "idle: nothing dirty");
        CHECK(client.render(frame) && frame.dirty_count == 0 && frame.frame_id == 1, "render without change: same id, no rectangles");

        // pointer -> dirty -> request_render (once per render)
        safian_bitmap_gui_pointer_event_t down{SAFIAN_BITMAP_GUI_POINTER_DOWN, SAFIAN_BITMAP_GUI_POINTER_KIND_TOUCH, 7, 10.5, 12.25, SAFIAN_BITMAP_GUI_BUTTON_PRIMARY, 0, 0.f};
        CHECK(client.pointer(down), "pointer event used");
        CHECK(fake::state.pointers.size() == 1 && fake::state.pointers[0].pointer_id == 7 && fake::state.pointers[0].x == 10.5, "pointer id and logical position pass unchanged");
        CHECK(!log.empty() && log.back() == "render", "request_render reaches the sink");
        CHECK(client.isDirty() && client.render(frame) && frame.dirty_count == 1 && frame.dirty[0].x == 2 && frame.frame_id == 2, "partial frame after input");

        // keys, text, focus, wheel
        client.setFocus(true);
        CHECK(fake::state.focus, "focus forwarded");
        safian_bitmap_gui_key_event_t key{SAFIAN_BITMAP_GUI_KEY_DOWN_EVENT, 'a', SAFIAN_BITMAP_GUI_MOD_CTRL, 0, "a"};
        CHECK(client.key(key) && fake::state.keys.size() == 1 && fake::state.keys[0].key_code == 'a', "key forwarded");
        CHECK(client.text("hé") && fake::state.text == "hé", "text input forwarded as UTF-8");
        safian_bitmap_gui_wheel_event_t wheel{1, 1, 0, 1, 0, 0};
        CHECK(!client.wheel(wheel), "an unused wheel event is reported unused");

        // size and scale
        uint32_t w = 0, h = 0;
        CHECK(client.resize(1000, 20) && client.size(w, h) && w == 256 && h == 20, "resize is clamped by the plug-in; size() tells the result");
        CHECK(client.render(frame) && (frame.flags & SAFIAN_BITMAP_GUI_FRAME_FULL) && frame.width == 512, "resize gives a full frame with the new pixel size");
        CHECK(client.setScale(1.0) && client.render(frame) && frame.width == 256, "set_scale changes the pixel size, logical size stays");

        // file request round trip
        log.clear();
        fake::state.hostTable->request_file(fake::state.host, 42, [] { static safian_bitmap_gui_file_request_t r{SAFIAN_BITMAP_GUI_FILE_OPEN, "Load", "*.wav", "", "sample"}; return &r; }());
        CHECK(log.size() == 1 && log[0] == "file42Load|*.wav|sample", "file request reaches the sink with every field");
        client.requestDone(42, SAFIAN_BITMAP_GUI_RESULT_OK, "/tmp/a.wav");
        client.requestDone(43, SAFIAN_BITMAP_GUI_RESULT_CANCELLED);
        CHECK(fake::state.answers.size() == 2 && fake::state.answers[0].first == 42 && fake::state.answers[0].second.second == "/tmp/a.wav" &&
              fake::state.answers[1].second.first == SAFIAN_BITMAP_GUI_RESULT_CANCELLED && fake::state.answers[1].second.second == "<null>", "answers (path / null) reach the plug-in");

        // text input request, cursor, url, share, resize request, tick request
        log.clear();
        safian_bitmap_gui_rect_t field{1, 2, 30, 10};
        fake::state.hostTable->request_text_input(fake::state.host, true, &field, SAFIAN_BITMAP_GUI_TEXT_KIND_NUMBER, "1.5k");
        fake::state.hostTable->request_text_input(fake::state.host, false, nullptr, 0, nullptr);
        fake::state.hostTable->set_cursor(fake::state.host, SAFIAN_BITMAP_GUI_CURSOR_TEXT);
        fake::state.hostTable->open_url(fake::state.host, "https://example.org");
        fake::state.hostTable->share_file(fake::state.host, 5, "/tmp/x.wav", "x");
        fake::state.hostTable->request_tick(fake::state.host, 16);
        CHECK(fake::state.hostTable->request_resize(fake::state.host, 100, 50), "resize request accepted");
        CHECK(log == (std::vector<std::string>{"text+11.5k", "text-0", "cursor3", "urlhttps://example.org", "share5/tmp/x.wav", "tick16", "resize100x50"}), "host requests arrive in order with their arguments");

        // remote pages both directions
        log.clear();
        fake::state.hostTable->set_active_remote_page(fake::state.host, 101);
        CHECK(log.size() == 1 && log[0] == "page101", "set_active_remote_page reaches the sink");
        CHECK(client.showRemotePage(2) && fake::state.shownPage == 2 && !client.showRemotePage(9), "show_remote_page goes to the plug-in and reports whether there is a panel");
        CHECK(host.remotePages().size() == 3 && host.remotePages()[1].name == "Second" && host.remotePages()[1].id == 101 && host.remotePages()[1].parameters[0] == 1001 &&
              host.remotePages()[1].parameters[1] == CLAP_INVALID_ID, "Host reads the plug-in's remote-control pages");

        // calls from another thread are ignored (the extension is main-thread only)
        log.clear();
        std::thread([&] { fake::state.hostTable->request_tick(fake::state.host, 99); fake::state.hostTable->open_url(fake::state.host, "x"); }).join();
        CHECK(log.empty(), "host-side calls from a foreign thread are ignored");

        // the plug-in goes away while the GUI is open: the GUI is destroyed first, the client is told
        log.clear();
        const int destroyedBefore = fake::state.destroyCalls;
        host.unload();
        CHECK(fake::state.destroyCalls == destroyedBefore + 1, "unload destroys the GUI before the plug-in (exactly once)");
        CHECK(!log.empty() && log.back() == "detached" && clientPointer == nullptr, "client is told that the plug-in is gone");
        CHECK(!client.isOpen() && !client.tick() && !client.pointer(down), "a detached client is inert");
    }
}

static void testPrecedence() {
    namespace fs = std::filesystem;
    auto& manager = PluginManager::getInstance();
    Descriptor builtin; builtin.path = "internal:static"; builtin.id = "org.kick.test.precedence"; builtin.name = "builtin";
    const auto tmp = fs::temp_directory_path() / "kick-precedence-test.clap";
    { std::ofstream(tmp) << "x"; }
    Descriptor binary; binary.path = tmp.string(); binary.id = builtin.id; binary.name = "binary";

    manager.setDiscovered({});
    CHECK(manager.resolve(builtin).path == "internal:static", "nothing discovered -> the built-in");
    manager.setDiscovered({binary});
    CHECK(manager.resolve(builtin).path == tmp.string(), "a discovered binary of the same id wins over the built-in");
    CHECK(manager.resolve(binary).path == tmp.string(), "an explicit binary request is returned unchanged");
    Descriptor other = builtin; other.id = "org.kick.test.other";
    CHECK(manager.resolve(other).path == "internal:static", "other ids are not redirected");
    Descriptor explicitStatic = builtin; explicitStatic.path = "/somewhere/else.clap";
    CHECK(manager.resolve(explicitStatic).path == "/somewhere/else.clap", "an explicit path is never rewritten");
    manager.setDiscovered({Descriptor{"internal:static", builtin.id, "b", "", "", "", {}}});
    CHECK(manager.resolve(builtin).path == "internal:static", "a discovered entry that is itself a built-in is ignored");
    manager.setDiscovered({binary});
    fs::remove(tmp);
    CHECK(manager.resolve(builtin).path == "internal:static", "a discovered binary that no longer exists falls back to the built-in");

    // installed package: wins; revoked / expired signature: does not
    const auto pkg = fs::temp_directory_path() / "kick-precedence-pkg.clap";
    { std::ofstream(pkg) << "x"; }
    PluginManifest manifest; manifest.id = builtin.id; manifest.name = "pkg"; manifest.kind = PluginKind::Instrument;
    manager.registerStaticPlugin(std::make_shared<StaticPluginLoader>(manifest, &fake::entry));
    manager.registerDynamicPlugin(std::make_shared<DynamicPluginLoader>(manifest, pkg, TrustLevel::Verified));
    manager.setDiscovered({binary});
    CHECK(manager.resolve(builtin).path == pkg.string(), "an installed trusted package wins over a discovered binary and the built-in");
    CHECK(manager.findLoaderById(builtin.id)->getBinaryPath() == pkg, "findLoaderById agrees");
    int listed = 0; for (const auto& l : manager.getAllLoaders()) if (l->getManifest().id == builtin.id) ++listed;
    CHECK(listed == 1, "the id is listed once");
    manager.registerDynamicPlugin(std::make_shared<DynamicPluginLoader>(manifest, pkg, TrustLevel::Revoked));
    CHECK(manager.resolve(builtin).path != pkg.string(), "a revoked package is not preferred");
    CHECK(manager.findLoaderById(builtin.id)->isStatic(), "findLoaderById falls back to the built-in for a revoked package");
    fs::remove(pkg);
    manager.setDiscovered({});
}

static void testRealModule(const char* path) {
    if (!path || !*path) { std::cout << "SKIPPED: real SynthyBoy module (pass its path as argv[1] or KICK_SYNTHYBOY_CLAP)\n"; return; }
    const auto scanned = inspect(path);
    CHECK(scanned.failures.empty() && !scanned.plugins.empty(), "the real module enumerates");
    if (scanned.plugins.empty()) return;
    Host host;
    CHECK(host.load(scanned.plugins[0], 48000, 256), "the real module loads through the dynamic path");
    CHECK(BitmapGuiClient::supported(host), "the real module offers com.safian.bitmap-gui/1");
    BitmapGuiClient client(host);
    uint32_t reqId = 0; std::string reqPurpose; bool texted = false; clap_id page = CLAP_INVALID_ID;
    client.sinks.requestRender = [](const BitmapGuiRect*) {};
    client.sinks.requestTick = [](uint32_t) {};
    client.sinks.requestResize = [](uint32_t, uint32_t) { return true; };
    client.sinks.setCursor = [](uint32_t) {};
    client.sinks.requestTextInput = [&](bool show, const BitmapGuiRect*, uint32_t, const std::string&) { texted = texted || show; };
    client.sinks.requestFile = [&](uint32_t id, const BitmapGuiFileRequest& r) { reqId = id; reqPurpose = r.purpose; return true; };
    client.sinks.setActiveRemotePage = [&](clap_id p) { page = p; };
    safian_bitmap_gui_info_t info{};
    CHECK(client.info(info) && info.pixel_format == SAFIAN_BITMAP_GUI_PIXEL_BGRA8888_PREMUL && info.max_dirty_rects >= 1, "real info");
    CHECK(client.open(1024, 600, 1.0), "real module opens 1024 x 600");
    BitmapGuiFrame frame{};
    CHECK(client.render(frame) && frame.width == 1024 && frame.height == 600 && (frame.flags & SAFIAN_BITMAP_GUI_FRAME_FULL) && frame.frame_id == 1, "first real frame is full 1024 x 600");
    size_t opaque = 0, lit = 0;
    for (uint32_t y = 0; y < frame.height; y += 7)
        for (uint32_t x = 0; x < frame.width; x += 7) {
            const uint8_t* px = frame.pixels + y * frame.stride_bytes + x * 4;
            opaque += px[3] == 255; lit += (px[0] | px[1] | px[2]) > 40;
        }
    CHECK(opaque > 0 && lit > 100, "the frame is opaque and not blank");
    // idle: ticks produce no frame (give the GUI's own start-up work a moment first)
    for (int i = 0; i < 40; ++i) { client.tick(); BitmapGuiFrame f; if (client.isDirty()) client.render(f); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
    const auto t0 = std::chrono::steady_clock::now();
    int dirtyTicks = 0;
    for (int i = 0; i < 2000; ++i) { client.tick(); dirtyTicks += client.isDirty() ? 1 : 0; }
    const auto idleUs = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / 2000.;
    std::cout << "real module idle tick()+is_dirty(): " << idleUs << " us, dirty ticks " << dirtyTicks << "/2000\n";
    CHECK(dirtyTicks == 0, "an idle real GUI stays clean");
    // a tap on the GUI changes pixels (touch pointer id 3), resize and scale work
    const uint64_t before = frame.frame_id;
    safian_bitmap_gui_pointer_event_t e{SAFIAN_BITMAP_GUI_POINTER_DOWN, SAFIAN_BITMAP_GUI_POINTER_KIND_TOUCH, 3, 300, 200, SAFIAN_BITMAP_GUI_BUTTON_PRIMARY, 0, 0.f};
    client.pointer(e); e.type = SAFIAN_BITMAP_GUI_POINTER_UP; e.buttons = 0; client.pointer(e);
    client.tick();
    if (client.isDirty()) { CHECK(client.render(frame) && frame.frame_id > before && frame.dirty_count >= 1, "a tap produces a partial frame"); }
    uint32_t w = 0, h = 0;
    CHECK(client.resize(800, 500) && client.size(w, h) && w == 800 && h == 500 && client.render(frame) && frame.width == 800 && (frame.flags & SAFIAN_BITMAP_GUI_FRAME_FULL), "resize to 800 x 500");
    CHECK(host.remotePages().size() > 10, "the real module's clap.remote-controls pages are read");
    (void)reqId; (void)reqPurpose; (void)texted; (void)page;
    client.close();
    CHECK(!client.isOpen(), "closed");
}

int main(int argc, char** argv) {
    testFake();
    testPrecedence();
    const char* real = argc > 1 ? argv[1] : std::getenv("KICK_SYNTHYBOY_CLAP");
    testRealModule(real);
    if (failures) { std::cerr << failures << " check(s) failed\n"; return 1; }
    std::cout << "bitmap gui tests passed\n";
    return 0;
}
