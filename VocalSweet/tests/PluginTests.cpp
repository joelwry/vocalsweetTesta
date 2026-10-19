// Plugin-level tests: runs the real VocalSweetAudioProcessor / editor classes inside a minimal
// in-process host. Usage: VocalSweetPluginTests <outputDir> [--snapshots]
// --snapshots renders the real editor (fed with live audio) to PNG files.

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../tests/TestUtils.h"

namespace
{
vt::Registry R;

std::unique_ptr<VocalSweetAudioProcessor> makeProcessor (double sr, int block, int channels)
{
    auto p = std::make_unique<VocalSweetAudioProcessor>();
    const auto set = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (set);
    layout.outputBuses.add (set);
    p->setBusesLayout (layout);
    p->setRateAndBufferSizeDetails (sr, block);
    p->prepareToPlay (sr, block);
    return p;
}

void setParam (VocalSweetAudioProcessor& p, const char* id, float plain)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (plain));
}

float getParam (VocalSweetAudioProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }

juce::AudioBuffer<float> toJuce (const vt::Buffer& b)
{
    juce::AudioBuffer<float> out ((int) b.size(), (int) b[0].size());
    for (int c = 0; c < out.getNumChannels(); ++c) out.copyFrom (c, 0, b[(size_t) c].data(), out.getNumSamples());
    return out;
}

void runBlocks (VocalSweetAudioProcessor& p, juce::AudioBuffer<float>& buf, int block, std::function<void (int)> perBlock = {})
{
    juce::MidiBuffer midi;
    for (int start = 0, k = 0; start < buf.getNumSamples(); start += block, ++k)
    {
        const int len = juce::jmin (block, buf.getNumSamples() - start);
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), buf.getNumChannels(), start, len);
        p.processBlock (view, midi);
        if (perBlock) perBlock (k);
    }
}

//==============================================================================
void testParametersAndLayouts()
{
    auto p = makeProcessor (48000, 512, 2);
    int found = 0; bool defaultsOk = true;
    for (const auto& d : vs::kParamDefs)
    {
        if (auto* raw = p->apvts.getRawParameterValue (d.id))
        {
            ++found;
            if (std::abs (raw->load() - d.def) > 1e-4f) defaultsOk = false;
        }
    }
    R.check ("All parameters registered with host-visible IDs", found == vs::kNumParams && (int) p->getParameters().size() == vs::kNumParams,
             std::to_string (found) + " / " + std::to_string (vs::kNumParams) + " parameters");
    R.check ("Parameter defaults match specification", defaultsOk, "all defaults verified");

    auto* hpf = p->apvts.getParameter ("hpfFreq");
    auto* thr = p->apvts.getParameter ("compThresh");
    const auto t1 = hpf->getText (hpf->convertTo0to1 (80.0f), 32), t2 = thr->getText (thr->convertTo0to1 (-18.0f), 32);
    R.check ("Parameter value text for automation lanes", t1 == "80 Hz" && t2 == "-18.0 dB", (t1 + ", " + t2).toStdString());

    juce::AudioProcessor::BusesLayout mono, stereo, monoToStereo, quad;
    mono.inputBuses.add (juce::AudioChannelSet::mono()); mono.outputBuses.add (juce::AudioChannelSet::mono());
    stereo.inputBuses.add (juce::AudioChannelSet::stereo()); stereo.outputBuses.add (juce::AudioChannelSet::stereo());
    monoToStereo.inputBuses.add (juce::AudioChannelSet::mono()); monoToStereo.outputBuses.add (juce::AudioChannelSet::stereo());
    quad.inputBuses.add (juce::AudioChannelSet::quadraphonic()); quad.outputBuses.add (juce::AudioChannelSet::quadraphonic());
    R.check ("Bus layouts: mono & stereo accepted, others rejected",
             p->checkBusesLayoutSupported (mono) && p->checkBusesLayoutSupported (stereo) && ! p->checkBusesLayoutSupported (monoToStereo)
                 && ! p->checkBusesLayoutSupported (quad),
             "mono->mono, stereo->stereo");
    R.check ("Host bypass parameter exposed", p->getBypassParameter() == p->apvts.getParameter ("bypass"), "getBypassParameter() == 'bypass'");
}

