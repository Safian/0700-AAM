// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ClapInstrumentProcessor.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <cstdlib>

static void check(bool ok, const char* why) {
    if (!ok) {
        std::cerr << "FAIL: " << why << '\n';
        std::exit(1);
    }
}

int main(int argc, char** argv) {
    std::cout << "Testing ClapInstrumentProcessor (PR #27)...\n";

    // 1. Test LockFreeEventRingBuffer
    {
        AudIO::LockFreeEventRingBuffer<int, 4> rb;
        int val = 0;
        check(!rb.pop(val), "Empty pop returns false");

        check(rb.push(10), "Push 10");
        check(rb.push(20), "Push 20");
        check(rb.push(30), "Push 30");
        // Capacity 4 can hold 3 items in standard SPSC ring buffer
        check(!rb.push(40), "Full push returns false (non-blocking)");

        check(rb.pop(val) && val == 10, "Pop FIFO 10");
        check(rb.pop(val) && val == 20, "Pop FIFO 20");
        check(rb.pop(val) && val == 30, "Pop FIFO 30");
        check(!rb.pop(val), "Empty after drain");

        rb.push(99);
        rb.clear();
        check(!rb.pop(val), "Empty after clear");

        std::cout << "PASS: LockFreeEventRingBuffer FIFO and RT non-blocking overflow\n";
    }

    // 2. Test Instantiation and Host Descriptor
    {
        AudIO::ClapInstrumentProcessor processor;
        // Verify audio processing on inactive processor outputs silence
        std::vector<float> outL(128, 1.0f);
        std::vector<float> outR(128, 1.0f);
        processor.processAudioBlock(nullptr, nullptr, outL.data(), outR.data(),
                                    nullptr, nullptr, nullptr, nullptr, 128);

        check(outL[0] == 0.0f && outL[127] == 0.0f, "Inactive processor outputs silence on L");
        check(outR[0] == 0.0f && outR[127] == 0.0f, "Inactive processor outputs silence on R");

        std::cout << "PASS: Processor default instantiation and silence output\n";
    }

    // 3. Test with Mock CLAP Plugin
    const char* mockClapPath = (argc > 1) ? argv[1] : "./mock_synth.clap";
    std::cout << "Loading mock CLAP plugin from: " << mockClapPath << "...\n";

    AudIO::ClapInstrumentProcessor processor;
    bool loaded = processor.loadPluginFromSdCard(mockClapPath, "org.antigravity.mock_clap_synth", 48000.0, 128);
    check(loaded, "Successfully loaded mock CLAP plugin");

    // 4. Test Event Translation: NoteOn / NoteOff
    {
        std::vector<float> outL(128, 0.0f);
        std::vector<float> outR(128, 0.0f);

        // Process a block without note -> mock synth outputs baseline 0.05f
        processor.processAudioBlock(nullptr, nullptr, outL.data(), outR.data(),
                                    nullptr, nullptr, nullptr, nullptr, 128);
        check(std::abs(outL[0] - 0.05f) < 1e-4f, "Baseline output is 0.05f");

        // Enqueue NoteOn
        AudIO::KickHardwareSynthEvent noteOnEvt{};
        noteOnEvt.type = AudIO::KickHardwareSynthEvent::Type::NoteOn;
        noteOnEvt.channel = 0;
        noteOnEvt.keyOrMacroIndex = 60; // C4
        noteOnEvt.normalizedValue = 0.9f;
        check(processor.enqueueHardwareEvent(noteOnEvt), "Enqueued NoteOn");

        // Process block with NoteOn -> mock synth outputs active 0.75f
        processor.processAudioBlock(nullptr, nullptr, outL.data(), outR.data(),
                                    nullptr, nullptr, nullptr, nullptr, 128);
        check(std::abs(outL[0] - 0.75f) < 1e-4f, "NoteOn triggers synth active output (0.75f)");

        // Enqueue NoteOff
        AudIO::KickHardwareSynthEvent noteOffEvt{};
        noteOffEvt.type = AudIO::KickHardwareSynthEvent::Type::NoteOff;
        noteOffEvt.channel = 0;
        noteOffEvt.keyOrMacroIndex = 60;
        check(processor.enqueueHardwareEvent(noteOffEvt), "Enqueued NoteOff");

        // Process block with NoteOff -> mock synth outputs back to 0.05f
        processor.processAudioBlock(nullptr, nullptr, outL.data(), outR.data(),
                                    nullptr, nullptr, nullptr, nullptr, 128);
        check(std::abs(outL[0] - 0.05f) < 1e-4f, "NoteOff restores baseline output (0.05f)");

        std::cout << "PASS: NoteOn / NoteOff event routing to CLAP synth\n";
    }

    // 5. Test Layer 1 Message-Thread Hooks: Parameter Querying & Descriptor Cache
    {
        uint32_t count = processor.getPluginParamCount();
        check(count == 8, "getPluginParamCount() returns 8 cached parameters");

        clap_param_info_t info{};
        check(processor.queryPluginParamInfo(0, &info), "queryPluginParamInfo(0) succeeds");
        check(info.id == 1000, "Param 0 id is 1000");
        check(std::string(info.name) == "Macro 1", "Param 0 name is 'Macro 1'");
        check(info.min_value == 0.0 && info.max_value == 1.0, "Param 0 range is [0.0, 1.0]");

        check(processor.queryPluginParamInfo(7, &info), "queryPluginParamInfo(7) succeeds");
        check(info.id == 1007, "Param 7 id is 1007");

        check(!processor.queryPluginParamInfo(8, &info), "queryPluginParamInfo(8) out of bounds returns false");
        check(!processor.queryPluginParamInfo(0, nullptr), "queryPluginParamInfo with nullptr returns false");

        std::cout << "PASS: Layer 1 Message-Thread Parameter Query Hooks (queryPluginParamInfo)\n";
    }

    // 6. Test AudiomParamEvent & 256-Slot SPSC Ring Buffer (pushNormalizedParamEvent)
    {
        std::vector<float> outL(128, 0.0f);
        std::vector<float> outR(128, 0.0f);
        std::vector<float> cvCutoff(128, 0.0f);
        std::vector<float> cvResonance(128, 0.0f);

        // A. Push value update via AudiomParamEvent (Encoder / Layer 2 UI)
        AudIO::AudiomParamEvent encEvt{};
        encEvt.paramId = 1000;
        encEvt.normalizedVal = 0.85;
        encEvt.modAmount = 0.0;
        encEvt.bankIndex = 0;
        encEvt.sourceFlags = AudIO::SourceFlag_Encoder | AudIO::SourceFlag_UI_Layer2;
        check(processor.pushNormalizedParamEvent(encEvt), "pushNormalizedParamEvent (Encoder/UI)");

        // Process block -> drains m_paramEventQueue and updates CV cutoff (0.85 * 5V = 4.25V)
        processor.processAudioBlock(nullptr, nullptr, outL.data(), outR.data(),
                                    cvCutoff.data(), cvResonance.data(), nullptr, nullptr, 128);
        check(std::abs(cvCutoff[0] - 4.25f) < 1e-4f, "CV Cutoff updated from AudiomParamEvent (4.25V)");

        // B. Push modulation update via AudiomParamEvent (Poly-aftertouch / TouchStrip)
        AudIO::AudiomParamEvent modEvt{};
        modEvt.paramId = 1000;
        modEvt.normalizedVal = 0.0;
        modEvt.modAmount = -0.5;
        modEvt.bankIndex = 0;
        modEvt.sourceFlags = AudIO::SourceFlag_PadPolyAT;
        check(processor.pushNormalizedParamEvent(modEvt), "pushNormalizedParamEvent (Poly-aftertouch mod)");

        processor.processAudioBlock(nullptr, nullptr, outL.data(), outR.data(),
                                    cvCutoff.data(), cvResonance.data(), nullptr, nullptr, 128);

        // C. Verify 256-slot capacity non-blocking overflow
        size_t pushedCount = 0;
        for (size_t i = 0; i < 300; ++i) {
            AudIO::AudiomParamEvent ev{};
            ev.paramId = 1000 + (i % 8);
            ev.normalizedVal = 0.5;
            ev.bankIndex = 0;
            ev.sourceFlags = AudIO::SourceFlag_UI_Layer2;
            if (processor.pushNormalizedParamEvent(ev)) {
                ++pushedCount;
            }
        }
        // Capacity 256 ring buffer holds 255 items before dropping safely without blocking
        check(pushedCount == 255, "m_paramEventQueue 256-slot capacity drops safely without blocking RT thread");

        // Drain the queue cleanly
        processor.processAudioBlock(nullptr, nullptr, outL.data(), outR.data(),
                                    cvCutoff.data(), cvResonance.data(), nullptr, nullptr, 128);

        std::cout << "PASS: AudiomParamEvent SPSC ring-buffer push & lock-free RT drain\n";
    }

    // 7. Test Macro Encoders and Hardware CV Generation
    {
        std::vector<float> outL(128, 0.0f);
        std::vector<float> outR(128, 0.0f);
        std::vector<float> cvCutoff(128, 0.0f);
        std::vector<float> cvResonance(128, 0.0f);

        // Turn Macro 1 (Cutoff) to 0.8f, Macro 2 (Resonance) to 0.6f
        AudIO::KickHardwareSynthEvent macro1{};
        macro1.type = AudIO::KickHardwareSynthEvent::Type::MacroEncoderChange;
        macro1.keyOrMacroIndex = 0;
        macro1.normalizedValue = 0.8f;
        processor.enqueueHardwareEvent(macro1);

        AudIO::KickHardwareSynthEvent macro2{};
        macro2.type = AudIO::KickHardwareSynthEvent::Type::MacroEncoderChange;
        macro2.keyOrMacroIndex = 1;
        macro2.normalizedValue = 0.6f;
        processor.enqueueHardwareEvent(macro2);

        processor.processAudioBlock(nullptr, nullptr, outL.data(), outR.data(),
                                    cvCutoff.data(), cvResonance.data(), nullptr, nullptr, 128);

        // Verify CV outputs: 0.8 * 5.0V = 4.0V, 0.6 * 5.0V = 3.0V
        check(std::abs(cvCutoff[0] - 4.0f) < 1e-4f, "CV Cutoff output is 4.0V (0.8 * 5V)");
        check(std::abs(cvResonance[0] - 3.0f) < 1e-4f, "CV Resonance output is 3.0V (0.6 * 5V)");

        std::cout << "PASS: Macro encoders to CLAP parameters and 0-5V CV output scaling\n";
    }

    // 6. Test Hybrid Analog Return Loop
    {
        std::vector<float> inL(128, 0.2f);
        std::vector<float> inR(128, 0.2f);
        std::vector<float> outL(128, 0.0f);
        std::vector<float> outR(128, 0.0f);

        // With hybrid analog loop DISABLED: out = synth (0.05f)
        processor.setHybridExternalFilterLoop(false);
        processor.processAudioBlock(inL.data(), inR.data(), outL.data(), outR.data(),
                                    nullptr, nullptr, nullptr, nullptr, 128);
        check(std::abs(outL[0] - 0.05f) < 1e-4f, "Hybrid loop disabled ignores analog line-in");

        // With hybrid analog loop ENABLED: out = synth (0.05f) + in (0.2f) = 0.25f
        processor.setHybridExternalFilterLoop(true);
        processor.processAudioBlock(inL.data(), inR.data(), outL.data(), outR.data(),
                                    nullptr, nullptr, nullptr, nullptr, 128);
        check(std::abs(outL[0] - 0.25f) < 1e-4f, "Hybrid loop enabled sums analog line-in return");

        std::cout << "PASS: Hybrid Analog Loop summing\n";
    }

    // 7. Test Plugin Unload
    {
        processor.unloadPlugin();
        std::vector<float> outL(128, 1.0f);
        std::vector<float> outR(128, 1.0f);
        processor.processAudioBlock(nullptr, nullptr, outL.data(), outR.data(),
                                    nullptr, nullptr, nullptr, nullptr, 128);
        check(outL[0] == 0.0f, "Unload zeroes output");

        std::cout << "PASS: Clean plugin unload\n";
    }

    std::cout << "ALL CLAP INSTRUMENT PROCESSOR TESTS PASSED SUCCESSFULLY!\n";
    return 0;
}
