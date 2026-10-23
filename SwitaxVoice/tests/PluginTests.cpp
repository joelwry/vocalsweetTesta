// Plugin-level tests: the real SwitaxVoiceAudioProcessor / editor classes inside a minimal
// in-process host. Usage: SwitaxPluginTests <outputDir> [--snapshots]

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SxSignals.h"
#include "TestUtils.h"

namespace
{
vt::Registry R;
using vt::fmt;

std::unique_ptr<SwitaxVoiceAudioProcessor> makeProcessor (double sr, int block, int channels, bool sidechain = false)
{
    auto p = std::make_unique<SwitaxVoiceAudioProcessor>();
    const auto set = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (set);
    layout.inputBuses.add (sidechain ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::disabled());
    layout.outputBuses.add (set);
    p->setBusesLayout (layout);
    p->setRateAndBufferSizeDetails (sr, block);
    p->prepareToPlay (sr, block);
    return p;
}

void setParam (SwitaxVoiceAudioProcessor& p, const char* id, float plain)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (plain));
}
float getParam (SwitaxVoiceAudioProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }

juce::AudioBuffer<float> toJuce (const sx::analysis::Buffer& b, int channels)
{
    juce::AudioBuffer<float> out (channels, (int) b[0].size());
    for (int c = 0; c < channels; ++c) out.copyFrom (c, 0, b[(size_t) std::min<int> (c, (int) b.size() - 1)].data(), out.getNumSamples());
    return out;
}

/** Runs the buffer through the processor in host-sized blocks. `realtimeish` paces the feed so the
    analysis thread can drain its FIFO (as in a real host, where audio arrives in real time). */
void runBlocks (SwitaxVoiceAudioProcessor& p, juce::AudioBuffer<float>& buf, int block, bool realtimeish = false,
                std::function<void (int)> perBlock = {})
{
    juce::MidiBuffer midi;
    const double sr = p.getSampleRate();
    double fed = 0.0;
    for (int start = 0, k = 0; start < buf.getNumSamples(); start += block, ++k)
    {
        const int len = juce::jmin (block, buf.getNumSamples() - start);
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), buf.getNumChannels(), start, len);
        p.processBlock (view, midi);
        if (perBlock) perBlock (k);
        fed += len / sr;
        if (realtimeish && fed > 0.25) { juce::Thread::sleep (25); fed = 0.0; }
    }
}

bool sameValues (const sx::ParamValues& a, const sx::ParamValues& b)
{
    for (int i = 0; i < sx::kNumParams; ++i)
        if (std::abs (a[(size_t) i] - b[(size_t) i]) > 1.0e-4f * std::max (1.0f, std::abs (a[(size_t) i]))) return false;
    return true;
}

sx::ParamValues neutral()
{
    auto v = sx::defaultParamValues();
    for (int i : { sx::pHpfOn, sx::pEqOn, sx::pCompOn, sx::pDeessOn, sx::pSatOn, sx::pExciteOn, sx::pDoubleOn, sx::pLimiterOn }) v[(size_t) i] = 0.0f;
    return v;
}

