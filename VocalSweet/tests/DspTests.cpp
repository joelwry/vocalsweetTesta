// VocalSweet DSP test-suite (framework independent). Usage: VocalSweetDspTests [outputDir]
// Every check prints PASS/FAIL with the actual measurement and a Markdown report is written.

#include "TestUtils.h"
#include <filesystem>

using namespace vt;
using vs::dsp::VocalChain;
namespace fs = std::filesystem;

static Registry R;

static vs::ParamValues presetValues (const char* name)
{
    for (auto& p : vs::factoryPresets()) if (p.name == name) return p.values;
    return vs::defaultParamValues();
}

static double maxAbsDiffDelayed (const Buffer& in, const Buffer& out, int delay, size_t from = 0)
{
    double m = 0.0;
    for (size_t c = 0; c < in.size(); ++c)
        for (size_t i = std::max<size_t> (from, (size_t) delay); i < out[c].size(); ++i)
            m = std::max (m, (double) std::abs (out[c][i] - in[c][i - (size_t) delay]));
    return m;
}

static double maxStep (const std::vector<float>& x, size_t from, size_t to)
{
    double m = 0.0;
    for (size_t i = std::max<size_t> (from, 1); i < std::min (to, x.size()); ++i) m = std::max (m, (double) std::abs (x[i] - x[i - 1]));
    return m;
}

//==============================================================================
static void testSilenceAndLowLevel()
{
    for (const auto& preset : vs::factoryPresets())
    {
        VocalChain c;
        prepareWith (c, 48000, preset.values);
        auto b = makeBuffer (2, 48000 * 2);
        processChain (c, b, 512);
        if (peak (b) > 1.0e-9f || ! allFinite (b))
        {
            R.check ("Silence in -> silence out (all presets)", false, preset.name + fmt (": peak %.3g", peak (b)));
            return;
        }
    }
    R.check ("Silence in -> silence out (all presets)", true, "peak output = 0.0 for all 9 presets (2 s stereo)");

    VocalChain c;
    prepareWith (c, 48000, presetValues ("Bright Pop Vocal"));
    auto b = makeBuffer (2, 48000 * 2);
    sine (b, 48000, 1000, 1.0e-5f);   // -100 dBFS
    processChain (c, b, 256);
    const double outDb = toDb (rms (b[0], 48000));
    R.check ("Very low level (-100 dBFS sine) stays stable & quiet", allFinite (b) && outDb < -80.0,
             fmt ("output RMS %.1f dBFS (input -103.0 dBFS RMS)", outDb));
}

static void testSineAndImpulse()
{
    {
        VocalChain c;
        prepareWith (c, 48000, vs::defaultParamValues());
        auto b = makeBuffer (2, 48000);
        sine (b, 48000, 1000, 0.125f);   // -18 dBFS peak
        const double inDb = toDb (rms (b[0]));
        processChain (c, b, 512);
        const double outDb = toDb (rms (b[0], 24000));
        R.check ("1 kHz sine -18 dBFS through default chain", allFinite (b) && std::abs (outDb - inDb) < 12.0,
                 fmt ("in %.1f dBFS RMS -> out %.1f dBFS RMS", inDb, outDb));
    }
    {
        auto v = allModulesOff();
        v[vs::pHpfOn] = 1; v[vs::pEqOn] = 1; v[vs::pMudGain] = -9; v[vs::pPresGain] = 9; v[vs::pAirGain] = 9; v[vs::pTone] = 50;
        VocalChain c;
        prepareWith (c, 48000, v);
        auto b = makeBuffer (1, 48000);
        b[0][100] = 1.0f;
        processChain (c, b, 64);
        double total = 0, tail = 0;
        for (size_t i = 0; i < b[0].size(); ++i) { total += (double) b[0][i] * b[0][i]; if (i > 24000) tail += (double) b[0][i] * b[0][i]; }
        R.check ("Impulse response finite and decaying (EQ +/-9 dB)", allFinite (b) && tail < total * 1.0e-10,
                 fmt ("energy after 0.5 s = %.2e of total", tail / std::max (total, 1e-30)));
    }
    {
        VocalChain c;
        prepareWith (c, 48000, allModulesOff());
        auto b = makeBuffer (2, 4800);
        whiteNoise (b, 0.3f);
        auto in = b;
        processChain (c, b, 100);
        const double err = maxAbsDiffDelayed (in, b, c.getLatencySamples());
        R.check ("All modules off = transparent (delayed by latency only)", err < 1.0e-6, fmt ("max |error| = %.2e", err));
    }
}

