#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "common/source/kick_logger.h"
#include "hardware/device_factory.h"
#include "hardware/pad_colour_map.h"
#include "hardware/led_map.h"
#include "hardware/key_transpose.h"
#include "hardware/strip_modes.h"
#include "hardware/hw_targets.h"
#include "hardware/modal_dialog_keys.h"
#include "hardware/pad_level.h"
#include "hardware/system_probe.h"
#include "hardware/touch_space.h"
#include "contracts/dcl/dcl_signals.h"
#include "contracts/dcl/dcl_contracts.h"
#include "contracts/dcl/dcl_contracts_helper.h"
#include "kick_ui/kick_ui.h"
#include "kick_ui_controls/source/kick_Theme.h"
#include "kick_ui_controls/source/kick_ControlBase.h"
#include "audio_engine/audio_engine.h"
#include <cerrno>
#include <cstring>
#include <chrono>
#include <atomic>
#include <array>
#include <cstdint>
#include <mutex>
#include <typeinfo>

class KickHardwareBridge : public IHardwareEventListener, private juce::Timer
{
private:
    std::unique_ptr<IHardwareAdapter> m_hw;
    juce::Component* m_root = nullptr;
    juce::DocumentWindow* m_window = nullptr;
    juce::Image m_framebuffer_img;
    static constexpr int kFbW = 1280; // framebuffer / touch reporting space (see timerCallback, touch()->start)
    static constexpr int kFbH = 800;
    int m_last_touch_x[10] = { 0 };
    int m_last_touch_y[10] = { 0 };
    juce::Component::SafePointer<juce::Component> m_touch_targets[10];
    // Manual path only: finger-down position (UI space) and "left the touch slop" latch, see exceedsTouchSlop().
    int  m_touch_down_x[10] = { 0 };
    int  m_touch_down_y[10] = { 0 };
    bool m_touch_dragging[10] = { false };
    // Double tap: the synthetic MouseEvents used to carry numberOfClicks = 1 always, so JUCE never saw a double click
    // (Sample Assign pad assignment, "double-tap to enter" in the MPC manual). A tap that starts within 450 ms and 24 px of
    // the previous release of the same slot counts as the next click.
    juce::int64 m_last_up_ms[10] = { 0 };
    int         m_last_up_x[10]  = { 0 };
    int         m_last_up_y[10]  = { 0 };
    int         m_click_count[10] = { 0 };
    // Peer-path touch source (message thread only): probed on the first delivery, see deliverPeerTouch().
    bool m_touch_src_probed = false;
    bool m_touch_src_ok     = true;
    std::chrono::steady_clock::time_point m_last_tap_time;
    // Akai MPC convention (T20): the Data Dial edits the tempo only while TAP TEMPO is physically held. The former
    // sticky "tempo edit" mode (set by TAP TEMPO, cleared by ENTER/STOP) hijacked +/- and the dial and is removed.
    // The original Kick UI has no consumer of EN_TAP_TEMPO (only MIDI-control mapping / hardware drawer), so the
    // bridge keeps the BPM handling itself.
    bool m_tap_tempo_held = false;
    // FULL LEVEL button: pad hits at 127 (Full Level) or at the user level 64 (Shift + button, Half Level). The state is
    // changed on the message thread (button) and read on the ALSA reader thread (pad hit), so it is a packed atomic.
    std::atomic<uint32_t> m_pad_level_bits { 0 };  // bit0 full, bit1 user level, bits 8..15 user level velocity
    int m_frame_counter = 0;
    bool m_theme_reapplied = false;
    int m_cmd_fd = -1;
    int m_cmd_fd_w = -1;
    std::string m_cmd_buf;
    // Two Shift buttons (left / right) share one Kick Shift state: EN_SHIFT goes out only on 0->1 / 1->0 (F10).
    kick_hw::ShiftCounter m_shift;
    // T12/Y4 touch strip mode (kick_hw::strip::Mode): Kick slider, pitch bend (spring back), pad level, mod wheel
    // (CC1), track level, sustain (CC64), expression (CC11), off. Mode and state are atomics: the strip arrives on the
    // ALSA reader thread, the mode is cycled there too (button) or on the message thread (kick_cmd injector).
    std::atomic<int>  m_strip_mode { 0 };
    std::atomic<bool> m_strip_pitch_sent { false };  // a pitch value was sent and not yet sprung back to centre
    std::atomic<int>  m_strip_last_pad { -1 };       // Pad Level mode target: pad index 0..15 of the last pad hit (the note is
                                                     // computed on the message thread from the ENGINE bank, see postStripPadLevel)
    // Newest strip value waiting for the message thread (-1 = none queued): the level modes edit the engine through
    // the same calls as the UI (message thread), and only one callAsync is pending at a time.
    std::atomic<int>  m_strip_pad_level_pending { -1 };
    std::atomic<int>  m_strip_track_level_pending { -1 };
    bool m_locate_pressed = false;
    // DCL code sent on press of the Shift-dependent TC / AUTOMATION buttons (-1 = none), so the release
    // goes to the same code even if Shift was released first.
    int  m_tc_sent_code = -1;
    static constexpr int kTcNoPacket = -2; // Shift+TC handled in the engine: no press packet, so no release packet
    int  m_automation_sent_code = -1;
    // Generic version for the other Shift-dependent buttons: DCL code sent on press, indexed by HardwareButton.
    struct PressedCodes { std::array<int, 128> v; PressedCodes() { v.fill(-1); } };
    PressedCodes m_pressed_code;
    // Buttons whose press closed the Timing Correct dialog: their release is swallowed too.
    std::array<bool, NUM_HW_BUTTONS> m_modal_consumed{};
    // Written on the message thread (button handlers), read by the MIDI reader thread (pad idle colour).
    std::atomic<int> m_current_bank { 0 };
    std::atomic<bool> m_stopping { false };
    // Pending callAsync lambdas capture this shared flag (by copy), not only `this`: it outlives the bridge, so a lambda
    // that runs after destruction sees false and returns without touching the freed object (m_stopping dies with it).
    std::shared_ptr<std::atomic<bool>> m_alive { std::make_shared<std::atomic<bool>>(true) };
    // Written on the message thread (OCT buttons), read by the MIDI reader thread (keybed note-on / note-off).
    std::mutex   m_transpose_mutex;
    KeyTranspose m_key_transpose;
    int m_key_pitch_map[128];

    // F4/F5: live MIDI must end on the track that received its start, not on whatever is selected at that moment
    // (track switch while a key / pedal / bend is held, ARP, sustain). Guarded by m_target_mutex (reader thread and
    // the kick_cmd injector on the message thread both reach the on_* callbacks).
    std::mutex m_target_mutex;
    kick_hw::NoteTargets<mt::MTSelected> m_key_target;
    kick_hw::HeldTarget<mt::MTSelected>  m_bend_target;       // pitch wheel (on_pitch_bend)
    kick_hw::HeldTarget<mt::MTSelected>  m_strip_bend_target; // touch strip in pitch bend mode (own target, G16)
    kick_hw::SustainState<mt::MTSelected> m_sustain; // physical pedal + touch strip Sustain (own target each) + strip flag; only under m_target_mutex

    // Project pad colours (DCL EN_PAD_SET_COLOUR_STATE). Written by the sendCommandToDcl handler
    // (any thread), read by the timer (message thread) and the MIDI reader thread. Lock-free.
    static_assert(contracts::DeviceControlContracts::EN_PAD_SET_COLOUR_STATE == 6, "pad colour packet command");
    static_assert(contracts::DeviceControlContracts::PadColourTrans == kick_hw::kPadColourTrans, "pad colour ids");
    static_assert(contracts::DeviceControlContracts::padColourDarkRed == 0 &&
                  contracts::DeviceControlContracts::PadColourNeutralGrey == kick_hw::kPadColourCount - 1, "pad colour ids");
    static_assert(contracts::DeviceControlContracts::StateOff == kick_hw::kPadStateOff &&
                  contracts::DeviceControlContracts::StateOn == kick_hw::kPadStateOn &&
                  contracts::DeviceControlContracts::StateSampleAssigned == kick_hw::kPadStateSampleAssigned &&
                  contracts::DeviceControlContracts::StateSampleNotAssigned == kick_hw::kPadStateSampleNotAssigned &&
                  contracts::DeviceControlContracts::StateOnBlink == kick_hw::kPadStateOnBlink &&
                  contracts::DeviceControlContracts::StateOffBlink == kick_hw::kPadStateOffBlink, "pad states");
    std::atomic<uint8_t> m_pad_colour_id[16] = {};
    std::atomic<uint8_t> m_pad_state[16] = {};
    std::atomic<uint16_t> m_pad_dirty { 0 };      // pads whose project colour changed, to be written by the timer
    std::atomic<uint16_t> m_pad_held { 0 };       // pads currently flashing (hit); the timer must not overwrite them
    std::atomic<bool> m_have_project_colours { false };
    std::atomic<bool> m_pad_aftertouch_enabled { false }; // set by the UI via EN_PAD_SET_AFTER_TOUCH
    std::atomic<uint8_t> m_pad_threshold { 4 };
    std::atomic<uint8_t> m_pad_sensitivity { 4 };
    std::atomic<uint8_t> m_pad_curve { 2 };
    std::atomic<uint8_t> m_pad_last_vel[16] = {};         // note-on velocity of the held pad (aftertouch packet field 3)
    sigslot::connection m_pad_colour_conn;
    sigslot::connection m_screen_conn;
    int m_last_screen = -1;      // last loaded kick_ui::Screen (message thread only), for the [DIAL] diagnostic log
    int m_dial_log_count = 0;    // handleJogDial calls so far (message thread only), thins the [DIAL] log
    sigslot::connection m_play_conn;

    void onDclCommand(std::shared_ptr<ipc::MTIPCJson> message)
    {
        CPktMgr::HWMsg_t msg;
        if (!contracts::DclContractsHelper::ipcMessageToHWMessage(message, msg)) return;
        if (msg.pktType == CPktMgr::EN_PKT_TYPE_PAD &&
            msg.pktCmd == contracts::DeviceControlContracts::EN_PAD_SET_AFTER_TOUCH && !msg.data.empty()) {
            // The UI (Hardware settings) switches pad aftertouch reports on/off, as it does on the Kick hardware.
            m_pad_aftertouch_enabled = msg.data[0] != 0;
            return;
        }
        if (msg.pktType == CPktMgr::EN_PKT_TYPE_PAD &&
            msg.pktCmd == contracts::DeviceControlContracts::EN_PAD_SETTINGS && msg.data.size() >= 4) {
            // [16, threshold, sensitivity, curve] from SettingsHardware
            m_pad_threshold.store(static_cast<uint8_t>(msg.data[1]));
            m_pad_sensitivity.store(static_cast<uint8_t>(msg.data[2]));
            m_pad_curve.store(static_cast<uint8_t>(msg.data[3]));
            return;
        }
        if (msg.pktType != CPktMgr::EN_PKT_TYPE_PAD ||
            msg.pktCmd != contracts::DeviceControlContracts::EN_PAD_SET_COLOUR_STATE) return;
        if (msg.data.size() < 32) return; // [16 x colour id][16 x state]
        uint16_t changed = 0;
        for (int i = 0; i < 16; ++i) {
            const auto id = static_cast<uint8_t>(msg.data[i]);
            const auto st = static_cast<uint8_t>(msg.data[16 + i]);
            if (m_pad_colour_id[i].exchange(id) != id) changed |= uint16_t(1u << i);
            if (m_pad_state[i].exchange(st) != st) changed |= uint16_t(1u << i);
        }
        m_pad_dirty.fetch_or(changed);
        // First packet: write all pads once, even if they equal the zero-initialised arrays.
        if (!m_have_project_colours.exchange(true)) m_pad_dirty.fetch_or(0xFFFF);
    }

    // Idle colour of a pad: project colour once a packet has been received (known id), else the bank colour.
    void getPadIdleColor(int pad, uint8_t& r, uint8_t& g, uint8_t& b) const
    {
        if (m_have_project_colours.load()) {
            kick_hw::PadRgb7 c;
            if (kick_hw::padColourToRgb7(m_pad_colour_id[pad].load(), m_pad_state[pad].load(), c)) {
                r = c.r; g = c.g; b = c.b;
                return;
            }
        }
        getBankColor(m_current_bank.load(), r, g, b);
    }

    void flushDirtyPadColors()
    {
        uint16_t dirty = m_pad_dirty.exchange(0);
        dirty &= static_cast<uint16_t>(~m_pad_held.load());
        if (!dirty || !m_hw || !m_hw->control_surface()) return;
        for (int i = 0; i < 16; ++i) {
            if (!(dirty & (1u << i))) continue;
            uint8_t r, g, b;
            getPadIdleColor(i, r, g, b);
            m_hw->control_surface()->set_pad_rgb(i, r, g, b);
        }
    }

    bool isShiftDown() const { return m_shift.down(); }
    bool isLocateDown() const { return m_locate_pressed; }