void testProcessing()
{
    bool ok = true; std::string detail;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (int block : { 64, 512, 2048 })
            for (int ch : { 1, 2 })
            {
                auto p = makeProcessor (sr, block, ch);
                p->setCurrentProgram (1);
                auto buf = toJuce (vt::syntheticVocal (sr, 1.0, ch, 0.5f));
                runBlocks (*p, buf, block);
                bool finite = true; float pk = 0;
                for (int c = 0; c < ch; ++c) for (int i = 0; i < buf.getNumSamples(); ++i)
                { const float s = buf.getSample (c, i); finite &= std::isfinite (s); pk = std::max (pk, std::abs (s)); }
                const int expectedLat = (int) std::lround (0.001 * sr);
                if (! finite || pk > 0.8914f || p->getLatencySamples() != expectedLat) { ok = false; detail = vt::fmt ("failed sr %.0f block %.0f", sr, block); }
            }
    R.check ("processBlock: 3 rates x 3 blocks x mono/stereo", ok, ok ? "finite, peak <= -1 dBFS, latency = 1 ms reported to host" : detail);

    // variable host block sizes (as FL Studio may deliver)
    auto p = makeProcessor (44100, 1024, 2);
    auto buf = toJuce (vt::syntheticVocal (44100, 2.0, 2, 0.5f));
    juce::MidiBuffer midi;
    juce::Random rnd (42);
    int pos = 0, calls = 0;
    while (pos < buf.getNumSamples())
    {
        const int len = juce::jmin (1 + rnd.nextInt (1024), buf.getNumSamples() - pos);
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, pos, len);
        p->processBlock (view, midi);
        pos += len; ++calls;
    }
    bool finite = true;
    for (int c = 0; c < 2; ++c) for (int i = 0; i < buf.getNumSamples(); ++i) finite &= std::isfinite (buf.getSample (c, i));
    R.check ("Randomly varying block sizes (1..1024)", finite, std::to_string (calls) + " processBlock calls, all finite");

    // NaN through the plugin wrapper
    auto q = makeProcessor (48000, 256, 2);
    juce::AudioBuffer<float> nb (2, 256);
    nb.clear();
    nb.setSample (0, 10, std::numeric_limits<float>::quiet_NaN());
    nb.setSample (1, 20, std::numeric_limits<float>::infinity());
    q->processBlock (nb, midi);
    bool f2 = true; for (int c = 0; c < 2; ++c) for (int i = 0; i < 256; ++i) f2 &= std::isfinite (nb.getSample (c, i));
    R.check ("NaN / Inf from host buffer sanitised", f2, "output finite");

    // processBlockBypassed must be latency-aligned dry
    auto b = makeProcessor (48000, 256, 2);
    auto src = toJuce (vt::syntheticVocal (48000, 0.5, 2, 0.3f));
    auto work = src;
    for (int start = 0; start < work.getNumSamples(); start += 256)
    {
        juce::AudioBuffer<float> view (work.getArrayOfWritePointers(), 2, start, juce::jmin (256, work.getNumSamples() - start));
        b->processBlockBypassed (view, midi);
    }
    const int L = b->getLatencySamples();
    double err = 0;
    for (int c = 0; c < 2; ++c) for (int i = L + 2000; i < work.getNumSamples(); ++i) err = std::max (err, (double) std::abs (work.getSample (c, i) - src.getSample (c, i - L)));
    R.check ("Host bypass (processBlockBypassed) = delayed dry", err == 0.0, vt::fmt ("max |error| %.2e", err));
}