static void testBroadband()
{
    bool ok = true; double worst = 0; std::string worstName;
    for (const auto& preset : vs::factoryPresets())
    {
        VocalChain c;
        prepareWith (c, 48000, preset.values);
        auto b = makeBuffer (2, 48000 * 2);
        pinkNoise (b, 1.0f);   // ~ -12 dBFS RMS pink noise, broadband
        processChain (c, b, 512);
        const double ceil = std::pow (10.0, preset.values[vs::pCeiling] / 20.0);
        const double pk = peak (b);
        if (! allFinite (b) || pk > ceil + 1e-6) ok = false;
        if (toDb (pk) > worst || worstName.empty()) { worst = toDb (pk); worstName = preset.name; }
    }
    R.check ("Broadband pink noise through all presets", ok, fmt ("max output peak %.2f dBFS (ceiling -1.0)", worst) + " [" + worstName + "]");

    VocalChain c;
    prepareWith (c, 48000, presetValues ("Afrobeat Lead"));
    auto b = makeBuffer (2, 48000 * 2);
    whiteNoise (b, 0.5f);
    processChain (c, b, 333);
    R.check ("White noise -9 dBFS RMS (Afrobeat Lead)", allFinite (b) && peak (b) <= 0.8913f + 1e-6f, fmt ("output peak %.2f dBFS", toDb (peak (b))));
}

static void testMonoStereo()
{
    auto v = presetValues ("Afrofusion Lead");
    VocalChain mono, stereo;
    prepareWith (mono, 48000, v);
    prepareWith (stereo, 48000, v);
    auto sig = syntheticVocal (48000, 2.0, 1);
    auto m = sig;
    Buffer s { sig[0], sig[0] };
    processChain (mono, m, 256);
    processChain (stereo, s, 256);
    double err = 0; for (size_t i = 0; i < m[0].size(); ++i) err = std::max (err, (double) std::abs (m[0][i] - s[0][i]));
    double lr = 0; for (size_t i = 0; i < s[0].size(); ++i) lr = std::max (lr, (double) std::abs (s[0][i] - s[1][i]));
    R.check ("Mono == stereo(L) for identical L/R input", err < 1e-6 && lr < 1e-6, fmt ("max mono/stereo diff %.2e, L/R diff %.2e", err, lr));

    // doubler must create a real stereo image from a mono source
    VocalChain d;
    prepareWith (d, 48000, presetValues ("Vocal Double"));
    Buffer ds { sig[0], sig[0] };
    processChain (d, ds, 256);
    double sxy = 0, sxx = 0, syy = 0;
    for (size_t i = 0; i < ds[0].size(); ++i) { sxy += ds[0][i] * ds[1][i]; sxx += ds[0][i] * ds[0][i]; syy += ds[1][i] * ds[1][i]; }
    const double corr = sxy / std::sqrt (sxx * syy + 1e-30);
    R.check ("Vocal Double widens a centred source", corr < 0.995 && allFinite (ds), fmt ("L/R correlation %.4f", corr));

    VocalChain dm;
    prepareWith (dm, 48000, presetValues ("Vocal Double"));
    auto dmb = sig;
    processChain (dm, dmb, 256);
    R.check ("Doubler on mono bus is stable", allFinite (dmb) && peak (dmb) <= 0.8913f + 1e-6f, fmt ("mono peak %.2f dBFS", toDb (peak (dmb))));
}

