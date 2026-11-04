// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AudIO_juce.h"
#include <cmath>

namespace mt {
namespace {
// The format writer owns its stream and writes the final header in its
// destructor. Capture failures through that final write and the disk flush.
class RecordingOutputStream final : public juce::FileOutputStream {
public:
    RecordingOutputStream(const juce::File& file, bool& success)
        : FileOutputStream(file, 0), written(success) {}
    ~RecordingOutputStream() override {
        flush();
        written = written && getStatus().wasOk();
    }
    bool write(const void* data, size_t bytes) override {
        const bool ok = FileOutputStream::write(data, bytes);
        written = written && ok;
        return ok;
    }
    bool setPosition(juce::int64 position) override {
        const bool ok = FileOutputStream::setPosition(position);
        written = written && ok;
        return ok;
    }
private:
    bool& written;
};
}

Recorder::Recorder(AudIO::Design& design)
    : AudIO::Object(2, 2, 0), AudIO::DawRouting(design, AudIO::BusType::Object) {
    AudIO::DawRouting::setID(getId());
    setName("Recorder");
    startTimerHz(30);
}

Recorder::~Recorder() {
    stopTimer();
    recording.store(false);
    preview.store(false);
}

void Recorder::prepareToPlay(uint32_t rate, uint32_t frames) {
    if (!rate || !frames) return;
    const juce::SpinLock::ScopedLockType lock(gate);
    audioBufferValid.store(false, std::memory_order_release);
    stop();
    const auto previousRate = getSampleRate();
    if (previousRate > 0 && previousRate != rate) {
        // Device setup runs off the render thread. Preserve take duration and
        // undo history when the driver negotiates a different sample rate.
        const auto resample = [&](juce::AudioBuffer<float>& source, int& valid) {
            if (!source.getNumSamples()) return;
            const double scale = static_cast<double>(rate) / previousRate;
            const auto count = static_cast<int>(std::llround(source.getNumSamples() * scale));
            juce::AudioBuffer<float> converted(source.getNumChannels(), count);
            for (int c = 0; c < source.getNumChannels(); ++c) {
                for (int n = 0; n < count; ++n) {
                    const double position = n / scale;
                    const auto first = juce::jmin(static_cast<int>(position), source.getNumSamples() - 1);
                    const auto next = juce::jmin(first + 1, source.getNumSamples() - 1);
                    converted.setSample(c, n, source.getSample(c, first) +
                        static_cast<float>(position - first) * (source.getSample(c, next) - source.getSample(c, first)));
                }
            }
            valid = juce::jlimit(0, count, static_cast<int>(std::llround(valid * scale)));
            source = std::move(converted);
        };
        auto valid = validFrames.load();
        resample(audio, valid);
        validFrames.store(valid);
        resample(history, historyFrames);
        writePosition = readPosition = 0;
        lastCopiedValid.store(0);
    }
    AudIO::Object::prepareToPlay(rate, frames);
    AudIO::DawInserts::prepareToPlay(rate, frames);
    audioBufferValid.store(true, std::memory_order_release);
}

void Recorder::setRecordingMode(Mode value) {
    if (value < L_MONO || value > LR_STEREO) return;
    mode.store(value);
}

void Recorder::setLength(double seconds) {
    if (!std::isfinite(seconds)) return;
    seconds = juce::jlimit(0., 600., seconds);
    {
        const juce::SpinLock::ScopedLockType lock(gate);
        audioBufferValid.store(false, std::memory_order_release);
        recording.store(false);
        preview.store(false);
        const int numChans = mode.load() <= LR_MONO ? 1 : 2;
        const int numSamps = static_cast<int>(seconds * getSampleRate());
        audio.setSize(numChans, numSamps, false, true);
        audio.clear();
        history.setSize(0, 0);
        validFrames.store(0);
        historyFrames = 0;
        length.store(seconds);
        writePosition = readPosition = 0;
        progress.store(0);
        lastCopiedValid.store(0);
        totalProcessedFrames.store(0);

        // Preallocate peak cache
        const int maxPeaks = (numSamps + PEAK_BLOCK_SIZE - 1) / PEAK_BLOCK_SIZE;
        peakCache.assign(maxPeaks > 0 ? maxPeaks : 1, WaveformPeak{0.0f, 0.0f});
        peakCount.store(0);
        peakBlockSampleCount = 0;
        currentPeakBlock = {0.0f, 0.0f};

        audioBufferValid.store(true, std::memory_order_release);
    }
    if (onRecorderLengthChanged) onRecorderLengthChanged(seconds);
}

void Recorder::startLocked(bool fromBeginning, bool skipThreshold) {
    // SA-07: History copy is only needed during overdub for undo
    if (overdub.load()) {
        history.makeCopyOf(audio);
        historyFrames = validFrames.load();
    }
    if (!overdub.load() && fromBeginning) {
        audio.clear();
        validFrames.store(0);
        lastCopiedValid.store(0);
        peakCount.store(0);
        peakBlockSampleCount = 0;
        currentPeakBlock = {0.0f, 0.0f};
    }
    if (fromBeginning || writePosition >= audio.getNumSamples() || writePosition < 0)
        writePosition = isForward() ? 0 : audio.getNumSamples() - 1;
    thresholdReached = skipThreshold || threshold.load() <= 0;
    preview.store(false);
    recording.store(audio.getNumSamples() > 0);
    if (recording.load()) notices.fetch_or(1);
}

void Recorder::start(bool fromBeginning, bool skipThreshold) {
    if (audio.getNumSamples() == 0 || audio.getNumChannels() != (mode.load() <= LR_MONO ? 1 : 2))
        setLength(length.load() > 0 ? length.load() : 30);
    const juce::SpinLock::ScopedLockType lock(gate);
    startLocked(fromBeginning, skipThreshold);
}

void Recorder::startRecording(bool fromBeginning) {
    start(fromBeginning, false);
}

void Recorder::stopRecording() {
    if (recording.exchange(false)) notices.fetch_or(2);
}

void Recorder::startPreview(double seconds) {
    recording.store(false);
    if (!std::isfinite(seconds) || validFrames.load() <= 0) return;
    readPosition = juce::jlimit(0, validFrames.load() - 1, static_cast<int>(seconds * getSampleRate()));
    if (seconds <= 0 && isReverse()) readPosition = validFrames.load() - 1;
    preview.store(true);
    notices.fetch_or(4);
}

void Recorder::stopPreview() {
    if (preview.exchange(false)) notices.fetch_or(8);
}

void Recorder::stop() {
    stopRecording();
    stopPreview();
}

void Recorder::reset() {
    const juce::SpinLock::ScopedLockType lock(gate);
    audioBufferValid.store(false, std::memory_order_release);
    stop();
    audio.clear();
    validFrames.store(0);
    lastCopiedValid.store(0);
    writePosition = readPosition = 0;
    progress.store(0);
    totalProcessedFrames.store(0);
    peakCount.store(0);
    peakBlockSampleCount = 0;
    currentPeakBlock = {0.0f, 0.0f};
    clearMarkers();
    audioBufferValid.store(true, std::memory_order_release);
}

juce::AudioBuffer<float>& Recorder::getAudioBuffer() {
    // Non-blocking, lock-free UI snapshot: never acquires gate!
    if (!audioBufferValid.load(std::memory_order_acquire))
        return view;

    const int channels = audio.getNumChannels();
    const int valid = validFrames.load(std::memory_order_acquire);

    if (channels == 0 || valid <= 0) {
        if (view.getNumSamples() != 0 || view.getNumChannels() != channels)
            view.setSize(channels, 0, false, false, true);
        lastCopiedValid.store(0, std::memory_order_relaxed);
        return view;
    }

    if (view.getNumChannels() != channels) {
        view.setSize(channels, valid, false, false, true);
        lastCopiedValid.store(0, std::memory_order_relaxed);
    }

    const int prev = lastCopiedValid.load(std::memory_order_relaxed);

    if (looping.load(std::memory_order_relaxed) || overdub.load(std::memory_order_relaxed)) {
        // In loop / overdub mode, previous samples are overwritten or accumulated.
        // Copy the active valid range without holding gate.
        view.setSize(channels, valid, false, false, true);
        for (int c = 0; c < channels; ++c) {
            view.copyFrom(c, 0, audio, c, 0, valid);
        }
        lastCopiedValid.store(valid, std::memory_order_relaxed);
    } else {
        // In linear recording mode, copy only newly appended frames incrementally!
        if (view.getNumSamples() != valid) {
            view.setSize(channels, valid, true, true, true); // preserve existing samples
        }
        if (prev < valid) {
            const int start = std::max(0, prev);
            const int count = valid - start;
            for (int c = 0; c < channels; ++c) {
                view.copyFrom(c, start, audio, c, start, count);
            }
            lastCopiedValid.store(valid, std::memory_order_relaxed);
        }
    }
    return view;
}

void Recorder::undo() {
    const juce::SpinLock::ScopedLockType lock(gate);
    audioBufferValid.store(false, std::memory_order_release);
    stop();
    if (!history.getNumSamples()) {
        audioBufferValid.store(true, std::memory_order_release);
        return;
    }
    std::swap(audio, history);
    const auto current = validFrames.exchange(historyFrames);
    historyFrames = current;
    lastCopiedValid.store(0);
    length.store(audio.getNumSamples() / getSampleRate());
    writePosition = readPosition = 0;
    progress.store(0);
    audioBufferValid.store(true, std::memory_order_release);
}

void Recorder::save(const juce::String& path) {
    stop();
    const int channels = audio.getNumChannels();
    if (channels == 0 || audio.getNumSamples() == 0) {
        error = "There is no recording to save";
        return;
    }
    const int valid = validFrames.load();
    if (valid <= 0) {
        error = "There is no recording to save";
        return;
    }

    // LO-07: Beat/bar-accurate export length
    const int exportFrames = recorder_position_logic::calculateExportFrames(
        looping.load(), valid, audio.getNumSamples(), numBars.load());
    if (exportFrames <= 0) {
        error = "There is no recording to save";
        return;
    }

    // Copy exact export frames of clean audio (LO-04)
    view.setSize(channels, exportFrames, false, false, true);
    for (int c = 0; c < channels; ++c) {
        view.copyFrom(c, 0, audio, c, 0, exportFrames);
    }

    juce::TemporaryFile temporary{juce::File(path)};
    bool written = true;
    auto output = std::make_unique<RecordingOutputStream>(temporary.getFile(), written);
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        output->openedOk()
            ? wav.createWriterFor(output.get(), getSampleRate(), static_cast<unsigned>(channels), 24, {}, 0)
            : nullptr);
    if (!writer) {
        error = "Cannot create recording file";
        return;
    }
    output.release(); // The writer now owns the stream.
    const bool audioWritten = writer->writeFromAudioSampleBuffer(view, 0, exportFrames);
    writer.reset();
    if (!audioWritten || !written || !temporary.overwriteTargetFileWithTemporary())
        error = "Cannot save recording";
    else
        error.clear();
}

