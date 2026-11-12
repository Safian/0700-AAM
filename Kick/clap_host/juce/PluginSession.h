// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <kick_clap/Host.h>
#include <filesystem>
#include <atomic>
#include <mutex>
#include <chrono>

namespace kick::clap {
// Shared by the KICK program and its audio object. All plugin control and
// ValueTree work happens on JUCE's message thread; rendering only reads atomics.
class PluginSession : private juce::Timer, private juce::ValueTree::Listener {
public:
    static std::shared_ptr<PluginSession> create(juce::ValueTree program);
    explicit PluginSession(juce::ValueTree program);
    ~PluginSession() override;
    Host& host() noexcept { return processor; }
    const juce::String& status() const noexcept { return statusMessage; }
    void select(const Descriptor&);
    void remove();
    bool save();
    void service();
    void prepare(double rate, uint32_t frames) noexcept;
    void render(juce::AudioBuffer<float>&) noexcept;
    void midi(const juce::MidiMessage&) noexcept;
    void setTransport(double tempo, double beats, bool playing) noexcept;
    void assignMacro(int index, clap_id id);
    clap_id macro(int index) const;
    void adjustMacro(int index, double normalizedDelta);
    // A relative encoder turn (fraction of the encoder range, signed) applied to any parameter; stepped parameters move by whole ticks.
    void adjustParameter(clap_id id, double normalizedDelta);
    // Looks (once per process, in the background) for plug-in binaries in the standard folders and the folders the user added, and lets
    // PluginManager::resolve prefer a valid binary of an id over the built-in of the same id. The session swaps a loaded built-in for it
    // (state kept) when the scan finishes. Idempotent; the panel's own scans feed the same list.
    static void discoverInBackground();
    static std::vector<std::filesystem::path> discoveryRoots();
    void panic() noexcept { processor.panic(); }
    juce::ValueTree stateTree() const { return state; }
private:
    void timerCallback() override { service(); }
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
    Host processor;
    juce::ValueTree state;
    juce::String statusMessage{"Choose a CLAP instrument"};
    std::atomic<bool> reload{true}, removed{false};
    std::atomic<double> requestedRate{0}, tempo{120}, beats{0};
    std::atomic<uint32_t> requestedFrames{512}, transportRevision{0};
    std::atomic<bool> playing{false};
    std::mutex transportWriters; // producers only; render never takes this lock
    double preparedRate = 0; // main thread
    uint32_t preparedFrames = 0;
    std::atomic<double> renderRate{0};
    uint32_t audioTransportRevision = 0;
    double audioBeats = 0;
    double audioTempo = 120;
    bool audioPlaying = false;
    juce::String loadedPath, loadedId;
    uint64_t seenDiscovery = 0;
    std::chrono::steady_clock::time_point snapshotDue{};
};
} // namespace kick::clap
