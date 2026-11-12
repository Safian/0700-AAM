// SPDX-License-Identifier: AGPL-3.0-or-later
#include "BitmapGuiComponent.h"
#include <chrono>
#include <cstdlib>
#include <cstring>

namespace kick::clap {
namespace {
uint32_t modifiersOf(const juce::ModifierKeys& m) {
    return (m.isShiftDown() ? SAFIAN_BITMAP_GUI_MOD_SHIFT : 0u) | (m.isCtrlDown() ? SAFIAN_BITMAP_GUI_MOD_CTRL : 0u) |
           (m.isAltDown() ? SAFIAN_BITMAP_GUI_MOD_ALT : 0u) | (m.isCommandDown() ? SAFIAN_BITMAP_GUI_MOD_META : 0u);
}
uint32_t buttonsOf(const juce::ModifierKeys& m) {
    return (m.isLeftButtonDown() ? SAFIAN_BITMAP_GUI_BUTTON_PRIMARY : 0u) | (m.isRightButtonDown() ? SAFIAN_BITMAP_GUI_BUTTON_SECONDARY : 0u) |
           (m.isMiddleButtonDown() ? SAFIAN_BITMAP_GUI_BUTTON_MIDDLE : 0u);
}
uint32_t keyCodeOf(const juce::KeyPress& key) {
    const int code = key.getKeyCode();
    if (code == juce::KeyPress::backspaceKey) return SAFIAN_BITMAP_GUI_KEY_BACKSPACE;
    if (code == juce::KeyPress::tabKey) return SAFIAN_BITMAP_GUI_KEY_TAB;
    if (code == juce::KeyPress::returnKey) return SAFIAN_BITMAP_GUI_KEY_ENTER;
    if (code == juce::KeyPress::escapeKey) return SAFIAN_BITMAP_GUI_KEY_ESCAPE;
    if (code == juce::KeyPress::deleteKey) return SAFIAN_BITMAP_GUI_KEY_DELETE;
    if (code == juce::KeyPress::insertKey) return SAFIAN_BITMAP_GUI_KEY_INSERT;
    if (code == juce::KeyPress::homeKey) return SAFIAN_BITMAP_GUI_KEY_HOME;
    if (code == juce::KeyPress::endKey) return SAFIAN_BITMAP_GUI_KEY_END;
    if (code == juce::KeyPress::pageUpKey) return SAFIAN_BITMAP_GUI_KEY_PAGE_UP;
    if (code == juce::KeyPress::pageDownKey) return SAFIAN_BITMAP_GUI_KEY_PAGE_DOWN;
    if (code == juce::KeyPress::leftKey) return SAFIAN_BITMAP_GUI_KEY_LEFT;
    if (code == juce::KeyPress::rightKey) return SAFIAN_BITMAP_GUI_KEY_RIGHT;
    if (code == juce::KeyPress::upKey) return SAFIAN_BITMAP_GUI_KEY_UP;
    if (code == juce::KeyPress::downKey) return SAFIAN_BITMAP_GUI_KEY_DOWN;
    if (code >= juce::KeyPress::F1Key && code <= juce::KeyPress::F12Key) return SAFIAN_BITMAP_GUI_KEY_F1 + static_cast<uint32_t>(code - juce::KeyPress::F1Key);
    if (code == juce::KeyPress::spaceKey) return ' ';
    if (code > 0 && code < 0x110000) return static_cast<uint32_t>(juce::CharacterFunctions::toLowerCase(static_cast<juce::juce_wchar>(code)));
    return 0;
}
} // namespace

double BitmapGuiComponent::renderScale(const juce::Component* forComponent) {
    static const double forced = [] { const char* v = std::getenv("KICK_BITMAP_GUI_SCALE"); return v ? std::atof(v) : 0.0; }();
    if (forced > 0.0) return forced;
    double result = 1.0;
    if (forComponent != nullptr)
        if (const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(forComponent->getScreenBounds()))
            result = display->scale;
    const double global = juce::Desktop::getInstance().getGlobalScaleFactor();
    return result * (global > 0.0 ? global : 1.0);
}

BitmapGuiComponent::BitmapGuiComponent(Host& h) : host(h), client(h) {
    setWantsKeyboardFocus(true);
    setOpaque(true);
    setMouseClickGrabsKeyboardFocus(true);
}

BitmapGuiComponent::~BitmapGuiComponent() {
    *alive = false;
    stopTimer();
    client.close();
}

bool BitmapGuiComponent::sizeLimits(juce::Rectangle<int>& minMax) const {
    safian_bitmap_gui_size_limits_t l{};
    if (!client.canResize() || !client.sizeLimits(l)) return false;
    minMax = juce::Rectangle<int>(static_cast<int>(l.min_width), static_cast<int>(l.min_height), static_cast<int>(l.max_width), static_cast<int>(l.max_height));
    return true;
}

void BitmapGuiComponent::wireSinks() {
    juce::Component::SafePointer<BitmapGuiComponent> safe(this);
    auto& s = client.sinks;
    s.requestRender = [safe](const BitmapGuiRect*) { if (safe) safe->scheduleSoon(1); };
    s.requestTick = [safe](uint32_t ms) { if (safe) safe->scheduleSoon(static_cast<int>(std::max<uint32_t>(1, std::min<uint32_t>(ms, 1000)))); };
    s.requestResize = [safe](uint32_t w, uint32_t h) {
        if (!safe) return false;
        juce::MessageManager::callAsync([safe, w, h] {
            if (!safe) return;
            // Our size is the parent's decision; honour a request by asking the parent to change it, or apply it to the plug-in directly.
            if (safe->getParentComponent() != nullptr && safe->onResizeRequest) safe->onResizeRequest(static_cast<int>(w), static_cast<int>(h));
            else safe->setSize(static_cast<int>(w), static_cast<int>(h));
        });
        return true;
    };
    s.setCursor = [safe](uint32_t c) {
        if (!safe) return;
        using MC = juce::MouseCursor;
        switch (c) {
            case SAFIAN_BITMAP_GUI_CURSOR_HIDDEN: safe->setMouseCursor(MC::NoCursor); break;
            case SAFIAN_BITMAP_GUI_CURSOR_POINTER: safe->setMouseCursor(MC::PointingHandCursor); break;
            case SAFIAN_BITMAP_GUI_CURSOR_TEXT: safe->setMouseCursor(MC::IBeamCursor); break;
            case SAFIAN_BITMAP_GUI_CURSOR_CROSSHAIR: safe->setMouseCursor(MC::CrosshairCursor); break;
            case SAFIAN_BITMAP_GUI_CURSOR_RESIZE_H: safe->setMouseCursor(MC::LeftRightResizeCursor); break;
            case SAFIAN_BITMAP_GUI_CURSOR_RESIZE_V: safe->setMouseCursor(MC::UpDownResizeCursor); break;
            case SAFIAN_BITMAP_GUI_CURSOR_RESIZE_ALL: safe->setMouseCursor(MC::UpDownLeftRightResizeCursor); break;
            case SAFIAN_BITMAP_GUI_CURSOR_GRAB: safe->setMouseCursor(MC::DraggingHandCursor); break;
            case SAFIAN_BITMAP_GUI_CURSOR_GRABBING: safe->setMouseCursor(MC::DraggingHandCursor); break;
            case SAFIAN_BITMAP_GUI_CURSOR_NOT_ALLOWED: safe->setMouseCursor(MC::NoCursor); break;
            default: safe->setMouseCursor(MC::NormalCursor); break;
        }
    };
    if (hooks.textInput)
        s.requestTextInput = [safe](bool show, const BitmapGuiRect* field, uint32_t kind, const std::string& text) {
            if (!safe || !safe->hooks.textInput) return;
            const juce::Rectangle<int> rect = field ? juce::Rectangle<int>(field->x, field->y, static_cast<int>(field->width), static_cast<int>(field->height)) : juce::Rectangle<int>();
            safe->hooks.textInput(show, rect, kind, juce::String::fromUTF8(text.c_str()));
        };
    if (hooks.shareFile)
        s.shareFile = [safe](uint32_t id, const std::string& path, const std::string& title) {
            if (!safe || !safe->hooks.shareFile) return false;
            auto flag = safe->alive;
            return safe->hooks.shareFile(juce::File(juce::String::fromUTF8(path.c_str())), juce::String::fromUTF8(title.c_str()), [safe, flag, id](bool ok) {
                if (*flag && safe) safe->client.requestDone(id, ok ? SAFIAN_BITMAP_GUI_RESULT_OK : SAFIAN_BITMAP_GUI_RESULT_FAILED);
            });
        };
    s.openUrl = [](const std::string& url) { return juce::URL(juce::String::fromUTF8(url.c_str())).launchInDefaultBrowser(); };
    if (hooks.chooseFile)
        s.requestFile = [safe](uint32_t id, const BitmapGuiFileRequest& request) {
            if (!safe || !safe->hooks.chooseFile) return false;
            auto flag = safe->alive;
            return safe->hooks.chooseFile(request, [safe, flag, id](uint32_t result, const juce::String& path) {
                if (*flag && safe) safe->client.requestDone(id, result, path.toStdString());
            });
        };
    if (hooks.remotePage) s.setActiveRemotePage = [safe](clap_id page) { if (safe && safe->hooks.remotePage) safe->hooks.remotePage(page); };
    s.detached = [safe] { if (safe) { safe->stopTimer(); if (safe->hooks.detached) safe->hooks.detached(); } };
}

bool BitmapGuiComponent::open(int w, int h) {
    if (client.isOpen()) return true;
    wireSinks();
    scale = renderScale(this);
    uint32_t ww = static_cast<uint32_t>(std::max(1, w)), hh = static_cast<uint32_t>(std::max(1, h));
    // A host whose native event loop runs on this thread (macOS, Windows, iOS: the JUCE app) sets the flag: the plug-in's timers and async messages are
    // delivered by that loop and tick() does not pump it. On Linux / the MPC there is no loop the plug-in's toolkit can join: tick() runs its queue.
#if JUCE_MAC || JUCE_WINDOWS || JUCE_IOS
    constexpr uint32_t createFlags = SAFIAN_BITMAP_GUI_CREATE_HOST_RUNS_EVENT_LOOP;
#else
    constexpr uint32_t createFlags = 0;
#endif
    if (!client.open(ww, hh, scale, createFlags)) return false;
    safian_bitmap_gui_info_t info{};
    if (client.info(info)) {
        const double clamped = std::max(info.min_scale, std::min(info.max_scale, scale));
        if (clamped != scale) { scale = clamped; client.setScale(scale); }
    }
    wanted = true;
    scheduleSoon(1);
    return true;
}

void BitmapGuiComponent::resized() {
    if (!client.isOpen() || getWidth() <= 0 || getHeight() <= 0) return;
    uint32_t w = 0, h = 0;
    if (client.size(w, h) && w == static_cast<uint32_t>(getWidth()) && h == static_cast<uint32_t>(getHeight())) return;
    client.resize(static_cast<uint32_t>(getWidth()), static_cast<uint32_t>(getHeight()));
    scheduleSoon(1);
}

void BitmapGuiComponent::parentHierarchyChanged() { visibilityChanged(); }
void BitmapGuiComponent::visibilityChanged() {
    const bool shown = isShowing() || (isVisible() && getParentComponent() != nullptr);
    if (!client.isOpen()) return;
    if (shown) { wanted = true; scheduleSoon(1); } else { wanted = false; stopTimer(); }
}

void BitmapGuiComponent::scheduleSoon(int ms) {
    if (!wanted && !isVisible()) return;
    if (!isTimerRunning() || getTimerInterval() > ms) startTimer(std::max(1, ms));
}

void BitmapGuiComponent::timerCallback() { step(); }

void BitmapGuiComponent::step() {
    if (!client.isOpen()) { stopTimer(); return; }
    ++stats.ticks;
    int next = idleMs;
    const auto tickStart = std::chrono::steady_clock::now();
    const bool dirty = client.tick() || client.isDirty();
    stats.tickMicroseconds += std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - tickStart).count();
    if (dirty) {
        const auto start = std::chrono::steady_clock::now();
        BitmapGuiFrame frame;
        if (client.render(frame)) {
            adoptFrame(frame);
            if (frame.flags & SAFIAN_BITMAP_GUI_FRAME_NEEDS_ANOTHER) next = static_cast<int>(std::max<uint32_t>(4, std::min<uint32_t>(frame.next_frame_ms ? frame.next_frame_ms : 16, idleMs)));
        }
        stats.renderMicroseconds += std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
    }
    startTimer(next);
}