void Recorder::pushMarker(int padId, int64_t frame) {
    if (frame < 0) {
        frame = totalProcessedFrames.load(std::memory_order_relaxed);
    }
    const double sec = getSampleRate() > 0 ? static_cast<double>(frame) / getSampleRate() : 0.0;
    Marker m{padId, frame, sec};

    const size_t w = markerWriteIdx.load(std::memory_order_relaxed);
    markerFifo[w % MARKER_FIFO_CAPACITY] = m;
    markerWriteIdx.store(w + 1, std::memory_order_release);
}

std::vector<Recorder::Marker> Recorder::getMarkers() {
    size_t r = markerReadIdx.load(std::memory_order_relaxed);
    const size_t w = markerWriteIdx.load(std::memory_order_acquire);
    while (r < w) {
        savedMarkers.push_back(markerFifo[r % MARKER_FIFO_CAPACITY]);
        ++r;
    }
    markerReadIdx.store(r, std::memory_order_release);
    return savedMarkers;
}

void Recorder::clearMarkers() {
    markerReadIdx.store(0, std::memory_order_relaxed);
    markerWriteIdx.store(0, std::memory_order_relaxed);
    savedMarkers.clear();
}

double Recorder::getRecordTimeSeconds() const {
    const auto rate = getSampleRate();
    if (rate <= 0) return 0.0;
    return static_cast<double>(totalProcessedFrames.load(std::memory_order_relaxed)) / rate;
}

