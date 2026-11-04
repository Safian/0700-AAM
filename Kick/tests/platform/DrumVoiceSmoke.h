// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <audio_program/synth/drum_voice.h>
#include <cmath>
#include <iostream>
#include <vector>

// PE-02 / PE-21 proof: the real mt::DrumVoice, bound to a real drum clip of a real project, renders a synthetic sample (220 Hz +
// 6 kHz sines) and the clip's envelope / LFO / filter properties change the rendered output (run inside the Program Edit smoke,
// KICK_SMOKE_PROGRAMEDIT=1). The voice reads the clip's atomics, which follow the model, so a value set with the program's setters
// (what the Envelopes / LFO / Filter pages call) must change the samples.
namespace drumvoice_smoke {

constexpr double kRate = 48000.0;

struct Render { std::vector<float> l, r; };

inline double rms(const std::vector<float>& v, size_t from, size_t to) {
    to = std::min(to, v.size());
    if (to <= from) return 0.0;
    double s = 0.0;
    for (size_t i = from; i < to; ++i) s += (double)v[i] * v[i];
    return std::sqrt(s / (double)(to - from));
}

// Magnitude of one frequency in a range (Goertzel).
inline double tone(const std::vector<float>& v, size_t from, size_t to, double hz) {
    to = std::min(to, v.size());
    if (to <= from) return 0.0;
    const double w = 2.0 * 3.14159265358979323846 * hz / kRate;
    double re = 0.0, im = 0.0;
    for (size_t i = from; i < to; ++i) { re += v[i] * std::cos(w * (double)(i - from)); im += v[i] * std::sin(w * (double)(i - from)); }
    return 2.0 * std::sqrt(re * re + im * im) / (double)(to - from);
}

inline int zeroCrossings(const std::vector<float>& v, size_t from, size_t to) {
    to = std::min(to, v.size());
    int n = 0;
    for (size_t i = from + 1; i < to; ++i) if ((v[i - 1] < 0) != (v[i] < 0)) ++n;
    return n;
}

inline Render render(mt::MTDrumClip& clip, int seconds100) {
    mt::MTIAtomicProgramMaster master;
    mt::DrumVoice voice(master);
    voice.bindParams(clip.voiceParams);
    voice.setSlice(clip.layers[0]->src);
    voice.prepareToPlay(kRate, 128);
    voice.noteOn(60, 127);
    Render out;
    juce::AudioBuffer<float> block(2, 128);
    const int total = (int)(kRate * seconds100 / 100.0) / 128 * 128;
    for (int done = 0; done < total; done += 128) {
        block.clear();
        static_cast<mt::ISynthVoice&>(voice).process(block);
        out.l.insert(out.l.end(), block.getReadPointer(0), block.getReadPointer(0) + 128);
        out.r.insert(out.r.end(), block.getReadPointer(1), block.getReadPointer(1) + 128);
    }
    return out;
}

} // namespace drumvoice_smoke

