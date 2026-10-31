#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "hardware/device_factory.h"
#include "contracts/dcl/dcl_signals.h"
#include "contracts/dcl/dcl_contracts.h"
#include "contracts/dcl/dcl_contracts_helper.h"
#include "kick_ui/kick_ui.h"
#include "kick_ui_controls/source/kick_Theme.h"
#include "audio_engine/audio_engine.h"
#include <chrono>

class KickHardwareBridge : public IHardwareEventListener, private juce::Timer
{
private:
    std::unique_ptr<IHardwareAdapter> m_hw;
    juce::Component* m_root = nullptr;
    juce::DocumentWindow* m_window = nullptr;
    juce::Image m_framebuffer_img;
    int m_last_touch_x[10] = { 0 };
    int m_last_touch_y[10] = { 0 };
    std::chrono::steady_clock::time_point m_last_tap_time;
    bool m_tempo_edit_active = false;
    int m_frame_counter = 0;

    void sendButtonMsg(int buttonCode, int state)
    {
        CPktMgr::HWMsg_t msg;
        msg.pktType = CPktMgr::EN_PKT_TYPE_BUTTON;
        msg.pktCmd = contracts::DeviceControlContracts::BtnCmd::EN_BTN_STATUS;
        msg.data = { buttonCode, state };
        contracts::DclSignals::dispatchHardwareInput(msg);
    }

