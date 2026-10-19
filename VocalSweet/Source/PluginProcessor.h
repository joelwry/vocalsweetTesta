#pragma once

#include <JuceHeader.h>
#include "PresetManager.h"
#include "dsp/VocalChain.h"

/** Lock-free single-producer/single-consumer sample FIFO feeding the editor's analyzer. */
class AnalyzerFifo
{
public:
    explicit AnalyzerFifo (int capacity) : fifo (capacity), buffer ((size_t) capacity, 0.0f) {}

    void push (const float* data, int num) noexcept
    {
        int s1, n1, s2, n2;
        fifo.prepareToWrite (num, s1, n1, s2, n2);   // drops samples if the UI is not reading
        if (n1 > 0) std::copy (data, data + n1, buffer.begin() + s1);
        if (n2 > 0) std::copy (data + n1, data + n1 + n2, buffer.begin() + s2);
        fifo.finishedWrite (n1 + n2);
    }

    int pull (float* dest, int num) noexcept
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead (num, s1, n1, s2, n2);
        if (n1 > 0) std::copy (buffer.begin() + s1, buffer.begin() + s1 + n1, dest);
        if (n2 > 0) std::copy (buffer.begin() + s2, buffer.begin() + s2 + n2, dest + n1);
        fifo.finishedRead (n1 + n2);
        return n1 + n2;
    }

private:
    juce::AbstractFifo fifo;
    std::vector<float> buffer;
};

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

class VocalSweetAudioProcessor : public juce::AudioProcessor
{
public:
    VocalSweetAudioProcessor();
    ~VocalSweetAudioProcessor() override = default;

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
    vs::ParamValues getParamValues() const noexcept;

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;

    // metering (audio thread -> UI)
    AtomicPeak inPeak[2], outPeak[2], compGr, deessGr, limGr;
    std::atomic<float> inRms[2] {}, outRms[2] {};
    AnalyzerFifo analyzerFifo { 16384 };
    std::atomic<int> watchdogResets { 0 };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void runChain (juce::AudioBuffer<float>& buffer, bool forceBypass);

    vs::dsp::VocalChain chain;
    std::array<std::atomic<float>*, vs::kNumParams> rawParams {};
    std::vector<float> analyzerScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalSweetAudioProcessor)
};