    void getBankColor(int bank, uint8_t& r, uint8_t& g, uint8_t& b) const
    {
        switch (bank % 8) {
            case 0: r = 35; g = 15; b = 0;  break; // Bank A: Warm Amber / Gold
            case 1: r = 0;  g = 25; b = 45; break; // Bank B: Electric Cyan
            case 2: r = 0;  g = 45; b = 10; break; // Bank C: Emerald Green
            case 3: r = 40; g = 0;  b = 45; break; // Bank D: Vivid Magenta
            case 4: r = 45; g = 35; b = 0;  break; // Bank E: Bright Yellow
            case 5: r = 45; g = 10; b = 20; break; // Bank F: Coral / Rose
            case 6: r = 15; g = 45; b = 25; break; // Bank G: Aqua Mint
            case 7: r = 25; g = 15; b = 45; break; // Bank H: Violet Lavender
            default: r = 25; g = 25; b = 25; break;
        }
    }

    void refreshPadColors()
    {
        if (m_hw && m_hw->control_surface()) {
            m_pad_dirty.store(0); // everything is written below
            for (int i = 0; i < 16; ++i) {
                uint8_t r, g, b;
                getPadIdleColor(i, r, g, b);
                m_hw->control_surface()->set_pad_rgb(i, r, g, b);
            }
        }
    }

    void updateBankLeds()
    {
        if (m_hw && m_hw->control_surface()) {
            // Banks A..D: full colour A of their LED; E..H: full colour B of the LED of (bank % 4).
            setLed(HW_LED_BANK_A, kick_hw::bankLedLevel(m_current_bank.load(), 0));
            setLed(HW_LED_BANK_B, kick_hw::bankLedLevel(m_current_bank.load(), 1));
            setLed(HW_LED_BANK_C, kick_hw::bankLedLevel(m_current_bank.load(), 2));
            setLed(HW_LED_BANK_D, kick_hw::bankLedLevel(m_current_bank.load(), 3));
            refreshPadColors();
        }
    }

    // --- Button LEDs (T13). Writes are cheap (the adapter caches the last level and writes only changes); they
    // may come from the message thread (screen signal, timer) or the MIDI reader thread (button handlers).
    void setLed(HardwareLed led, int level)
    {
        if (m_hw && m_hw->control_surface()) m_hw->control_surface()->set_led_level(led, level);
    }

    // Who gets hardware REC / OVERDUB: the Looper keeps them while its screen is shown or it is recording (it is the only
    // EN_RECORD / EN_OVERDUB consumer of the original Kick); on every other screen they drive the SEQUENCE recording
    // (arm the sequence, PLAY starts the take). Decided by the screen, as the original Kick routes buttons per screen.
    static bool recOverdubGoesToLooper()
    {
        return mt::getAudioInterface().isLooperRecording()
            || kick_ui::KickHome::Instance().getCurrentScreen() == kick_ui::Screen::MenuLooper;
    }

    // REC / OVERDUB LEDs follow the looper state while the looper records, else the SEQUENCE recording state (armed = steady
    // red, recording = blinking red, overdub = green), polled from the 30 Hz timer on the message thread like
    // kick_LoopRecorder does. The state getters read atomics / a short lock. The adapter cache drops unchanged writes,
    // so the per-tick call costs nothing on the wire.
    void updateRecOverdubLeds()
    {
        if (!m_hw || !m_hw->control_surface()) return;
        auto& ae = mt::getAudioInterface();
        if (!ae.isProjectLoaded()) return;
        const long long ms = (long long)juce::Time::getMillisecondCounter();
        int rec = 0, od = 0;
        kick_hw::recOverdubLedLevels(ae.isLooperRecording(), ae.isLooperOverdubbing(), ae.isSequenceRecArmed(),
                                     ae.isSequenceRecording(), ae.isSequenceOverdub(), kick_hw::blinkPhase(ms), rec, od);
        setLed(HW_LED_REC, rec);
        setLed(HW_LED_OVERDUB, od);
    }

    // Mode button of a screen, as the original Kick lights it (kick_leds.cpp: onScreenLoaded -> setLEDON), but with
    // the Akai panel's buttons: Shift-alternates of a button get the second colour (two-colour LED) or dim.
    struct ModeLed { HardwareLed led; int level; };
    static ModeLed modeLedForScreen(kick_ui::Screen s)
    {
        using S = kick_ui::Screen;
        switch (s) {
            case S::HomeGridView:       return { HW_LED_GRID,             kick_hw::kLedFullA };
            case S::HomeSequencerView:  return { HW_LED_GRID,             kick_hw::kLedDimA };   // Shift+GRID
            case S::MenuChannelMixer:   return { HW_LED_MIX,              kick_hw::kLedFullA };
            case S::MenuPadMixer:       return { HW_LED_MIX,              kick_hw::kLedFullB };  // Shift+MIX
            case S::MenuTrackMute:      return { HW_LED_MUTE,             kick_hw::kLedFullA };
            case S::MenuPadMute:        return { HW_LED_MUTE,             kick_hw::kLedFullB };  // Shift+MUTE
            case S::MenuProgramEdit:    return { HW_LED_EDIT,             kick_hw::kLedFullA };
            case S::HomeXYView:         return { HW_LED_TOUCH_FX,         kick_hw::kLedFullA };
            case S::MenuSampleEdit:     return { HW_LED_SAMPLE_EDIT,      kick_hw::kLedFullA };
            case S::MenuSampler:        return { HW_LED_SAMPLE_EDIT,      kick_hw::kLedDimA };   // Shift+SAMPLE EDIT
            case S::MenuPadPerform:     return { HW_LED_KEYBOARD_CONTROL, kick_hw::kLedFullA };
            case S::MenuMidiControl:    return { HW_LED_KEYBOARD_CONTROL, kick_hw::kLedDimA };   // Shift+KEYBOARD CONTROL
            case S::HomeListView:       return { HW_LED_MAIN,             kick_hw::kLedFullA };
            case S::MenuFileBrowser:    return { HW_LED_SOUNDS,           kick_hw::kLedFullA };
            case S::HomeOptions:        return { HW_LED_MENU,             kick_hw::kLedFullA };
            case S::MenuKnobEdit:       return { HW_LED_QLINK,            kick_hw::kLedFullA };  // Shift+Q-LINK
            default:                    return { NUM_HW_LEDS, 0 };                               // no mode button
        }
    }

    // Lights the mode button of the current screen, all other mode buttons off.
    void updateModeLeds(kick_ui::Screen current)
    {
        static const HardwareLed modeLeds[] = { HW_LED_GRID, HW_LED_MIX, HW_LED_MUTE, HW_LED_EDIT, HW_LED_TOUCH_FX,
                                                HW_LED_SAMPLE_EDIT, HW_LED_KEYBOARD_CONTROL, HW_LED_MAIN,
                                                HW_LED_SOUNDS, HW_LED_MENU, HW_LED_QLINK };
        const ModeLed m = modeLedForScreen(current);
        for (HardwareLed l : modeLeds) setLed(l, l == m.led ? m.level : kick_hw::kLedOff);
    }

    void updateShiftLed() { setLed(HW_LED_SHIFT, m_shift.down() ? kick_hw::kLedFullA : kick_hw::kLedOff); }

    void updateArpLeds()
    {
        auto& ae = mt::getAudioInterface();
        const auto sel = ae.getSelected();
        const bool arp = ae.isArpEnabled(sel);
        setLed(HW_LED_ARP, arp ? kick_hw::kLedFullA : kick_hw::kLedOff);
        setLed(HW_LED_LATCH, (arp && ae.isArpLatch(sel)) ? kick_hw::kLedFullA : kick_hw::kLedOff);
    }

    void updateTimingCorrectLed()
    {
        setLed(HW_LED_TIMING_CORRECT, mt::getAudioInterface().isTimingCorrectEnabled() ? kick_hw::kLedFullA : kick_hw::kLedOff);
    }

    // Start-up state: every button LED off, then the ones that reflect the current state.
    void initButtonLeds()
    {
        if (!m_hw || !m_hw->control_surface()) return;
        for (int l = 0; l < NUM_HW_LEDS; ++l) setLed((HardwareLed)l, kick_hw::kLedOff);
        setLed(HW_LED_STOP, kick_hw::kLedFullA);
        updateBankLeds();
        updateOctaveLeds();
        updateModeLeds(kick_ui::KickHome::Instance().getCurrentScreen());
        updateShiftLed();
        updateArpLeds();
        updateTimingCorrectLed();
        setLed(HW_LED_TOUCH_STRIP, kick_hw::strip::modeLedLevel(m_strip_mode.load())); // strip mode LED (Kick slider = dim) from the start
    }

    void updateOctaveLeds()
    {
        if (m_hw && m_hw->control_surface()) {
            m_hw->control_surface()->set_led(HW_LED_OCTAVE_DOWN, m_key_transpose.octave() < 0);
            m_hw->control_surface()->set_led(HW_LED_OCTAVE_UP, m_key_transpose.octave() > 0);
            for (int pos = -3; pos <= 3; ++pos)
                m_hw->control_surface()->set_led_level(kick_hw::octaveBarLed(pos), kick_hw::octaveBarLevel(m_key_transpose.octave(), pos));
        }
    }

    void sendButtonMsg(int buttonCode, int state)
    {
        if (buttonCode == contracts::DeviceControlContracts::BtnLedSequence::EN_SHIFT) {
            contracts::shiftBtnState = (state == contracts::DeviceControlContracts::BTN_PRESSED);
        }
        CPktMgr::HWMsg_t msg;
        msg.pktType = CPktMgr::EN_PKT_TYPE_BUTTON;
        msg.pktCmd = contracts::DeviceControlContracts::BtnCmd::EN_BTN_STATUS;
        msg.data = { buttonCode, state };
        contracts::DclSignals::dispatchHardwareInput(msg);
    }

    // Press: remember which DCL code went out for this physical button, so the release matches it.
    void sendTrackedPress(HardwareButton btn, int code)
    {
        if (btn >= 0 && btn < (int)m_pressed_code.v.size())
            m_pressed_code.v[(size_t)btn] = code;
        sendButtonMsg(code, contracts::DeviceControlContracts::BTN_PRESSED);
    }

    // Release: BTN_RELEASED for the remembered code (then forgotten); else for fallbackCode; fallbackCode < 0 = send nothing.
    void sendTrackedRelease(HardwareButton btn, int fallbackCode)
    {
        int code = fallbackCode;
        if (btn >= 0 && btn < (int)m_pressed_code.v.size() && m_pressed_code.v[(size_t)btn] >= 0) {
            code = m_pressed_code.v[(size_t)btn];
            m_pressed_code.v[(size_t)btn] = -1;
        }
        if (code >= 0)
            sendButtonMsg(code, contracts::DeviceControlContracts::BTN_RELEASED);
    }

    // LOCATE + "<" / ">": move the playhead to the previous / next note start of the SELECTED track (Akai MPC
    // convention), no wrap. No earlier event -> sequence start; no later event -> stay. This runs on the MIDI reader
    // thread, but the note list is a juce::ValueTree edited on the message thread, so the lookup and the seek are
    // done there (same pattern as dispatchTouch). The seek itself is applied on the tick thread by the engine.
    void locateToEvent(int direction)
    {
        juce::MessageManager::callAsync([direction]() {
            auto&      ae   = mt::getAudioInterface();
            const auto sel  = ae.getSelected();
            const int  from = ae.getSequenceSeekTick(sel.sequence);
            if (from < 0) return;
            int target = ae.findEventTick(sel, from, direction);
            if (target < 0 && direction < 0 && from > 0) target = 0;
            if (target >= 0) ae.seekToTick(sel.sequence, target);
        });
    }

    // G7: "<" / ">" step the playhead by one grid step (Akai MPC convention): the TIMING CORRECT grid, 24 ticks (1/16)
    // when it is 0 (= one bar) or unset. Relative to the pending/parked seek or the live playhead; the engine clamps
    // the tick. Runs on the message thread (like locateToEvent); the seek itself is applied on the tick thread.
    void stepPlayhead(int direction)
    {
        juce::MessageManager::callAsync([this, alive = m_alive, direction]() {
            if (!alive->load()) return;
            auto&      ae   = mt::getAudioInterface();
            const int  seq  = ae.getSelectedSequence();
            const int  from = ae.getSequenceSeekTick(seq);
            if (from < 0) return;
            const int grid = ae.getTimingCorrectParams().gridTicks;
            const int step = grid > 0 ? grid : 24;
            ae.seekToTick(seq, from + direction * step);
        });
    }