//==============================================================================
void testParametersAndLayouts()
{
    auto p = makeProcessor (48000, 512, 2);
    int found = 0; bool defaultsOk = true;
    for (const auto& d : sx::kParamDefs)
        if (auto* raw = p->apvts.getRawParameterValue (d.id)) { ++found; if (std::abs (raw->load() - d.def) > 1e-4f) defaultsOk = false; }
    R.check ("All parameters registered with stable host IDs", found == sx::kNumParams && (int) p->getParameters().size() == sx::kNumParams,
             fmt ("%.0f / %.0f parameters", found, sx::kNumParams));
    R.check ("Parameter defaults match the specification", defaultsOk, "all defaults verified");
    auto* hpf = p->apvts.getParameter ("hpfFreq");
    auto* thr = p->apvts.getParameter ("compThresh");
    const auto t1 = hpf->getText (hpf->convertTo0to1 (80.0f), 32), t2 = thr->getText (thr->convertTo0to1 (-18.0f), 32);
    R.check ("Parameter value text for automation lanes", t1 == "80 Hz" && t2 == "-18.0 dB", (t1 + ", " + t2).toStdString());

    auto layout = [] (juce::AudioChannelSet main, juce::AudioChannelSet sc, juce::AudioChannelSet out)
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (main); l.inputBuses.add (sc); l.outputBuses.add (out);
        return l;
    };
    using S = juce::AudioChannelSet;
    R.check ("Bus layouts: mono/stereo, optional mono/stereo sidechain; others rejected",
             p->checkBusesLayoutSupported (layout (S::mono(), S::disabled(), S::mono())) && p->checkBusesLayoutSupported (layout (S::stereo(), S::disabled(), S::stereo()))
                 && p->checkBusesLayoutSupported (layout (S::stereo(), S::stereo(), S::stereo())) && p->checkBusesLayoutSupported (layout (S::mono(), S::mono(), S::mono()))
                 && ! p->checkBusesLayoutSupported (layout (S::mono(), S::disabled(), S::stereo())) && ! p->checkBusesLayoutSupported (layout (S::quadraphonic(), S::disabled(), S::quadraphonic())),
             "mono->mono, stereo->stereo, sidechain off/mono/stereo");
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
                auto buf = toJuce (sxt::voice ({ 1.5, sr, ch }), ch);
                runBlocks (*p, buf, block);
                bool finite = true; float pk = 0;
                for (int c = 0; c < ch; ++c) for (int i = 0; i < buf.getNumSamples(); ++i) { const float s = buf.getSample (c, i); finite &= std::isfinite (s); pk = std::max (pk, std::abs (s)); }
                if (! finite || pk > 0.8914f || p->getLatencySamples() != (int) std::lround (0.001 * sr)) { ok = false; detail = fmt ("failed sr %.0f block %.0f ch %.0f", sr, block, ch); }
            }
    R.check ("processBlock: 3 rates x 3 block sizes x mono/stereo", ok, ok ? "finite, peak <= -1 dBFS, 1 ms latency reported" : detail);

    auto p = makeProcessor (44100, 1024, 2);
    auto buf = toJuce (sxt::voice ({ 2.0, 44100.0, 2 }), 2);
    juce::MidiBuffer midi;
    juce::Random rnd (42);
    int pos = 0, calls = 0;
    while (pos < buf.getNumSamples())
    {
        const int len = juce::jmin (1 + rnd.nextInt (3000), buf.getNumSamples() - pos);   // also larger than the prepared size
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, pos, len);
        p->processBlock (view, midi);
        pos += len; ++calls;
    }
    bool finite = true;
    for (int c = 0; c < 2; ++c) for (int i = 0; i < buf.getNumSamples(); ++i) finite &= std::isfinite (buf.getSample (c, i));
    R.check ("Randomly varying host block sizes (1..3000)", finite, fmt ("%.0f processBlock calls, all finite", calls));

    // host bypass = delayed dry
    auto b = makeProcessor (48000, 256, 2);
    auto src = toJuce (sxt::voice ({ 1.0, 48000.0, 2 }), 2);
    auto work = src;
    for (int start = 0; start < work.getNumSamples(); start += 256)
    {
        juce::AudioBuffer<float> view (work.getArrayOfWritePointers(), 2, start, juce::jmin (256, work.getNumSamples() - start));
        b->processBlockBypassed (view, midi);
    }
    const int L = b->getLatencySamples();
    double err = 0;
    for (int c = 0; c < 2; ++c) for (int i = L + 2000; i < work.getNumSamples(); ++i) err = std::max (err, (double) std::abs (work.getSample (c, i) - src.getSample (c, i - L)));
    R.check ("Host bypass (processBlockBypassed) = latency-aligned dry", err == 0.0, fmt ("max |error| %.2e", err));
}