static void testRatesAndBlocks()
{
    const double rates[] = { 22050, 44100, 48000, 88200, 96000, 176400, 192000 };
    const int blocks[] = { 1, 7, 32, 64, 128, 441, 512, 1024, 4096 };
    int runs = 0; bool ok = true; std::string bad;
    for (double sr : rates)
        for (int bs : blocks)
        {
            VocalChain c;
            prepareWith (c, sr, presetValues ("Bright Pop Vocal"), bs);
            auto b = syntheticVocal (sr, 1.0, 2, 0.6f);
            processChain (c, b, bs);
            ++runs;
            if (! allFinite (b) || peak (b) > 0.8913f + 1e-6f || rms (b[0]) < 1e-4) { ok = false; bad = fmt ("sr %.0f block %.0f", sr, bs); }
        }
    R.check ("7 sample rates x 9 buffer sizes (22.05k-192k, 1-4096)", ok, std::to_string (runs) + " runs stable, peak <= ceiling" + (ok ? "" : " FAILED at " + bad));

    // block-size invariance (static parameters)
    auto ref = syntheticVocal (48000, 1.5, 2);
    auto a = ref, b = ref;
    VocalChain c1, c2;
    prepareWith (c1, 48000, presetValues ("Melodic Rap"));
    prepareWith (c2, 48000, presetValues ("Melodic Rap"));
    processChain (c1, a, 1);
    processChain (c2, b, 4096);
    double err = 0;
    for (size_t ch = 0; ch < 2; ++ch) for (size_t i = 0; i < a[ch].size(); ++i) err = std::max (err, (double) std::abs (a[ch][i] - b[ch][i]));
    R.check ("Block-size invariance (block 1 vs 4096)", err < 1.0e-5, fmt ("max difference %.2e", err));

    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        VocalChain c;
        prepareWith (c, sr, allModulesOff());
        auto imp = makeBuffer (1, 1024);
        imp[0][10] = 1.0f;
        processChain (c, imp, 128);
        int at = 0; for (int i = 0; i < 1024; ++i) if (std::abs (imp[0][(size_t) i]) > 0.5f) { at = i - 10; break; }
        R.check (fmt ("Latency @ %.1f kHz", sr / 1000.0), at == c.getLatencySamples(),
                 fmt ("measured %.0f samples = %.3f ms (reported %.0f)", at, 1000.0 * at / sr, c.getLatencySamples()));
    }
}

static double measureGainDb (const vs::ParamValues& v, double sr, double freq)
{
    VocalChain c;
    prepareWith (c, sr, v);
    const int n = (int) sr;
    auto b = makeBuffer (1, n);
    sine (b, sr, freq, 0.1f);
    processChain (c, b, 512);
    const size_t len = (size_t) (sr * 0.5);
    return toDb (toneAmplitude (b[0], sr, freq, (size_t) n - len, len) / 0.1);
}

static void testFilterResponse()
{
    const double sr = 48000;
    double worst = 0; std::string detail;
    for (int slope : { 0, 1 })
    {
        auto v = allModulesOff();
        v[vs::pHpfOn] = 1; v[vs::pHpfFreq] = 100; v[vs::pHpfSlope] = (float) slope;
        auto eff = vs::computeEffective (v);
        for (double f : { 30.0, 60.0, 100.0, 200.0, 1000.0 })
        {
            const double meas = measureGainDb (v, sr, f);
            const double expect = VocalChain::responseDb (eff, sr, f);
            worst = std::max (worst, std::abs (meas - expect));
            if (f == 30.0) detail += std::string (slope ? "24dB" : "12dB") + fmt (": %.1f dB @30Hz, ", meas);
        }
    }
    R.check ("HPF response (12/24 dB) matches design", worst < 0.25, detail + fmt ("max dev %.3f dB", worst));

    auto v = allModulesOff();
    v[vs::pEqOn] = 1; v[vs::pMudGain] = -6; v[vs::pMudFreq] = 300; v[vs::pBoxGain] = -4; v[vs::pPresGain] = 6; v[vs::pAirGain] = 6;
    v[vs::pTone] = 40; v[vs::pWarmth] = 0;
    auto eff = vs::computeEffective (v);
    worst = 0; detail.clear();
    for (double f : { 100.0, 300.0, 750.0, 1000.0, 3500.0, 6000.0, 11000.0, 16000.0 })
    {
        const double meas = measureGainDb (v, sr, f);
        const double expect = VocalChain::responseDb (eff, sr, f);
        worst = std::max (worst, std::abs (meas - expect));
        if (f == 300.0 || f == 3500.0 || f == 11000.0) detail += fmt ("%.0f Hz %+.2f dB, ", f, meas);
    }
    R.check ("EQ + Tone response == EQ display curve", worst < 0.25, detail + fmt ("max dev %.3f dB", worst));
}

