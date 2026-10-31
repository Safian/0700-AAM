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
    juce::Component::SafePointer<juce::Component> m_touch_targets[10];
    std::chrono::steady_clock::time_point m_last_tap_time;
    bool m_tempo_edit_active = false;
    int m_frame_counter = 0;
    int m_cmd_fd = -1;
    int m_cmd_fd_w = -1;
    std::string m_cmd_buf;
    bool m_shift_pressed = false;
    bool m_locate_pressed = false;
    int m_current_bank = 0;
    int m_octave_offset = 0;
    int m_key_pitch_map[128];

    bool isShiftDown() const { return m_shift_pressed; }
    bool isLocateDown() const { return m_locate_pressed; }

    void updateBankLeds()
    {
        if (m_hw && m_hw->control_surface()) {
            int b = m_current_bank % 4;
            m_hw->control_surface()->set_led(HW_LED_BANK_A, b == 0);
            m_hw->control_surface()->set_led(HW_LED_BANK_B, b == 1);
            m_hw->control_surface()->set_led(HW_LED_BANK_C, b == 2);
            m_hw->control_surface()->set_led(HW_LED_BANK_D, b == 3);
        }
    }

    void updateOctaveLeds()
    {
        if (m_hw && m_hw->control_surface()) {
            m_hw->control_surface()->set_led(HW_LED_OCTAVE_DOWN, m_octave_offset < 0);
            m_hw->control_surface()->set_led(HW_LED_OCTAVE_UP, m_octave_offset > 0);
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

    void dispatchTouch(int slot, int x, int y, bool is_down, bool is_up)
    {
        if (slot >= 0 && slot < 10) {
            m_last_touch_x[slot] = x;
            m_last_touch_y[slot] = y;
        }

        juce::MessageManager::callAsync([this, slot, x, y, is_down, is_up]() {
            auto* root = (m_window != nullptr) ? (juce::Component*)m_window : m_root;
            if (!root) return;

            int ix = x;
            int iy = y;

            auto* target = root->getComponentAt(ix, iy);
            if (!target) target = root;

            auto* peer = m_window ? m_window->getPeer() : nullptr;
            juce::ModifierKeys mods = is_up ? juce::ModifierKeys() : juce::ModifierKeys::leftButtonModifier;

            if (peer)
            {
                peer->handleMouseEvent(juce::MouseInputSource::InputSourceType::touch,
                                       juce::Point<float>((float)ix, (float)iy),
                                       mods,
                                       1.0f,
                                       0.0f,
                                       juce::Time::currentTimeMillis(),
                                       {},
                                       slot);
            }

            auto localPt = target->getLocalPoint(root, juce::Point<int>(ix, iy)).toFloat();
            juce::MouseEvent e(juce::Desktop::getInstance().getMainMouseSource(),
                               localPt, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                               target, target,
                               juce::Time::getCurrentTime(),
                               localPt,
                               juce::Time::getCurrentTime(),
                               1, false);

            if (is_down)
            {
                if (slot >= 0 && slot < 10) m_touch_targets[slot] = target;
                target->mouseDown(e);
            }
            else if (is_up)
            {
                auto* downTarget = (slot >= 0 && slot < 10) ? m_touch_targets[slot].getComponent() : nullptr;
                auto* effectiveTarget = downTarget ? downTarget : target;
                if (effectiveTarget) {
                    auto downLocalPt = effectiveTarget->getLocalPoint(root, juce::Point<int>(ix, iy)).toFloat();
                    juce::MouseEvent downE(juce::Desktop::getInstance().getMainMouseSource(),
                                           downLocalPt, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                           effectiveTarget, effectiveTarget,
                                           juce::Time::getCurrentTime(),
                                           downLocalPt,
                                           juce::Time::getCurrentTime(),
                                           1, false);
                    effectiveTarget->mouseUp(downE);
                }

                auto* btn = dynamic_cast<juce::Button*>(effectiveTarget);
                if (!btn && effectiveTarget) btn = effectiveTarget->findParentComponentOfClass<juce::Button>();
                if (btn) {
                    printf("[TOUCH] Button released: '%s' (pos %d,%d, toggle=%d)\n",
                           btn->getName().toRawUTF8(), ix, iy, (int)btn->getToggleState());
                    fflush(stdout);
                }

                if (slot >= 0 && slot < 10) m_touch_targets[slot] = nullptr;
            }
            else
            {
                auto* downTarget = (slot >= 0 && slot < 10) ? m_touch_targets[slot].getComponent() : nullptr;
                if (downTarget) {
                    auto downLocalPt = downTarget->getLocalPoint(root, juce::Point<int>(ix, iy)).toFloat();
                    juce::MouseEvent downE(juce::Desktop::getInstance().getMainMouseSource(),
                                           downLocalPt, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                           downTarget, downTarget,
                                           juce::Time::getCurrentTime(),
                                           downLocalPt,
                                           juce::Time::getCurrentTime(),
                                           1, false);
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
        printf("[BRIDGE] Saved snapshot %dx%d to %s (non-bg pixels: %d / %d = %.1f%%)\n",
               w, h, filepath, non_default_px, w * h, 100.0f * non_default_px / (float)(w * h));
        fflush(stdout);
    }

    void timerCallback() override
    {
        pollCommands();

        if (!m_hw || !m_root || !m_hw->display()) return;

        auto* targetComp = (m_window != nullptr) ? (juce::Component*)m_window : m_root;
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
                m_hw->control_surface()->set_led(HW_LED_STOP, true);
                m_hw->control_surface()->set_led(HW_LED_PLAY, false);
                m_hw->control_surface()->set_led(HW_LED_BANK_A, true);
            }

            mt::signals::playStateChanged.connect([this](mt::TransportState state) {
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

            printf("[HAL] KickHardwareBridge fully operational (Display 1280x800 @ 30FPS, Touch, ControlSurface, CmdFIFO)\n");
        }
    }

    void stop()
    {
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

    void on_button_press(HardwareButton btn) override
    {
        auto& ae = mt::getAudioInterface();
        switch (btn)
        {
            case HW_BTN_SHIFT:
            {
                m_shift_pressed = true;
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
                if (isShiftDown()) {
                    // Shift + STOP: Return to start (01:01:00)
                    ae.setSequenceStop();
                    ae.setSequencePlay(ae.getSelectedSequence(), false, false);
                    ae.setSequenceStop();
                } else {
                    ae.setSequenceStop();
                }
                ae.stopPreview();
                auto sel = ae.getSelected();
                for (int ch = 1; ch <= 16; ++ch) {
                    ae.addLiveMidiMsg(sel, juce::MidiMessage::allNotesOff(ch));
                }
                m_tempo_edit_active = false;
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
                if (isShiftDown()) {
                    sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_MASTER, contracts::DeviceControlContracts::BTN_PRESSED);
                } else {
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
                }
                break;
            }
            case HW_BTN_METRONOME:
            {
                if (isShiftDown()) {
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::HomeOptions);
                } else {
                    auto& metronome = mt::getSettings().metronome;
                    metronome.enabled = !metronome.enabled();
                    printf("[BRIDGE] Metronome %s\n", metronome.enabled() ? "ON" : "OFF");
                    fflush(stdout);
                }
                break;
            }
            case HW_BTN_ARP:
            {
                if (isShiftDown()) {
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuPadPerform);
                } else {
                    sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_NOTE_REPEAT, contracts::DeviceControlContracts::BTN_PRESSED);
                }
                break;
            }
            case HW_BTN_LATCH:
            {
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_LATCH, contracts::DeviceControlContracts::BTN_PRESSED);
                break;
            }
            case HW_BTN_OCTAVE_DOWN:
            {
                if (m_octave_offset > -3) {
                    m_octave_offset--;
                    updateOctaveLeds();
                    printf("[BRIDGE] Octave transposed to %d\n", m_octave_offset);
                    fflush(stdout);
                }
                break;
            }
            case HW_BTN_OCTAVE_UP:
            {
                if (m_octave_offset < 3) {
                    m_octave_offset++;
                    updateOctaveLeds();
                    printf("[BRIDGE] Octave transposed to %d\n", m_octave_offset);
                    fflush(stdout);
                }
                break;
            }
            case HW_BTN_BANK_A:
            {
                int bank = isShiftDown() ? 4 : 0;
                m_current_bank = bank;
                ae.setBank(juce::String::charToString((char)('A' + bank)));
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_A + bank, contracts::DeviceControlContracts::BTN_PRESSED);
                updateBankLeds();
                break;
            }
            case HW_BTN_BANK_B:
            {
                int bank = isShiftDown() ? 5 : 1;
                m_current_bank = bank;
                ae.setBank(juce::String::charToString((char)('A' + bank)));
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_A + bank, contracts::DeviceControlContracts::BTN_PRESSED);
                updateBankLeds();
                break;
            }
            case HW_BTN_BANK_C:
            {
                int bank = isShiftDown() ? 6 : 2;
                m_current_bank = bank;
                ae.setBank(juce::String::charToString((char)('A' + bank)));
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_A + bank, contracts::DeviceControlContracts::BTN_PRESSED);
                updateBankLeds();
                break;
            }
            case HW_BTN_BANK_D:
            {
                int bank = isShiftDown() ? 7 : 3;
                m_current_bank = bank;
                ae.setBank(juce::String::charToString((char)('A' + bank)));
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_A + bank, contracts::DeviceControlContracts::BTN_PRESSED);
                updateBankLeds();
                break;
            }
            case HW_BTN_TOUCH_STRIP:
            {
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
                    sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_LATCH, contracts::DeviceControlContracts::BTN_PRESSED);
                } else {
                    sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_NOTE_REPEAT, contracts::DeviceControlContracts::BTN_PRESSED);
                }
                break;
            }
            case HW_BTN_FULL_LEVEL:
            {
                if (isShiftDown()) {
                    sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_HALF_LEVEL, contracts::DeviceControlContracts::BTN_PRESSED);
                    contracts::DclContractsHelper::setHalfLevel(true);
                } else {
                    sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_FULL_LEVEL, contracts::DeviceControlContracts::BTN_PRESSED);
                    contracts::DclContractsHelper::setHalfLevel(false);
                }
                break;
            }
            case HW_BTN_16_LEVELS:
            {
                if (isShiftDown()) {
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuPadPerform);
                } else {
                    sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_16LEVELS, contracts::DeviceControlContracts::BTN_PRESSED);
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
                    sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_ERASE, contracts::DeviceControlContracts::BTN_PRESSED);
                } else {
                    sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_COPY, contracts::DeviceControlContracts::BTN_PRESSED);
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
                kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuFileBrowser);
                break;
            }
            case HW_BTN_MAIN:
            {
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
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_AUTOMATION, contracts::DeviceControlContracts::BTN_PRESSED);
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
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_TC, contracts::DeviceControlContracts::BTN_PRESSED);
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
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_MINUS, contracts::DeviceControlContracts::BTN_PRESSED);
                break;
            }
            case HW_BTN_STEP_NEXT:
            {
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_PLUS, contracts::DeviceControlContracts::BTN_PRESSED);
                break;
            }
            case HW_BTN_BAR_PREV:
            {
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_BACK, contracts::DeviceControlContracts::BTN_PRESSED);
                break;
            }
            case HW_BTN_BAR_NEXT:
            {
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_ENTER, contracts::DeviceControlContracts::BTN_PRESSED);
                break;
            }
            case HW_BTN_NEXT_SEQ:
            {
                kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuNextSequence);
                break;
            }
            case HW_BTN_CURSOR_UP:
                on_jog_dial(1);
                break;
            case HW_BTN_CURSOR_DOWN:
                on_jog_dial(-1);
                break;
            case HW_BTN_CURSOR_LEFT:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_MINUS, contracts::DeviceControlContracts::BTN_PRESSED);
                break;
            case HW_BTN_CURSOR_RIGHT:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_PLUS, contracts::DeviceControlContracts::BTN_PRESSED);
                break;
            default:
                break;
        }
    }

    void on_button_release(HardwareButton btn) override
    {
        switch (btn)
        {
            case HW_BTN_SHIFT:
                m_shift_pressed = false;
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_SHIFT, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_LOCATE:
                m_locate_pressed = false;
                break;
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
            case HW_BTN_BANK_A:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_A, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_BANK_B:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_B, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_BANK_C:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_C, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_BANK_D:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_D, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_CURSOR_LEFT:
            case HW_BTN_STEP_PREV:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_MINUS, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_CURSOR_RIGHT:
            case HW_BTN_STEP_NEXT:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_PLUS, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_BAR_PREV:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_BACK, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_BAR_NEXT:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_ENTER, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_TAP_TEMPO:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_TAP_TEMPO, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_NOTE_REPEAT:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_NOTE_REPEAT, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_LATCH:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_LATCH, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_FULL_LEVEL:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_FULL_LEVEL, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_16_LEVELS:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_16LEVELS, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_ERASE:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_ERASE, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_COPY:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_COPY, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_TC:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_TC, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_AUTOMATION:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_AUTOMATION, contracts::DeviceControlContracts::BTN_RELEASED);
                break;
            case HW_BTN_QLINK:
                sendButtonMsg(contracts::DeviceControlContracts::BtnLedSequence::EN_LINK, contracts::DeviceControlContracts::BTN_RELEASED);
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
    void on_touch_strip(int val) override
    {
        CPktMgr::HWMsg_t msg;
        msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_SLIDER;
        msg.pktCmd  = contracts::DeviceControlContracts::SliderCmd::EN_SLIDER_TOUCH_INPUT;
        msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
        msg.data    = { juce::jlimit(0, 100, (int)std::round(val * 100.0f / 127.0f)) };
        contracts::DclSignals::dispatchHardwareInput(msg);

        if (m_hw && m_hw->control_surface())
            m_hw->control_surface()->set_touch_strip_leds(val);
    }
    void on_pitch_bend(int val) override
    {
        mt::getAudioInterface().addLiveMidiMsg(mt::getAudioInterface().getSelected(),
                                               juce::MidiMessage::pitchWheel(1, val));
    }
    void on_sustain_pedal(bool down) override
    {
        mt::getAudioInterface().addLiveMidiMsg(mt::getAudioInterface().getSelected(),
                                               juce::MidiMessage::controllerEvent(1, 64, down ? 127 : 0));
    }

    void on_pad_note_on(int pad_num, int vel) override
    {
        if (pad_num >= 0 && pad_num < 16)
        {
            CPktMgr::HWMsg_t msg;
            msg.pktType = CPktMgr::EN_PKT_TYPE_PAD;
            msg.pktCmd = contracts::DeviceControlContracts::PadCmd::EN_PAD_STATE;
            msg.data = { pad_num, contracts::DeviceControlContracts::BTN_PRESSED, vel, 0 };
            contracts::DclSignals::dispatchHardwareInput(msg);

            int note = pad_num + mt::getAudioInterface().getBankOffset();
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

            int note = pad_num + mt::getAudioInterface().getBankOffset();
            mt::getAudioInterface().stopLiveMidiNote(mt::getAudioInterface().getSelected(), note);
        }
    }

    void on_keybed_note_on(int pitch, int vel) override
    {
        int sounding_pitch = juce::jlimit(0, 127, pitch + m_octave_offset * 12);
        if (pitch >= 0 && pitch < 128) {
            m_key_pitch_map[pitch] = sounding_pitch;
        }
        mt::getAudioInterface().addLiveMidiNote(mt::getAudioInterface().getSelected(), sounding_pitch, vel);
    }

    void on_keybed_note_off(int pitch) override
    {
        int sounding_pitch = (pitch >= 0 && pitch < 128 && m_key_pitch_map[pitch] >= 0)
                             ? m_key_pitch_map[pitch]
                             : juce::jlimit(0, 127, pitch + m_octave_offset * 12);
        if (pitch >= 0 && pitch < 128) {
            m_key_pitch_map[pitch] = -1;
        }
        mt::getAudioInterface().stopLiveMidiNote(mt::getAudioInterface().getSelected(), sounding_pitch);
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

    void processCommand(const std::string& cmd)
    {
        char op[64] = { 0 };
        int a1 = 0, a2 = 0;
        char sval[128] = { 0 };
        if (sscanf(cmd.c_str(), "%63s %d %d", op, &a1, &a2) == 3) {
            if (strcmp(op, "touch") == 0) {
                printf("[CMD] touch %d %d\n", a1, a2);
                fflush(stdout);
                on_touch_down(0, a1, a2);
                on_touch_up(0);
            } else if (strcmp(op, "touch_down") == 0) {
                printf("[CMD] touch_down %d %d\n", a1, a2);
                fflush(stdout);
                on_touch_down(0, a1, a2);
            } else if (strcmp(op, "touch_move") == 0) {
                printf("[CMD] touch_move %d %d\n", a1, a2);
                fflush(stdout);
                on_touch_move(0, a1, a2);
            } else if (strcmp(op, "pad") == 0) {
                printf("[CMD] pad %d vel=%d\n", a1, a2);
                fflush(stdout);
                if (a2 > 0) on_pad_note_on(a1, a2);
                else on_pad_note_off(a1);
            } else if (strcmp(op, "key") == 0) {
                printf("[CMD] key %d vel=%d\n", a1, a2);
                fflush(stdout);
                if (a2 > 0) on_keybed_note_on(a1, a2);
                else on_keybed_note_off(a1);
            } else if (strcmp(op, "encoder") == 0) {
                printf("[CMD] encoder %d delta=%d\n", a1, a2);
                fflush(stdout);
                on_encoder_turn(a1, a2);
            }
        } else if (sscanf(cmd.c_str(), "%63s %127s", op, sval) == 2) {
            if (strcmp(op, "btn") == 0) {
                printf("[CMD] btn %s\n", sval);
                fflush(stdout);
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
                    on_button_press(b);
                    on_button_release(b);
                }
            } else if (strcmp(op, "jog") == 0) {
                int delta = atoi(sval);
                printf("[CMD] jog %d\n", delta);
                fflush(stdout);
                on_jog_dial(delta);
            } else if (strcmp(op, "strip") == 0) {
                int val = atoi(sval);
                printf("[CMD] strip %d\n", val);
                fflush(stdout);
                on_touch_strip(val);
            } else if (strcmp(op, "snapshot") == 0) {
                printf("[CMD] snapshot %s\n", sval);
                fflush(stdout);
                saveSnapshot(m_framebuffer_img, sval);
            }
        } else if (strcmp(cmd.c_str(), "touch_up") == 0) {
            printf("[CMD] touch_up\n");
            fflush(stdout);
            on_touch_up(0);
        }
    }
};