inline juce::String runDrumVoiceProof(mt::MTDrumProgram& prog, int programIndex) {
    using namespace drumvoice_smoke;
    auto& api = mt::getAudioInterface();
    const auto fail = [](const juce::String& m) { std::cout << "DRUMVOICE FAIL: " << m << "\n" << std::flush; return m; };
    const auto say  = [](const juce::String& m) { std::cout << "DRUMVOICE: " << m << "\n" << std::flush; };

    // A synthetic sample: 220 Hz + 6 kHz, one second, centred.
    const auto wav = juce::File(juce::SystemStats::getEnvironmentVariable("KICK_DATA_ROOT", {})).getChildFile("programedit/synth.wav");
    {
        wav.deleteFile();
        auto os = wav.createOutputStream();
        juce::WavAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatWriter> writer(os ? fmt.createWriterFor(os.get(), kRate, 2, 16, {}, 0) : nullptr);
        if (!writer) return fail("cannot write the synthetic sample");
        os.release();
        juce::AudioBuffer<float> b(2, (int)kRate);
        for (int i = 0; i < b.getNumSamples(); ++i) {
            const auto t = (double)i / kRate;
            const float v = 0.4f * (float)(std::sin(2 * 3.14159265358979 * 220.0 * t) + std::sin(2 * 3.14159265358979 * 6000.0 * t));
            b.setSample(0, i, v);
            b.setSample(1, i, v);
        }
        writer->writeFromAudioSampleBuffer(b, 0, b.getNumSamples());
    }
    const int asset = api.getAssetManager().add(wav.getFullPathName());
    if (asset < 0) return fail("the synthetic sample was not imported");
    api.addSampleToProgram(programIndex, 0, api.getAsset(mt::AssetTypes::Samples, asset));
    mt::MTDrumClip* clip = nullptr;
    for (int i = 0; i < 100; ++i) {
        clip = prog.getDrumClip(0);
        if (clip && clip->layers[0]->src) break;
        juce::Thread::sleep(50);
    }
    if (!clip || !clip->layers[0]->src)
        return fail("the synthetic sample was not loaded into pad 1 (asset " + juce::String(asset) + " '" + api.getAsset(mt::AssetTypes::Samples, asset) + "', clip "
                    + juce::String(clip ? 1 : 0) + ", source '" + (clip ? clip->layers[0]->source.get() : juce::String()) + "')");

    const auto reset = [&]() {
        for (auto t : { mt::EnvelopeType::AMPLITUDE, mt::EnvelopeType::FILTER, mt::EnvelopeType::PITCH }) {
            prog.setEnvEnabled(0, t, false);
            prog.setEnvType(0, mt::ADSRType::Type_AD, t);
            prog.setEnvelopeAttack(0, t, 0);
            prog.setEnvelopeHold(0, t, 0);
            prog.setEnvelopeDecay(0, t, 0);
            prog.setEnvelopeSustain(0, t, 0);
            prog.setEnvelopeDepth(0, t, 0);
        }
        prog.setLFOEnabled(0, false);
        prog.setLFOType(0, 0);
        prog.setLFOFrequency(0, 1.0);
        prog.setLFOLevel(0, 0);
        prog.setLFOPitch(0, 0);
        prog.setLFOFilter(0, 0);
        prog.setFilterType(0, 0);
        prog.setFilterCutoff(0, 20000);
        prog.setFilterResonance(0, 10);
    };
    const auto N = [](double s) { return (size_t)(s * kRate); };

    // 0. Defaults: a centred sample stays centred and keeps its level for the whole length.
    reset();
    const auto base = render(*clip, 60);
    const double baseEarly = rms(base.l, N(0.02), N(0.10)), baseLate = rms(base.l, N(0.40), N(0.55));
    say("default: rms early " + juce::String(baseEarly, 4) + " late " + juce::String(baseLate, 4)
        + " L/R " + juce::String(rms(base.l, N(0.02), N(0.55)) / std::max(1e-9, rms(base.r, N(0.02), N(0.55))), 3));
    if (baseEarly < 0.05) return fail("the default render is silent");
    if (std::abs(rms(base.l, N(0.02), N(0.55)) / std::max(1e-9, rms(base.r, N(0.02), N(0.55))) - 1.0) > 0.05) return fail("a centred sample is not centred by default (pan bias)");
    if (baseLate < 0.9 * baseEarly) return fail("the default render is not steady: an envelope is acting although all are off");

    // 1. Amplitude envelope: off = no effect (even with a Decay set); on = the amplitude falls.
    reset();
    prog.setEnvelopeDecay(0, mt::EnvelopeType::AMPLITUDE, 30);
    const auto ampOff = render(*clip, 60);
    if (std::abs(rms(ampOff.l, N(0.40), N(0.55)) - baseLate) > 0.02 * baseLate) return fail("a disabled amplitude envelope changed the sound");
    prog.setEnvEnabled(0, mt::EnvelopeType::AMPLITUDE, true);
    const auto ampOn = render(*clip, 60);
    const double ampEarly = rms(ampOn.l, N(0.00), N(0.03)), ampLate = rms(ampOn.l, N(0.40), N(0.55));
    say("amp AD decay 30: rms start " + juce::String(ampEarly, 4) + " late " + juce::String(ampLate, 4));
    if (!(ampLate < 0.1 * ampEarly)) return fail("the amplitude envelope (AD, Decay 30) does not make the sound decay");
    prog.setEnvelopeDecay(0, mt::EnvelopeType::AMPLITUDE, 100);
    const auto ampLong = render(*clip, 60);
    if (!(rms(ampLong.l, N(0.20), N(0.30)) > 3.0 * rms(ampOn.l, N(0.20), N(0.30)))) return fail("a longer Decay does not give a longer sound");

    // 2. Pitch envelope: Depth -> the pitch falls (zero crossings of the start vs the end).
    reset();
    prog.setEnvEnabled(0, mt::EnvelopeType::PITCH, true);
    prog.setEnvelopeDecay(0, mt::EnvelopeType::PITCH, 60);
    prog.setEnvelopeDepth(0, mt::EnvelopeType::PITCH, 24);
    const auto pit = render(*clip, 60);
    const int zcA = zeroCrossings(pit.l, N(0.00), N(0.06)), zcB = zeroCrossings(pit.l, N(0.45), N(0.51)), zcRef = zeroCrossings(base.l, N(0.00), N(0.06));
    say("pitch env depth 24: zero crossings start " + juce::String(zcA) + " end " + juce::String(zcB) + " (default " + juce::String(zcRef) + ")");
    if (!(zcB < 0.7 * zcA)) return fail("the pitch envelope does not lower the pitch");

    // 3. LFO: amplitude amount at 6 Hz makes the level wobble; disabled = steady.
    reset();
    prog.setLFOType(0, 0);
    prog.setLFOFrequency(0, 6.0);
    prog.setLFOLevel(0, 127);
    const auto lfoOff = render(*clip, 60);
    if (std::abs(rms(lfoOff.l, N(0.40), N(0.55)) - baseLate) > 0.02 * baseLate) return fail("a disabled LFO changed the sound");
    prog.setLFOEnabled(0, true);
    const auto lfoOn = render(*clip, 60);
    double lo = 1e9, hi = 0;
    for (size_t s = N(0.05); s + N(0.01) < N(0.55); s += N(0.01)) { const auto v = rms(lfoOn.l, s, s + N(0.01)); lo = std::min(lo, v); hi = std::max(hi, v); }
    say("LFO amp 127 @ 6 Hz: window rms min " + juce::String(lo, 4) + " max " + juce::String(hi, 4));
    if (!(hi > 3.0 * std::max(lo, 1e-6) || lo < 0.3 * baseLate)) return fail("the LFO does not modulate the level");

    // 4. Filter: low pass at 800 Hz removes the 6 kHz tone, keeps 220 Hz; high pass the other way.
    reset();
    const double lowRef = tone(base.l, N(0.1), N(0.5), 220.0), highRef = tone(base.l, N(0.1), N(0.5), 6000.0);
    prog.setFilterType(0, 0);
    prog.setFilterCutoff(0, 800);
    const auto lp = render(*clip, 60);
    say("filter LP 800: 220 Hz " + juce::String(tone(lp.l, N(0.1), N(0.5), 220.0) / lowRef, 3) + " x, 6 kHz " + juce::String(tone(lp.l, N(0.1), N(0.5), 6000.0) / highRef, 4) + " x");
    if (!(tone(lp.l, N(0.1), N(0.5), 6000.0) < 0.1 * highRef && tone(lp.l, N(0.1), N(0.5), 220.0) > 0.7 * lowRef)) return fail("the low pass filter does not filter");
    prog.setFilterType(0, 1);
    const auto hp = render(*clip, 60);
    say("filter HP 800: 220 Hz " + juce::String(tone(hp.l, N(0.1), N(0.5), 220.0) / lowRef, 4) + " x, 6 kHz " + juce::String(tone(hp.l, N(0.1), N(0.5), 6000.0) / highRef, 3) + " x");
    if (!(tone(hp.l, N(0.1), N(0.5), 220.0) < 0.1 * lowRef && tone(hp.l, N(0.1), N(0.5), 6000.0) > 0.7 * highRef)) return fail("the high pass filter does not filter");

    // 5. Filter envelope: a low cutoff opened by the envelope, so the 6 kHz tone is there at the start and gone later; off = closed.
    reset();
    prog.setFilterType(0, 0);
    prog.setFilterCutoff(0, 300);
    prog.setEnvelopeDecay(0, mt::EnvelopeType::FILTER, 30);
    prog.setEnvelopeDepth(0, mt::EnvelopeType::FILTER, 127);
    const auto fOff = render(*clip, 60);
    prog.setEnvEnabled(0, mt::EnvelopeType::FILTER, true);
    const auto fOn = render(*clip, 60);
    const double hfOff = tone(fOff.l, N(0.00), N(0.04), 6000.0), hfStart = tone(fOn.l, N(0.00), N(0.04), 6000.0), hfLate = tone(fOn.l, N(0.40), N(0.55), 6000.0);
    say("filter env depth 127, cutoff 300: 6 kHz off " + juce::String(hfOff, 5) + " start " + juce::String(hfStart, 4) + " late " + juce::String(hfLate, 5) + " (ref " + juce::String(highRef, 4) + ")");
    if (!(hfOff < 0.02 * highRef)) return fail("a disabled filter envelope opens the filter");
    if (!(hfStart > 0.15 * highRef && hfLate < 0.05 * highRef)) return fail("the filter envelope does not sweep the cutoff");

    reset();
    say("PASS: the drum voice follows the clip: amp / pitch / filter envelope, LFO, filter type and cutoff change the rendered samples");
    return {};
}