std::vector<Recorder::WaveformPeak> Recorder::getWaveformPeaks(int maxPeaks) const {
    const int count = peakCount.load(std::memory_order_acquire);
    if (count <= 0 || peakCache.empty()) return {};
    const int total = std::min(count, static_cast<int>(peakCache.size()));
    if (maxPeaks <= 0 || total <= maxPeaks) {
        return std::vector<WaveformPeak>(peakCache.begin(), peakCache.begin() + total);
    }
    std::vector<WaveformPeak> result;
    result.reserve(maxPeaks);
    const double step = static_cast<double>(total) / maxPeaks;
    for (int i = 0; i < maxPeaks; ++i) {
        const int start = static_cast<int>(i * step);
        const int end = std::min(total, static_cast<int>((i + 1) * step));
        float minVal = 0.0f, maxVal = 0.0f;
        for (int j = start; j < end; ++j) {
            minVal = std::min(minVal, peakCache[j].minSample);
            maxVal = std::max(maxVal, peakCache[j].maxSample);
        }
        result.push_back({minVal, maxVal});
    }
    return result;
}

int Recorder::getPeakCount() const {
    return peakCount.load(std::memory_order_acquire);
}

AudIO::MeterData Recorder::readInputMeter() const {
    return {{juce::Decibels::gainToDecibels(inputRms[0].load(), -100.f), juce::Decibels::gainToDecibels(inputPeak[0].load(), -100.f)},
            {juce::Decibels::gainToDecibels(inputRms[1].load(), -100.f), juce::Decibels::gainToDecibels(inputPeak[1].load(), -100.f)}};
}