void BitmapGuiComponent::adoptFrame(const BitmapGuiFrame& frame) {
    if (frame.pixels == nullptr || frame.width == 0 || frame.height == 0) return;
    ++stats.renders;
    if (!image.isValid() || image.getWidth() != static_cast<int>(frame.width) || image.getHeight() != static_cast<int>(frame.height)) {
        image = juce::Image(juce::Image::ARGB, static_cast<int>(frame.width), static_cast<int>(frame.height), false); // only on a resize
    }
    {
        juce::Image::BitmapData data(image, juce::Image::BitmapData::writeOnly);
        for (uint32_t i = 0; i < frame.dirty_count; ++i) {
            const auto& r = frame.dirty[i];
            if (r.x < 0 || r.y < 0 || static_cast<uint32_t>(r.x) + r.width > frame.width || static_cast<uint32_t>(r.y) + r.height > frame.height) continue;
            for (uint32_t row = 0; row < r.height; ++row)
                std::memcpy(data.getLinePointer(r.y + static_cast<int>(row)) + static_cast<size_t>(r.x) * 4,
                            frame.pixels + static_cast<size_t>(r.y + static_cast<int>(row)) * frame.stride_bytes + static_cast<size_t>(r.x) * 4, static_cast<size_t>(r.width) * 4);
            stats.pixelsCopied += static_cast<uint64_t>(r.width) * r.height;
            repaint(juce::Rectangle<float>(static_cast<float>(r.x) / static_cast<float>(scale), static_cast<float>(r.y) / static_cast<float>(scale),
                                           static_cast<float>(r.width) / static_cast<float>(scale), static_cast<float>(r.height) / static_cast<float>(scale)).getSmallestIntegerContainer().expanded(1));
        }
    }
}