static void testCompressor()
{
    auto v = allModulesOff();
    v[vs::pCompOn] = 1; v[vs::pCompThresh] = -20; v[vs::pCompRatio] = 4; v[vs::pCompAttack] = 5; v[vs::pCompRelease] = 100; v[vs::pCompMakeup] = 0;
    VocalChain c;
    prepareWith (c, 48000, v);
    auto b = makeBuffer (1, 48000);
    sine (b, 48000, 1000, 0.5f);   // -6 dBFS peak
    const double inDb = toDb (rms (b[0]));
    processChain (c, b, 256);
    const double outDb = toDb (rms (b[0], 24000));
    const double autoMakeup = vs::computeEffective (v).compMakeup;   // automatic makeup is part of the design
    const double gr = inDb - outDb + autoMakeup;
    R.check ("Compressor GR: -6 dBFS sine, thr -20, 4:1", gr > 6.0 && gr < 11.5 && std::abs (gr - c.getMeters().compGrDb) < 1.0,
             fmt ("measured GR %.2f dB (static peak-based 10.5 dB), meter %.2f dB, ", gr, c.getMeters().compGrDb) + fmt ("auto makeup %.2f dB", autoMakeup));

    VocalChain c2;
    prepareWith (c2, 48000, v);
    auto q = makeBuffer (1, 48000);
    sine (q, 48000, 1000, 0.0316f);   // -30 dBFS
    processChain (c2, q, 256);
    R.check ("Compressor below threshold: no gain reduction", c2.getMeters().compGrDb < 0.5,
             fmt ("GR %.3f dB at -30 dBFS", c2.getMeters().compGrDb));

    // attack timing: GR reaches ~63%% of final within a few attack times
    VocalChain c3;
    v[vs::pCompAttack] = 10;
    prepareWith (c3, 48000, v);
    auto st = makeBuffer (1, 48000);
    sine (st, 48000, 1000, 0.5f);
    std::vector<float> grTrace;
    std::vector<float*> ptr (1);
    for (int i = 0; i < 48000; i += 48) { ptr[0] = st[0].data() + i; c3.process (ptr.data(), 1, 48); grTrace.push_back (c3.getMeters().compGrDb); }
    const float fin = grTrace.back();
    int t63 = 0; for (size_t i = 0; i < grTrace.size(); ++i) if (grTrace[i] >= 0.63f * fin) { t63 = (int) i; break; }
    R.check ("Compressor attack time (10 ms setting)", t63 >= 1 && t63 <= 40, fmt ("63%% of final GR after ~%.0f ms", (double) t63));
}

static void testSaturation()
{
    auto base = allModulesOff();
    base[vs::pSatOn] = 1;
    auto run = [&] (float drive, float warmth, double f, float amp, double& h2, double& h3, double& lvl, double& dc, Buffer* outBuf = nullptr)
    {
        auto v = base; v[vs::pSatDrive] = drive; v[vs::pWarmth] = warmth;
        VocalChain c; prepareWith (c, 48000, v);
        auto b = makeBuffer (1, 48000);
        sine (b, 48000, f, amp);
        const double inR = rms (b[0]);
        processChain (c, b, 512);
        const size_t from = 24000, len = 24000;
        const double a1 = toneAmplitude (b[0], 48000, f, from, len);
        h2 = toDb (toneAmplitude (b[0], 48000, 2 * f, from, len) / a1);
        h3 = toDb (toneAmplitude (b[0], 48000, 3 * f, from, len) / a1);
        lvl = toDb (rms (b[0], from) / inR);
        double s = 0; for (size_t i = from; i < b[0].size(); ++i) s += b[0][i]; dc = s / (double) len;
        if (outBuf) *outBuf = b;
    };
    double h2, h3, lvl, dc;
    run (0, 0, 200, 0.5f, h2, h3, lvl, dc);
    R.check ("Saturation drive 0 adds no harmonics", h2 < -90 && h3 < -90, fmt ("H2 %.1f dB, H3 %.1f dB", h2, h3));
    run (60, 0, 200, 0.5f, h2, h3, lvl, dc);
    R.check ("Saturation drive 60%: controlled harmonics", h3 > -50 && h3 < -10 && std::abs (lvl) < 3.0,
             fmt ("H2 %.1f dB, H3 %.1f dB, level change %.2f dB", h2, h3, lvl));
    run (60, 100, 200, 0.5f, h2, h3, lvl, dc);
    R.check ("Warmth adds even harmonics (asymmetry), DC blocked", h2 > -45 && std::abs (dc) < 1e-3,
             fmt ("H2 %.1f dB, DC %.1e", h2, dc));
    // aliasing check: 7 kHz at full drive; 5th harmonic (35 kHz) would alias to 13 kHz
    Buffer out;
    run (100, 0, 7000, 0.7f, h2, h3, lvl, dc, &out);
    const double a1 = toneAmplitude (out[0], 48000, 7000, 24000, 24000);
    const double alias = toDb (toneAmplitude (out[0], 48000, 13000, 24000, 24000) / a1);
    R.check ("Saturation aliasing (ADAA) at 7 kHz, drive 100%", alias < -45, fmt ("13 kHz alias component %.1f dB rel. fundamental", alias));
}