    // Peer-path delivery. The JUCE multitouch patch allows touch input sources without XInput2 too (canUseTouch() is
    // true), so a finger is a real touch source and Button::mouseDrag/mouseUp use its own position. The first call still
    // probes whether JUCE made a touch source; if not (unpatched JUCE), finger 0 is delivered as InputSourceType::mouse (source 0) from then on. Real XI
    // touch on a display, macOS and iOS keep the touch type. Message thread only.
    void deliverPeerTouch(juce::ComponentPeer* peer, int ix, int iy, juce::ModifierKeys mods, int slot, bool log)
    {
        using ST = juce::MouseInputSource::InputSourceType;
        auto send = [&](ST type, int index) {
            peer->handleMouseEvent(type, peer->globalToLocal(juce::Point<float>((float)ix, (float)iy)), mods,
                                   1.0f, 0.0f, juce::Time::currentTimeMillis(), {}, index);
        };
        auto route = kick_hw::probe::chooseTouchRoute(m_touch_src_ok, slot);
        if (route == kick_hw::probe::TouchRoute::Touch)
        {
            send(ST::touch, slot);
            if (!m_touch_src_probed)
            {
                m_touch_src_probed = true;
                auto& desk = juce::Desktop::getInstance();
                bool  have = false;
                for (int i = 0; i < desk.getNumMouseSources(); ++i)
                    if (auto* s = desk.getMouseSource(i))
                        if (s->getType() == ST::touch && s->getIndex() == slot) have = true;
                if (!have)
                {
                    m_touch_src_ok = false;
                    route = kick_hw::probe::chooseTouchRoute(false, slot);
                    if (route == kick_hw::probe::TouchRoute::Mouse) send(ST::mouse, 0);
                }
            }
        }
        else if (route == kick_hw::probe::TouchRoute::Mouse)
            send(ST::mouse, 0);
        if (log)
        {
            kick_log("touch", "[TOUCH] source=%s slot=%d touchSrcOk=%d\n", kick_hw::probe::touchRouteName(route), slot, (int)m_touch_src_ok);
        }
    }

    void dispatchTouch(int slot, int x, int y, bool is_down, bool is_up)
    {
        if (slot >= 0 && slot < 10) {
            m_last_touch_x[slot] = x;
            m_last_touch_y[slot] = y;
        }

        juce::MessageManager::callAsync([this, alive = m_alive, slot, x, y, is_down, is_up]() {
            if (!alive->load()) return;
            auto* root = (m_window != nullptr) ? (juce::Component*)m_window : m_root;
            if (!root) return;
            // G3: an invisible (headless, no display) window is skipped by Component::getComponentAt(), so the hit
            // test starts at the content component, which keeps its own visible flag. Coordinates are converted
            // from the screen / desktop space the touch is reported in.
            juce::Component* hitRoot = root;
            if (m_window != nullptr)
                if (auto* content = m_window->getContentComponent()) hitRoot = content;

            // The touch (physical and kick_cmd "touch") is reported in the 1280x800 framebuffer space, but the UI
            // tree is root->getWidth() x getHeight() (1024x600) and painted scaled up to the framebuffer in
            // timerCallback(). Convert back, otherwise every hit lands 25% (x) / 33% (y) off the control.
            int ix = x;
            int iy = y;
            kick_hw::probe::fbToUi(x, y, kFbW, kFbH, root->getWidth(), root->getHeight(), ix, iy);

            // Search desktop components first (modal dialogs, popups, dropdowns)
            auto* target = juce::Desktop::getInstance().findComponentAt(juce::Point<int>(ix, iy));
            if (!target) target = hitRoot->getComponentAt(hitRoot->getLocalPoint(nullptr, juce::Point<int>(ix, iy)));
            if (!target) target = root;
            // A slider's value box (a Label child) hands its mouse events to the slider through a MouseListener (LinearBar
            // styles). The direct mouseDown()/mouseDrag() calls below do not run listeners, so a touch on the value box
            // would never reach the slider: deliver it to the slider itself.
            if (dynamic_cast<juce::Label*>(target) != nullptr)
                if (auto* sl = target->findParentComponentOfClass<juce::Slider>())
                    target = sl;

            // Diagnostics (hardware debugging, down / up only): fb -> UI point, hit root, target, peer.
            if (is_down || is_up)
            {
                auto* b = dynamic_cast<juce::Button*>(target);
                kick_log("touch", "[TOUCH] %s slot=%d fb=(%d,%d) ui=(%d,%d) root=%s %dx%d hitRoot=%s %dx%d vis=%d target=%s '%s' %dx%d%s peer=%d\n",
                       is_down ? "down" : "up", slot, x, y, ix, iy,
                       typeid(*root).name(), root->getWidth(), root->getHeight(),
                       typeid(*hitRoot).name(), hitRoot->getWidth(), hitRoot->getHeight(), (int)hitRoot->isVisible(),
                       typeid(*target).name(), target->getName().toRawUTF8(), target->getWidth(), target->getHeight(),
                       b ? " (Button)" : "", target->getPeer() != nullptr ? 1 : 0);
            }

            auto* peer = target->getPeer();
            if (!peer && m_window) peer = m_window->getPeer();
            juce::ModifierKeys mods = is_up ? juce::ModifierKeys() : juce::ModifierKeys::leftButtonModifier;

            // F12: a popup / dropdown / dialog is its own desktop window. Its peer takes peer-local coordinates (not
            // screen coordinates) and dispatches the event to the right component itself, so it is the ONLY dispatch
            // path for such a target (the manual mouseDown/Up below would deliver the event a second time).
            auto* top = target->getTopLevelComponent();
            if (peer && top != nullptr && top != root && top->isOnDesktop() && top->isShowing() && top->getPeer() == peer)
            {
                if (is_down || is_up) { kick_log("touch", "[TOUCH] popup peer path\n"); }
                deliverPeerTouch(peer, ix, iy, mods, slot, is_down || is_up);
                if (is_up && slot >= 0 && slot < 10) m_touch_targets[slot] = nullptr;
                return;
            }

            // G15: with a native peer the peer dispatches the event to the right component itself; the manual
            // mouseDown / mouseUp / mouseDrag below is for a window the peer cannot hit-test: no peer, or a hidden
            // window (no X display, Main.cpp): the peer exists but Component::getComponentAt() rejects an invisible
            // component, so ComponentPeer::handleMouseEvent would find no target.
            const bool windowShowing = root->isShowing();
            const auto path = kick_hw::probe::chooseDeliveryPath(peer != nullptr, windowShowing);
            if (is_down || is_up)
            {
                kick_log("touch", "[TOUCH] delivery=%s (peer=%d showing=%d)\n", kick_hw::probe::deliveryPathName(path),
                       peer != nullptr ? 1 : 0, (int)windowShowing);
            }
            if (path == kick_hw::probe::DeliveryPath::Peer)
            {
                if (is_down || is_up) { kick_log("touch", "[TOUCH] native peer path (peer visible=1)\n"); }
                deliverPeerTouch(peer, ix, iy, mods, slot, is_down || is_up);
                if (is_up && slot >= 0 && slot < 10) m_touch_targets[slot] = nullptr;
                return;
            }

            auto localPt = target->getLocalPoint(nullptr, juce::Point<int>(ix, iy)).toFloat();
            int clicks = 1;
            if (slot >= 0 && slot < 10)
            {
                if (is_down)
                {
                    const auto now = juce::Time::currentTimeMillis();
                    const bool again = m_click_count[slot] > 0 && now - m_last_up_ms[slot] <= 450
                                       && !kick_hw::probe::exceedsTouchSlop(m_last_up_x[slot], m_last_up_y[slot], ix, iy, 24);
                    m_click_count[slot] = again ? m_click_count[slot] + 1 : 1;
                }
                clicks = m_click_count[slot] > 0 ? m_click_count[slot] : 1;
            }
            auto mouseSrc = juce::Desktop::getInstance().getMainMouseSource();
            if (peer && slot >= 0) {
                peer->handleMouseEvent(juce::MouseInputSource::InputSourceType::touch, {}, {}, 0, 0, 0, {}, slot);
                for (auto& s : juce::Desktop::getInstance().getMouseSources())
                    if (s.getType() == juce::MouseInputSource::InputSourceType::touch && s.getIndex() == slot) { mouseSrc = s; break; }
            }
            juce::MouseEvent e(mouseSrc,
                               localPt, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                               target, target,
                               juce::Time::getCurrentTime(),
                               localPt,
                               juce::Time::getCurrentTime(),
                               clicks, false);

            if (is_down)
            {
                if (slot >= 0 && slot < 10)
                {
                    m_touch_targets[slot] = target;
                    m_touch_down_x[slot] = ix;
                    m_touch_down_y[slot] = iy;
                    m_touch_dragging[slot] = false;
                }
                // No window focus without an X display: remember the touched control as the one the data dial / +/- act on.
                ui::selectControlFromTouch(target);
                target->mouseDown(e);
                kick_log("touch", "[TOUCH] manual mouseDown delivered to '%s'\n", target->getName().toRawUTF8());
            }
            else if (is_up)
            {
                auto* downTarget = (slot >= 0 && slot < 10) ? m_touch_targets[slot].getComponent() : nullptr;
                auto* effectiveTarget = downTarget ? downTarget : target;
                if (effectiveTarget) {
                    auto downLocalPt = effectiveTarget->getLocalPoint(nullptr, juce::Point<int>(ix, iy)).toFloat();
                    const bool wasDragged = slot >= 0 && slot < 10 && m_touch_dragging[slot];
                    juce::MouseEvent downE(mouseSrc,
                                           downLocalPt, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                           effectiveTarget, effectiveTarget,
                                           juce::Time::getCurrentTime(),
                                           downLocalPt,
                                           juce::Time::getCurrentTime(),
                                           clicks, wasDragged);
                    juce::Component::SafePointer<juce::Component> alive(effectiveTarget);
                    effectiveTarget->mouseUp(downE);
                    if (clicks >= 2 && !wasDragged && alive != nullptr)
                        effectiveTarget->mouseDoubleClick(downE);   // JUCE order: mouseUp, then mouseDoubleClick
                    if (slot >= 0 && slot < 10)
                    {
                        m_last_up_ms[slot] = juce::Time::currentTimeMillis();
                        m_last_up_x[slot]  = ix;
                        m_last_up_y[slot]  = iy;
                    }
                    kick_log("touch", "[TOUCH] manual mouseUp delivered to '%s'\n", effectiveTarget->getName().toRawUTF8());
                }

                auto* btn = dynamic_cast<juce::Button*>(effectiveTarget);
                if (!btn && effectiveTarget) btn = effectiveTarget->findParentComponentOfClass<juce::Button>();
                if (btn) {
                    kick_log("touch", "[TOUCH] Button released: '%s' (pos %d,%d, toggle=%d)\n",
                           btn->getName().toRawUTF8(), ix, iy, (int)btn->getToggleState());
                }

                if (slot >= 0 && slot < 10) { m_touch_targets[slot] = nullptr; m_touch_dragging[slot] = false; }
            }
            else
            {
                auto* downTarget = (slot >= 0 && slot < 10) ? m_touch_targets[slot].getComponent() : nullptr;
                // Drag-jitter: Button::mouseDrag would reset a pressed button (mouse source, no real hover), so
                // drags below the touch slop are not forwarded; past it they are, and stay forwarded (latched).
                const bool inSlot = slot >= 0 && slot < 10;
                if (inSlot && !m_touch_dragging[slot]
                    && kick_hw::probe::exceedsTouchSlop(m_touch_down_x[slot], m_touch_down_y[slot], ix, iy))
                    m_touch_dragging[slot] = true;
                if (inSlot && !m_touch_dragging[slot]) {
                    // finger still within the tap slop: nothing to deliver
                } else if (downTarget) {
                    // A real drag event carries where the press began and that the mouse moved since: JUCE sliders
                    // (LinearBar with an editable value box) treat a drag with mouseWasClicked() as a click and ignore it.
                    auto downLocalPt = downTarget->getLocalPoint(nullptr, juce::Point<int>(ix, iy)).toFloat();
                    auto pressLocalPt = inSlot ? downTarget->getLocalPoint(nullptr, juce::Point<int>(m_touch_down_x[slot], m_touch_down_y[slot])).toFloat()
                                               : downLocalPt;
                    juce::MouseEvent downE(mouseSrc,
                                           downLocalPt, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                           downTarget, downTarget,
                                           juce::Time::getCurrentTime(),
                                           pressLocalPt,
                                           juce::Time::getCurrentTime(),
                                           clicks, true);
                    downTarget->mouseDrag(downE);
                } else {
                    target->mouseDrag(e);
                }
            }
        });
    }

    void saveSnapshot(const juce::Image& img, const char* filepath)
    {
        FILE* f = fopen(filepath, "wb");
        if (!f) return;
        int w = img.getWidth();
        int h = img.getHeight();
        fprintf(f, "P6\n%d %d\n255\n", w, h);
        juce::Image::BitmapData bdata(img, juce::Image::BitmapData::readOnly);
        int non_default_px = 0;
        for (int y = 0; y < h; ++y) {
            const uint8_t* row = bdata.getLinePointer(y);
            for (int x = 0; x < w; ++x) {
                uint8_t b = row[x * 4 + 0];
                uint8_t g = row[x * 4 + 1];
                uint8_t r = row[x * 4 + 2];
                if (r != 20 || g != 20 || b != 22) non_default_px++;
                uint8_t rgb[3] = { r, g, b };
                fwrite(rgb, 1, 3, f);
            }
        }
        fclose(f);
        kick_log("bridge", "[BRIDGE] Saved snapshot %dx%d to %s (non-bg pixels: %d / %d = %.1f%%)\n",
               w, h, filepath, non_default_px, w * h, 100.0f * non_default_px / (float)(w * h));
    }