void testStateAndPresets()
{
    auto a = makeProcessor (48000, 512, 2);
    juce::Random rnd (7);
    for (int i = 0; i < vs::kNumParams; ++i)
        if (auto* param = a->apvts.getParameter (vs::kParamDefs[(size_t) i].id))
            param->setValueNotifyingHost (rnd.nextFloat());
    a->presets.loadPreset (3);
    setParam (*a, "sweeten", 77.0f);
    setParam (*a, "tone", -33.0f);

    juce::MemoryBlock state;
    a->getStateInformation (state);
    auto b = makeProcessor (48000, 512, 2);
    b->setStateInformation (state.getData(), (int) state.getSize());
    int mismatches = 0;
    for (const auto& d : vs::kParamDefs)
        if (std::abs (getParam (*a, d.id) - getParam (*b, d.id)) > 1e-5f) ++mismatches;
    R.check ("State save -> restore (all parameters)", mismatches == 0 && b->presets.getCurrentName() == a->presets.getCurrentName(),
             std::to_string (state.getSize()) + " bytes, " + std::to_string (mismatches) + " mismatches, preset '" + b->presets.getCurrentName().toStdString() + "'");

    // corrupt / foreign state must be ignored safely
    auto c = makeProcessor (48000, 512, 2);
    const char junk[] = "this is not a VocalSweet state";
    c->setStateInformation (junk, (int) sizeof (junk));
    juce::MemoryBlock empty;
    c->setStateInformation (empty.getData(), 0);
    R.check ("Corrupt / empty state ignored safely", std::abs (getParam (*c, "sweeten") - 40.0f) < 1e-4f, "defaults retained");

    // factory programs
    const auto& factory = vs::factoryPresets();
    bool progOk = a->getNumPrograms() >= (int) factory.size();
    juce::StringArray names;
    for (int i = 0; i < (int) factory.size(); ++i)
    {
        a->setCurrentProgram (i);
        names.add (a->getProgramName (i));
        for (int k = 0; k < vs::kNumParams; ++k)
        {
            if (k == vs::pBypass) continue;
            const float want = factory[(size_t) i].values[(size_t) k];
            const float got = getParam (*a, vs::kParamDefs[(size_t) k].id);
            if (std::abs (want - got) > 0.02f * std::max (1.0f, std::abs (want))) progOk = false;
        }
        progOk &= a->getCurrentProgram() == i;
    }
    R.check ("Factory presets load exact values via host programs", progOk, names.joinIntoString (", ").toStdString());

    // user preset save / reload / delete
    auto& pm = a->presets;
    a->setCurrentProgram (1);
    setParam (*a, "warmth", 88.0f);
    const int idx = pm.saveUserPreset ("zz UnitTest Preset");
    setParam (*a, "warmth", 5.0f);
    const bool loaded = idx >= 0 && pm.loadPreset (idx);
    const float w = getParam (*a, "warmth");
    const auto file = PresetManager::getUserPresetDirectory().getChildFile ("zz UnitTest Preset.vspreset");
    const bool existed = file.existsAsFile();
    const bool deleted = pm.deleteUserPreset (idx);
    R.check ("User preset save / load / delete", loaded && std::abs (w - 88.0f) < 0.01f && existed && deleted,
             ("saved to " + file.getFullPathName() + ", warmth restored to " + juce::String (w, 1)).toStdString());
}

void testAutomationSmoothing()
{
    auto p = makeProcessor (48000, 128, 2);
    juce::AudioBuffer<float> buf (2, 48000);
    for (int i = 0; i < buf.getNumSamples(); ++i) buf.setSample (0, i, 0.3f * std::sin (2.0f * 3.14159265f * 250.0f * (float) i / 48000.0f)), buf.setSample (1, i, buf.getSample (0, i));
    runBlocks (*p, buf, 128, [&p] (int k)
    {
        // host automation: toggle SWEETEN and CONTROL between extremes every 10 blocks
        if (k % 10 == 0) { setParam (*p, "sweeten", (k / 10) % 2 ? 100.0f : 0.0f); setParam (*p, "control", (k / 10) % 2 ? 0.0f : 100.0f); }
    });
    double maxStep = 0;
    for (int i = 2000; i < buf.getNumSamples(); ++i) maxStep = std::max (maxStep, (double) std::abs (buf.getSample (0, i) - buf.getSample (0, i - 1)));
    const double limit = 0.3 * 2.0 * 3.14159265 * 250.0 / 48000.0 * 3.0;
    R.check ("Fast host automation of macros is click-free", maxStep < limit, vt::fmt ("max sample step %.4f (limit %.4f)", maxStep, limit));
}