    void dispatchTouch(int slot, int x, int y, bool is_down, bool is_up)
    {
        if (slot >= 0 && slot < 10) {
            m_last_touch_x[slot] = x;
            m_last_touch_y[slot] = y;
        }
        // Map 1280x800 logical landscape coordinates to 1024x600 Kick UI coordinates
        float kx = x * (1024.0f / 1280.0f);
        float ky = y * (600.0f / 800.0f);

        juce::MessageManager::callAsync([this, slot, kx, ky, is_up]() {
            if (!m_window) return;
            auto* peer = m_window->getPeer();
            if (!peer) return;
            juce::Point<float> pt(kx, ky);
            juce::ModifierKeys mods = is_up ? juce::ModifierKeys() : juce::ModifierKeys::leftButtonModifier;
            peer->handleMouseEvent(juce::MouseInputSource::InputSourceType::touch,
                                   pt, mods, 1.0f, 0.0f, juce::Time::currentTimeMillis(), {}, slot);
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
        printf("[BRIDGE] Saved snapshot %dx%d to %s (non-bg pixels: %d / %d = %.1f%%)\n",
               w, h, filepath, non_default_px, w * h, 100.0f * non_default_px / (float)(w * h));
        fflush(stdout);
    }

    void timerCallback() override
    {
        if (!m_hw || !m_root || !m_hw->display()) return;

        auto* targetComp = m_root;
        int compW = targetComp->getWidth();
        int compH = targetComp->getHeight();
        if (compW <= 0 || compH <= 0) return;

        if (m_frame_counter == 30) {
            printf("[BRIDGE] Transitioning to Screen::HomeListView (AppMainmode)...\n");
            kick_ui::signals::sigScreenClose(kick_ui::Screen::HomeListView);
            ui::Theme::loadTheme("Default", true);
            juce::File demoPrj("/data/demos/Demo 01/Demo 01.kickPrj");
            if (demoPrj.existsAsFile()) {
                printf("[BRIDGE] Found '%s', loading project...\n", demoPrj.getFullPathName().toRawUTF8());
                mt::getAudioInterface().loadProject(demoPrj);
            }
            fflush(stdout);
        }

        static bool project_refresh_triggered = false;
        if (!project_refresh_triggered && m_frame_counter > 40 && mt::getAudioInterface().isProjectLoaded()) {
            project_refresh_triggered = true;
            printf("[BRIDGE] Project loaded! Triggering UI refresh...\n");
            kick_ui::signals::sigScreenClose(kick_ui::Screen::HomeListView);
            fflush(stdout);
        }

        if (m_frame_counter < 5 || m_frame_counter % 60 == 0) {
            printf("[BRIDGE] frame=%d rendering %dx%d to 1280x800, root bounds=(%d,%d,%d,%d) children=%d currentScreen=%d\n",
                   m_frame_counter, compW, compH,
                   m_root->getX(), m_root->getY(), m_root->getWidth(), m_root->getHeight(),
                   m_root->getNumChildComponents(),
                   (int)kick_ui::KickHome::Instance().getCurrentScreen());
            if (m_frame_counter == 1 || m_frame_counter == 60) {
                printf("[BRIDGE] --- Dump KickHome children (%d) at frame %d ---\n", m_root->getNumChildComponents(), m_frame_counter);
                for (int i = 0; i < m_root->getNumChildComponents(); ++i) {
                    auto* c = m_root->getChildComponent(i);
                    if (c->isVisible()) {
                        printf("  child[%d]: name='%s' vis=%d bounds=(%d,%d,%d,%d) numSub=%d\n",
                               i, c->getName().toRawUTF8(), c->isVisible(),
                               c->getX(), c->getY(), c->getWidth(), c->getHeight(), c->getNumChildComponents());
                        for (int j = 0; j < c->getNumChildComponents(); ++j) {
                            auto* sub = c->getChildComponent(j);
                            printf("     sub[%d]: name='%s' vis=%d bounds=(%d,%d,%d,%d)\n",
                                   j, sub->getName().toRawUTF8(), sub->isVisible(),
                                   sub->getX(), sub->getY(), sub->getWidth(), sub->getHeight());
                        }
                    }
                }
            }
            fflush(stdout);
        }

        {
            juce::Graphics g(m_framebuffer_img);
            g.fillAll(juce::Colour(0xff141416));
            if (compW != 1280 || compH != 800) {
                g.addTransform(juce::AffineTransform::scale(1280.0f / (float)compW, 800.0f / (float)compH));
            }
            targetComp->paintEntireComponent(g, true);
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

public:
    KickHardwareBridge()
        : m_framebuffer_img(juce::Image::ARGB, 1280, 800, false)
    {
        m_last_tap_time = std::chrono::steady_clock::now();
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
        printf("[BRIDGE] getAppDirs(MTAssets) = '%s' exists=%d\n", assetsDir.getFullPathName().toRawUTF8(), assetsDir.exists());
        ui::Theme::loadTheme("Default", true);

        m_hw.reset(HardwareDeviceFactory::create());
        if (m_hw)
        {
            m_hw->init();

            if (m_hw->touch()) {
                m_hw->touch()->start(this, 1280, 800);
            }
            if (m_hw->control_surface()) {
                m_hw->control_surface()->start(this);
            }

            // Start 30 FPS framebuffer render loop
            startTimerHz(30);
            printf("[HAL] KickHardwareBridge fully operational (Display 1280x800 @ 30FPS, Touch, ControlSurface)\n");
        }
    }

    void stop()
    {
        stopTimer();
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

    void on_button_press(HardwareButton btn) override
    {
        auto& ae = mt::getAudioInterface();
        switch (btn)
        {
            case HW_BTN_STOP:
            {
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_STOP, contracts::DeviceControlContracts::BTN_PRESSED);
                ae.setSequenceStop();
                ae.stopPreview();
                auto sel = ae.getSelected();
                for (int ch = 1; ch <= 16; ++ch) {
                    ae.addLiveMidiMsg(sel, juce::MidiMessage::allNotesOff(ch));
                }
                m_tempo_edit_active = false;
                break;
            }
            case HW_BTN_PLAY:
            {
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_PLAY, contracts::DeviceControlContracts::BTN_PRESSED);
                ae.setSequencePlay(ae.getSelectedSequence(), false, true);
                break;
            }
            case HW_BTN_PLAY_START:
            {
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_PLAYSTART, contracts::DeviceControlContracts::BTN_PRESSED);
                ae.setSequencePlay(ae.getSelectedSequence(), true, true);
                break;
            }
            case HW_BTN_REC:
            {
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_RECORD, contracts::DeviceControlContracts::BTN_PRESSED);
                break;
            }
            case HW_BTN_OVERDUB:
            {
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_OVERDUB, contracts::DeviceControlContracts::BTN_PRESSED);
                break;
            }
            case HW_BTN_TAP_TEMPO:
            {
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_TAP_TEMPO, contracts::DeviceControlContracts::BTN_PRESSED);
                m_tempo_edit_active = true;
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
                break;
            }
            case HW_BTN_ENTER:
            {
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_ENTER, contracts::DeviceControlContracts::BTN_PRESSED);
                m_tempo_edit_active = false;
                break;
            }
            case HW_BTN_PLUS:
            {
                if (m_tempo_edit_active) {
                    int cur_bpm = ae.getMasterBPM() + 1;
                    if (cur_bpm <= 300) {
                        ae.setSequenceBPM(ae.getSelectedSequence(), cur_bpm);
                        ae.setMasterBPM(cur_bpm);
                    }
                } else {
                    sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_PLUS, contracts::DeviceControlContracts::BTN_PRESSED);
                }
                break;
            }
            case HW_BTN_MINUS:
            {
                if (m_tempo_edit_active) {
                    int cur_bpm = ae.getMasterBPM() - 1;
                    if (cur_bpm >= 30) {
                        ae.setSequenceBPM(ae.getSelectedSequence(), cur_bpm);
                        ae.setMasterBPM(cur_bpm);
                    }
                } else {
                    sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_MINUS, contracts::DeviceControlContracts::BTN_PRESSED);
                }
                break;
            }
            case HW_BTN_MAIN:
            {
                kick_ui::signals::sigScreenClose(kick_ui::Screen::HomeListView);
                break;
            }
            case HW_BTN_BROWSE:
            {
                kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuFileBrowser);
                break;
            }
            case HW_BTN_STEP_SEQ:
            {
                kick_ui::signals::sigScreenClose(kick_ui::Screen::HomeSequencerView);
                break;
            }
            case HW_BTN_PAD_MIX:
            {
                kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuPadMixer);
                break;
            }
            case HW_BTN_TRACK_MIX:
            {
                kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuChannelMixer);
                break;
            }
            case HW_BTN_SAMPLER:
            {
                kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuSampler);
                break;
            }
            case HW_BTN_SAMPLE_EDIT:
            {
                kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuSampleEdit);
                break;
            }
            case HW_BTN_PROG_EDIT:
            {
                kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuProgramEdit);
                break;
            }
            case HW_BTN_PAD_MUTE:
            {
                kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuPadMute);
                break;
            }
            case HW_BTN_TRACK_MUTE:
            {
                kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuTrackMute);
                break;
            }
            case HW_BTN_NEXT_SEQ:
            {
                kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuNextSequence);
                break;
            }
            case HW_BTN_MENU:
            {
                kick_ui::signals::sigScreenClose(kick_ui::Screen::HomeOptions);
                break;
            }
            case HW_BTN_FULL_LEVEL:
            {
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_FULL_LEVEL, contracts::DeviceControlContracts::BTN_PRESSED);
                contracts::DclContractsHelper::setHalfLevel(false);
                break;
            }
            case HW_BTN_16_LEVELS:
            {
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_16LEVELS, contracts::DeviceControlContracts::BTN_PRESSED);
                break;
            }
            case HW_BTN_NOTE_REPEAT:
            {
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_NOTE_REPEAT, contracts::DeviceControlContracts::BTN_PRESSED);
                break;
            }
            case HW_BTN_ERASE:
            {
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_ERASE, contracts::DeviceControlContracts::BTN_PRESSED);
                break;
            }
            case HW_BTN_UNDO:
            {
                ae.getUndoManager().undo();
                break;
            }
            default:
                break;
        }
    }

    void on_button_release(HardwareButton btn) override
    {
        switch (btn)
        {
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
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_RECORD, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_OVERDUB:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_OVERDUB, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_ENTER:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_ENTER, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            default:
                break;
        }
    }

    void on_jog_dial(int delta) override
    {
        auto& ae = mt::getAudioInterface();
        if (m_tempo_edit_active)
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

    void on_encoder_touch(int /*encoder_idx*/, bool /*touched*/) override {}
    void on_touch_strip(int /*val*/) override {}
    void on_pitch_bend(int /*val*/) override {}
    void on_sustain_pedal(bool /*down*/) override {}

    void on_pad_note_on(int pad_num, int vel) override
    {
        if (pad_num >= 0 && pad_num < 16)
        {
            CPktMgr::HWMsg_t msg;
            msg.pktType = CPktMgr::EN_PKT_TYPE_PAD;
            msg.pktCmd = contracts::DeviceControlContracts::PadCmd::EN_PAD_STATE;
            msg.data = { pad_num, contracts::DeviceControlContracts::BTN_PRESSED, vel, 0 };
            contracts::DclSignals::dispatchHardwareInput(msg);

            int note = 36 + pad_num;
            mt::getAudioInterface().addLiveMidiNote(mt::getAudioInterface().getSelected(), note, vel);
        }
    }

    void on_pad_note_off(int pad_num) override
    {
        if (pad_num >= 0 && pad_num < 16)
        {
            CPktMgr::HWMsg_t msg;
            msg.pktType = CPktMgr::EN_PKT_TYPE_PAD;
            msg.pktCmd = contracts::DeviceControlContracts::PadCmd::EN_PAD_STATE;
            msg.data = { pad_num, contracts::DeviceControlContracts::BTN_RELEASED, 0, 0 };
            contracts::DclSignals::dispatchHardwareInput(msg);

            int note = 36 + pad_num;
            mt::getAudioInterface().stopLiveMidiNote(mt::getAudioInterface().getSelected(), note);
        }
    }

    void on_keybed_note_on(int pitch, int vel) override
    {
        mt::getAudioInterface().addLiveMidiNote(mt::getAudioInterface().getSelected(), pitch, vel);
    }

    void on_keybed_note_off(int pitch) override
    {
        mt::getAudioInterface().stopLiveMidiNote(mt::getAudioInterface().getSelected(), pitch);
    }
};