static void testDeEsser()
{
    auto v = allModulesOff();
    v[vs::pDeessOn] = 1; v[vs::pDeessThresh] = -30; v[vs::pDeessRange] = 10; v[vs::pDeessFreq] = 6500;
    VocalChain c; prepareWith (c, 48000, v);
    auto b = makeBuffer (1, 48000);
    whiteNoise (b, 1.0f, 3);
    {   // band-limit to 5-9 kHz "sibilance"
        vs::dsp::SvfState s1, s2;
        auto bp = vs::dsp::designSvf (vs::dsp::SvfType::BandPass, 48000, 7000, 1.2);
        for (auto& x : b[0]) x = 0.5f * s2.process (bp, s1.process (bp, x));
    }
    auto in = b;
    processChain (c, b, 256);
    const double red = toDb (rms (b[0], 24000) / rms (in[0], 24000));
    R.check ("De-esser reduces sibilant band", red < -3.0 && c.getMeters().deessGrDb > 3.0,
             fmt ("7 kHz band level change %.2f dB, GR meter %.2f dB", red, c.getMeters().deessGrDb));

    VocalChain c2; prepareWith (c2, 48000, v);
    auto t = makeBuffer (1, 48000);
    sine (t, 48000, 300, 0.25f);
    auto tin = t;
    processChain (c2, t, 256);
    const double chg = toDb (rms (t[0], 24000) / rms (tin[0], 24000));
    R.check ("De-esser leaves 300 Hz vowel tone untouched", std::abs (chg) < 0.1 && c2.getMeters().deessGrDb < 0.5,
             fmt ("level change %.3f dB, GR %.2f dB", chg, c2.getMeters().deessGrDb));
}

static void testParamExtremes()
{
    std::mt19937 rng (99);
    int runs = 0; bool ok = true; std::string bad;
    auto runOne = [&] (const vs::ParamValues& v, const std::string& label)
    {
        VocalChain c;
        prepareWith (c, 48000, v, 512);
        auto b = syntheticVocal (48000, 0.5, 2, 0.9f);
        processChain (c, b, 512);
        ++runs;
        const bool limOn = v[vs::pLimiterOn] > 0.5f && v[vs::pBypass] < 0.5f;
        const double ceil = std::pow (10.0, v[vs::pCeiling] / 20.0);
        if (! allFinite (b) || (limOn && peak (b) > ceil + 1e-6)) { ok = false; bad = label; }
    };
    vs::ParamValues mn {}, mx {};
    for (int i = 0; i < vs::kNumParams; ++i) { mn[(size_t) i] = vs::kParamDefs[(size_t) i].min; mx[(size_t) i] = vs::kParamDefs[(size_t) i].max; }
    mn[vs::pBypass] = 0; mx[vs::pBypass] = 0;
    runOne (mn, "all-min");
    runOne (mx, "all-max");
    for (int k = 0; k < 150; ++k)
    {
        vs::ParamValues v {};
        for (int i = 0; i < vs::kNumParams; ++i)
        {
            const auto& d = vs::kParamDefs[(size_t) i];
            std::uniform_real_distribution<float> u (d.min, d.max);
            v[(size_t) i] = vs::clampParam (i, u (rng));
        }
        runOne (v, "random #" + std::to_string (k));
    }
    R.check ("Parameter min / max / 150 random combinations", ok, std::to_string (runs) + " runs finite, limiter ceiling respected" + (ok ? "" : " FAILED " + bad));

    // macro layer: all macros at 0 must leave detailed values unchanged
    auto v = vs::defaultParamValues();
    for (int i : { vs::pSweeten, vs::pTone, vs::pWarmth, vs::pPresence, vs::pControl }) v[(size_t) i] = 0;
    auto e = vs::computeEffective (v);
    const bool same = e.mudGain == v[vs::pMudGain] && e.presGain == v[vs::pPresGain] && e.compThresh == v[vs::pCompThresh]
                      && e.compRatio == v[vs::pCompRatio] && e.deessThresh == v[vs::pDeessThresh] && e.bodyGain == 0 && e.tiltHiGain == 0
                      && std::abs (e.satDrive - v[vs::pSatDrive] * 0.01f) < 1e-6f;
    R.check ("Macros at zero = detailed settings exactly", same, "effective == base for EQ, comp, de-ess, saturation");
}