AudIO::MeterData Recorder::readOutputMeter() const {
    return {{juce::Decibels::gainToDecibels(outputRms[0].load(), -100.f), juce::Decibels::gainToDecibels(outputPeak[0].load(), -100.f)},
            {juce::Decibels::gainToDecibels(outputRms[1].load(), -100.f), juce::Decibels::gainToDecibels(outputPeak[1].load(), -100.f)}};
}

void Recorder::timerCallback() {
    viewProgress = progress.load();
    const auto rec = recording.load(), playing = preview.load();
    const auto events = notices.exchange(0);
    const auto notifyStart = [&] { if (onRecorderStarted) onRecorderStarted(); };
    const auto notifyStop = [&] {
        if (onRecorderStopped) onRecorderStopped();
        if (onRecorderLengthChanged) onRecorderLengthChanged(validFrames.load() / getSampleRate());
    };
    const auto notifyPreviewStart = [&] { if (onRecorderPreviewStarted) onRecorderPreviewStarted(); };
    const auto notifyPreviewStop = [&] { if (onRecorderPreviewStopped) onRecorderPreviewStopped(); };

    if ((events & 3) == 3) {
        if (rec) { notifyStop(); notifyStart(); } else { notifyStart(); notifyStop(); }
    } else if (events & 1) {
        notifyStart();
    } else if (events & 2) {
        notifyStop();
    } else if (rec != hadRecording) {
        if (rec) notifyStart(); else notifyStop();
    }
    hadRecording = rec;

    if (events & 4) notifyPreviewStart();
    if (events & 8) notifyPreviewStop();
    if (playing != hadPreview) {
        if (playing) notifyPreviewStart(); else notifyPreviewStop();
        hadPreview = playing;
    }
}