void testAuditionCompareAccept()
{
    // audition: output follows the audition values while the parameters stay untouched
    auto p = makeProcessor (48000, 512, 2);
    p->applyValues (neutral());
    const auto before = p->getParamValues();
    auto loud = neutral();
    loud[sx::pOutputGain] = -12.0f;
    auto a = toJuce (sxt::voice ({ 1.0, 48000.0, 2 }), 2), b = a;
    runBlocks (*p, a, 512);
    p->startAudition (loud);
    runBlocks (*p, b, 512);
    p->stopAudition();
    const double ga = vt::toDb (b.getRMSLevel (0, 24000, 20000) / a.getRMSLevel (0, 24000, 20000));
    R.check ("Audition changes the sound but not the parameters", p->getParamValues() == before && std::abs (ga + 12.0) < 0.3,
             fmt ("audition output %.2f dB, parameters unchanged", ga));

    // level-matched compare: dry, delayed, with the matching gain
    auto q = makeProcessor (48000, 512, 2);
    q->setCompare (true, -4.0f);
    auto src = toJuce (sxt::voice ({ 1.0, 48000.0, 2 }), 2);
    auto c = src;
    runBlocks (*q, c, 512);
    const double gc = vt::toDb (c.getRMSLevel (0, 24000, 20000) / src.getRMSLevel (0, 24000 - 48, 20000));
    R.check ("A/B compare = unprocessed input with level-match gain", std::abs (gc + 4.0) < 0.1, fmt ("%.2f dB", gc));

    // accept path: applyValues notifies the host for each changed parameter
    struct Listener : juce::AudioProcessorListener
    {
        int changes = 0, gestures = 0;
        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override { ++changes; }
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
        void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int) override { ++gestures; }
    } l;
    auto r = makeProcessor (48000, 512, 2);
    r->addListener (&l);
    auto v = r->getParamValues();
    v[sx::pLowGain] = -3.5f; v[sx::pLowFreq] = 300.0f; v[sx::pDeessThresh] = -40.0f;
    r->applyValues (v);
    r->removeListener (&l);
    R.check ("Accepting a proposal = host-visible, undoable parameter gestures",
             std::abs (getParam (*r, "lowGain") + 3.5f) < 0.01f && l.changes == 3 && l.gestures == 3,
             fmt ("%.0f parameter changes, %.0f gestures", l.changes, l.gestures));
}