static void testMixAndBypass()
{
    auto v = vs::defaultParamValues();
    v[vs::pCeiling] = 0;
    auto sig = syntheticVocal (48000, 1.0, 2, 0.15f);
    auto render = [&] (float mix)
    {
        auto p = v; p[vs::pMix] = mix;
        VocalChain c; prepareWith (c, 48000, p);
        auto b = sig; processChain (c, b, 256); return b;
    };
    auto dry = render (0), wet = render (100), half = render (50);
    VocalChain tmp; tmp.prepare (48000, 256);
    const int L = tmp.getLatencySamples();
    const double errDry = maxAbsDiffDelayed (sig, dry, L);
    double errHalf = 0;
    for (size_t c = 0; c < 2; ++c) for (size_t i = 0; i < half[c].size(); ++i)
        errHalf = std::max (errHalf, (double) std::abs (half[c][i] - 0.5f * (dry[c][i] + wet[c][i])));
    R.check ("Dry/Wet 0% = original (latency aligned)", errDry < 1e-6, fmt ("max |error| %.2e", errDry));
    R.check ("Dry/Wet 50% = linear blend of dry and wet", errHalf < 1e-5, fmt ("max |error| %.2e", errHalf));

    {
        auto p = presetValues ("Bright Pop Vocal"); p[vs::pBypass] = 1;
        VocalChain c; prepareWith (c, 48000, p);
        auto b = sig; processChain (c, b, 256);
        const double err = maxAbsDiffDelayed (sig, b, c.getLatencySamples());
        R.check ("Bypass = bit-exact delayed input", err == 0.0, fmt ("max |error| %.2e", err));
    }
    {
        // toggle bypass mid-stream: crossfade must not create a step
        auto p = presetValues ("Bright Pop Vocal");
        VocalChain c; prepareWith (c, 48000, p);
        auto b = makeBuffer (2, 48000);
        sine (b, 48000, 220, 0.4f);
        std::vector<float*> ptr (2);
        double steady = 0, during = 0;
        for (int i = 0; i < 48000; i += 256)
        {
            if (i == 24064) { p[vs::pBypass] = 1; c.setParams (vs::computeEffective (p)); }
            ptr[0] = b[0].data() + i; ptr[1] = b[1].data() + i;
            c.process (ptr.data(), 2, std::min (256, 48000 - i));
        }
        steady = maxStep (b[0], 12000, 24000);
        during = maxStep (b[0], 24064, 26000);
        const double dryStep = 0.4 * 2.0 * vs::dsp::kPi * 220.0 / 48000.0;
        const double limit = 1.2 * std::max (steady, dryStep);
        R.check ("Bypass toggle is click-free (15 ms crossfade)", during < limit,
                 fmt ("max sample step during switch %.4f (processed %.4f, dry %.4f)", during, steady, dryStep));
    }
}

static void testInvalidInput()
{
    VocalChain c;
    prepareWith (c, 48000, presetValues ("Afrobeat Lead"));
    auto b = syntheticVocal (48000, 1.0, 2);
    const float bad[] = { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                          -std::numeric_limits<float>::infinity(), 1.0e30f, -1.0e30f, 1.0e-40f };
    for (int k = 0; k < 6; ++k)
        for (int i = 0; i < 64; ++i) b[0][(size_t) (10000 + k * 2000 + i)] = bad[k], b[1][(size_t) (10000 + k * 2000 + i * 2)] = bad[(k + 1) % 6];
    processChain (c, b, 128);
    const double tail = rms (b[0], 40000);
    R.check ("NaN / Inf / 1e30 / denormal input injection", allFinite (b) && tail > 1e-3,
             fmt ("all output finite, recovers (tail RMS %.1f dBFS), watchdog resets %.0f", toDb (tail), (double) c.getWatchdogResets()));
}