    void timerCallback() override
    {
        pollCommands();
        flushDirtyPadColors();
        updateRecOverdubLeds();

        if (!m_hw || !m_root || !m_hw->display()) return;

        auto* targetComp = (m_window != nullptr) ? (juce::Component*)m_window : m_root;
        int compW = targetComp->getWidth();
        int compH = targetComp->getHeight();
        if (compW <= 0 || compH <= 0) return;

        // The lazily built screens exist now (KickHome builds them after its constructor): re-apply the theme once so they
        // pick up the MPC theme images. Event based, not a frame count. Project loading is NOT done here: the start-up
        // decision (last project / Demo 01 / none) is made once by ui::LoadProjectAndAnimation::requestStartupProject().
        if (!m_theme_reapplied && kick_ui::KickHome::Instance().areLazyScreensReady()) {
            m_theme_reapplied = true;
            kick_log("bridge", "[BRIDGE] screens ready, re-applying theme\n");
            ui::Theme::loadTheme("Default", true);
        }

        if (kick_logger::is_category_enabled("frame")) {
            kick_log("frame", "[BRIDGE] frame=%d rendering %dx%d to 1280x800, root bounds=(%d,%d,%d,%d) children=%d currentScreen=%d\n",
                   m_frame_counter, compW, compH,
                   m_root->getX(), m_root->getY(), m_root->getWidth(), m_root->getHeight(),
                   m_root->getNumChildComponents(),
                   (int)kick_ui::KickHome::Instance().getCurrentScreen());
            if (m_frame_counter == 1 || m_frame_counter == 60) {
                kick_log("frame", "[BRIDGE] --- Dump KickHome children (%d) at frame %d ---\n", m_root->getNumChildComponents(), m_frame_counter);
                for (int i = 0; i < m_root->getNumChildComponents(); ++i) {
                    auto* c = m_root->getChildComponent(i);
                    if (c->isVisible()) {
                        kick_log("frame", "  child[%d]: name='%s' vis=%d bounds=(%d,%d,%d,%d) numSub=%d\n",
                               i, c->getName().toRawUTF8(), c->isVisible(),
                               c->getX(), c->getY(), c->getWidth(), c->getHeight(), c->getNumChildComponents());
                        for (int j = 0; j < c->getNumChildComponents(); ++j) {
                            auto* sub = c->getChildComponent(j);
                            kick_log("frame", "     sub[%d]: name='%s' vis=%d bounds=(%d,%d,%d,%d)\n",
                                   j, sub->getName().toRawUTF8(), sub->isVisible(),
                                   sub->getX(), sub->getY(), sub->getWidth(), sub->getHeight());
                        }
                    }
                }
            }
        }

        {
            juce::Graphics g(m_framebuffer_img);
            g.fillAll(juce::Colour(0xff141416));
            if (compW != 1280 || compH != 800) {
                g.addTransform(juce::AffineTransform::scale(1280.0f / (float)compW, 800.0f / (float)compH));
            }
            targetComp->paintEntireComponent(g, true);

            // Composite any desktop popups, dropdowns, and modal dialogs on top
            int numDesktop = juce::Desktop::getInstance().getNumComponents();
            for (int i = 0; i < numDesktop; ++i) {
                auto* c = juce::Desktop::getInstance().getComponent(i);
                if (c != nullptr && c != targetComp && c->isVisible()) {
                    g.saveState();
                    g.setOrigin(c->getX(), c->getY());
                    c->paintEntireComponent(g, true);
                    g.restoreState();
                }
            }
        }

        {
            juce::Image::BitmapData bdata(m_framebuffer_img, juce::Image::BitmapData::readOnly);
            int stride_px = bdata.lineStride / bdata.pixelStride;
            m_hw->display()->blit((const uint32_t*)bdata.data, 1280, 800, stride_px);
        }

        if (++m_frame_counter == 45 || m_frame_counter == 90 || m_frame_counter == 120 || (m_frame_counter % 300 == 0)) {
            saveSnapshot(m_framebuffer_img, "/data/kick_screen.ppm");
        }
    }

    // Startup problem: state (-> dialog) + kick.log.
    static void reportFault(kick_startup::Fault fault, const std::string& detail)
    {
        const auto p = kick_startup::state().fail(fault, detail);
        kick_log("error", "[ERROR] [BRIDGE] %s\n", p.cause.c_str());
    }

    // "<what>: <strerror(errno)>" when the failed call left an errno, else just <what>.
    static std::string errnoText(const char* what)
    {
        const int e = errno;
        return e != 0 ? std::string(what) + ": " + std::strerror(e) : std::string(what);
    }

public:
    KickHardwareBridge()
        : m_framebuffer_img(juce::Image::ARGB, 1280, 800, false)
    {
        m_last_tap_time = std::chrono::steady_clock::now();
        for (int i = 0; i < 128; ++i) m_key_pitch_map[i] = -1;
    }

    ~KickHardwareBridge() override
    {
        stop();
    }

    void start(juce::DocumentWindow* window, juce::Component* root)
    {
        m_window = window;
        m_root = root;
        if (m_root) {
            m_root->setBufferedToImage(false);
        }

        auto assetsDir = getAppDirs(MTAssets);
        kick_log("bridge", "[BRIDGE] getAppDirs(MTAssets) = '%s' exists=%d\n", assetsDir.getFullPathName().toRawUTF8(), assetsDir.exists());
        ui::Theme::loadTheme("Default", true);

        // Device default project for the start-up decision (see ui::LoadProjectAndAnimation::requestStartupProject).
        ui::LoadProjectAndAnimation::setDefaultProject(juce::File("/data/demos/Demo 01/Demo 01.kickPrj"));

        auto& startup = kick_startup::state();
        startup.set(kick_startup::Step::Display, kick_startup::StepState::Running, "framebuffer");
        startup.set(kick_startup::Step::Midi, kick_startup::StepState::Running, "control surface");

        m_hw.reset(HardwareDeviceFactory::create());
        if (!m_hw)
        {
            reportFault(kick_startup::Fault::Framebuffer, "no hardware adapter for this device");
            startup.set(kick_startup::Step::Midi, kick_startup::StepState::Skipped, "no hardware adapter");
        }
        if (m_hw)
        {
            // The adapters report failure by return value (and perror); these used to be dropped. Now they reach the startup
            // state (dialog with cause + action) and kick.log.
            bool displayOk = true;
            errno = 0;
            if (!m_hw->init()) {
                displayOk = false;
                reportFault(kick_startup::Fault::Framebuffer, errnoText("cannot open the framebuffer /dev/fb0"));
            }

            if (m_hw->touch()) {
                errno = 0;
                if (!m_hw->touch()->start(this, kFbW, kFbH)) {
                    displayOk = false;
                    reportFault(kick_startup::Fault::Touch, errnoText("cannot open the touch device"));
                } else {
                    auto* uiRoot = (m_window != nullptr) ? (juce::Component*)m_window : m_root;
                    kick_log("touch", "[TOUCH] init: framebuffer touch space %dx%d -> UI %dx%d (ui = fb * ui / fb size)\n",
                           kFbW, kFbH, uiRoot ? uiRoot->getWidth() : 0, uiRoot ? uiRoot->getHeight() : 0);
                }
            }
            if (displayOk) startup.set(kick_startup::Step::Display, kick_startup::StepState::Ok, "1280x800, touch");

            bool midiOk = true;
            if (m_hw->control_surface()) {
                errno = 0;
                if (!m_hw->control_surface()->start(this)) {
                    midiOk = false;
                    reportFault(kick_startup::Fault::MidiPort, errnoText("MIDI port 1 (control surface) is not available"));
                } else {
                    initButtonLeds();
                }
            }
            if (midiOk) startup.set(kick_startup::Step::Midi, kick_startup::StepState::Ok, m_hw->get_device_badge());

            if (!m_screen_conn.connected()) {
                m_screen_conn = kick_ui::signals::onScreenLoaded.connect(
                    [this](kick_ui::Screen s) { m_last_screen = static_cast<int>(s); ui::clearSelectedControl(); updateModeLeds(s); });
            }

            m_have_project_colours = false;
            if (!m_pad_colour_conn.connected()) {
                m_pad_colour_conn = contracts::DclSignals::sendCommandToDcl.connect(
                    [this](std::shared_ptr<ipc::MTIPCJson> message) { onDclCommand(std::move(message)); });
            }

            if (!m_play_conn.connected()) m_play_conn = mt::signals::playStateChanged.connect([this](mt::TransportState state) {
                bool isPlaying = (state == mt::TransportState::Playing);
                if (m_hw && m_hw->control_surface()) {
                    m_hw->control_surface()->set_led(HW_LED_PLAY, isPlaying);
                    m_hw->control_surface()->set_led(HW_LED_STOP, !isPlaying);
                }
            });

            // Start 30 FPS framebuffer render loop
            startTimerHz(30);

            // Create command FIFO for testing / remote control
            unlink("/data/kick_cmd");
            mkfifo("/data/kick_cmd", 0666);
            m_cmd_fd = open("/data/kick_cmd", O_RDONLY | O_NONBLOCK);
            m_cmd_fd_w = open("/data/kick_cmd", O_WRONLY | O_NONBLOCK);

            kick_log("info", "[HAL] KickHardwareBridge fully operational (Display 1280x800 @ 30FPS, Touch, ControlSurface, CmdFIFO)\n");
        }
    }

    void stop()
    {
        m_stopping.store(true);
        m_alive->store(false);
        m_pad_colour_conn.disconnect();
        m_screen_conn.disconnect();
        m_play_conn.disconnect();
        stopTimer();
        if (m_cmd_fd >= 0) { close(m_cmd_fd); m_cmd_fd = -1; }
        if (m_cmd_fd_w >= 0) { close(m_cmd_fd_w); m_cmd_fd_w = -1; }
        unlink("/data/kick_cmd");
        if (m_hw) {
            m_hw->shutdown();
            m_hw.reset();
        }
    }

    // --- IHardwareEventListener Implementation ---

    void on_touch_down(int slot, int x, int y) override
    {
        dispatchTouch(slot, x, y, true, false);
    }

    void on_touch_move(int slot, int x, int y) override
    {
        dispatchTouch(slot, x, y, false, false);
    }

    void on_touch_up(int slot) override
    {
        dispatchTouch(slot, m_last_touch_x[slot], m_last_touch_y[slot], false, true);
    }

    // G1: the adapter calls these listener methods from the ALSA reader thread. The button / dial bodies touch UI
    // signals (sigScreenClose), the undo manager, BPM, bank and arp state, so they all run on the message thread.
    // callAsync is FIFO, so the order of presses / releases / dial steps is preserved. The kick_cmd injector (already
    // on the message thread) calls the handle* bodies directly.
    void on_button_press(HardwareButton btn) override
    {
        // H5: strip mode is cycled on the reader thread (same thread as on_touch_strip_touch/fine), not via callAsync.
        if (btn == HW_BTN_TOUCH_STRIP) { cycleStripMode(); return; }
        juce::MessageManager::callAsync([this, alive = m_alive, btn]() { if (!alive->load()) return; handleButtonPress(btn); });
    }
    void on_button_release(HardwareButton btn) override
    {
        juce::MessageManager::callAsync([this, alive = m_alive, btn]() { if (!alive->load()) return; handleButtonRelease(btn); });
    }
    void on_jog_dial(int delta) override
    {
        juce::MessageManager::callAsync([this, alive = m_alive, delta]() { if (!alive->load()) return; handleJogDial(delta); });
    }

    // T12/Y4 (Kick has only the slider): cycle Kick slider -> pitch bend -> pad level -> mod wheel -> track level ->
    // sustain -> expression -> off (manual order, MPC v3.10 p. 101, minus the modes Kick lacks). Uses only atomics,
    // set_led_level, stripPitchSpringBack / stripSustainSet (m_target_mutex), so it is safe on the ALSA reader thread.
    void cycleStripMode()
    {
        const int old_mode = m_strip_mode.load();
        const int new_mode = kick_hw::strip::nextMode(old_mode);
        m_strip_mode.store(new_mode);
        if (old_mode == kick_hw::strip::kPitchBend) stripPitchSpringBack();
        // Leaving Sustain lifts the pedal. Unconditional (a no-edge call sends nothing): on the kick_cmd path this runs on
        // the message thread while the reader may still be inside on_touch_strip with the old mode.
        (void)old_mode;
        stripSustainSet(false);
        if (m_hw && m_hw->control_surface())
            m_hw->control_surface()->set_led_level(HW_LED_TOUCH_STRIP, kick_hw::strip::modeLedLevel(new_mode));
        kick_log("bridge", "[STRIP] mode %d (0 slider, 1 pitch bend, 2 pad level, 3 mod wheel, 4 track level, 5 sustain, 6 expression, 7 off)\n", new_mode);
    }