void testStateAndPresets()
{
    auto a = makeProcessor (48000, 512, 2);
    juce::Random rnd (7);
    for (int i = 0; i < sx::kNumParams; ++i)
        if (auto* param = a->apvts.getParameter (sx::kParamDefs[(size_t) i].id)) param->setValueNotifyingHost (rnd.nextFloat());
    a->presets.loadPreset (3);
    setParam (*a, "harshGain", -5.0f);
    a->assist.context = sx::analysis::Context::VocalBus;
    a->assist.genre = 3;
    a->assist.captureSeconds = 30.0;
    juce::MemoryBlock state;
    a->getStateInformation (state);
    auto b = makeProcessor (48000, 512, 2);
    b->setStateInformation (state.getData(), (int) state.getSize());
    int mismatches = 0;
    for (const auto& d : sx::kParamDefs) if (std::abs (getParam (*a, d.id) - getParam (*b, d.id)) > 1e-5f) ++mismatches;
    R.check ("State save -> restore (parameters, preset name, Assist settings)",
             mismatches == 0 && b->presets.getCurrentName() == a->presets.getCurrentName() && b->assist.context == sx::analysis::Context::VocalBus
                 && b->assist.genre == 3 && b->assist.captureSeconds == 30.0,
             fmt ("%.0f bytes, %.0f mismatches", (double) state.getSize(), mismatches));

    auto c = makeProcessor (48000, 512, 2);
    const char junk[] = "this is not a SwitaxVoice state";
    c->setStateInformation (junk, (int) sizeof (junk));
    juce::MemoryBlock empty;
    c->setStateInformation (empty.getData(), 0);
    R.check ("Corrupt / empty state ignored safely", std::abs (getParam (*c, "compThresh") + 18.0f) < 1e-4f, "defaults retained");

    const auto& factory = sx::factoryPresets();
    bool progOk = a->getNumPrograms() >= (int) factory.size();
    juce::StringArray names;
    for (int i = 0; i < (int) factory.size(); ++i)
    {
        a->setCurrentProgram (i);
        names.add (a->getProgramName (i));
        for (int k = 0; k < sx::kNumParams; ++k)
        {
            if (k == sx::pBypass) continue;
            const float want = factory[(size_t) i].values[(size_t) k], got = getParam (*a, sx::kParamDefs[(size_t) k].id);
            if (std::abs (want - got) > 0.02f * std::max (1.0f, std::abs (want))) progOk = false;
        }
        progOk &= a->getCurrentProgram() == i;
    }
    R.check ("Factory presets load exact values via host programs", progOk, names.joinIntoString (", ").toStdString());

    auto& pm = a->presets;
    a->setCurrentProgram (1);
    setParam (*a, "satDrive", 77.0f);
    const int idx = pm.saveUserPreset ("zz UnitTest Preset");
    setParam (*a, "satDrive", 5.0f);
    const bool loaded = idx >= 0 && pm.loadPreset (idx);
    const float w = getParam (*a, "satDrive");
    const auto file = PresetManager::getUserPresetDirectory().getChildFile ("zz UnitTest Preset.sxpreset");
    const bool existed = file.existsAsFile();
    const bool deleted = pm.deleteUserPreset (idx);
    R.check ("User preset save / load / delete (.sxpreset)", loaded && std::abs (w - 77.0f) < 0.01f && existed && deleted,
             ("saved to " + file.getFullPathName()).toStdString());
}

void testAutomationSmoothing()
{
    auto p = makeProcessor (48000, 128, 2);
    juce::AudioBuffer<float> buf (2, 48000);
    for (int i = 0; i < buf.getNumSamples(); ++i) { const float s = 0.3f * std::sin (2.0f * 3.14159265f * 250.0f * (float) i / 48000.0f); buf.setSample (0, i, s); buf.setSample (1, i, s); }
    runBlocks (*p, buf, 128, false, [&p] (int k)
    {
        if (k % 10 == 0)
        {
            setParam (*p, "lowGain", (k / 10) % 2 ? -12.0f : 6.0f);
            setParam (*p, "compThresh", (k / 10) % 2 ? -50.0f : 0.0f);
            setParam (*p, "eqOn", (float) ((k / 10) % 2));
        }
    });
    double maxStep = 0;
    for (int i = 2000; i < buf.getNumSamples(); ++i) maxStep = std::max (maxStep, (double) std::abs (buf.getSample (0, i) - buf.getSample (0, i - 1)));
    const double limit = 0.3 * 2.0 * 3.14159265 * 250.0 / 48000.0 * 4.0;
    R.check ("Fast host automation (EQ, threshold, switches) is click-free", maxStep < limit, fmt ("max sample step %.4f (limit %.4f)", maxStep, limit));
}