void Recorder::process(AudIO::Buffer<float>& buffer) {
    // Non-blocking RT execution: NEVER try-locks gate or drops blocks due to snapshots!
    if (!audioBufferValid.load(std::memory_order_acquire) || !enabled.load(std::memory_order_relaxed)) {
        buffer.clearOutputs();
        return;
    }

    const auto frames = buffer.getBlockSize();
    const auto activeMode = mode.load(std::memory_order_relaxed);
    const auto g = gain.load(std::memory_order_relaxed);
    const auto p = pan.load(std::memory_order_relaxed);
    const std::array<float, 2> gains{g * (p > 0 ? 1 - p : 1), g * (p < 0 ? 1 + p : 1)};
    const auto dir = isForward() ? recorder_position_logic::Direction::Forward : recorder_position_logic::Direction::Reverse;

    std::array<double, 2> inSum{}, outSum{};
    std::array<float, 2> inMax{}, outMax{};

    float blockMin = currentPeakBlock.minSample;
    float blockMax = currentPeakBlock.maxSample;
    int blockCount = peakBlockSampleCount;
    int pIdx = peakCount.load(std::memory_order_relaxed);

    for (uint32_t n = 0; n < frames; ++n) {
        std::array<float, 2> input{}, recorded{}, output{};
        for (int c = 0; c < 2; ++c) {
            const auto* source = buffer.getInputBuffer(c);
            input[c] = source && std::isfinite(source[n]) ? source[n] : 0.0f;
            inSum[c] += input[c] * input[c];
            inMax[c] = std::max(inMax[c], std::abs(input[c]));
        }

        // LO-04: Clean input selection without multiplying by playback gains
        switch (activeMode) {
            case L_MONO: case L_STEREO: recorded = {input[0], input[0]}; break;
            case R_MONO: case R_STEREO: recorded = {input[1], input[1]}; break;
            case LR_MONO: recorded[0] = recorded[1] = (input[0] + input[1]) * 0.5f; break;
            default: recorded = {input[0], input[1]}; break;
        }

        // Monitor routing: clean input scaled by monitorGain
        if (monitor.load(std::memory_order_relaxed)) {
            const float mGain = monitorGain.load(std::memory_order_relaxed);
            for (int c = 0; c < 2; ++c) output[c] = recorded[c] * mGain;
        }

        if (recording.load(std::memory_order_relaxed) && audio.getNumSamples() > 0) {
            if (!thresholdReached && std::max(std::abs(input[0]), std::abs(input[1])) >= threshold.load(std::memory_order_relaxed))
                thresholdReached = true;

            if (thresholdReached) {
                const bool isOverdub = overdub.load(std::memory_order_relaxed);

                // LO-01: During overdub, play back existing audio so performer hears the loop!
                if (isOverdub) {
                    for (int c = 0; c < 2; ++c) {
                        const float loopSample = audio.getSample(std::min(c, audio.getNumChannels() - 1), writePosition);
                        // LO-04: Apply gain and pan once on playback
                        output[c] += loopSample * gains[c];
                    }
                }

                // Record into audio buffer:
                for (int c = 0; c < audio.getNumChannels(); ++c) {
                    const float prevSample = isOverdub ? audio.getSample(c, writePosition) : 0.0f;
                    const float newSample = isOverdub
                        ? recorder_position_logic::softLimit(prevSample + recorded[c])
                        : recorded[c];
                    audio.setSample(c, writePosition, newSample);
                    blockMin = std::min(blockMin, newSample);
                    blockMax = std::max(blockMax, newSample);
                }

                const int nextValid = std::max(validFrames.load(std::memory_order_relaxed), writePosition + 1);
                validFrames.store(nextValid, std::memory_order_release);

                writePosition += isForward() ? 1 : -1;
                totalProcessedFrames.fetch_add(1, std::memory_order_relaxed);

                if (recorder_position_logic::isAtWrapBoundary(writePosition, audio.getNumSamples(), dir)) {
                    auto transition = recorder_position_logic::handleRecordWrap(
                        looping.load(std::memory_order_relaxed),
                        oneShotRecord.load(std::memory_order_relaxed),
                        validFrames.load(std::memory_order_relaxed),
                        audio.getNumSamples(),
                        dir
                    );
                    writePosition = transition.nextWritePos;
                    readPosition = transition.nextReadPos;
                    validFrames.store(transition.validFrames, std::memory_order_release);
                    if (!transition.keepRecording) {
                        recording.store(false, std::memory_order_release);
                        notices.fetch_or(2); // onRecorderStopped
                    }
                    if (transition.startPreview) {
                        preview.store(true, std::memory_order_release);
                        notices.fetch_or(4); // onRecorderPreviewStarted
                    }
                }
            }
        } else if (preview.load(std::memory_order_relaxed) && validFrames.load(std::memory_order_relaxed) > 0) {
            const int totalValid = validFrames.load(std::memory_order_relaxed);
            for (int c = 0; c < 2; ++c) {
                const float s = audio.getSample(std::min(c, audio.getNumChannels() - 1), readPosition);
                // LO-04: Apply gain and pan once on playback
                output[c] += s * gains[c];
                blockMin = std::min(blockMin, s);
                blockMax = std::max(blockMax, s);
            }
            readPosition += isForward() ? 1 : -1;
            totalProcessedFrames.fetch_add(1, std::memory_order_relaxed);

            if (recorder_position_logic::isAtWrapBoundary(readPosition, totalValid, dir)) {
                auto transition = recorder_position_logic::handlePreviewWrap(
                    looping.load(std::memory_order_relaxed),
                    totalValid,
                    dir
                );
                readPosition = transition.nextReadPos;
                if (!transition.startPreview) {
                    preview.store(false, std::memory_order_release);
                    notices.fetch_or(8); // onRecorderPreviewStopped
                }
            }
        }

        // Peak cache tracking (SA-07)
        ++blockCount;
        if (blockCount >= PEAK_BLOCK_SIZE) {
            if (pIdx < static_cast<int>(peakCache.size())) {
                peakCache[pIdx] = {blockMin, blockMax};
                peakCount.store(pIdx + 1, std::memory_order_release);
                ++pIdx;
            }
            blockMin = 0.0f;
            blockMax = 0.0f;
            blockCount = 0;
        }

        for (int c = 0; c < 2; ++c) {
            if (auto* target = buffer.getOutputBuffer(c)) target[n] = output[c];
            outSum[c] += output[c] * output[c];
            outMax[c] = std::max(outMax[c], std::abs(output[c]));
        }
    }

    currentPeakBlock = {blockMin, blockMax};
    peakBlockSampleCount = blockCount;

    AudIO::DawInserts::process(buffer);

    for (int c = 0; c < 2; ++c) {
        inputRms[c].store(frames ? static_cast<float>(std::sqrt(inSum[c] / frames)) : 0.0f);
        inputPeak[c].store(inMax[c]);
        outputRms[c].store(frames ? static_cast<float>(std::sqrt(outSum[c] / frames)) : 0.0f);
        outputPeak[c].store(outMax[c]);
    }

    const auto at = preview.load(std::memory_order_relaxed) ? readPosition : writePosition;
    progress.store(recorder_position_logic::calculateProgress(at, audio.getNumSamples()));
}

} // namespace mt