//==============================================================================
void saveSnapshot (juce::Component& c, const juce::File& file, float scale)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, scale);
    file.deleteFile();
    juce::FileOutputStream os (file);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, os);
}

void renderSnapshots (const juce::File& dir)
{
    auto p = makeProcessor (48000, 512, 2);
    p->setCurrentProgram (1);   // Afrobeat Lead
    std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
    auto* editor = dynamic_cast<VocalSweetAudioProcessorEditor*> (ed.get());
    R.check ("Editor opens", editor != nullptr && editor->getWidth() == 1080 && editor->getHeight() == 700,
             vt::fmt ("editor size %.0f x %.0f", (double) ed->getWidth(), (double) ed->getHeight()));
    if (editor == nullptr) return;

    // feed live audio so meters, analyzer and GR history show the real processor state
    auto vocal = toJuce (vt::syntheticVocal (48000, 6.0, 2, 0.45f));
    runBlocks (*p, vocal, 512, [editor] (int k) { if (k % 3 == 0) editor->refreshNow(); });
    saveSnapshot (*editor, dir.getChildFile ("01_main_interface_afrobeat_lead.png"), 1.5f);

    p->setCurrentProgram (4);   // Bright Pop
    setParam (*p, "sweeten", 72.0f);
    auto vocal2 = toJuce (vt::syntheticVocal (48000, 4.0, 2, 0.45f));
    runBlocks (*p, vocal2, 512, [editor] (int k) { if (k % 3 == 0) editor->refreshNow(); });
    saveSnapshot (*editor, dir.getChildFile ("02_main_bright_pop_sweeten72.png"), 1.5f);

    editor->showAdvanced (true);
    auto vocal3 = toJuce (vt::syntheticVocal (48000, 3.0, 2, 0.45f));
    runBlocks (*p, vocal3, 512, [editor] (int k) { if (k % 3 == 0) editor->refreshNow(); });
    saveSnapshot (*editor, dir.getChildFile ("03_advanced_controls.png"), 1.5f);

    p->setCurrentProgram (7);   // Vocal Double
    setParam (*p, "compOn", 0.0f);
    auto vocal4 = toJuce (vt::syntheticVocal (48000, 3.0, 2, 0.45f));
    runBlocks (*p, vocal4, 512, [editor] (int k) { if (k % 3 == 0) editor->refreshNow(); });
    saveSnapshot (*editor, dir.getChildFile ("04_advanced_vocal_double_comp_off.png"), 1.5f);

    R.check ("Editor snapshots rendered", dir.getChildFile ("01_main_interface_afrobeat_lead.png").existsAsFile(),
             ("PNG files in " + dir.getFullPathName()).toStdString());
    ed.reset();
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory().getChildFile ("test-output"));
    outDir.createDirectory();
    bool snapshots = false;
    for (int i = 2; i < argc; ++i) snapshots |= juce::String (argv[i]) == "--snapshots";

    std::printf ("VocalSweet plugin test-suite\n============================\n");
    testParametersAndLayouts();
    testProcessing();
    testStateAndPresets();
    testAutomationSmoothing();
    if (snapshots)
    {
        auto shots = outDir.getChildFile ("editor-snapshots");
        shots.createDirectory();
        renderSnapshots (shots);
    }

    R.writeReport (outDir.getChildFile ("plugin_test_report.md").getFullPathName().toStdString(), "VocalSweet plugin test report");
    std::printf ("\n%zu tests, %d failed\n", R.all().size(), R.failures());
    return R.failures() == 0 ? 0 : 1;
}
