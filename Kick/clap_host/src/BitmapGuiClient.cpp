// SPDX-License-Identifier: AGPL-3.0-or-later
#include <kick_clap/BitmapGui.h>

namespace kick::clap {

BitmapGuiClient::BitmapGuiClient(Host& h) : host(h) {}
BitmapGuiClient::~BitmapGuiClient() { close(); }

bool BitmapGuiClient::supported(const Host& host) {
    const auto* api = host.bitmapGui();
    return api && api->create && api->destroy && api->get_info && api->get_preferred_size && api->can_resize && api->get_size_limits &&
           api->get_size && api->set_size && api->set_scale && api->tick && api->is_dirty && api->render && api->pointer_event &&
           api->wheel_event && api->key_event && api->text_input && api->set_focus && api->request_done;
}

bool BitmapGuiClient::open(uint32_t width, uint32_t height, double scale, uint32_t flags) {
    if (opened || !supported(host)) return false;
    api = host.bitmapGui();
    plugin = host.plugin();
    if (!api || !plugin) return false;
    BitmapGuiHostHandler::Capabilities caps;
    caps.resize = static_cast<bool>(sinks.requestResize);
    caps.cursor = static_cast<bool>(sinks.setCursor);
    caps.textInput = static_cast<bool>(sinks.requestTextInput);
    caps.file = static_cast<bool>(sinks.requestFile);
    caps.share = static_cast<bool>(sinks.shareFile);
    caps.url = static_cast<bool>(sinks.openUrl);
    caps.remotePage = static_cast<bool>(sinks.setActiveRemotePage);
    host.setBitmapGuiHandler(this, caps);   // before create(): the plug-in reads the host table there
    opened = api->create(plugin, width, height, scale, flags);
    if (!opened) { host.setBitmapGuiHandler(nullptr); api = nullptr; plugin = nullptr; }
    return opened;
}

void BitmapGuiClient::close() {
    if (opened && api && plugin) api->destroy(plugin);
    const bool wasInstalled = api != nullptr;
    opened = false; api = nullptr; plugin = nullptr;
    if (wasInstalled) host.setBitmapGuiHandler(nullptr);
}

void BitmapGuiClient::pluginUnloading() {
    if (opened && api && plugin) api->destroy(plugin);
    opened = false; api = nullptr; plugin = nullptr;
    host.setBitmapGuiHandler(nullptr);
    if (sinks.detached) sinks.detached();
}

// Size queries are valid before create(): look the extension up on demand.
bool BitmapGuiClient::info(safian_bitmap_gui_info_t& out) const {
    const auto* a = api ? api : host.bitmapGui();
    const auto* p = plugin ? plugin : host.plugin();
    out = {};
    return a && p && a->get_info && a->get_info(p, &out);
}
bool BitmapGuiClient::preferredSize(uint32_t& w, uint32_t& h) const {
    const auto* a = api ? api : host.bitmapGui();
    const auto* p = plugin ? plugin : host.plugin();
    return a && p && a->get_preferred_size && a->get_preferred_size(p, &w, &h);
}
bool BitmapGuiClient::canResize() const {
    const auto* a = api ? api : host.bitmapGui();
    const auto* p = plugin ? plugin : host.plugin();
    return a && p && a->can_resize && a->can_resize(p);
}
bool BitmapGuiClient::sizeLimits(safian_bitmap_gui_size_limits_t& out) const {
    const auto* a = api ? api : host.bitmapGui();
    const auto* p = plugin ? plugin : host.plugin();
    out = {};
    return a && p && a->get_size_limits && a->get_size_limits(p, &out);
}
bool BitmapGuiClient::size(uint32_t& w, uint32_t& h) const { return opened && api->get_size(plugin, &w, &h); }
bool BitmapGuiClient::resize(uint32_t w, uint32_t h) { return opened && api->set_size(plugin, w, h); }
bool BitmapGuiClient::setScale(double scale) { return opened && api->set_scale(plugin, scale); }

bool BitmapGuiClient::tick() { return opened && api->tick(plugin); }
bool BitmapGuiClient::isDirty() const { return opened && api->is_dirty(plugin); }
bool BitmapGuiClient::render(BitmapGuiFrame& frame) { frame = {}; return opened && api->render(plugin, &frame); }

bool BitmapGuiClient::pointer(const safian_bitmap_gui_pointer_event_t& e) { return opened && api->pointer_event(plugin, &e); }
bool BitmapGuiClient::wheel(const safian_bitmap_gui_wheel_event_t& e) { return opened && api->wheel_event(plugin, &e); }
bool BitmapGuiClient::key(const safian_bitmap_gui_key_event_t& e) { return opened && api->key_event(plugin, &e); }
bool BitmapGuiClient::text(const std::string& utf8) { return opened && api->text_input(plugin, utf8.c_str()); }
void BitmapGuiClient::setFocus(bool focused) { if (opened) api->set_focus(plugin, focused); }
void BitmapGuiClient::requestDone(uint32_t id, uint32_t result, const std::string& path) {
    if (opened) api->request_done(plugin, id, result, path.empty() ? nullptr : path.c_str());
}
bool BitmapGuiClient::showRemotePage(clap_id page) { return opened && api->show_remote_page && api->show_remote_page(plugin, page); }

void BitmapGuiClient::requestTextInput(bool show, const BitmapGuiRect* field, uint32_t kind, const char* text) {
    if (sinks.requestTextInput) sinks.requestTextInput(show, field, kind, text ? std::string(text) : std::string());
}
bool BitmapGuiClient::requestFile(uint32_t id, const safian_bitmap_gui_file_request_t& r) {
    if (!sinks.requestFile) return false;
    BitmapGuiFileRequest request;
    request.mode = r.mode;
    if (r.title) request.title = r.title;
    if (r.wildcards) request.wildcards = r.wildcards;
    if (r.suggested_name) request.suggestedName = r.suggested_name;
    if (r.purpose) request.purpose = r.purpose;
    return sinks.requestFile(id, request);
}

} // namespace kick::clap
