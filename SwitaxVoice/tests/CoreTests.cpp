// Framework-independent tests for SwitaxVoice: measurement accuracy against standard test cases,
// DSP chain behaviour, and advisor behaviour on synthetic signals with injected problems and on
// real recordings (VocalSet, CC BY 4.0) when present.
// Usage: SwitaxCoreTests <outputDir> [realClipsDir]

#include "SxSignals.h"
#include "TestUtils.h"   // VocalSweet test registry / helpers (shared)

#include <chrono>
#include <dirent.h>
#include <sys/stat.h>

using namespace sx::analysis;
using vt::fmt;

namespace
{
vt::Registry R;
std::string outDir = "test-output";
std::string realDir = "/opt/testdata/vocalset/clips";

double near (double a, double b) { return std::abs (a - b); }

LoudnessMeter measureLoudness (const Buffer& b, double sr)
{
    LoudnessMeter lm;
    lm.prepare (sr, (int) b.size());
    const float* p[2] = { b[0].data(), b[b.size() - 1].data() };
    lm.process (p, (int) b.size(), (int) b[0].size());
    return lm;
}

//==============================================================================
void testLoudnessStandards()
{
    const double sr = 48000.0;
    // EBU Tech 3341 test cases (1 kHz stereo sine)
    {
        auto lm = measureLoudness (sxt::sine (sr, 20.0, 1000.0, -23.0), sr);
        R.check ("EBU 3341 case 1: -23 dBFS sine -> -23.0 LUFS (M, S, I)",
                 near (lm.integrated(), -23.0) <= 0.1 && near (lm.momentary(), -23.0) <= 0.1 && near (lm.shortTerm(), -23.0) <= 0.1,
                 fmt ("I %.3f, M %.3f, S %.3f", lm.integrated(), lm.momentary(), lm.shortTerm()));
    }
    {
        auto lm = measureLoudness (sxt::sine (sr, 20.0, 1000.0, -33.0), sr);
        R.check ("EBU 3341 case 2: -33 dBFS sine -> -33.0 LUFS", near (lm.integrated(), -33.0) <= 0.1, fmt ("I %.3f", lm.integrated()));
    }
    {
        auto b = sxt::concat ({ sxt::sine (sr, 10, 1000, -36), sxt::sine (sr, 60, 1000, -23), sxt::sine (sr, 10, 1000, -36) });
        auto lm = measureLoudness (b, sr);
        R.check ("EBU 3341 case 3: relative gate (-36/-23/-36) -> -23.0", near (lm.integrated(), -23.0) <= 0.1, fmt ("I %.3f", lm.integrated()));
    }
    {
        auto b = sxt::concat ({ sxt::sine (sr, 10, 1000, -72), sxt::sine (sr, 10, 1000, -36), sxt::sine (sr, 60, 1000, -23),
                                sxt::sine (sr, 10, 1000, -36), sxt::sine (sr, 10, 1000, -72) });
        auto lm = measureLoudness (b, sr);
        R.check ("EBU 3341 case 4: absolute + relative gate -> -23.0", near (lm.integrated(), -23.0) <= 0.1, fmt ("I %.3f", lm.integrated()));
    }
    // EBU Tech 3342 loudness range
    struct LraCase { double a, b, expect; };
    for (auto c : { LraCase { -20, -30, 10 }, LraCase { -20, -15, 5 }, LraCase { -40, -20, 20 } })
    {
        auto b = sxt::concat ({ sxt::sine (sr, 20, 1000, c.a), sxt::sine (sr, 20, 1000, c.b) });
        auto lm = measureLoudness (b, sr);
        R.check (fmt ("EBU 3342 LRA: %.0f / %.0f dBFS -> %.0f LU", c.a, c.b, c.expect), near (lm.loudnessRange(), c.expect) <= 1.0,
                 fmt ("LRA %.2f LU", lm.loudnessRange()));
    }
    // sample-rate independence
    bool ok = true; std::string d;
    for (double rate : { 22050.0, 44100.0, 88200.0, 96000.0, 192000.0 })
    {
        auto lm = measureLoudness (sxt::sine (rate, 10.0, 1000.0, -23.0), rate);
        d += fmt ("%.0f:%.2f ", rate, lm.integrated());
        ok &= near (lm.integrated(), -23.0) <= 0.1;
    }
    R.check ("Loudness at 22.05-192 kHz within 0.1 LU", ok, d);
    {
        auto lm = measureLoudness (sxt::sine (sr, 10.0, 1000.0, -20.0, 1), sr);
        R.check ("Mono channel (G = 1.0): -20 dBFS sine -> -23.0 LUFS", near (lm.integrated(), -23.0) <= 0.1, fmt ("I %.3f", lm.integrated()));
    }
    KWeighting k;
    k.prepare (48000.0);
    R.check ("K-weighting response (1 kHz +0.7 dB, 10 kHz +4.0 dB, 30 Hz < -6 dB)",
             near (k.magnitudeDb (48000, 1000), 0.69) < 0.05 && near (k.magnitudeDb (48000, 10000), 4.0) < 0.2 && k.magnitudeDb (48000, 30) < -6.0,
             fmt ("%.3f, %.3f, %.3f dB", k.magnitudeDb (48000, 1000), k.magnitudeDb (48000, 10000), k.magnitudeDb (48000, 30)));
}

void testTruePeak()
{
    const double sr = 48000.0;
    {
        // fs/4 sine at 45 degrees: samples land at +/-0.707 while the waveform peaks at 1.0
        auto b = sxt::sine (sr, 1.0, sr / 4.0, 0.0, 1, vs::dsp::kPi / 4.0);
        TruePeakMeter tp;
        const float* p[1] = { b[0].data() };
        tp.process (p, 1, (int) b[0].size());
        R.check ("True peak: fs/4 sine at 45 deg (sample peak -3.01 dB) -> 0 dBTP",
                 near (tp.samplePeakDb(), -3.01) < 0.05 && near (tp.truePeakDb(), 0.0) < 0.3,
                 fmt ("sample %.2f dB, true %.2f dBTP", tp.samplePeakDb(), tp.truePeakDb()));
    }
    {
        auto b = sxt::sine (sr, 1.0, 997.0, -6.0, 2);
        TruePeakMeter tp;
        const float* p[2] = { b[0].data(), b[1].data() };
        tp.process (p, 2, (int) b[0].size());
        R.check ("True peak: 997 Hz sine at -6 dBFS -> -6.0 dBTP", near (tp.truePeakDb(), -6.0) < 0.1, fmt ("%.3f dBTP", tp.truePeakDb()));
    }
}

void testSpectrum()
{
    const double sr = 48000.0;
    Buffer pink ((size_t) 1, std::vector<float> ((size_t) sr * 8));
    {
        std::mt19937 rng (5);
        std::uniform_real_distribution<float> d (-1, 1);
        float b0 = 0, b1 = 0, b2 = 0;
        for (auto& s : pink[0]) { const float w = d (rng); b0 = 0.99765f * b0 + w * 0.0990460f; b1 = 0.96300f * b1 + w * 0.2965164f; b2 = 0.57000f * b2 + w * 1.0526913f; s = 0.12f * (b0 + b1 + b2 + w * 0.1848f); }
    }
    auto m = analyse (pink, sr, AnalysisOptions {});
    double lo = 1e9, hi = -1e9;
    for (int b = 0; b < kNumBands; ++b)
        if (bandCentreHz (b) > 63 && bandCentreHz (b) < 16000) { lo = std::min (lo, m.ltasDb[(size_t) b]); hi = std::max (hi, m.ltasDb[(size_t) b]); }
    R.check ("Spectrum: pink noise (Kellet approximation) flat within +/-2 dB, 63 Hz-16 kHz", hi - lo < 4.0, fmt ("spread %.2f dB", hi - lo));

    // Parseval: band powers sum to the signal's mean square (white noise)
    Buffer white ((size_t) 1, std::vector<float> ((size_t) sr * 4));
    std::mt19937 rng (9);
    std::uniform_real_distribution<float> d (-0.5f, 0.5f);
    for (auto& s : white[0]) s = d (rng);
    BandAnalyzer ba;
    ba.prepare (sr);
    BandArray bands {};
    ba.analyse (white[0].data() + 10000, bands);
    double sum = 0, ms = 0;
    for (int b = 0; b < kNumBands; ++b) sum += bands[(size_t) b];
    for (int i = 0; i < ba.frameSize(); ++i) ms += (double) white[0][(size_t) (10000 + i)] * white[0][(size_t) (10000 + i)];
    ms /= ba.frameSize();
    // bands cover 20 Hz - 20.5 kHz of 0-24 kHz: expect about 85 % of the total power
    R.check ("Spectrum calibration: band sum = 20 Hz-20 kHz share of mean square", near (powToDb (sum), powToDb (ms * 20460.0 / 24000.0)) < 0.6,
             fmt ("bands %.2f dB, expected %.2f dB", powToDb (sum), powToDb (ms * 20460.0 / 24000.0)));

    auto s = sxt::sine (sr, 3.0, 1000.0, 0.0, 1);
    auto ms2 = analyse (s, sr, AnalysisOptions {});
    const int b1k = bandForFreq (1000.0);
    R.check ("Spectrum: 0 dBFS 1 kHz sine -> -3.01 dB in its band", near (ms2.ltasDb[(size_t) b1k], -3.01) < 0.3,
             fmt ("%.2f dB at %.0f Hz band", ms2.ltasDb[(size_t) b1k], bandCentreHz (b1k)));
}

void testPitch()
{
    const double sr = 44100.0;
    bool ok = true; std::string d;
    for (double f0 : { 98.0, 196.0, 440.0, 660.0 })
    {
        // harmonic tone (sawtooth-like, 1/h)
        Buffer b ((size_t) 1, std::vector<float> ((size_t) sr * 3));
        for (size_t i = 0; i < b[0].size(); ++i)
        {
            double v = 0;
            for (int h = 1; h * f0 < 0.45 * sr && h <= 30; ++h) v += std::sin (2 * vs::dsp::kPi * h * f0 * (double) i / sr) / h;
            b[0][i] = 0.3f * (float) v;
        }
        auto act = detectActivity (b[0], sr);
        auto r = PitchTracker::track (b[0], sr, act);
        d += fmt ("%.0f->%.1f ", f0, r.p50);
        ok &= near (r.p50, f0) / f0 < 0.01 && r.voicedFraction > 0.9;
    }
    R.check ("Pitch (YIN): harmonic tones 98-660 Hz within 1 %", ok, d);
    Buffer n ((size_t) 1, std::vector<float> ((size_t) sr * 3));
    std::mt19937 rng (3);
    std::uniform_real_distribution<float> u (-0.3f, 0.3f);
    for (auto& s : n[0]) s = u (rng);
    auto r = PitchTracker::track (n[0], sr, detectActivity (n[0], sr));
    R.check ("Pitch (YIN): white noise is reported unvoiced", r.voicedFraction < 0.1, fmt ("voiced %.0f %%", r.voicedFraction * 100));
}

void testActivityClippingStereo()
{
    const double sr = 48000.0;
    auto half = sxt::concat ({ sxt::sine (sr, 5.0, 300.0, -12.0, 1), sxt::sine (sr, 5.0, 300.0, -200.0, 1) });
    auto m = analyse (half, sr, AnalysisOptions {});
    R.check ("Activity gating: 5 s tone + 5 s silence -> 5 s active", near (m.activeSeconds, 5.0) < 0.3, fmt ("%.2f s", m.activeSeconds));

    auto clipped = sxt::sine (sr, 2.0, 220.0, 6.0, 1);
    for (auto& x : clipped[0]) x = std::clamp (x, -1.0f, 1.0f);
    auto clean = sxt::sine (sr, 2.0, 220.0, -0.5, 1);
    auto flat = sxt::sine (sr, 2.0, 220.0, 0.0, 1);
    for (auto& x : flat[0]) x = std::clamp (x * 0.6f, -0.4f, 0.4f);
    const auto mc = analyse (clipped, sr, AnalysisOptions {}), mn = analyse (clean, sr, AnalysisOptions {}), mf = analyse (flat, sr, AnalysisOptions {});
    R.check ("Clipping: hard-clipped sine detected, clean -0.5 dBFS sine not",
             mc.clipRuns > 100 && mn.clipRuns == 0 && mn.flatTopRuns == 0, fmt ("clipped %.0f runs, clean %.0f", mc.clipRuns, mn.clipRuns));
    R.check ("Clipping: flat tops below full scale (pre-gain clipping) detected", mf.flatTopRuns > 100 && mf.clipRuns == 0, fmt ("%.0f flat-top runs", mf.flatTopRuns));

    auto st = sxt::sine (sr, 3.0, 440.0, -10.0, 2);
    auto inv = st; for (auto& x : inv[1]) x = -x;
    Buffer un ((size_t) 2, std::vector<float> ((size_t) sr * 3));
    std::mt19937 rng (4);
    std::uniform_real_distribution<float> u (-0.2f, 0.2f);
    for (auto& c : un) for (auto& x : c) x = u (rng);
    const auto a = analyse (st, sr, AnalysisOptions {}), b = analyse (inv, sr, AnalysisOptions {}), c = analyse (un, sr, AnalysisOptions {});
    R.check ("Stereo correlation: identical +1, inverted -1, independent ~0",
             a.correlation > 0.99 && b.correlation < -0.99 && std::abs (c.correlation) < 0.1 && b.monoLossDb < -40,
             fmt ("%.3f / %.3f / %.3f", a.correlation, b.correlation, c.correlation) + fmt (", inverted mono loss %.0f dB", b.monoLossDb));
}

void testSibilance()
{
    sxt::VoiceSpec s;
    s.seconds = 10;
    s.sibilance = 0.9;
    auto loud = analyse (sxt::voice (s), s.sampleRate, AnalysisOptions {});
    s.sibilance = 0.0;
    auto none = analyse (sxt::voice (s), s.sampleRate, AnalysisOptions {});
    R.check ("Sibilance: strong 's' bursts detected; none when absent",
             loud.sibilance.events >= 8 && none.sibilance.events <= 1,
             fmt ("%.0f events with, %.0f without", loud.sibilance.events, none.sibilance.events));
    R.check ("Sibilance: tonal balance excludes sibilant frames", loud.sibilantFramesSkipped > 0 && none.sibilantFramesSkipped == 0,
             fmt ("%.0f frames skipped (with), %.0f (without)", loud.sibilantFramesSkipped, none.sibilantFramesSkipped));
}

//==============================================================================
void processChain (sx::dsp::Chain& c, Buffer& b, int block)
{
    const int n = (int) b[0].size(), chs = (int) b.size();
    for (int s = 0; s < n; s += block)
    {
        const int len = std::min (block, n - s);
        float* p[2] = { b[0].data() + s, b[(size_t) (chs - 1)].data() + s };
        c.process (p, chs, len);
    }
}

sx::ParamValues neutral()
{
    auto v = sx::defaultParamValues();
    for (int i : { sx::pHpfOn, sx::pEqOn, sx::pCompOn, sx::pDeessOn, sx::pSatOn, sx::pExciteOn, sx::pDoubleOn, sx::pLimiterOn }) v[(size_t) i] = 0.0f;
    return v;
}

void testChain()
{
    const double sr = 48000.0;
    using namespace sx;
    sx::dsp::Chain c;
    c.prepare (sr, 512); c.setParams (defaultParamValues()); c.reset();
    R.check ("Chain latency = 1 ms look-ahead", c.getLatencySamples() == 48, fmt ("%.0f samples", c.getLatencySamples()));

    Buffer z ((size_t) 2, std::vector<float> ((size_t) sr, 0.0f));
    processChain (c, z, 512);
    R.check ("Chain: silence in -> silence out (below -180 dBFS)", vt::peak (z) < 1e-9f, fmt ("peak %.2e", vt::peak (z)));

    // block-size invariance
    auto voice = sxt::voice (sxt::VoiceSpec { 4.0, sr, 2 });
    auto a = voice, b = voice;
    sx::dsp::Chain c1, c2;
    c1.prepare (sr, 512); c1.setParams (defaultParamValues()); c1.reset();
    c2.prepare (sr, 512); c2.setParams (defaultParamValues()); c2.reset();
    processChain (c1, a, 512);
    processChain (c2, b, 37);
    float diff = 0;
    for (size_t ch = 0; ch < 2; ++ch) for (size_t i = 0; i < a[ch].size(); ++i) diff = std::max (diff, std::abs (a[ch][i] - b[ch][i]));
    R.check ("Chain: block size 512 vs 37 identical output", diff < 1e-5f, fmt ("max diff %.2e", diff));

    // bit-exact bypass after the ramp, and bypass gain (A/B compare)
    auto byp = defaultParamValues(); byp[pBypass] = 1.0f;
    sx::dsp::Chain cb; cb.prepare (sr, 512); cb.setParams (byp); cb.reset();
    auto in = voice, out = voice;
    processChain (cb, out, 256);
    bool exact = true;
    for (size_t ch = 0; ch < 2; ++ch) for (size_t i = 48; i < in[ch].size(); ++i) exact &= out[ch][i] == in[ch][i - 48];
    R.check ("Chain: bypass is bit-exact (latency compensated)", exact, "all samples identical");
    cb.setBypassGainDb (-6.0f);
    auto out2 = voice;
    processChain (cb, out2, 256);
    const double ratio = vt::rms (out2[0], 48000, 150000) / vt::rms (voice[0], 48000 - 48, 150000 - 48);
    R.check ("Chain: level-matched compare gain on the bypass path", near (vt::toDb (ratio), -6.0) < 0.1, fmt ("%.2f dB", vt::toDb (ratio)));

    // EQ response matches the displayed curve
    auto eqv = neutral();
    eqv[pEqOn] = 1; eqv[pHpfOn] = 1; eqv[pHpfFreq] = 90;
    eqv[pLowGain] = -4; eqv[pLowFreq] = 300; eqv[pHarshGain] = -6; eqv[pHarshFreq] = 3200; eqv[pPresGain] = 3; eqv[pAirGain] = 4;
    double worst = 0;
    for (double f : { 60.0, 150.0, 300.0, 1000.0, 3200.0, 5000.0, 12000.0 })
    {
        sx::dsp::Chain ce; ce.prepare (sr, 512); ce.setParams (eqv); ce.reset();
        auto t = sxt::sine (sr, 1.0, f, -20.0, 1);
        processChain (ce, t, 512);
        const double meas = vt::toDb (vt::toneAmplitude (t[0], sr, f, 12000, 24000) / std::pow (10.0, -1.0));
        worst = std::max (worst, near (meas, sx::dsp::Chain::responseDb (eqv, sr, f)));
    }
    R.check ("Chain: EQ display curve matches measured response", worst < 0.25, fmt ("max error %.3f dB", worst));

    // robustness: NaN/Inf input and random parameter extremes
    std::mt19937 rng (77);
    bool finite = true;
    for (int k = 0; k < 60; ++k)
    {
        auto v = defaultParamValues();
        for (int i = 0; i < kNumParams; ++i)
        {
            std::uniform_real_distribution<float> u (kParamDefs[(size_t) i].min, kParamDefs[(size_t) i].max);
            v[(size_t) i] = clampParam (i, u (rng));
        }
        sx::dsp::Chain cr; cr.prepare (sr, 256); cr.setParams (v); cr.reset();
        auto t = sxt::voice (sxt::VoiceSpec { 1.0, sr, 2 });
        t[0][1000] = std::numeric_limits<float>::quiet_NaN();
        t[1][2000] = std::numeric_limits<float>::infinity();
        processChain (cr, t, 256);
        finite &= vt::allFinite (t) && vt::peak (t) < 70.0f;
    }
    R.check ("Chain: 60 random parameter sets + NaN/Inf input stay finite", finite, "all outputs finite");

    // CPU: stereo 48 kHz, every module on
    auto all = defaultParamValues();
    for (int i : { pExciteOn, pDoubleOn }) all[(size_t) i] = 1.0f;
    sx::dsp::Chain cc; cc.prepare (sr, 512); cc.setParams (all); cc.reset();
    auto t = sxt::voice (sxt::VoiceSpec { 20.0, sr, 2 });
    const auto t0 = std::chrono::steady_clock::now();
    processChain (cc, t, 512);
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    R.check ("Chain CPU (stereo 48 kHz, all modules): < 10 % of one core", secs / 20.0 < 0.10, fmt ("%.2f %% of real time", 100.0 * secs / 20.0));
}

//==============================================================================
Report advise (const Buffer& b, double sr, Target t, sx::ParamValues p, Context c = Context::IsolatedVocal, const Buffer* sc = nullptr)
{
    AdvisorInput in;
    in.raw = &b; in.sidechain = sc; in.sampleRate = sr; in.target = std::move (t); in.current = p; in.context = c;
    return Advisor (in).run();
}

const Recommendation* find (const Report& r, const std::string& key)
{
    for (auto& x : r.recs) if (x.key == key) return &x;
    return nullptr;
}

float changeTo (const Recommendation* r, int index)
{
    if (r != nullptr) for (auto& c : r->changes) if (c.index == index) return c.to;
    return std::numeric_limits<float>::quiet_NaN();
}

std::string keys (const Report& r)
{
    std::string s;
    for (auto& x : r.recs) s += (s.empty() ? "" : ",") + x.key;
    return s.empty() ? "none" : s;
}

void testAdvisorSynthetic()
{
    const double sr = 48000.0;
    sxt::VoiceSpec base;
    base.seconds = 12.0;
    const auto clean = sxt::voice (base);
    const auto ref = referenceTarget (analyse (clean, sr, AnalysisOptions {}), "clean");

    {
        Buffer tiny ((size_t) 1, std::vector<float> ((size_t) sr * 2, 0.0f));
        auto r = advise (tiny, sr, genreTarget (Genre::AfrobeatLead), neutral());
        R.check ("Advisor: silence -> no recommendations, explains why", ! r.valid && r.recs.empty() && ! r.status.empty(), r.status);
    }
    {
        auto r = advise (clean, sr, ref, neutral());
        R.check ("Advisor: clean voice vs its own reference -> no recommendations (no false positives)", r.valid && r.recs.empty(), keys (r));
    }
    {
        auto s = base; s.mudDb = 8.0;
        auto r = advise (sxt::voice (s), sr, ref, neutral());
        const auto* m = find (r, "lowmid");
        const float f = changeTo (m, sx::pLowFreq), gdb = changeTo (m, sx::pLowGain);
        R.check ("Advisor: +8 dB mud at 300 Hz -> low-mid cut near 300 Hz, at most 4 dB",
                 m != nullptr && f > 240 && f < 380 && gdb < 0 && gdb >= -4.0f && m->predicted.size() == 1,
                 m ? fmt ("%.0f Hz, %.1f dB; ", f, gdb) + m->predicted.front() : keys (r));
    }
    {
        auto s = base; s.harshDb = 9.0;
        auto r = advise (sxt::voice (s), sr, ref, neutral());
        const auto* m = find (r, "harsh");
        const float f = changeTo (m, sx::pHarshFreq);
        R.check ("Advisor: +9 dB resonance at 3.2 kHz -> harsh cut near 3.2 kHz", m != nullptr && f > 2700 && f < 3900,
                 m ? fmt ("%.0f Hz, %.1f dB", f, changeTo (m, sx::pHarshGain)) : keys (r));
    }
    {
        auto s = base; s.rumble = 0.03;
        auto r = advise (sxt::voice (s), sr, ref, neutral());
        const auto* m = find (r, "lowend");
        const float f = changeTo (m, sx::pHpfFreq);
        R.check ("Advisor: rumble -> high-pass below the lowest sung note", m != nullptr && f > 40 && f <= 0.8f * (float) base.baseHz,
                 m ? fmt ("HPF %.0f Hz (lowest note %.0f Hz)", f, base.baseHz) : keys (r));
    }
    {
        auto s = base; s.sibilance = 1.0;
        auto r = advise (sxt::voice (s), sr, ref, neutral());
        const auto* m = find (r, "deess");
        R.check ("Advisor: strong sibilance -> de-esser proposal with simulated reduction", m != nullptr && m->predicted.size() >= 2,
                 m ? m->predicted[0] + " / " + m->predicted[1] : keys (r));
    }
    {
        auto s = base; s.phraseSwingDb = 14.0;
        auto r = advise (sxt::voice (s), sr, ref, neutral());
        const auto* m = find (r, "comp");
        R.check ("Advisor: +/-7 dB phrase swings -> compression, bounded gain reduction", m != nullptr && m->predicted.size() >= 2,
                 m ? m->predicted[0] + " / " + m->predicted[1] : keys (r));
    }
    {
        // pre-compressed source: strong limiting before the plugin
        auto v = clean;
        for (auto& ch : v) for (auto& x : ch) x = std::tanh (x * 8.0f) * 0.3f;
        auto r = advise (v, sr, genreTarget (Genre::AfrobeatLead), sx::defaultParamValues());
        R.check ("Advisor: heavily limited source -> no extra compression proposed", find (r, "comp") == nullptr, keys (r));
    }
    {
        auto s = base; s.clipGain = 6.0;
        auto r = advise (sxt::voice (s), sr, genreTarget (Genre::AfrobeatLead), sx::defaultParamValues());
        const auto* m = find (r, "clipping");
        R.check ("Advisor: clipped recording -> clipping note (information only, no parameter changes)",
                 m != nullptr && ! m->isActionable() && m->severity >= 2, m ? m->evidence.front().text : keys (r));
    }
    {
        auto s = base; s.channels = 2; s.invertRight = true;
        auto r = advise (sxt::voice (s), sr, ref, neutral());
        R.check ("Advisor: polarity-inverted stereo -> mono-compatibility warning", find (r, "stereo") != nullptr, keys (r));
    }
    {
        auto s = base; s.channels = 2;
        auto r = advise (sxt::voice (s), sr, mixTarget (Genre::AfrobeatLead), sx::defaultParamValues(), Context::FullMix);
        bool onlyInfo = true;
        for (auto& x : r.recs) if (x.isActionable() && x.key != "mixonly") onlyInfo = false;
        R.check ("Advisor: full-mix context -> no vocal processing changes, offers analysis-only mode",
                 onlyInfo && find (r, "mixonly") != nullptr && find (r, "mixloud") != nullptr, keys (r));
    }
    {
        auto s = base; s.channels = 2;
        const auto v = sxt::voice (s);
        auto beat = sxt::sine (sr, s.seconds, 60.0, -6.0, 2);
        auto r = advise (v, sr, ref, neutral(), Context::IsolatedVocal, &beat);
        R.check ("Advisor: sidechain beat -> vocal-vs-beat level check uses it", r.hasSidechain && find (r, "beat") != nullptr, keys (r));
    }
    {
        // the whole plan must move the measured spectrum towards the target
        auto s = base; s.mudDb = 7.0; s.harshDb = 7.0;
        auto r = advise (sxt::voice (s), sr, ref, neutral());
        auto err = [&] (const Measurements& m)
        {
            const auto d = detail::deviation (m, ref);
            double e = 0; for (int b = 0; b < kNumBands; ++b) if (m.bandValid[(size_t) b] && bandCentreHz (b) > 100 && bandCentreHz (b) < 12000) e += std::abs (d[(size_t) b]);
            return e;
        };
        R.check ("Advisor: applying the plan reduces the total deviation from the target", err (r.predicted) < err (r.current) * 0.8,
                 fmt ("sum |dev| %.0f -> %.0f dB", err (r.current), err (r.predicted)));
    }
    {
        // safety bounds across random voices and every genre target
        std::mt19937 rng (21);
        std::uniform_real_distribution<double> u (0, 1);
        bool ok = true; std::string why;
        for (int k = 0; k < 8; ++k)
        {
            auto s = base;
            s.seconds = 8; s.seed = (unsigned) k + 100; s.baseHz = 110 + 300 * u (rng); s.mudDb = 10 * u (rng) - 3; s.harshDb = 10 * u (rng) - 3;
            s.sibilance = u (rng); s.phraseSwingDb = 12 * u (rng); s.rumble = 0.02 * u (rng);
            const auto v = sxt::voice (s);
            for (int g = 0; g < (int) Genre::kNumGenres; ++g)
            {
                auto r = advise (v, sr, genreTarget ((Genre) g), sx::defaultParamValues());
                for (auto& x : r.recs)
                    for (auto& c : x.changes)
                    {
                        const std::string id = sx::kParamDefs[(size_t) c.index].id;
                        const float delta = c.to - c.from;
                        if ((id == "lowGain" || id == "boxGain" || id == "harshGain") && delta < -4.01f) { ok = false; why = id + fmt (" cut %.1f", delta); }
                        if ((id == "presGain" || id == "airGain") && delta > 3.01f) { ok = false; why = id + fmt (" boost %.1f", delta); }
                    }
                if (r.compGrTypicalPlan > 6.5) { ok = false; why = fmt ("plan GR %.1f dB", r.compGrTypicalPlan); }
                if (! vt::allFinite ({ std::vector<float> { (float) r.predicted.integratedLufs } })) { ok = false; why = "non-finite"; }
            }
        }
        R.check ("Advisor bounds (8 random voices x 6 targets): cuts <= 4 dB, boosts <= 3 dB, GR <= 6.5 dB", ok, ok ? "48 analyses within bounds" : why);
    }
}

void testAdvisorReal (const std::string& reportPath)
{
    std::vector<std::string> files;
    if (DIR* d = opendir (realDir.c_str()))
    {
        while (auto* e = readdir (d)) if (std::string (e->d_name).size() > 4 && std::string (e->d_name).find (".wav") != std::string::npos) files.push_back (e->d_name);
        closedir (d);
    }
    std::sort (files.begin(), files.end());
    if (files.empty())
    {
        std::printf ("[SKIP] real-recording tests: no clips in %s\n", realDir.c_str());
        return;
    }
    auto load = [&] (const std::string& f) { Buffer b; double sr; sxt::readWav (realDir + "/" + f, b, sr); return std::make_pair (b, sr); };
    std::vector<Buffer> sungParts, spokenParts;
    double sr = 44100.0;
    for (auto& f : files)
    {
        auto [b, rate] = load (f);
        sr = rate;
        (f.rfind ("spoken", 0) == 0 ? spokenParts : sungParts).push_back (b);
    }
    auto cat = [] (const std::vector<Buffer>& parts) { Buffer out ((size_t) 1); for (auto& p : parts) out[0].insert (out[0].end(), p[0].begin(), p[0].end()); return out; };
    const auto sung = cat (sungParts), spoken = cat (spokenParts);

    std::ofstream rep (reportPath);
    rep << "# SwitaxVoice Assist on real recordings\n\nMaterial: VocalSet (Wilkins et al. 2018, CC BY 4.0), " << files.size()
        << " clips: singer f1 sung (arpeggios, scales, long tones; belt, breathy, straight, vibrato, forte) and three singers speaking.\n"
        << "Default SwitaxVoice settings unless noted. These reports are produced by the same code the plugin runs.\n\n";
    auto dump = [&] (const char* title, const Report& r)
    {
        rep << "## " << title << "\n\n**" << r.status << "**\n\n";
        rep << detail::fmt ("Raw: %.1f LUFS, level range %.1f dB, crest %.1f dB, f0 %.0f-%.0f Hz (%.0f %% voiced), sibilant events %.0f, noise floor %.1f dBFS.\n\n",
                    r.raw.integratedLufs, r.raw.levelRange, r.raw.crestDb, r.raw.pitch.p5, r.raw.pitch.p95, r.raw.pitch.voicedFraction * 100.0,
                    (double) r.raw.sibilance.events, r.raw.noiseFloorDb);
        for (auto& x : r.recs)
        {
            rep << "### [" << x.category << "] " << x.title << fmt ("  (confidence %.0f %%)\n", x.confidence * 100.0);
            for (auto& e : x.evidence) rep << "- **" << basisLabel (e.basis) << "**: " << e.text << "\n";
            rep << "- **HEURISTIC**: " << x.issue << "\n- **PROPOSAL**: " << x.proposal << "\n";
            for (auto& p : x.predicted) rep << "- **SIMULATED**: " << p << "\n";
            rep << "- *Limitations*: " << x.limitations << "\n\n";
        }
    };

    {
        auto r = advise (sung, sr, referenceTarget (analyse (sung, sr, AnalysisOptions {}), "self"), neutral());
        R.check ("Real voice: sung take vs its own reference -> no recommendations", r.valid && r.recs.empty(), keys (r));
        dump ("Real sung voice vs its own reference (neutral settings)", r);
    }
    {
        auto mud = sung;
        vs::dsp::SvfState st;
        const auto c = vs::dsp::designSvf (vs::dsp::SvfType::Bell, sr, 300, 1.2, 7);
        for (auto& x : mud[0]) x = st.process (c, x);
        auto r = advise (mud, sr, referenceTarget (analyse (sung, sr, AnalysisOptions {}), "original"), neutral());
        const auto* m = find (r, "lowmid");
        const float f = changeTo (m, sx::pLowFreq);
        R.check ("Real voice: +7 dB at 300 Hz injected -> found within 1/3 octave", m != nullptr && f > 240 && f < 380, m ? fmt ("%.0f Hz", f) : keys (r));
        dump ("Real sung voice with +7 dB at 300 Hz injected, vs the original", r);
    }
    {
        auto hs = sung;
        vs::dsp::SvfState st;
        const auto c = vs::dsp::designSvf (vs::dsp::SvfType::Bell, sr, 3500, 3.0, 8);
        for (auto& x : hs[0]) x = st.process (c, x);
        auto r = advise (hs, sr, referenceTarget (analyse (sung, sr, AnalysisOptions {}), "original"), neutral());
        const auto* m = find (r, "harsh");
        const float f = changeTo (m, sx::pHarshFreq);
        R.check ("Real voice: +8 dB at 3.5 kHz injected -> found within 1/3 octave", m != nullptr && f > 2800 && f < 4400, m ? fmt ("%.0f Hz", f) : keys (r));
        dump ("Real sung voice with +8 dB at 3.5 kHz injected, vs the original", r);
    }
    {
        auto r = advise (spoken, sr, genreTarget (Genre::AfrobeatLead), sx::defaultParamValues());
        R.check ("Real voice: speech has real sibilants detected, not flagged as excessive",
                 r.raw.sibilance.events >= 5 && find (r, "deess") == nullptr, fmt ("%.0f events, ", (double) r.raw.sibilance.events) + keys (r));
        dump ("Three real speakers vs Afrobeat Lead house target (default settings)", r);
    }
    {
        auto r = advise (sung, sr, genreTarget (Genre::AfrobeatLead), sx::defaultParamValues());
        bool bounded = true;
        for (auto& x : r.recs) for (auto& c : x.changes)
        {
            const std::string id = sx::kParamDefs[(size_t) c.index].id;
            if ((id == "lowGain" || id == "boxGain" || id == "harshGain") && c.to - c.from < -4.01f) bounded = false;
            if ((id == "presGain" || id == "airGain") && c.to - c.from > 3.01f) bounded = false;
        }
        R.check ("Real voice: sung take vs Afrobeat house target -> bounded proposals", r.valid && bounded, keys (r));
        dump ("Real sung voice vs Afrobeat Lead house target (default settings)", r);
    }
}

} // namespace

int main (int argc, char** argv)
{
    if (argc > 1) outDir = argv[1];
    if (argc > 2) realDir = argv[2];
    ::mkdir (outDir.c_str(), 0755);
    const auto t0 = std::chrono::steady_clock::now();

    testLoudnessStandards();
    testTruePeak();
    testSpectrum();
    testPitch();
    testActivityClippingStereo();
    testSibilance();
    testChain();
    testAdvisorSynthetic();
    testAdvisorReal (outDir + "/real_recordings_report.md");

    R.writeReport (outDir + "/core_test_report.md", "SwitaxVoice core tests");
    std::printf ("\n%d tests, %d failures, %.1f s\n", (int) R.all().size(), R.failures(),
                 std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count());
    return R.failures() == 0 ? 0 : 1;
}