void testLiveMetersAndSidechain()
{
    auto p = makeProcessor (48000, 512, 2, true);
    p->applyValues (neutral());
    p->analysis.resetIntegrated();
    const auto tone = sxt::sine (48000, 6.0, 1000.0, -23.0, 2);
    const auto beat = sxt::sine (48000, 6.0, 60.0, -12.0, 2);
    juce::AudioBuffer<float> buf (4, (int) tone[0].size());
    for (int c = 0; c < 2; ++c) { buf.copyFrom (c, 0, tone[(size_t) c].data(), buf.getNumSamples()); buf.copyFrom (c + 2, 0, beat[(size_t) c].data(), buf.getNumSamples()); }
    runBlocks (*p, buf, 512, true);
    juce::Thread::sleep (200);
    const auto m = p->analysis.getLiveMeters();
    R.check ("Live BS.1770 meters through the plugin: -23 dBFS stereo 1 kHz -> -23 LUFS in & out",
             std::abs (m.inIntegrated + 23.0) < 0.2 && std::abs (m.outIntegrated + 23.0) < 0.2 && m.droppedFrames == 0,
             fmt ("in %.2f, out %.2f LUFS", m.inIntegrated, m.outIntegrated) + fmt (", dropped %.0f", (double) m.droppedFrames));
    R.check ("Sidechain bus reaches the analyser", m.sidechainActive && m.scShortTerm > -40.0, fmt ("sidechain short-term %.1f LUFS", m.scShortTerm));
}

void testWindowSizing()
{
    using Ed = SwitaxVoiceAudioProcessorEditor;
    // The window (plus ~90 px for the host's title bar) must fit common work areas, including
    // laptop screens and Windows display scaling of 125/150/175 %.
    struct Screen { const char* name; int w, h; };
    bool ok = true; std::string d;
    for (auto s : { Screen { "1366x768 @100%", 1366, 728 }, Screen { "1920x1080 @125%", 1536, 832 }, Screen { "1920x1080 @150%", 1280, 693 },
                    Screen { "1920x1080 @175%", 1097, 594 }, Screen { "2560x1440 @100%", 2560, 1400 }, Screen { "1280x720 @100%", 1280, 680 } })
    {
        const juce::Rectangle<int> area (0, 0, s.w, s.h);
        const double sc = Ed::fitScale (area);
        const int w = juce::roundToInt (Ed::designWidth * sc), h = juce::roundToInt (Ed::designHeight * sc);
        const bool fits = w <= s.w && h + 60 <= s.h;
        ok &= fits;
        d += std::string (s.name) + " " + std::to_string (w) + "x" + std::to_string (h) + (fits ? " ok; " : " TOO BIG; ");
    }
    R.check ("Default window size fits typical screens (laptop and 125-175 % display scaling)", ok, d);

    auto p = makeProcessor (48000, 512, 2);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
    auto* editor = dynamic_cast<Ed*> (ed.get());
    if (editor == nullptr) { R.check ("Window sizing", false, "no editor"); return; }
    const auto area = Ed::smallestUserArea();
    R.check ("Editor never opens larger than the screen work area",
             area.isEmpty() || (ed->getWidth() <= area.getWidth() && ed->getHeight() + 60 <= area.getHeight()),
             fmt ("editor %.0f x %.0f", ed->getWidth(), ed->getHeight()) + fmt (", work area %.0f x %.0f", area.getWidth(), area.getHeight()));

    editor->setScale (0.6);
    const int w60 = ed->getWidth(), h60 = ed->getHeight();
    editor->setScale (0.01);
    const bool minClamp = ed->getWidth() >= (int) (1180 * 0.5) - 1;
    editor->setScale (10.0);
    const bool maxClamp = area.isEmpty() || (ed->getWidth() <= area.getWidth() && ed->getHeight() + 60 <= area.getHeight());
    R.check ("setScale: exact size, clamped between 50 % and what fits the screen",
             std::abs (w60 - 708) <= 1 && std::abs (h60 - 456) <= 1 && minClamp && maxClamp, fmt ("60 %% -> %.0f x %.0f", w60, h60));

    // drag handles on the plugin's own edges
    editor->setScale (0.8);
    auto& corner = editor->getCornerHandleForTest();
    R.check ("Edge handles exist and sit on the plugin's right/bottom edges (hit area >= 8 px)",
             editor->getRightHandleForTest().getWidth() >= 8 && editor->getBottomHandleForTest().getHeight() >= 8 && corner.getWidth() >= 24,
             fmt ("right %.0f px, bottom %.0f px, corner %.0f px", editor->getRightHandleForTest().getWidth(), editor->getBottomHandleForTest().getHeight(), corner.getWidth()));

    // size menu
    editor->getSizeBoxForTest().setSelectedId (100 + 75, juce::sendNotificationSync);
    const bool menu75 = std::abs (ed->getWidth() - 885) <= 1;
    editor->getSizeBoxForTest().setSelectedId (1, juce::sendNotificationSync);
    const bool menuFit = std::abs (editor->getScale() - Ed::fitScale (Ed::smallestUserArea())) < 0.01 || area.isEmpty();
    R.check ("Size menu: 75 % and Fit to screen", menu75 && menuFit, fmt ("scale now %.2f", editor->getScale()));

    // size persists in the plugin state (and is clamped on restore)
    editor->setScale (0.7);
    juce::MemoryBlock state;
    p->getStateInformation (state);
    ed.reset();
    auto q = makeProcessor (48000, 512, 2);
    q->setStateInformation (state.getData(), (int) state.getSize());
    std::unique_ptr<juce::AudioProcessorEditor> ed2 (q->createEditor());
    R.check ("Window size is remembered in the project state", std::abs (ed2->getWidth() - 826) <= 2, fmt ("reopened at %.0f x %.0f", ed2->getWidth(), ed2->getHeight()));
    ed2.reset();
    auto big = makeProcessor (48000, 512, 2);
    big->uiScale.store (9.0f);
    std::unique_ptr<juce::AudioProcessorEditor> ed3 (big->createEditor());
    R.check ("A bogus / oversized saved size is clamped to the screen", area.isEmpty() || (ed3->getWidth() <= area.getWidth() && ed3->getHeight() + 60 <= area.getHeight()),
             fmt ("opened at %.0f x %.0f", ed3->getWidth(), ed3->getHeight()));
}