    void handleButtonPress(HardwareButton btn)
    {
        auto& ae = mt::getAudioInterface();
        // The Timing Correct dialog is modal with touch-only buttons: while it is open, ENTER/STOP apply and
        // Shift+STOP/MAIN cancel it (modal_dialog_keys.h); nothing else of these buttons runs.
        if (kick_ui::ListEditTimingCorrect::isOpen()) {
            const auto act = kick_hw::timingCorrectDialogAction(btn, isShiftDown());
            if (act != kick_hw::ModalDialogAction::None) {
                // Swallow the button (and its release) only if the dialog really closed; with a nested list popup
                // on top closeFromHardware() does nothing and returns false, so the button runs normally.
                if (kick_ui::ListEditTimingCorrect::closeFromHardware(act == kick_hw::ModalDialogAction::Apply)) {
                    if (btn >= 0 && btn < NUM_HW_BUTTONS) m_modal_consumed[(size_t)btn] = true;
                    return;
                }
            }
        }
        switch (btn)
        {
            case HW_BTN_SHIFT:
            {
                const bool first = m_shift.press();
                updateShiftLed();
                if (first)
                    sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_SHIFT, contracts::DeviceControlContracts::BTN_PRESSED);
                break;
            }
            case HW_BTN_LOCATE:
            {
                m_locate_pressed = true;
                break;
            }
            case HW_BTN_STOP:
            {
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_STOP, contracts::DeviceControlContracts::BTN_PRESSED);
                // STOP and Shift+STOP (RETURN) both stop the transport. A stop already rewinds the tracks
                // (MTTrack::stop resets the playheads) and the next play starts from the loop start / bar 1.
                // The former Shift variant called setSequencePlay(.., Now=false), which only queued a stale
                // "play at next bar" flag (_nextBar) and had no other effect.
                // Shift+STOP = RETURN (Akai MPC convention): after stopping, park the playhead on bar 1 so the
                // displayed position resets too (MTSequence::stop() alone leaves it). A parked seek is
                // consumed by the next play() and discarded by the next stop().
                ae.setSequenceStop();
                if (isShiftDown()) {
                    ae.seekToBar(ae.getSelectedSequence(), 0);
                }
                ae.stopPreview();
                auto sel = ae.getSelected();
                for (int ch = 1; ch <= 16; ++ch) {
                    ae.addLiveMidiMsg(sel, juce::MidiMessage::allNotesOff(ch));
                }
                if (m_hw && m_hw->control_surface()) {
                    m_hw->control_surface()->set_led(HW_LED_STOP, true);
                    m_hw->control_surface()->set_led(HW_LED_PLAY, false);
                }
                break;
            }
            case HW_BTN_PLAY:
            {
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_PLAY, contracts::DeviceControlContracts::BTN_PRESSED);
                ae.setSequencePlay(ae.getSelectedSequence(), false, true);
                if (m_hw && m_hw->control_surface()) {
                    m_hw->control_surface()->set_led(HW_LED_PLAY, true);
                    m_hw->control_surface()->set_led(HW_LED_STOP, false);
                }
                break;
            }
            case HW_BTN_PLAY_START:
            {
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_PLAYSTART, contracts::DeviceControlContracts::BTN_PRESSED);
                ae.setSequencePlay(ae.getSelectedSequence(), true, true);
                if (m_hw && m_hw->control_surface()) {
                    m_hw->control_surface()->set_led(HW_LED_PLAY, true);
                    m_hw->control_surface()->set_led(HW_LED_STOP, false);
                }
                break;
            }
            case HW_BTN_REC:
            {
                if (isShiftDown()) {
                    // REC+Shift = RECALL: commit the live notes captured on the selected track. Engine-only, no DCL packet
                    // (so no matching release is sent either, see sendTrackedRelease(btn, -1) below).
                    // Already on the message thread (G1): the call edits the ValueTree / UndoManager (F3).
                    const mt::MTSelected sel = ae.getSelected();
                    ae.recallLiveMidi(sel);
                } else if (recOverdubGoesToLooper()) {
                    sendTrackedPress(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_RECORD);
                } else {
                    // Sequence recording: REC arms the selected sequence, PLAY starts the take (engine-only, no DCL packet).
                    ae.sequenceRecPressed();
                    updateRecOverdubLeds();
                }
                break;
            }
            case HW_BTN_OVERDUB:
            {
                if (recOverdubGoesToLooper()) {
                    sendTrackedPress(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_OVERDUB);
                } else {
                    ae.sequenceOverdubPressed();
                    updateRecOverdubLeds();
                }
                break;
            }
            case HW_BTN_TAP_TEMPO:
            {
                if (isShiftDown()) {
                    sendTrackedPress(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_MASTER);
                } else {
                    sendTrackedPress(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_TAP_TEMPO);
                    m_tap_tempo_held = true;
                    auto now = std::chrono::steady_clock::now();
                    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_tap_time).count();
                    m_last_tap_time = now;
                    if (diff >= 200 && diff <= 2000) {
                        int tap_bpm = (int)(60000.0 / diff + 0.5);
                        if (tap_bpm >= 30 && tap_bpm <= 300) {
                            ae.setSequenceBPM(ae.getSelectedSequence(), tap_bpm);
                            ae.setMasterBPM(tap_bpm);
                        }
                    }
                }
                break;
            }
            case HW_BTN_METRONOME:
            {
                if (isShiftDown()) {
                    // +Shift = CONFIG -> DCL EN_PREF (opens HomePreferencesView in kick_AppHome handleCommand).
                    sendTrackedPress(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_PREF);
                } else {
                    auto& metronome = mt::getSettings().metronome;
                    metronome.enabled = !metronome.enabled();
                    kick_log("bridge", "[BRIDGE] Metronome %s\n", metronome.enabled() ? "ON" : "OFF");
                }
                break;
            }
            case HW_BTN_ARP:
            {
                if (isShiftDown()) {
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuPadPerform);
                } else {
                    // Plain ARP toggles the arpeggiator of the selected track (no DCL press -> nothing to release).
                    const auto sel = ae.getSelected();
                    const bool on = !ae.isArpEnabled(sel);
                    ae.setArpEnabled(sel, on);
                    updateArpLeds();
                    kick_log("bridge", "[BRIDGE] Arpeggiator %s\n", on ? "ON" : "OFF");
                }
                break;
            }
            case HW_BTN_LATCH:
            {
                const auto sel = ae.getSelected();
                if (ae.isArpEnabled(sel)) {
                    // Arp running: LATCH toggles arp latch (no DCL press -> nothing to release).
                    const bool on = !ae.isArpLatch(sel);
                    ae.setArpLatch(sel, on);
                    updateArpLeds();
                    kick_log("bridge", "[BRIDGE] Arp latch %s\n", on ? "ON" : "OFF");
                } else {
                    sendTrackedPress(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_LATCH);
                }
                break;
            }
            case HW_BTN_OCTAVE_DOWN:
            case HW_BTN_OCTAVE_UP:
            {
                // OCT-/OCT+ = octave offset, Shift+OCT-/OCT+ = semitone transpose, both together = reset both.
                // Sounding pitches of held keys are stored at note-on (m_key_pitch_map), so this is safe mid-note.
                KeyTranspose::Action act;
                {
                    std::lock_guard<std::mutex> lk(m_transpose_mutex);
                    act = m_key_transpose.press(btn == HW_BTN_OCTAVE_UP, isShiftDown());
                }
                if (act != KeyTranspose::None) {
                    updateOctaveLeds();
                    printf("[BRIDGE] Octave %d, transpose %d semitones\n", m_key_transpose.octave(), m_key_transpose.semitone());
                    fflush(stdout);
                }
                break;
            }
            case HW_BTN_BANK_A:
            {
                int bank = isShiftDown() ? 4 : 0;
                m_current_bank = bank;
                ae.setBank(juce::String::charToString((char)('A' + bank)));
                sendTrackedPress(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_A + bank);
                updateBankLeds();
                break;
            }
            case HW_BTN_BANK_B:
            {
                int bank = isShiftDown() ? 5 : 1;
                m_current_bank = bank;
                ae.setBank(juce::String::charToString((char)('A' + bank)));
                sendTrackedPress(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_A + bank);
                updateBankLeds();
                break;
            }
            case HW_BTN_BANK_C:
            {
                int bank = isShiftDown() ? 6 : 2;
                m_current_bank = bank;
                ae.setBank(juce::String::charToString((char)('A' + bank)));
                sendTrackedPress(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_A + bank);
                updateBankLeds();
                break;
            }
            case HW_BTN_BANK_D:
            {
                int bank = isShiftDown() ? 7 : 3;
                m_current_bank = bank;
                ae.setBank(juce::String::charToString((char)('A' + bank)));
                sendTrackedPress(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_A + bank);
                updateBankLeds();
                break;
            }
            case HW_BTN_TOUCH_STRIP:
            {
                cycleStripMode(); // kick_cmd path; the hardware path calls it from on_button_press (reader thread)
                break;
            }
            case HW_BTN_TOUCH_FX:
            {
                kick_ui::signals::sigScreenClose(kick_ui::Screen::HomeXYView);
                break;
            }
            case HW_BTN_NOTE_REPEAT:
            {
                if (isShiftDown()) {
                    sendTrackedPress(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_LATCH);
                } else {
                    sendTrackedPress(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_NOTE_REPEAT);
                }
                break;
            }
            case HW_BTN_FULL_LEVEL:
            {
                // MPC manual (v3.10 p. 343): press = Full Level on/off, Shift + press = Half Level on/off. The state lives
                // here (the Kick UI only knows a one-way "Half Level" flag for the step sequencer), the LED follows it.
                // No DCL press is sent: AppHome would force the flag back and lights an LED the bridge does not map.
                kick_hw::padlevel::State st = padLevelState();
                st = isShiftDown() ? kick_hw::padlevel::toggleUser(st) : kick_hw::padlevel::toggleFull(st);
                setPadLevelState(st);
                contracts::DclContractsHelper::setHalfLevel(st.user);
                setLed(HW_LED_FULL_LEVEL, kick_hw::padlevel::ledOn(st) ? kick_hw::kLedFullA : kick_hw::kLedOff);
                break;
            }
            case HW_BTN_16_LEVELS:
            {
                if (isShiftDown()) {
                    // ARCHITECT-CLARIFY: no distinct "Notes" screen exists; DCL EN_NOTES also opens MenuPadPerform
                    // (kick_AppHome.cpp), the same screen as KEYBOARD CONTROL. Kept as is.
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuPadPerform);
                } else {
                    sendTrackedPress(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_16LEVELS);
                }
                break;
            }
            case HW_BTN_ERASE:
            {
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_ERASE, contracts::DeviceControlContracts::BTN_PRESSED);
                break;
            }
            case HW_BTN_COPY:
            {
                if (isShiftDown()) {
                    sendTrackedPress(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_ERASE);
                } else {
                    sendTrackedPress(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_COPY);
                }
                break;
            }
            case HW_BTN_MENU:
            {
                if (isShiftDown()) {
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuFileBrowser);
                } else {
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::HomeOptions);
                }
                break;
            }
            case HW_BTN_SOUNDS:
            {
                if (isShiftDown()) {
                    // FAVORITES: open the file browser and jump to the next assigned favourite slot.
                    // Both signals reach JUCE components; already on the message thread (G1), called in order.
                    kick_ui::signals::sigBrowserShowFavourites();
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuFileBrowser);
                    break;
                }
                kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuFileBrowser);
                break;
            }
            case HW_BTN_MAIN:
            {
                if (isShiftDown()) {
                    // TRACK: the Track tab (index 1) of HomeListView. Already on the message thread (G1); select tab,
                    // then close/open the screen.
                    kick_ui::signals::sigMainModeSelectTab(1);
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::HomeListView);
                    break;
                }
                kick_ui::signals::sigScreenClose(kick_ui::Screen::HomeListView);
                break;
            }
            case HW_BTN_QLINK:
            {
                if (isShiftDown()) {
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuKnobEdit);
                } else {
                    sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_LINK, contracts::DeviceControlContracts::BTN_PRESSED);
                }
                break;
            }
            case HW_BTN_ENTER:
            {
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_ENTER, contracts::DeviceControlContracts::BTN_PRESSED);
                break;
            }
            case HW_BTN_PLUS:
            {
                // Always navigates (always sends a press), so the release below always has a matching press.
                sendTrackedPress(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_PLUS);
                break;
            }
            case HW_BTN_MINUS:
            {
                sendTrackedPress(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_MINUS);
                break;
            }
            case HW_BTN_PROG_EDIT:
            {
                if (isShiftDown()) {
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::HomeXYView);
                } else {
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuProgramEdit);
                }
                break;
            }
            case HW_BTN_GRID:
            {
                if (isShiftDown()) {
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::HomeSequencerView);
                } else {
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::HomeGridView);
                }
                break;
            }
            case HW_BTN_STEP_SEQ:
            {
                kick_ui::signals::sigScreenClose(kick_ui::Screen::HomeSequencerView);
                break;
            }
            case HW_BTN_TRACK_MIX:
            {
                if (isShiftDown()) {
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuPadMixer);
                } else {
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuChannelMixer);
                }
                break;
            }
            case HW_BTN_PAD_MIX:
            {
                kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuPadMixer);
                break;
            }
            case HW_BTN_TRACK_MUTE:
            {
                if (isShiftDown()) {
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuPadMute);
                } else {
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuTrackMute);
                }
                break;
            }
            case HW_BTN_PAD_MUTE:
            {
                kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuPadMute);
                break;
            }
            case HW_BTN_AUTOMATION:
            {
                // MPC: AUTOMATION toggles Read <-> Write (from Off it goes to Read); +Shift = Off. Drives the engine's
                // automation mode directly (no DCL packet: nothing consumes EN_AUTOMATION / EN_ON_OFF), so the release
                // sends nothing either (kTcNoPacket marker). LED: Read = colour A (3), Write = colour B (4), Off = dark.
                m_automation_sent_code = kTcNoPacket;
                const int amCur  = ae.getAutomationMode(); // 0 Off, 1 Read, 2 Write
                const int amNext = isShiftDown() ? 0 : (amCur == 1 ? 2 : 1);
                // Already on the message thread (G1): set synchronously so a quick second press reads the new mode.
                ae.setAutomationMode(amNext);
                setLed(HW_LED_AUTOMATION, amNext == 2 ? kick_hw::kLedFullB : (amNext == 1 ? kick_hw::kLedFullA : kick_hw::kLedOff));
                break;
            }
            case HW_BTN_KEYBOARD_CONTROL:
            {
                if (isShiftDown()) {
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuMidiControl);
                } else {
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuPadPerform);
                }
                break;
            }
            case HW_BTN_SAMPLE_EDIT:
            {
                if (isShiftDown()) {
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuSampler);
                } else {
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuSampleEdit);
                }
                break;
            }
            case HW_BTN_SAMPLER:
            {
                kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuSampler);
                break;
            }
            case HW_BTN_TC:
            {
                // +Shift = TC ON/OFF: toggles the engine's global timing-correct flag directly (no DCL packet, nothing
                // consumes EN_ON_OFF); m_tc_sent_code = kTcNoPacket so the release sends nothing either.
                // Plain TC sends EN_TC, which opens the Timing Correct dialog (ListEditMode).
                if (isShiftDown()) {
                    m_tc_sent_code = kTcNoPacket;
                    ae.toggleTimingCorrectEnabled();
                    updateTimingCorrectLed();
                } else {
                    m_tc_sent_code = contracts::DeviceControlContracts::BtnLedSequence::EN_TC;
                    sendButtonMsg(m_tc_sent_code, contracts::DeviceControlContracts::BTN_PRESSED);
                }
                break;
            }
            case HW_BTN_UNDO:
            {
                if (isShiftDown()) {
                    ae.getUndoManager().redo();
                } else {
                    ae.getUndoManager().undo();
                }
                break;
            }
            case HW_BTN_STEP_PREV:
            {
                // LOCATE + button = jump to the previous event, plain = one grid step back (G7). Neither sends
                // EN_MINUS, so the tracked release sends nothing (CURSOR_LEFT / RIGHT keep EN_MINUS / EN_PLUS).
                // Shift + button = one bar back (same bar step as "<<"; handleButtonPress already runs on the message thread).
                if (isLocateDown()) locateToEvent(-1);
                else if (isShiftDown()) ae.seekBars(ae.getSelectedSequence(), -1);
                else stepPlayhead(-1);
                break;
            }
            case HW_BTN_STEP_NEXT:
            {
                // LOCATE + button = jump to the next event, plain = one grid step forward (G7); no EN_PLUS.
                // Shift + button = one bar forward (same bar step as ">>").
                if (isLocateDown()) locateToEvent(1);
                else if (isShiftDown()) ae.seekBars(ae.getSelectedSequence(), 1);
                else stepPlayhead(1);
                break;
            }
            case HW_BTN_BAR_PREV:
            {
                // Engine bars are 0-based (UI: +1). LOCATE + "<<" = bar 1; stopped/paused parks the bar for PLAY.
                const int seq = ae.getSelectedSequence();
                if (isLocateDown()) ae.seekToBar(seq, 0);
                else ae.seekBars(seq, -1);
                break;
            }
            case HW_BTN_BAR_NEXT:
            {
                // LOCATE + ">>" = start of the last bar (loopEnd - 1 when looping, else length - 1).
                const int seq = ae.getSelectedSequence();
                if (isLocateDown()) ae.seekToBar(seq, ae.getSequenceLastBar(seq));
                else ae.seekBars(seq, 1);
                break;
            }
            case HW_BTN_NEXT_SEQ:
            {
                kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuNextSequence);
                break;
            }
            case HW_BTN_CURSOR_UP:
                handleJogDial(1);
                break;
            case HW_BTN_CURSOR_DOWN:
                handleJogDial(-1);
                break;
            case HW_BTN_CURSOR_LEFT:
                sendTrackedPress(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_MINUS);
                break;
            case HW_BTN_CURSOR_RIGHT:
                sendTrackedPress(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_PLUS);
                break;
            default:
                break;
        }
    }

    void handleButtonRelease(HardwareButton btn)
    {
        if (btn >= 0 && btn < NUM_HW_BUTTONS && m_modal_consumed[(size_t)btn]) {
            m_modal_consumed[(size_t)btn] = false; // press closed the Timing Correct dialog: no packet went out
            return;
        }
        switch (btn)
        {
            case HW_BTN_SHIFT:
                // Releasing one of two held Shift buttons keeps Shift on (F10).
                if (m_shift.release()) {
                    updateShiftLed();
                    sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_SHIFT, contracts::DeviceControlContracts::BTN_RELEASED);
                }
                break;
            case HW_BTN_LOCATE:
                m_locate_pressed = false;
                break;
            case HW_BTN_OCTAVE_DOWN:
            case HW_BTN_OCTAVE_UP:
            {
                std::lock_guard<std::mutex> lk(m_transpose_mutex);
                m_key_transpose.release(btn == HW_BTN_OCTAVE_UP);
                break;
            }
            case HW_BTN_STOP:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_STOP, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_PLAY:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_PLAY, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_PLAY_START:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_PLAYSTART, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_REC:
                sendTrackedRelease(btn, -1); // RECALL (Shift) sent no press -> nothing to release
                break;
            case HW_BTN_OVERDUB:
                sendTrackedRelease(btn, -1); // sequence OVERDUB sent no press -> nothing to release
                break;
            case HW_BTN_ENTER:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_ENTER, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_BANK_A:
                sendTrackedRelease(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_A);
                break;
            case HW_BTN_BANK_B:
                sendTrackedRelease(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_B);
                break;
            case HW_BTN_BANK_C:
                sendTrackedRelease(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_C);
                break;
            case HW_BTN_BANK_D:
                sendTrackedRelease(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_D);
                break;
            case HW_BTN_CURSOR_LEFT:
            case HW_BTN_STEP_PREV:
            case HW_BTN_MINUS:
                sendTrackedRelease(btn, -1); // only if the matching EN_MINUS press was sent by this button
                break;
            case HW_BTN_CURSOR_RIGHT:
            case HW_BTN_STEP_NEXT:
            case HW_BTN_PLUS:
                sendTrackedRelease(btn, -1); // only if the matching EN_PLUS press was sent by this button
                break;
            case HW_BTN_BAR_PREV:
            case HW_BTN_BAR_NEXT:
                break; // act on press only
            case HW_BTN_TAP_TEMPO:
                m_tap_tempo_held = false;
                sendTrackedRelease(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_TAP_TEMPO);
                break;
            case HW_BTN_NOTE_REPEAT:
                sendTrackedRelease(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_NOTE_REPEAT);
                break;
            case HW_BTN_METRONOME:
                sendTrackedRelease(btn, -1); // plain press toggles the click directly (no DCL press) -> release only if EN_PREF was sent
                break;
            case HW_BTN_ARP:
                sendTrackedRelease(btn, -1); // Shift variant opens a screen and plain press toggles the arp directly: no DCL press to release
                break;
            case HW_BTN_LATCH:
                sendTrackedRelease(btn, -1); // arp-latch toggle sends no press -> release only if EN_LATCH was pressed
                break;
            case HW_BTN_FULL_LEVEL:
                sendTrackedRelease(btn, -1); // the press sends no DCL packet (see handleButtonPress)
                break;
            case HW_BTN_16_LEVELS:
                sendTrackedRelease(btn, -1); // Shift variant opens a screen (no press) -> release only if a press was sent
                break;
            case HW_BTN_ERASE:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_ERASE, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_COPY:
                sendTrackedRelease(btn, contracts::DeviceControlContracts::BtnLedSequence::EN_COPY);
                break;
            case HW_BTN_TC:
                if (m_tc_sent_code != kTcNoPacket)
                    sendButtonMsg(m_tc_sent_code >= 0 ? m_tc_sent_code : (int)contracts::DeviceControlContracts::BtnLedSequence::EN_TC, contracts::DeviceControlContracts::BTN_RELEASED);
                m_tc_sent_code = -1;
                break;
            case HW_BTN_AUTOMATION:
                // Press changed the engine mode directly and sent no packet, so there is no release packet either.
                m_automation_sent_code = -1;
                break;
            case HW_BTN_QLINK:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_LINK, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            default:
                break;
        }
    }

    void handleJogDial(int delta)
    {
        // AE1 diagnostics (no behaviour change): first 20 calls, then every 50th.
        ++m_dial_log_count;
        if (m_dial_log_count <= 20 || m_dial_log_count % 50 == 0)
        {
            kick_log("dial", "[DIAL] handleJogDial steps=%d screen=%d\n", delta, m_last_screen);
        }
        auto& ae = mt::getAudioInterface();
        if (m_tap_tempo_held)
        {
            int cur_bpm = ae.getMasterBPM() + delta;
            if (cur_bpm < 30) cur_bpm = 30;
            if (cur_bpm > 300) cur_bpm = 300;
            ae.setSequenceBPM(ae.getSelectedSequence(), cur_bpm);
            ae.setMasterBPM(cur_bpm);
        }
        else
        {
            CPktMgr::HWMsg_t msg;
            msg.pktType = CPktMgr::EN_PKT_TYPE_KNOBS;
            msg.pktCmd = contracts::DeviceControlContracts::KnobCmd::EN_KNOB_ROTARY;
            msg.data = { contracts::DeviceControlContracts::KnobSequence::EN_KNOB_CENTER,
                         delta > 0 ? (int)contracts::DeviceControlContracts::KnobDirection::EN_KNOB_CLOCKWISE : (int)contracts::DeviceControlContracts::KnobDirection::EN_KNOB_ANTICLOCKWISE,
                         abs(delta) };
            contracts::DclSignals::dispatchHardwareInput(msg);
        }
    }

    void on_encoder_turn(int encoder_idx, int delta) override
    {
        if (encoder_idx >= 0 && encoder_idx < 4)
        {
            CPktMgr::HWMsg_t msg;
            msg.pktType = CPktMgr::EN_PKT_TYPE_KNOBS;
            msg.pktCmd = contracts::DeviceControlContracts::KnobCmd::EN_KNOB_QLINK;
            msg.data = { contracts::DeviceControlContracts::KnobSequence::EN_KNOB_1 + encoder_idx,
                         delta > 0 ? (int)contracts::DeviceControlContracts::KnobDirection::EN_KNOB_CLOCKWISE : (int)contracts::DeviceControlContracts::KnobDirection::EN_KNOB_ANTICLOCKWISE,
                         abs(delta) };
            contracts::DclSignals::dispatchHardwareInput(msg);
        }
    }

    // T11 (investigated, deliberately empty): Kick has no knob-touch DCL message. A zero-count Q-Link packet is
    // NOT harmless: it passes isValidHardwareInput and reaches linkRotated(ID, 0) of the Q-Link pages, where
    // PadMixer/ProgramMixer do `mute.setToggleState(value > 0); solo.setToggleState(value <= 0)` (flips the
    // mute/solo toggle state), and number controls call knobRotated(0) -> setValue(val_ + 0). Nothing highlights
    // the assigned parameter. So the touch is not forwarded.
    void on_encoder_touch(int /*encoder_idx*/, bool /*touched*/) override {}

    // T12: pitch wheel back to the centre (strip finger left / mode left).
    // Pitch bend: the track that received the first off-centre value gets the spring-back too (F5).
    void sendPitchBend(kick_hw::HeldTarget<mt::MTSelected>& held, int val)
    {
        auto& ae = mt::getAudioInterface();
        mt::MTSelected target;
        {
            std::lock_guard<std::mutex> lk(m_target_mutex);
            target = kick_hw::routeBend(held, val, ae.getSelected());
        }
        ae.addLiveMidiMsg(target, juce::MidiMessage::pitchWheel(1, val));
    }
    // CC1 / CC11: continuous controllers, always the currently selected track (no held target, see routeCc).
    void sendCc(int cc, int val)
    {
        auto& ae = mt::getAudioInterface();
        ae.addLiveMidiMsg(kick_hw::routeCc(val, ae.getSelected()), juce::MidiMessage::controllerEvent(1, cc, val));
    }
    void stripPitchSpringBack()
    {
        if (!m_strip_pitch_sent.exchange(false)) return;
        sendPitchBend(m_strip_bend_target, kick_hw::strip::pitchSpringBackTarget());
    }
    // Effect place of a CC64 sent to `sel` (see kick_hw::SustainKey): the program the track uses, or for a MIDI program
    // its external (port, channel). Called on a press only, under m_target_mutex; it takes the engine's own short
    // lookups (getProgramForTrack -> manager->getTrack, as addLiveMidiMsg does) and never calls back into the bridge.
    static kick_hw::SustainKey resolveSustainKey(mt::MTAudioInterface& ae, const mt::MTSelected& sel)
    {
        kick_hw::SustainKey k;
        if (auto* p = ae.getProgramForTrack(sel.sequence, sel.track))
        {
            if (auto* m = dynamic_cast<mt::MTMidiProgram*>(p))
            {
                k.port    = m->getPort().toStdString();
                k.channel = m->getMidiChannel();
            }
            else
                k.program = p->id.get().toStdString();
        }
        return k;
    }
    static auto sustainKeyResolver(mt::MTAudioInterface& ae)
    {
        return [&ae](const mt::MTSelected& sel) { return resolveSustainKey(ae, sel); };
    }
    // Sustain mode: CC64 on the edges only (same path as the physical pedal, so the release goes to the track that
    // got the press). AG1b: the strip flag change, the press/release decision AND the mode check are ONE critical
    // section of m_target_mutex, so a mode change (message thread, kick_cmd) cannot interleave with a press from the
    // reader thread. With `onlyInSustainMode` a press is turned into a release when the mode is no longer Sustain; the
    // mode is stored before cycleStripMode takes this lock, so either the press sees the new mode, or the press comes
    // first and the mode change's release follows it.
    void stripSustainSet(bool down, bool onlyInSustainMode = false)
    {
        auto& ae = mt::getAudioInterface();
        std::lock_guard<std::mutex> lk(m_target_mutex);
        if (onlyInSustainMode && m_strip_mode.load() != kick_hw::strip::kSustain) down = false;
        mt::MTSelected target;
        if (m_sustain.strip(down, ae.getSelected(), sustainKeyResolver(ae), target))
            ae.addLiveMidiMsg(target, juce::MidiMessage::controllerEvent(1, 64, down ? 127 : 0));
    }
    // Level modes: apply the newest strip value on the message thread (setNoteGain / setTrackLevel are UI-thread calls).
    void postStripPadLevel(int val)
    {
        if (m_strip_pad_level_pending.exchange(val) != -1) return; // a queued task will pick up the newest value
        juce::MessageManager::callAsync([this, alive = m_alive]() {
            if (!alive->load()) return;
            const int v = m_strip_pad_level_pending.exchange(-1), pad = m_strip_last_pad.load();
            if (v < 0 || pad < 0) return; // no pad hit yet: nothing to set (DUMMY: "current pad" = last pad hit)
            // The bank is the engine's (the touch screen switches it too, m_current_bank only follows the BANK buttons):
            // same note as the Grid plays, ae.getBankOffset() + pad.
            auto& ae = mt::getAudioInterface();
            ae.setLevelForSelectedProgram(kick_hw::strip::padNote(ae.getBankOffset(), pad), v);
        });
    }
    void postStripTrackLevel(int val)
    {
        if (m_strip_track_level_pending.exchange(val) != -1) return;
        juce::MessageManager::callAsync([this, alive = m_alive]() {
            if (!alive->load()) return;
            const int v = m_strip_track_level_pending.exchange(-1);
            if (v < 0) return;
            auto& ae = mt::getAudioInterface();
            const mt::MTSelected sel = ae.getSelected();
            ae.setTrackLevel(sel.sequence, sel.track, v);
        });
    }
    void on_touch_strip_touch(bool down) override
    {
        if (down) return;
        const int mode = m_strip_mode.load();
        if (mode == kick_hw::strip::kPitchBend) stripPitchSpringBack();
        stripSustainSet(false); // release always lifts the strip pedal, whatever the mode is now (a mode change may have raced)
        // Pad / track level, mod wheel, expression: the value stays (DUMMY, the manual has no release behaviour).
    }
    void on_touch_strip_fine(int pos14) override
    {
        if (m_strip_mode.load() != kick_hw::strip::kPitchBend) return;
        m_strip_pitch_sent.store(true);
        sendPitchBend(m_strip_bend_target, kick_hw::strip::pitchFromPosition(pos14));
    }
    void on_touch_strip(int val) override
    {
        const int mode = m_strip_mode.load();
        if (mode == kick_hw::strip::kPitchBend) return; // pitch bend uses on_touch_strip_fine (14 bit)
        switch (mode)
        {
            case kick_hw::strip::kModWheel:   sendCc(1, kick_hw::strip::modWheelFromPosition(val)); return;
            case kick_hw::strip::kExpression: sendCc(11, kick_hw::strip::expressionFromPosition(val)); return;
            case kick_hw::strip::kSustain:
                // The mode is re-checked under m_target_mutex (cycleStripMode may have changed it after the load above).
                stripSustainSet(kick_hw::strip::sustainDownFromPosition(val), true);
                return;
            case kick_hw::strip::kPadLevel:   postStripPadLevel(kick_hw::strip::padLevelFromPosition(val)); return;
            case kick_hw::strip::kTrackLevel: postStripTrackLevel(kick_hw::strip::trackLevelFromPosition(val)); return;
            case kick_hw::strip::kOff:        return; // sends nothing
            default: break; // kSlider: the Kick slider packet below
        }
        CPktMgr::HWMsg_t msg;
        msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_SLIDER;
        msg.pktCmd  = contracts::DeviceControlContracts::SliderCmd::EN_SLIDER_TOUCH_INPUT;
        msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
        msg.data    = { juce::jlimit(0, 100, (int)std::round(val * 100.0f / 127.0f)) };
        contracts::DclSignals::dispatchHardwareInput(msg);
        // No touch strip LED write: the MPC manual (Q-Link Edit / MIDI Control, "Light LED") says the strip / Q-Link
        // LED row has no function on the hardware, so it stays dark by design.
    }
    void on_pitch_bend(int val) override
    {
        sendPitchBend(m_bend_target, val);
    }
    // F4(b): the release goes to the track that got the press, even after a track change in between. The physical pedal
    // and the strip Sustain mode each keep their own target; CC64=0 is skipped while the other source still holds the
    // pedal on the same (sequence, track) lane. The CC64 is sent INSIDE the critical section (also in stripSustainSet):
    // addLiveMidiMsg is one short non-blocking call, and this way the output order equals the state-change order.
    void on_sustain_pedal(bool down) override
    {
        auto& ae = mt::getAudioInterface();
        std::lock_guard<std::mutex> lk(m_target_mutex);
        mt::MTSelected target;
        if (m_sustain.pedal(down, ae.getSelected(), sustainKeyResolver(ae), target))
            ae.addLiveMidiMsg(target, juce::MidiMessage::controllerEvent(1, 64, down ? 127 : 0));
    }

    void on_channel_aftertouch(int val) override
    {
        mt::getAudioInterface().addLiveMidiMsg(mt::getAudioInterface().getSelected(),
                                               juce::MidiMessage::channelPressureChange(1, val));
    }
    void on_mod_wheel(int val) override
    {
        sendCc(1, val);
    }
    void on_expression(int val) override
    {
        sendCc(11, val);
    }
    // Hardware Main Volume knob -> Kick master level (the Mixer "Master" strip level slider: output bus 0, gain =
    // CommonUtils::midiToGain(0..127), exactly what MasterRouteDisplay does). Same thread rule as the other
    // incoming events: the reader thread only posts, the bus is touched on the message thread.
    // ARCHITECT-CLARIFY: the knob sets the project's master bus level (as the Mixer slider does), not an
    // analog/codec hardware volume; whether it should be non-persistent is open (MORNING_DECISIONS).
    void on_main_volume(int val127) override
    {
        juce::MessageManager::callAsync([this, alive = m_alive, val127]() { if (!alive->load()) return; handleMainVolume(val127); });
    }
    void handleMainVolume(int val127)
    {
        if (val127 < 0) val127 = 0;
        if (val127 > 127) val127 = 127;
        if (auto* bus = mt::getAudioInterface().getBusManager().getOutput(0))
        {
            bus->gain = CommonUtils::midiToGain(val127);
            kick_log("info", "[INFO] [BRIDGE] Main volume %d/127 -> gain %.4f\n", val127, (double)bus->gain.get());
        }
    }
    void on_footswitch2(bool down) override
    {
        mt::getAudioInterface().addLiveMidiMsg(mt::getAudioInterface().getSelected(),
                                               juce::MidiMessage::controllerEvent(1, 65, down ? 127 : 0));
    }

    kick_hw::padlevel::State padLevelState() const
    {
        const uint32_t b = m_pad_level_bits.load();
        kick_hw::padlevel::State s;
        s.full      = (b & 1u) != 0;
        s.user      = (b & 2u) != 0;
        s.userLevel = (int)((b >> 8) & 0xFFu);
        if (s.userLevel == 0) s.userLevel = kick_hw::padlevel::kDefaultUserVelocity;
        return s;
    }
    void setPadLevelState(const kick_hw::padlevel::State& s)
    {
        m_pad_level_bits.store((s.full ? 1u : 0u) | (s.user ? 2u : 0u) | ((uint32_t)kick_hw::padlevel::clampVel(s.userLevel) << 8));
    }

    void on_pad_note_on(int pad_num, int rawVel) override
    {
        const int thr = m_pad_threshold.load();
        if (rawVel > 0 && rawVel < thr) return;

        int scaledRaw = rawVel;
        const int sens = m_pad_sensitivity.load();
        if (sens != 4 && rawVel > 0) {
            scaledRaw = std::max(1, std::min(127, (rawVel * sens) / 4));
        }

        const int vel = kick_hw::padlevel::padVelocity(padLevelState(), scaledRaw);
        if (pad_num >= 0 && pad_num < 16)
        {
            m_pad_held.fetch_or(uint16_t(1u << pad_num));
            m_strip_last_pad.store(pad_num); // Pad Level strip mode target (note resolved later from the engine bank)
            m_pad_last_vel[pad_num] = static_cast<uint8_t>(vel);
            if (m_hw && m_hw->control_surface()) {
                // Flash pad bright white/cyan on hit!
                m_hw->control_surface()->set_pad_rgb(pad_num, 127, 127, 127);
            }

            CPktMgr::HWMsg_t msg;
            msg.pktType = CPktMgr::EN_PKT_TYPE_PAD;
            msg.pktCmd = contracts::DeviceControlContracts::PadCmd::EN_PAD_STATE;
            msg.data = { pad_num, contracts::DeviceControlContracts::BTN_PRESSED, vel, 0 };
            // The sound is started by the active screen's handleCommand() (Grid, PadMixer, PadPerform,
            // PadColor, FileBrowser, MainMode) from this DCL packet. Do not also start the note here,
            // otherwise those screens trigger every hit twice.
            contracts::DclSignals::dispatchHardwareInput(msg);
        }
    }

    // DISABLED (hardware report item 5, held pad re-triggers): Kick has no pressure-only packet. The only carrier is
    // EN_PAD_STATE with state BTN_PRESSED and the aftertouch in byte 3, and every sound-starting screen (Grid, PadMixer,
    // PadPerform, PadColor, FileBrowser, MainMode, ProgramEdit, ...) treats BTN_PRESSED as a NEW note-on and ignores byte 3
    // (only SettingsHardware / MidiControlHardware read it). No other state value is safe: screens test `== BTN_PRESSED`
    // or `if (padState)`. A safe shape cannot be proven from code, so no aftertouch packets are sent until a screen-side
    // "pressure only" discriminator exists. The pad-aftertouch enable flag is still tracked.
    static constexpr bool kSendPadAftertouchPackets = false;

    void on_pad_aftertouch(int pad_num, int val) override
    {
        // Kick reports pad pressure as the 4th field of EN_PAD_STATE (see DclContractsHelper::getPadAftertouch).
        // That packet also starts the sound in several screens, so (like the Kick hardware, which only reports
        // it after EN_PAD_SET_AFTER_TOUCH) send it only when enabled and while the pad is held.
        if (!kSendPadAftertouchPackets) return;
        if (pad_num >= 0 && pad_num < 16 && m_pad_aftertouch_enabled.load() &&
            (m_pad_held.load() & (1u << pad_num)) != 0)
        {
            CPktMgr::HWMsg_t msg;
            msg.pktType = CPktMgr::EN_PKT_TYPE_PAD;
            msg.pktCmd = contracts::DeviceControlContracts::PadCmd::EN_PAD_STATE;
            msg.data = { pad_num, contracts::DeviceControlContracts::BTN_PRESSED, (int)m_pad_last_vel[pad_num].load(),
                         juce::jlimit(0, 127, val) };
            contracts::DclSignals::dispatchHardwareInput(msg);
        }
    }

    void on_pad_note_off(int pad_num) override
    {
        if (pad_num >= 0 && pad_num < 16)
        {
            m_pad_held.fetch_and(static_cast<uint16_t>(~(1u << pad_num)));
            if (m_hw && m_hw->control_surface()) {
                // Restore pad to its own idle colour (project colour, or bank colour before the first packet)
                uint8_t r = 35, g = 15, b = 0;
                getPadIdleColor(pad_num, r, g, b);
                m_hw->control_surface()->set_pad_rgb(pad_num, r, g, b);
            }

            CPktMgr::HWMsg_t msg;
            msg.pktType = CPktMgr::EN_PKT_TYPE_PAD;
            msg.pktCmd = contracts::DeviceControlContracts::PadCmd::EN_PAD_STATE;
            msg.data = { pad_num, contracts::DeviceControlContracts::BTN_RELEASED, 0, 0 };
            contracts::DclSignals::dispatchHardwareInput(msg);
        }
    }

    void on_keybed_note_on(int pitch, int vel) override
    {
        int sounding_pitch;
        {
            std::lock_guard<std::mutex> lk(m_transpose_mutex);
            if (m_key_transpose.isChordActive()) {
                KeyTranspose::Action act = m_key_transpose.pressKey(pitch);
                if (act != KeyTranspose::None) {
                    updateOctaveLeds();
                    kick_log("bridge", "[BRIDGE] Octave %d, transpose %d semitones (chord key %d)\n",
                             m_key_transpose.octave(), m_key_transpose.semitone(), pitch);
                }
                return; // Chord key does not sound
            }
            sounding_pitch = m_key_transpose.soundingPitch(pitch);
        }
        if (pitch >= 0 && pitch < 128) {
            m_key_pitch_map[pitch] = sounding_pitch;
        }
        auto& ae = mt::getAudioInterface();
        const mt::MTSelected sel = ae.getSelected();
        {
            std::lock_guard<std::mutex> lk(m_target_mutex);
            m_key_target.set(pitch, sel); // F5: the note-off goes to the track that got the note-on
        }
        ae.addLiveMidiNote(sel, sounding_pitch, vel);
    }

    void on_keybed_note_off(int pitch) override
    {
        if (pitch >= 0 && pitch < 128 && m_key_pitch_map[pitch] < 0) {
            return; // Not sounding (chord key or stray release)
        }
        int sounding_pitch;
        if (pitch >= 0 && pitch < 128 && m_key_pitch_map[pitch] >= 0) {
            sounding_pitch = m_key_pitch_map[pitch];
        } else {
            std::lock_guard<std::mutex> lk(m_transpose_mutex);
            sounding_pitch = m_key_transpose.soundingPitch(pitch);
        }
        if (pitch >= 0 && pitch < 128) {
            m_key_pitch_map[pitch] = -1;
        }
        auto& ae = mt::getAudioInterface();
        mt::MTSelected target;
        {
            std::lock_guard<std::mutex> lk(m_target_mutex);
            target = m_key_target.take(pitch, ae.getSelected());
        }
        ae.stopLiveMidiNote(target, sounding_pitch);
    }

    void pollCommands()
    {
        if (m_cmd_fd < 0) return;
        char buf[512];
        int n = (int)read(m_cmd_fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            m_cmd_buf += buf;
            size_t pos;
            while ((pos = m_cmd_buf.find('\n')) != std::string::npos) {
                std::string line = m_cmd_buf.substr(0, pos);
                m_cmd_buf.erase(0, pos + 1);
                while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
                if (!line.empty()) processCommand(line);
            }
        }
    }

    // kick_cmd: "rawmidi <port 0|1|2> <hex byte> [<hex byte> ...]" feeds raw MIDI bytes through the same parser and
    // dispatch path as the control surface reader thread of that port (no hardware needed), e.g.
    //   echo "rawmidi 1 90 52 7F" > /data/kick_cmd     (Port 1, ch1, note 82 = PLAY press, velocity 127)
    //   echo "rawmidi 1 99 25 64" > /data/kick_cmd     (Port 1, ch10, note 37 = pad 1, velocity 100)
    // Bytes are hex pairs (0x prefix optional); the whole command is ignored when any token is invalid.
    void processRawMidiCommand(const std::string& cmd)
    {
        const char* p = cmd.c_str() + 7; // after "rawmidi"
        char* end = nullptr;
        long port = strtol(p, &end, 10);
        if (end == p || port < 0 || port > 2) {
            kick_log("cmd", "[CMD] rawmidi: bad port (0..2)\n");
            return;
        }
        p = end;
        std::vector<uint8_t> bytes;
        for (;;) {
            while (*p == ' ' || *p == '\t') ++p;
            if (*p == '\0') break;
            long v = strtol(p, &end, 16);
            if (end == p || v < 0 || v > 0xFF || (*end != '\0' && *end != ' ' && *end != '\t')) {
                kick_log("cmd", "[CMD] rawmidi: bad hex byte\n");
                return;
            }
            bytes.push_back((uint8_t)v);
            p = end;
        }
        kick_log("cmd", "[CMD] rawmidi port=%ld bytes=%d\n", port, (int)bytes.size());
        if (!bytes.empty() && m_hw && m_hw->control_surface())
            m_hw->control_surface()->inject_bytes((int)port, bytes.data(), bytes.size());
    }

    // kick_cmd "probe": READ-ONLY system facts (/proc, /sys, /etc text files and directory listings, plus what the
    // adapter discovery picks) to stdout and /data/kick_probe.txt. No shell, no ioctl, no /dev node is opened
    // (/dev/i2c-* and /dev/snd are only listed by name). Answers the T32 / T19 checklist with one command:
    //   echo probe > /data/kick_cmd
    void processProbeCommand()
    {
        const std::string report = kick_hw::sysprobe::buildReport();
        kick_log("probe", "[PROBE] begin\n%s[PROBE] end\n", report.c_str());
        FILE* f = fopen("/data/kick_probe.txt", "w");
        if (f) {
            fputs(report.c_str(), f);
            fclose(f);
            kick_log("probe", "[PROBE] written to /data/kick_probe.txt\n");
        } else {
            kick_log("probe", "[PROBE] cannot write /data/kick_probe.txt\n");
        }
    }

    void processCommand(const std::string& cmd)
    {
        if (cmd.compare(0, 8, "rawmidi ") == 0) { processRawMidiCommand(cmd); return; }
        if (cmd == "probe") { processProbeCommand(); return; }
        char op[64] = { 0 };
        int a1 = 0, a2 = 0;
        char sval[128] = { 0 };
        if (sscanf(cmd.c_str(), "%63s %d %d", op, &a1, &a2) == 3) {
            if (strcmp(op, "touch") == 0) {
                kick_log("cmd", "[CMD] touch %d %d\n", a1, a2);
                on_touch_down(0, a1, a2);
                on_touch_up(0);
            } else if (strcmp(op, "touch_down") == 0) {
                kick_log("cmd", "[CMD] touch_down %d %d\n", a1, a2);
                on_touch_down(0, a1, a2);
            } else if (strcmp(op, "touch_move") == 0) {
                kick_log("cmd", "[CMD] touch_move %d %d\n", a1, a2);
                on_touch_move(0, a1, a2);
            } else if (strcmp(op, "pad") == 0) {
                kick_log("cmd", "[CMD] pad %d vel=%d\n", a1, a2);
                if (a2 > 0) on_pad_note_on(a1, a2);
                else on_pad_note_off(a1);
            } else if (strcmp(op, "key") == 0) {
                kick_log("cmd", "[CMD] key %d vel=%d\n", a1, a2);
                if (a2 > 0) on_keybed_note_on(a1, a2);
                else on_keybed_note_off(a1);
            } else if (strcmp(op, "encoder") == 0) {
                kick_log("cmd", "[CMD] encoder %d delta=%d\n", a1, a2);
                on_encoder_turn(a1, a2);
            }
        } else if (sscanf(cmd.c_str(), "%63s %127s", op, sval) == 2) {
            if (strcmp(op, "btn") == 0) {
                kick_log("cmd", "[CMD] btn %s\n", sval);
                HardwareButton b = HW_BTN_NONE;
                if (strcmp(sval, "PLAY") == 0) b = HW_BTN_PLAY;
                else if (strcmp(sval, "STOP") == 0) b = HW_BTN_STOP;
                else if (strcmp(sval, "PLAY_START") == 0) b = HW_BTN_PLAY_START;
                else if (strcmp(sval, "REC") == 0) b = HW_BTN_REC;
                else if (strcmp(sval, "OVERDUB") == 0) b = HW_BTN_OVERDUB;
                else if (strcmp(sval, "MAIN") == 0) b = HW_BTN_MAIN;
                else if (strcmp(sval, "BROWSE") == 0) b = HW_BTN_BROWSE;
                else if (strcmp(sval, "GRID") == 0) b = HW_BTN_GRID;
                else if (strcmp(sval, "STEP_SEQ") == 0) b = HW_BTN_STEP_SEQ;
                else if (strcmp(sval, "PAD_MIX") == 0) b = HW_BTN_PAD_MIX;
                else if (strcmp(sval, "TRACK_MIX") == 0) b = HW_BTN_TRACK_MIX;
                else if (strcmp(sval, "SAMPLER") == 0) b = HW_BTN_SAMPLER;
                else if (strcmp(sval, "SAMPLE_EDIT") == 0) b = HW_BTN_SAMPLE_EDIT;
                else if (strcmp(sval, "PROG_EDIT") == 0) b = HW_BTN_PROG_EDIT;
                else if (strcmp(sval, "PAD_MUTE") == 0) b = HW_BTN_PAD_MUTE;
                else if (strcmp(sval, "TRACK_MUTE") == 0) b = HW_BTN_TRACK_MUTE;
                else if (strcmp(sval, "NEXT_SEQ") == 0) b = HW_BTN_NEXT_SEQ;
                else if (strcmp(sval, "MENU") == 0) b = HW_BTN_MENU;
                else if (strcmp(sval, "BANK_A") == 0) b = HW_BTN_BANK_A;
                else if (strcmp(sval, "BANK_B") == 0) b = HW_BTN_BANK_B;
                else if (strcmp(sval, "BANK_C") == 0) b = HW_BTN_BANK_C;
                else if (strcmp(sval, "BANK_D") == 0) b = HW_BTN_BANK_D;
                else if (strcmp(sval, "TAP_TEMPO") == 0) b = HW_BTN_TAP_TEMPO;
                else if (strcmp(sval, "FULL_LEVEL") == 0) b = HW_BTN_FULL_LEVEL;
                else if (strcmp(sval, "16_LEVELS") == 0) b = HW_BTN_16_LEVELS;
                else if (strcmp(sval, "NOTE_REPEAT") == 0) b = HW_BTN_NOTE_REPEAT;
                else if (strcmp(sval, "ERASE") == 0) b = HW_BTN_ERASE;
                else if (strcmp(sval, "UNDO") == 0) b = HW_BTN_UNDO;
                else if (strcmp(sval, "CURSOR_UP") == 0) b = HW_BTN_CURSOR_UP;
                else if (strcmp(sval, "CURSOR_DOWN") == 0) b = HW_BTN_CURSOR_DOWN;
                else if (strcmp(sval, "CURSOR_LEFT") == 0) b = HW_BTN_CURSOR_LEFT;
                else if (strcmp(sval, "CURSOR_RIGHT") == 0) b = HW_BTN_CURSOR_RIGHT;
                else if (strcmp(sval, "METRONOME") == 0) b = HW_BTN_METRONOME;
                else if (strcmp(sval, "ARP") == 0) b = HW_BTN_ARP;
                else if (strcmp(sval, "LATCH") == 0) b = HW_BTN_LATCH;
                else if (strcmp(sval, "OCT_DOWN") == 0) b = HW_BTN_OCTAVE_DOWN;
                else if (strcmp(sval, "OCT_UP") == 0) b = HW_BTN_OCTAVE_UP;
                else if (strcmp(sval, "TOUCH_STRIP") == 0) b = HW_BTN_TOUCH_STRIP;
                else if (strcmp(sval, "TOUCH_FX") == 0) b = HW_BTN_TOUCH_FX;
                else if (strcmp(sval, "COPY") == 0) b = HW_BTN_COPY;
                else if (strcmp(sval, "SHIFT") == 0) b = HW_BTN_SHIFT;
                else if (strcmp(sval, "SOUNDS") == 0) b = HW_BTN_SOUNDS;
                else if (strcmp(sval, "QLINK") == 0) b = HW_BTN_QLINK;
                else if (strcmp(sval, "ENTER") == 0) b = HW_BTN_ENTER;
                else if (strcmp(sval, "PLUS") == 0) b = HW_BTN_PLUS;
                else if (strcmp(sval, "MINUS") == 0) b = HW_BTN_MINUS;
                else if (strcmp(sval, "AUTOMATION") == 0) b = HW_BTN_AUTOMATION;
                else if (strcmp(sval, "KEYBOARD_CONTROL") == 0) b = HW_BTN_KEYBOARD_CONTROL;
                else if (strcmp(sval, "TC") == 0) b = HW_BTN_TC;
                else if (strcmp(sval, "STEP_PREV") == 0) b = HW_BTN_STEP_PREV;
                else if (strcmp(sval, "STEP_NEXT") == 0) b = HW_BTN_STEP_NEXT;
                else if (strcmp(sval, "LOCATE") == 0) b = HW_BTN_LOCATE;
                else if (strcmp(sval, "BAR_PREV") == 0) b = HW_BTN_BAR_PREV;
                else if (strcmp(sval, "BAR_NEXT") == 0) b = HW_BTN_BAR_NEXT;
                if (b != HW_BTN_NONE) {
                    handleButtonPress(b);
                    handleButtonRelease(b);
                }
            } else if (strcmp(op, "jog") == 0) {
                int delta = atoi(sval);
                kick_log("cmd", "[CMD] jog %d\n", delta);
                handleJogDial(delta);
            } else if (strcmp(op, "strip") == 0) {
                int val = atoi(sval);
                kick_log("cmd", "[CMD] strip %d\n", val);
                on_touch_strip(val);
            } else if (strcmp(op, "snapshot") == 0) {
                kick_log("cmd", "[CMD] snapshot %s\n", sval);
                saveSnapshot(m_framebuffer_img, sval);
            }
        } else if (strcmp(cmd.c_str(), "touch_up") == 0) {
            kick_log("cmd", "[CMD] touch_up\n");
            on_touch_up(0);
        }
    }
};
