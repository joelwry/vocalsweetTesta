#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::String SwitaxVoiceAudioProcessor::formatValue (const sx::ParamDef& d, float v)
{
    const juce::String unit (d.unit);
    if (unit == "Hz")
        return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz"
                            : juce::String (juce::roundToInt (v)) + " Hz";
    if (unit == "dB") return std::abs (v) < 0.05f ? juce::String ("0.0 dB") : (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB";
    if (unit == "ms") return juce::String (v, v < 10.0f ? 1 : 0) + " ms";
    if (unit == ":1") return juce::String (v, 1) + ":1";
    if (unit == "%")  return juce::String (juce::roundToInt (v)) + "%";
    return juce::String (v, 1);
}

juce::AudioProcessorValueTreeState::ParameterLayout SwitaxVoiceAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& d : sx::kParamDefs)
    {
        const juce::ParameterID pid { d.id, 1 };
        switch (d.kind)
        {
            case sx::ParamKind::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, d.name, d.def > 0.5f));
                break;
            case sx::ParamKind::Choice:
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, d.name, juce::StringArray { "12 dB/oct", "24 dB/oct" }, (int) d.def));
                break;
            case sx::ParamKind::Float:
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
SwitaxVoiceAudioProcessor::SwitaxVoiceAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Beat (sidechain)", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "SwitaxVoiceState", createParameterLayout()),
      presets (apvts)
{
    for (int i = 0; i < sx::kNumParams; ++i)
    {
        rawParams[(size_t) i] = apvts.getRawParameterValue (sx::kParamDefs[(size_t) i].id);
        jassert (rawParams[(size_t) i] != nullptr);
        auditionValues[(size_t) i].store (sx::kParamDefs[(size_t) i].def);
    }
    apvts.state.setProperty ("presetName", "Clean Start", nullptr);
}

bool SwitaxVoiceAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (in != out) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void SwitaxVoiceAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    chain.prepare (sampleRate, samplesPerBlock);
    chain.setParams (getParamValues());
    chain.reset();   // snap all smoothers to the current parameter values
    setLatencySamples (chain.getLatencySamples());
    analysis.prepare (sampleRate, juce::jmax (1, getMainBusNumInputChannels()));
}

sx::ParamValues SwitaxVoiceAudioProcessor::getParamValues() const noexcept
{
    sx::ParamValues v {};
    for (int i = 0; i < sx::kNumParams; ++i)
        v[(size_t) i] = sx::clampParam (i, rawParams[(size_t) i]->load (std::memory_order_relaxed));
    return v;
}

void SwitaxVoiceAudioProcessor::startAudition (const sx::ParamValues& values)
{
    for (int i = 0; i < sx::kNumParams; ++i)
        auditionValues[(size_t) i].store (sx::clampParam (i, values[(size_t) i]), std::memory_order_relaxed);
    auditionActive.store (true, std::memory_order_release);
}

void SwitaxVoiceAudioProcessor::stopAudition() { auditionActive.store (false, std::memory_order_release); }

void SwitaxVoiceAudioProcessor::setCompare (bool on, float gainDb)
{
    compareGainDb.store (juce::jlimit (-24.0f, 24.0f, gainDb));
    compareActive.store (on);
}

void SwitaxVoiceAudioProcessor::applyValues (const sx::ParamValues& values) { presets.applyValues (values); }