static void testClipping()
{
    auto run = [] (bool lim, float amp, double& pk)
    {
        auto v = vs::defaultParamValues(); v[vs::pLimiterOn] = lim ? 1.0f : 0.0f; v[vs::pInputGain] = 6;
        VocalChain c; prepareWith (c, 48000, v);
        auto b = makeBuffer (2, 48000); sine (b, 48000, 440, amp);
        processChain (c, b, 512); pk = peak (b); return allFinite (b);
    };
    double pk;
    bool ok = run (true, 4.0f, pk);
    R.check ("+12 dBFS input, limiter on, ceiling -1 dBFS", ok && pk <= 0.8913 + 1e-6, fmt ("output peak %.3f dBFS", toDb (pk)));
    double pk2;
    run (false, 4.0f, pk2);
    R.check ("Limiter off: output passes level (no hidden clipping)", pk2 > 0.8913, fmt ("output peak %.2f dBFS", toDb (pk2)));

    VocalChain c; prepareWith (c, 48000, presetValues ("Melodic Rap"));
    auto b = syntheticVocal (48000, 3.0, 2, 1.5f);   // very hot vocal
    processChain (c, b, 512);
    R.check ("Hot synthetic vocal (+3.5 dB over) stays under ceiling", peak (b) <= 0.8913f + 1e-6f, fmt ("output peak %.3f dBFS", toDb (peak (b))));
}

static void testSmoothing()
{
    // abrupt macro jumps and preset switches on a steady tone must not create steps
    auto p = vs::defaultParamValues();
    VocalChain c; prepareWith (c, 48000, p);
    auto b = makeBuffer (2, 96000);
    sine (b, 48000, 330, 0.3f);
    std::vector<float*> ptr (2);
    const auto& presets = vs::factoryPresets();
    for (int i = 0, k = 0; i < 96000; i += 512, ++k)
    {
        if (k > 0 && k % 20 == 0)
        {
            auto v = presets[(size_t) ((k / 20) % (int) presets.size())].values;
            v[vs::pSweeten] = (k / 20) % 2 ? 100.0f : 0.0f;
            c.setParams (vs::computeEffective (v));
        }
        ptr[0] = b[0].data() + i; ptr[1] = b[1].data() + i;
        c.process (ptr.data(), 2, std::min (512, 96000 - i));
    }
    const double maxD = maxStep (b[0], 2000, 96000);
    const double ideal = 0.3 * 2.0 * vs::dsp::kPi * 330.0 / 48000.0 * 2.5; // 2.5x a 330 Hz sine's max slope after +8 dB gain
    R.check ("Abrupt preset / Sweeten jumps are smoothed (no clicks)", maxD < ideal && allFinite (b),
             fmt ("max sample step %.4f (limit %.4f)", maxD, ideal));
}