void BitmapGuiComponent::paint(juce::Graphics& g) {
    if (!image.isValid()) { g.fillAll(juce::Colour(0xff15191e)); return; }
    const auto drawn = juce::Rectangle<int>(0, 0, juce::roundToInt(image.getWidth() / scale), juce::roundToInt(image.getHeight() / scale));
    if (drawn.getWidth() < getWidth() || drawn.getHeight() < getHeight()) { g.setColour(juce::Colour(0xff15191e)); g.fillAll(); }
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    if (scale == 1.0) g.drawImageAt(image, 0, 0);
    else g.drawImageTransformed(image, juce::AffineTransform::scale(static_cast<float>(1.0 / scale)));
}

// ---- input ------------------------------------------------------------------------------------------------------------------------------
void BitmapGuiComponent::sendPointer(const juce::MouseEvent& e, uint32_t type) {
    if (!client.isOpen()) return;
    safian_bitmap_gui_pointer_event_t p{};
    p.type = type;
    const int slot = touchSlotProvider ? touchSlotProvider() : -1;
    p.kind = slot >= 0 || e.source.isTouch() ? SAFIAN_BITMAP_GUI_POINTER_KIND_TOUCH : (e.source.isPen() ? SAFIAN_BITMAP_GUI_POINTER_KIND_PEN : SAFIAN_BITMAP_GUI_POINTER_KIND_MOUSE);
    // mouse = 0; a finger is the bridge's slot, else its input source index, plus 1
    p.pointer_id = p.kind == SAFIAN_BITMAP_GUI_POINTER_KIND_MOUSE ? 0u : static_cast<uint32_t>(slot >= 0 ? slot : e.source.getIndex()) + 1u;
    p.x = e.position.x; p.y = e.position.y;
    const bool released = type == SAFIAN_BITMAP_GUI_POINTER_UP || type == SAFIAN_BITMAP_GUI_POINTER_HOVER || type == SAFIAN_BITMAP_GUI_POINTER_LEAVE;
    p.buttons = released ? 0u : buttonsOf(e.mods);
    if (p.buttons == 0 && !released) p.buttons = SAFIAN_BITMAP_GUI_BUTTON_PRIMARY; // a touch is the primary button
    p.modifiers = modifiersOf(e.mods);
    p.pressure = e.isPressureValid() ? e.pressure : 0.f;
    client.pointer(p);
    scheduleSoon(1);
}
void BitmapGuiComponent::mouseDown(const juce::MouseEvent& e) { grabKeyboardFocus(); sendPointer(e, SAFIAN_BITMAP_GUI_POINTER_DOWN); }
void BitmapGuiComponent::mouseDrag(const juce::MouseEvent& e) { sendPointer(e, SAFIAN_BITMAP_GUI_POINTER_MOVE); }
void BitmapGuiComponent::mouseUp(const juce::MouseEvent& e) { sendPointer(e, SAFIAN_BITMAP_GUI_POINTER_UP); }
void BitmapGuiComponent::mouseMove(const juce::MouseEvent& e) { if (!e.source.isTouch()) sendPointer(e, SAFIAN_BITMAP_GUI_POINTER_HOVER); }
void BitmapGuiComponent::mouseExit(const juce::MouseEvent& e) { if (!e.source.isTouch()) sendPointer(e, SAFIAN_BITMAP_GUI_POINTER_LEAVE); }

void BitmapGuiComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
    if (!client.isOpen()) return;
    safian_bitmap_gui_wheel_event_t ev{};
    ev.x = e.position.x; ev.y = e.position.y;
    // JUCE reports a detent as about 0.1 of deltaY (and a precise gesture in the same unit): 10 per notch
    ev.delta_x = w.deltaX * 10.0 * (w.isReversed ? -1 : 1); ev.delta_y = w.deltaY * 10.0 * (w.isReversed ? -1 : 1);
    ev.modifiers = modifiersOf(e.mods);
    ev.flags = w.isSmooth ? SAFIAN_BITMAP_GUI_WHEEL_PRECISE : 0u;
    client.wheel(ev);
    scheduleSoon(1);
}

bool BitmapGuiComponent::keyPressed(const juce::KeyPress& key) {
    if (!client.isOpen()) return false;
    const auto code = keyCodeOf(key);
    if (code == 0) return false;
    const auto character = key.getTextCharacter();
    const bool printable = character >= 32 && character != 127 && !key.getModifiers().isCommandDown() && !key.getModifiers().isCtrlDown();
    const auto utf8 = printable ? juce::String::charToString(character).toStdString() : std::string();
    safian_bitmap_gui_key_event_t ev{SAFIAN_BITMAP_GUI_KEY_DOWN_EVENT, code, modifiersOf(key.getModifiers()), 0u, printable ? utf8.c_str() : nullptr};
    const bool used = client.key(ev);
    ev.type = SAFIAN_BITMAP_GUI_KEY_UP_EVENT; ev.text = nullptr;
    client.key(ev); // JUCE reports no key release here: the press is complete
    scheduleSoon(1);
    return used;
}

void BitmapGuiComponent::commitText(const juce::String& text, bool replaceAll) {
    if (!client.isOpen()) return;
    if (replaceAll) {
        safian_bitmap_gui_key_event_t all{SAFIAN_BITMAP_GUI_KEY_DOWN_EVENT, 'a', SAFIAN_BITMAP_GUI_MOD_CTRL, 0u, nullptr};
        client.key(all);
        all.type = SAFIAN_BITMAP_GUI_KEY_UP_EVENT; client.key(all);
    }
    if (text.isNotEmpty()) client.text(text.toStdString());
    else { safian_bitmap_gui_key_event_t del{SAFIAN_BITMAP_GUI_KEY_DOWN_EVENT, SAFIAN_BITMAP_GUI_KEY_BACKSPACE, 0u, 0u, nullptr}; client.key(del); }
    scheduleSoon(1);
}

void BitmapGuiComponent::focusGained(FocusChangeType) { client.setFocus(true); }
void BitmapGuiComponent::focusLost(FocusChangeType) { client.setFocus(false); scheduleSoon(1); }
} // namespace kick::clap
