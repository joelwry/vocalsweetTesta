#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::String formatValue (const vs::ParamDef& d, float v)
{
    const juce::String unit (d.unit);
    if (unit == "Hz")
        return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz"
                            : juce::String (juce::roundToInt (v)) + " Hz";
    if (unit == "dB") return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB";
    if (unit == "ms") return juce::String (v, v < 10.0f ? 1 : 0) + " ms";
    if (unit == ":1") return juce::String (v, 1) + ":1";
    if (unit == "%")  return juce::String (juce::roundToInt (v)) + "%";
    if (juce::String (d.id) == "tone")
    {
        const int t = juce::roundToInt (v);
        return t == 0 ? juce::String ("Neutral") : (t < 0 ? "Dark " + juce::String (-t) : "Bright " + juce::String (t));
    }
    return juce::String (v, 1);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout VocalSweetAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& d : vs::kParamDefs)
    {
        const juce::ParameterID pid { d.id, 1 };
        switch (d.kind)
        {
            case vs::ParamKind::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, d.name, d.def > 0.5f));
                break;

            case vs::ParamKind::Choice:
                layout.add (std::make_unique<juce::AudioParameterChoice> (
                    pid, d.name, juce::StringArray { "12 dB/oct", "24 dB/oct" }, (int) d.def));
                break;

            case vs::ParamKind::Float:
            {
                juce::NormalisableRange<float> range (d.min, d.max);
                if (d.centre > d.min && d.centre < d.max) range.setSkewForCentre (d.centre);
                auto def = d;
                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    pid, d.name, range, d.def,
                    juce::AudioParameterFloatAttributes()
                        .withLabel (d.unit)
                        .withStringFromValueFunction ([def] (float v, int) { return formatValue (def, v); })));
                break;
            }
        }
    }
    return layout;
}

//==============================================================================
VocalSweetAudioProcessor::VocalSweetAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VocalSweetState", createParameterLayout()),
      presets (apvts)
{
    for (int i = 0; i < vs::kNumParams; ++i)
    {
        rawParams[(size_t) i] = apvts.getRawParameterValue (vs::kParamDefs[(size_t) i].id);
        jassert (rawParams[(size_t) i] != nullptr);
    }
    apvts.state.setProperty ("presetName", "Default", nullptr);
}

bool VocalSweetAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == out;
}

void VocalSweetAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    chain.prepare (sampleRate, samplesPerBlock);
    chain.setParams (vs::computeEffective (getParamValues()));
    chain.reset();   // snap all smoothers to the current parameter values
    setLatencySamples (chain.getLatencySamples());
    analyzerScratch.assign ((size_t) juce::jmax (samplesPerBlock, 32) * 2, 0.0f);
}

vs::ParamValues VocalSweetAudioProcessor::getParamValues() const noexcept
{
    vs::ParamValues v {};
    for (int i = 0; i < vs::kNumParams; ++i)
        v[(size_t) i] = vs::clampParam (i, rawParams[(size_t) i]->load (std::memory_order_relaxed));
    return v;
}

void VocalSweetAudioProcessor::runChain (juce::AudioBuffer<float>& buffer, bool forceBypass)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    for (int ch = numIn; ch < numOut; ++ch) buffer.clear (ch, 0, n);
    if (n == 0) return;

    auto values = getParamValues();
    if (forceBypass) values[vs::pBypass] = 1.0f;
    chain.setParams (vs::computeEffective (values));

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    chain.process (buffer.getArrayOfWritePointers(), numCh, n);

    const auto& m = chain.getMeters();
    for (int ch = 0; ch < 2; ++ch)
    {
        inPeak[ch].update (m.inPeak[ch]);
        outPeak[ch].update (m.outPeak[ch]);
        inRms[ch].store (m.inRms[ch], std::memory_order_relaxed);
        outRms[ch].store (m.outRms[ch], std::memory_order_relaxed);
    }
    compGr.update (m.compGrDb);
    deessGr.update (m.deessGrDb);
    limGr.update (m.limGrDb);
    watchdogResets.store (chain.getWatchdogResets(), std::memory_order_relaxed);

    // mono sum of the output for the analyzer (scratch is preallocated; chunk if host block grows)
    const float* l = buffer.getReadPointer (0);
    const float* r = numCh > 1 ? buffer.getReadPointer (1) : l;
    const int cap = (int) analyzerScratch.size();
    for (int start = 0; start < n && cap > 0; start += cap)
    {
        const int len = juce::jmin (cap, n - start);
        for (int i = 0; i < len; ++i) analyzerScratch[(size_t) i] = 0.5f * (l[start + i] + r[start + i]);
        analyzerFifo.push (analyzerScratch.data(), len);
    }
}

void VocalSweetAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    runChain (buffer, false);
}

void VocalSweetAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // host-side bypass: keep the latency-compensated dry path running so timing stays aligned
    runChain (buffer, true);
}

juce::AudioProcessorParameter* VocalSweetAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

//==============================================================================
int VocalSweetAudioProcessor::getNumPrograms() { return juce::jmax (1, presets.getNumPresets()); }
int VocalSweetAudioProcessor::getCurrentProgram() { return presets.getCurrentIndex(); }
void VocalSweetAudioProcessor::setCurrentProgram (int index) { presets.loadPreset (index); }
const juce::String VocalSweetAudioProcessor::getProgramName (int index) { return presets.getPresetName (index); }

void VocalSweetAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presets.getCurrentName(), nullptr);
    state.setProperty ("pluginVersion", JucePlugin_VersionString, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalSweetAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    auto tree = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (tree);

    // APVTS skips parameters whose *plain* value did not change (e.g. a bool already "on" but
    // currently holding normalised 0.54). Re-assert every value so getValue() matches the state.
    for (int i = 0; i < vs::kNumParams; ++i)
    {
        if (auto* p = apvts.getParameter (vs::kParamDefs[(size_t) i].id))
        {
            const float norm = p->convertTo0to1 (rawParams[(size_t) i]->load());
            if (std::abs (p->getValue() - norm) > 1.0e-6f)
                p->setValueNotifyingHost (norm);
        }
    }
    presets.setCurrentFromState (tree.getProperty ("presetName").toString());
}

juce::AudioProcessorEditor* VocalSweetAudioProcessor::createEditor()
{
    return new VocalSweetAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalSweetAudioProcessor();
}