void SwitaxVoiceAudioProcessor::runChain (juce::AudioBuffer<float>& buffer, bool forceBypass)
{
    juce::ScopedNoDenormals noDenormals;

    const int numMain = juce::jmin (2, getMainBusNumOutputChannels());
    const int n = buffer.getNumSamples();
    // clear any output channels that have no matching input
    for (int ch = getMainBusNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        if (ch < buffer.getNumChannels()) buffer.clear (ch, 0, n);
    if (n == 0 || numMain <= 0) return;

    sx::ParamValues values;
    if (auditionActive.load (std::memory_order_acquire))
        for (int i = 0; i < sx::kNumParams; ++i) values[(size_t) i] = auditionValues[(size_t) i].load (std::memory_order_relaxed);
    else
        values = getParamValues();
    if (forceBypass) values[sx::pBypass] = 1.0f;
    const bool comparing = compareActive.load();
    if (comparing) values[sx::pBypass] = 1.0f;
    chain.setBypassGainDb (comparing ? compareGainDb.load() : 0.0f);
    chain.setParams (values);

    // sidechain (beat), if the host enabled the bus
    const float* scL = nullptr;
    const float* scR = nullptr;
    if (getBusCount (true) > 1)
        if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled())
        {
            auto sc = getBusBuffer (buffer, true, 1);
            if (sc.getNumChannels() > 0)
            {
                scL = sc.getReadPointer (0);
                scR = sc.getNumChannels() > 1 ? sc.getReadPointer (1) : scL;
            }
        }

    float* io[2] = { buffer.getWritePointer (0), buffer.getWritePointer (numMain > 1 ? 1 : 0) };
    float compMax = 0, deessMax = 0, limMax = 0;
    for (int start = 0; start < n; start += kChunk)
    {
        const int len = juce::jmin (kChunk, n - start);
        // keep a copy of the input for the analyser (preallocated, no allocation here)
        for (int ch = 0; ch < numMain; ++ch) inScratch.copyFrom (ch, 0, io[ch] + start, len);
        float* chunkPtr[2] = { io[0] + start, io[1] + start };
        chain.process (chunkPtr, numMain, len);

        const auto& m = chain.getMeters();
        for (int ch = 0; ch < 2; ++ch)
        {
            inPeak[ch].update (m.inPeak[ch]);
            outPeak[ch].update (m.outPeak[ch]);
            inRms[ch].store (m.inRms[ch], std::memory_order_relaxed);
            outRms[ch].store (m.outRms[ch], std::memory_order_relaxed);
        }
        compMax = juce::jmax (compMax, m.compGrDb);
        deessMax = juce::jmax (deessMax, m.deessGrDb);
        limMax = juce::jmax (limMax, m.limGrDb);

        const float* inL = inScratch.getReadPointer (0);
        const float* inR = inScratch.getReadPointer (numMain > 1 ? 1 : 0);
        analysis.push (inL, inR, chunkPtr[0], numMain > 1 ? chunkPtr[1] : chunkPtr[0],
                       scL != nullptr ? scL + start : nullptr, scR != nullptr ? scR + start : nullptr, len);
    }
    compGr.update (compMax);
    deessGr.update (deessMax);
    limGr.update (limMax);
    watchdogResets.store (chain.getWatchdogResets(), std::memory_order_relaxed);
}

void SwitaxVoiceAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    runChain (buffer, false);
}

void SwitaxVoiceAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // host-side bypass: keep the latency-compensated dry path running so timing stays aligned
    runChain (buffer, true);
}

juce::AudioProcessorParameter* SwitaxVoiceAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

//==============================================================================
int SwitaxVoiceAudioProcessor::getNumPrograms() { return juce::jmax (1, presets.getNumPresets()); }
int SwitaxVoiceAudioProcessor::getCurrentProgram() { return presets.getCurrentIndex(); }
void SwitaxVoiceAudioProcessor::setCurrentProgram (int index) { presets.loadPreset (index); }
const juce::String SwitaxVoiceAudioProcessor::getProgramName (int index) { return presets.getPresetName (index); }

void SwitaxVoiceAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presets.getCurrentName(), nullptr);
    state.setProperty ("pluginVersion", JucePlugin_VersionString, nullptr);
    state.setProperty ("assistContext", (int) assist.context, nullptr);
    state.setProperty ("assistTarget", assist.genre, nullptr);
    state.setProperty ("assistSeconds", assist.captureSeconds, nullptr);
    state.setProperty ("uiScale", (double) uiScale.load(), nullptr);
    state.setProperty ("referenceFile", analysis.getReferenceFile().getFullPathName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SwitaxVoiceAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    auto tree = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (tree);

    // APVTS skips parameters whose *plain* value did not change; re-assert every value so
    // getValue() matches the state.
    for (int i = 0; i < sx::kNumParams; ++i)
        if (auto* p = apvts.getParameter (sx::kParamDefs[(size_t) i].id))
        {
            const float norm = p->convertTo0to1 (rawParams[(size_t) i]->load());
            if (std::abs (p->getValue() - norm) > 1.0e-6f) p->setValueNotifyingHost (norm);
        }
    presets.setCurrentFromState (tree.getProperty ("presetName").toString());

    assist.context = (sx::analysis::Context) juce::jlimit (0, (int) sx::analysis::Context::kNumContexts - 1,
                                                            (int) tree.getProperty ("assistContext", 0));
    assist.genre = juce::jlimit (-1, (int) sx::analysis::Genre::kNumGenres - 1, (int) tree.getProperty ("assistTarget", 0));
    assist.captureSeconds = juce::jlimit (5.0, 60.0, (double) tree.getProperty ("assistSeconds", 20.0));
    uiScale.store ((float) juce::jlimit (0.0, 2.0, (double) tree.getProperty ("uiScale", 0.0)));
    const juce::String ref = tree.getProperty ("referenceFile").toString();
    if (ref.isNotEmpty() && juce::File::isAbsolutePath (ref) && juce::File (ref).existsAsFile())
    {
        if (juce::File (ref) != analysis.getReferenceFile()) analysis.loadReference (juce::File (ref));
    }
    else if (assist.genre < 0)
    {
        assist.genre = 0;   // reference missing on this machine: fall back to a house target
    }
}

juce::AudioProcessorEditor* SwitaxVoiceAudioProcessor::createEditor()
{
    return new SwitaxVoiceAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SwitaxVoiceAudioProcessor();
}