static void testMacrosAndPresets (const std::string& outDir)
{
    const double sr = 48000;
    auto sig = syntheticVocal (sr, 4.0, 1, 0.3f);
    auto measure = [&] (const vs::ParamValues& v, std::array<double, 6>& bands, double& gr, double& pk)
    {
        VocalChain c; prepareWith (c, sr, v);
        auto b = sig;
        std::vector<float*> ptr (1);
        gr = 0;
        for (size_t i = 0; i < b[0].size(); i += 512)
        {
            ptr[0] = b[0].data() + i;
            c.process (ptr.data(), 1, (int) std::min<size_t> (512, b[0].size() - i));
            gr = std::max (gr, (double) c.getMeters().compGrDb);
        }
        const size_t from = (size_t) sr, len = (size_t) (sr * 2.5);
        const double total = toDb (rms (b[0], from, from + len));
        const double edges[7] = { 40, 120, 400, 1500, 4000, 9000, 16000 };
        for (int k = 0; k < 6; ++k) bands[(size_t) k] = bandEnergyDb (b[0], sr, edges[k], edges[k + 1], from, len) - total;
        pk = peak (b);
    };

    // SWEETEN sweep, measured on a voiced-only window (1.10-1.95 s) and a sibilant window (2.00-2.07 s)
    auto sweep = [&] (float sweeten, double& clarity, double& air, double& sib, double& gr, double& pk)
    {
        auto v = vs::defaultParamValues(); v[vs::pSweeten] = sweeten;
        VocalChain c; prepareWith (c, sr, v);
        auto b = sig;
        std::vector<float*> ptr (1);
        gr = 0;
        for (size_t i = 0; i < b[0].size(); i += 512)
        {
            ptr[0] = b[0].data() + i;
            c.process (ptr.data(), 1, (int) std::min<size_t> (512, b[0].size() - i));
            gr = std::max (gr, (double) c.getMeters().compGrDb);
        }
        const size_t v0 = (size_t) (1.10 * sr), vl = (size_t) (0.85 * sr), s0 = (size_t) (2.0 * sr), sl = (size_t) (0.07 * sr);
        const double mud = bandEnergyDb (b[0], sr, 120, 400, v0, vl);
        clarity = bandEnergyDb (b[0], sr, 1500, 4000, v0, vl) - mud;
        air = bandEnergyDb (b[0], sr, 5000, 12000, v0, vl) - mud;
        sib = bandEnergyDb (b[0], sr, 5000, 12000, s0, sl) - mud;
        pk = peak (b);
    };
    double c0, a0, s0, g0, p0, c1, a1, s1, g1, p1;
    sweep (0, c0, a0, s0, g0, p0);
    sweep (100, c1, a1, s1, g1, p1);
    R.check ("SWEETEN 0 -> 100: clearer, airier, more controlled", c1 > c0 && a1 > a0 && g1 > g0 && (s1 - s0) < (a1 - a0) && p1 <= 0.8913 + 1e-6,
             fmt ("clarity %+.1f dB, air %+.1f dB, ", c1 - c0, a1 - a0) + fmt ("sibilance %+.1f dB, comp GR %.1f -> ", s1 - s0, g0) + fmt ("%.1f dB", g1));

    struct Row { std::string name; std::array<double, 6> bands; double gr, pk; };
    std::vector<Row> rows;
    for (const auto& p : vs::factoryPresets())
    {
        Row r { p.name, {}, 0, 0 };
        measure (p.values, r.bands, r.gr, r.pk);
        rows.push_back (r);
    }
    double minDist = 1e9; std::string pair;
    for (size_t i = 1; i < rows.size(); ++i)
        for (size_t j = i + 1; j < rows.size(); ++j)
        {
            double d = 0; for (int k = 0; k < 6; ++k) d = std::max (d, std::abs (rows[i].bands[(size_t) k] - rows[j].bands[(size_t) k]));
            d = std::max (d, std::abs (rows[i].gr - rows[j].gr));
            if (d < minDist) { minDist = d; pair = rows[i].name + " / " + rows[j].name; }
        }
    R.check ("8 named presets are measurably distinct", minDist > 0.75, fmt ("closest pair differs by %.2f dB", minDist) + " (" + pair + ")");

    std::ofstream csv (outDir + "/preset_measurements.csv");
    csv << "preset,40-120Hz,120-400Hz,400-1.5k,1.5-4k,4-9k,9-16k,maxCompGR_dB,outPeak_dBFS\n";
    for (auto& r : rows)
    {
        csv << '"' << r.name << '"';
        for (double x : r.bands) csv << ',' << fmt ("%.2f", x);
        csv << ',' << fmt ("%.2f", r.gr) << ',' << fmt ("%.2f", toDb (r.pk)) << "\n";
    }
}

static void testPerformance()
{
    const double sr = 48000; const int secs = 20;
    auto v = presetValues ("Smooth Background Vocal");   // every module incl. doubler active
    VocalChain c; prepareWith (c, sr, v, 256);
    auto b = syntheticVocal (sr, secs, 2);
    const auto t0 = std::chrono::steady_clock::now();
    processChain (c, b, 256);
    const double el = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    const double cpu = 100.0 * el / secs;
    R.check ("Performance: stereo 48 kHz, all modules + doubler", cpu < 10.0,
             fmt ("%.1f s audio in %.3f s = %.2f%% of one core", (double) secs, el, cpu) + fmt (" (%.0fx real-time)", secs / el));
}

int main (int argc, char** argv)
{
    const std::string outDir = argc > 1 ? argv[1] : "test-output";
    fs::create_directories (outDir);
    std::printf ("VocalSweet DSP test-suite\n=========================\n");

    testSilenceAndLowLevel();
    testSineAndImpulse();
    testBroadband();
    testMonoStereo();
    testRatesAndBlocks();
    testFilterResponse();
    testCompressor();
    testSaturation();
    testDeEsser();
    testParamExtremes();
    testMixAndBypass();
    testInvalidInput();
    testClipping();
    testSmoothing();
    testMacrosAndPresets (outDir);
    testPerformance();

    R.writeReport (outDir + "/dsp_test_report.md", "VocalSweet DSP test report");
    std::printf ("\n%zu tests, %d failed\n", R.all().size(), R.failures());
    return R.failures() == 0 ? 0 : 1;
}