void testLearnEndToEnd (const juce::File& outDir, bool snapshots)
{
    auto p = makeProcessor (48000, 512, 2);
    p->setCurrentProgram (0);
    p->assist.captureSeconds = 10.0;
    p->assist.genre = 0;
    auto* pp = p.get();
    p->analysis.startLearning (p->assist, [pp] { return pp->getParamValues(); });
    sxt::VoiceSpec s;
    s.seconds = 14.0; s.channels = 2; s.mudDb = 6.0; s.sibilance = 0.8; s.rumble = 0.02; s.phraseSwingDb = 10.0;
    auto vocal = toJuce (sxt::voice (s), 2);

    double maxBlockMs = 0.0;
    std::vector<double> blockMs;
    juce::MidiBuffer midi;
    double fed = 0;
    for (int start = 0; start < vocal.getNumSamples(); start += 512)
    {
        const int len = juce::jmin (512, vocal.getNumSamples() - start);
        juce::AudioBuffer<float> view (vocal.getArrayOfWritePointers(), 2, start, len);
        const auto t0 = juce::Time::getHighResolutionTicks();
        p->processBlock (view, midi);
        blockMs.push_back (1000.0 * juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0));
        maxBlockMs = std::max (maxBlockMs, blockMs.back());
        fed += len / 48000.0;
        if (fed > 0.25) { juce::Thread::sleep (25); fed = 0; }
    }
    // keep processing (as a host would) while the analysis runs in the background
    const auto deadline = juce::Time::getMillisecondCounter() + 120000;
    auto more = toJuce (sxt::voice ({ 1.0, 48000.0, 2 }), 2);
    while (p->analysis.getState() != AnalysisEngine::State::Ready && juce::Time::getMillisecondCounter() < deadline)
    {
        juce::AudioBuffer<float> blk (2, 512);
        blk.copyFrom (0, 0, more, 0, 0, 512); blk.copyFrom (1, 0, more, 1, 0, 512);
        const auto t0 = juce::Time::getHighResolutionTicks();
        p->processBlock (blk, midi);
        blockMs.push_back (1000.0 * juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0));
        maxBlockMs = std::max (maxBlockMs, blockMs.back());
        juce::Thread::sleep (10);
    }
    const auto rep = p->analysis.getReport();
    std::string keys;
    if (rep != nullptr) for (auto& r : rep->recs) keys += (keys.empty() ? "" : ",") + r.key;
    R.check ("LEARN end-to-end: capture through processBlock -> background analysis -> report",
             rep != nullptr && rep->valid && ! rep->recs.empty(), rep != nullptr ? rep->status + " [" + keys + "]" : "no report");
    {
        std::sort (blockMs.begin(), blockMs.end());
        const double p999 = blockMs.empty() ? 0.0 : blockMs[(size_t) ((double) (blockMs.size() - 1) * 0.999)];
        const double median = blockMs.empty() ? 0.0 : blockMs[blockMs.size() / 2];
        // Pass on the 99.9th percentile; the single worst block is reported but can include
        // OS scheduling pauses of the (shared) machine running the test (5-7 ms seen on a bare spin loop).
        R.check ("Audio thread stays fast while the advisor runs (99.9th percentile < 4 ms)", p999 < 4.0,
                 fmt ("median %.3f ms, p99.9 %.3f ms, worst %.3f ms over ", median, p999, maxBlockMs) + std::to_string (blockMs.size()) + " blocks (budget 10.7 ms)");
    }
    if (rep == nullptr || ! rep->valid) return;

    // editor: cards are built from the report, accept + undo through the UI logic
    std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
    auto* editor = dynamic_cast<SwitaxVoiceAudioProcessorEditor*> (ed.get());
    R.check ("Editor opens at a size that fits the screen, with the design aspect ratio",
             editor != nullptr && std::abs ((double) ed->getWidth() / ed->getHeight() - 1180.0 / 760.0) < 0.01,
             fmt ("%.0f x %.0f", ed->getWidth(), ed->getHeight()));
    if (editor == nullptr) return;
    auto& assist = editor->getAssistPage();
    editor->refreshNow();
    R.check ("Assist page shows one card per recommendation", assist.getNumCards() == (int) rep->recs.size(), fmt ("%.0f cards", assist.getNumCards()));

    sx::ui::RecCard* card = nullptr;
    for (int i = 0; i < assist.getNumCards(); ++i) if (assist.getCard (i)->rec.isActionable()) { card = assist.getCard (i); break; }
    if (card != nullptr)
    {
        const auto before = p->getParamValues();
        assist.toggleAudition (*card);
        const bool auditionOk = p->isAuditioning() && p->getParamValues() == before;
        assist.toggleAudition (*card);
        card->amountSlider.setValue (50.0, juce::sendNotificationSync);
        assist.accept (*card);
        const auto after = p->getParamValues();
        bool changed = false, halfway = true;
        for (auto& c : card->rec.changes)
        {
            changed |= after[(size_t) c.index] != before[(size_t) c.index];
            if (sx::kParamDefs[(size_t) c.index].kind == sx::ParamKind::Float)
            {
                const float want = sx::clampParam (c.index, before[(size_t) c.index] + 0.5f * (c.to - before[(size_t) c.index]));
                halfway &= std::abs (after[(size_t) c.index] - want) < 0.02f * std::max (1.0f, std::abs (want));
            }
        }
        assist.undo();
        const bool undone = sameValues (p->getParamValues(), before);
        R.check ("Card: audition (params untouched), accept at 50 % amount, undo restores",
                 auditionOk && changed && halfway && undone && card->status == sx::ui::RecCard::Status::Pending,
                 "'" + card->rec.title + "': " + card->rec.proposal
                     + fmt (" | audition %.0f changed %.0f halfway %.0f", auditionOk, changed, halfway) + fmt (" undone %.0f", undone));
        assist.reject (*card);
        R.check ("Card: reject marks it without touching parameters", card->status == sx::ui::RecCard::Status::Rejected && sameValues (p->getParamValues(), before), "rejected");
        card->setStatus (sx::ui::RecCard::Status::Pending);
    }

    if (snapshots)
    {
        auto dir = outDir.getChildFile ("editor-snapshots");
        dir.createDirectory();
        auto save = [&] (const char* name)
        {
            for (int k = 0; k < 4; ++k) editor->refreshNow();
            auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.25f);
            auto f = dir.getChildFile (name);
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
        };
        auto feed = [&] (int secs) { auto v = toJuce (sxt::voice ({ (double) secs, 48000.0, 2 }), 2); runBlocks (*p, v, 512, true, [editor] (int k) { if (k % 4 == 0) editor->refreshNow(); }); };
        feed (3);
        save ("01_assist_report.png");
        if (assist.getNumCards() > 0) { assist.getCard (0)->detailsButton.triggerClick(); }
        if (card != nullptr) assist.toggleAudition (*card);
        feed (2);
        save ("02_assist_details_auditioning.png");
        assist.stopAudition();
        assist.acceptAll();
        editor->showPage (SwitaxVoiceAudioProcessorEditor::Page::Channel);
        feed (3);
        save ("03_channel_after_accept_all.png");
        editor->showPage (SwitaxVoiceAudioProcessorEditor::Page::Assist);
        save ("04_assist_after_accept_all.png");
        // laptop-sized window (1366x768 screen @100 %: 991 x 638) and the smallest allowed size
        editor->setScale (991.0 / 1180.0);
        feed (2);
        auto saveAt = [&] (const char* name, float scale)
        {
            for (int k = 0; k < 4; ++k) editor->refreshNow();
            auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
            auto f = dir.getChildFile (name);
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
        };
        saveAt ("05_laptop_size_991x638.png", 1.0f);
        editor->setScale (0.5);
        saveAt ("06_smallest_size_590x380.png", 1.0f);
        R.check ("Editor snapshots rendered (real editor, live audio)", dir.getChildFile ("04_assist_after_accept_all.png").existsAsFile(), dir.getFullPathName().toStdString());
    }
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

    std::printf ("SwitaxVoice plugin test-suite\n=============================\n");
    testParametersAndLayouts();
    testProcessing();
    testAuditionCompareAccept();
    testStateAndPresets();
    testAutomationSmoothing();
    testLiveMetersAndSidechain();
    testWindowSizing();
    testLearnEndToEnd (outDir, snapshots);

    // export every factory preset as a user-loadable .sxpreset file (deliverable)
    {
        auto dir = outDir.getChildFile ("presets");
        dir.createDirectory();
        int n = 0;
        for (auto& pr : sx::factoryPresets())
        {
            juce::XmlElement xml ("SwitaxVoicePreset");
            xml.setAttribute ("name", juce::String (pr.name));
            xml.setAttribute ("version", 1);
            for (int i = 0; i < sx::kNumParams; ++i)
                if (i != sx::pBypass) xml.setAttribute (sx::kParamDefs[(size_t) i].id, (double) pr.values[(size_t) i]);
            n += xml.writeTo (dir.getChildFile (juce::String (pr.name) + ".sxpreset")) ? 1 : 0;
        }
        R.check ("Factory presets exported as .sxpreset files", n == (int) sx::factoryPresets().size(), dir.getFullPathName().toStdString());
    }

    R.writeReport (outDir.getChildFile ("plugin_test_report.md").getFullPathName().toStdString(), "SwitaxVoice plugin test report");
    std::printf ("\n%zu tests, %d failed\n", R.all().size(), R.failures());
    return R.failures() == 0 ? 0 : 1;
}
