#pragma once

#include <JuceHeader.h>
#include "AnalysisEngine.h"
#include "PresetManager.h"
#include "core/Chain.h"

/** Peak-since-last-read meter value written by the audio thread, consumed by the UI. */
struct AtomicPeak
{
    std::atomic<float> value { 0.0f };
    void update (float v) noexcept
    {
        float prev = value.load (std::memory_order_relaxed);
        while (v > prev && ! value.compare_exchange_weak (prev, v, std::memory_order_relaxed)) {}
    }
    float read() noexcept { return value.exchange (0.0f, std::memory_order_relaxed); }
};

class SwitaxVoiceAudioProcessor : public juce::AudioProcessor
{
public:
    SwitaxVoiceAudioProcessor();
    ~SwitaxVoiceAudioProcessor() override = default;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    //==========================================================================
    /** Current plain parameter values (thread-safe snapshot of the atomics). */
    sx::ParamValues getParamValues() const noexcept;

    /** Audition: the audio thread uses `values` instead of the parameters until stopped.
        Message thread only. Parameters (and the host's automation) are not touched. */
    void startAudition (const sx::ParamValues& values);
    void stopAudition();
    bool isAuditioning() const noexcept { return auditionActive.load(); }

    /** Level-matched compare: hear the unprocessed input, gain-matched to the processed output
        by `gainDb` (measured loudness difference). */
    void setCompare (bool on, float gainDb);
    bool isComparing() const noexcept { return compareActive.load(); }

    /** Applies a full set of values as one undoable host gesture per parameter. */
    void applyValues (const sx::ParamValues& values);

    /** Editor size as a fraction of the design size (0 = not chosen yet). Saved in the state. */
    std::atomic<float> uiScale { 0.0f };

    // Assist settings (stored in the plugin state)
    AnalysisEngine::AssistSettings assist;

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;
    AnalysisEngine analysis;

    // metering (audio thread -> UI)
    AtomicPeak inPeak[2], outPeak[2], compGr, deessGr, limGr;
    std::atomic<float> inRms[2] {}, outRms[2] {};
    std::atomic<int> watchdogResets { 0 };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::String formatValue (const sx::ParamDef& d, float v);

private:
    void runChain (juce::AudioBuffer<float>& buffer, bool forceBypass);

    sx::dsp::Chain chain;
    std::array<std::atomic<float>*, sx::kNumParams> rawParams {};
    std::array<std::atomic<float>, sx::kNumParams> auditionValues {};
    std::atomic<bool> auditionActive { false }, compareActive { false };
    std::atomic<float> compareGainDb { 0.0f };

    static constexpr int kChunk = 1024;
    juce::AudioBuffer<float> inScratch { 2, kChunk };
    std::vector<float> silence = std::vector<float> ((size_t) kChunk, 0.0f);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwitaxVoiceAudioProcessor)
};
