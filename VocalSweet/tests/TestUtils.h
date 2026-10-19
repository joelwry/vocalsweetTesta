#pragma once
// Shared helpers for the VocalSweet test executables: signal generators, measurements, WAV I/O,
// and a minimal test registry. No external dependencies.

#include "dsp/VocalChain.h"
#include "dsp/Presets.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace vt
{
using Buffer = std::vector<std::vector<float>>; // [channel][sample]

inline Buffer makeBuffer (int channels, int samples) { return Buffer ((size_t) channels, std::vector<float> ((size_t) samples, 0.0f)); }

inline void sine (Buffer& b, double sr, double freq, float amp, double phase0 = 0.0)
{
    for (auto& ch : b)
        for (size_t i = 0; i < ch.size(); ++i)
            ch[i] = amp * (float) std::sin (phase0 + 2.0 * vs::dsp::kPi * freq * (double) i / sr);
}

inline void whiteNoise (Buffer& b, float amp, unsigned seed = 1)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);
    for (auto& ch : b) for (auto& s : ch) s = amp * d (rng);
}

/** Pink-ish noise (Paul Kellet's economy filter) */
inline void pinkNoise (Buffer& b, float amp, unsigned seed = 7)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);
    for (auto& ch : b)
    {
        float b0 = 0, b1 = 0, b2 = 0;
        for (auto& s : ch)
        {
            const float w = d (rng);
            b0 = 0.99765f * b0 + w * 0.0990460f;
            b1 = 0.96300f * b1 + w * 0.2965164f;
            b2 = 0.57000f * b2 + w * 1.0526913f;
            s = amp * 0.25f * (b0 + b1 + b2 + w * 0.1848f);
        }
    }
}

/** Synthetic "vocal-like" test signal (NOT a real voice): band-limited glottal pulse train with
    vibrato and a short melody, shaped by moving vowel formants, syllable envelopes, sibilant
    noise bursts ("s"), breath noise, a plosive thump and 45 Hz rumble. */
inline Buffer syntheticVocal (double sr, double seconds, int channels = 1, float level = 0.35f)
{
    const int n = (int) (sr * seconds);
    Buffer out = makeBuffer (channels, n);
    std::mt19937 rng (1234);
    std::uniform_real_distribution<float> uni (-1.0f, 1.0f);

    const double notes[] = { 220.0, 246.94, 261.63, 293.66, 261.63, 246.94, 220.0, 196.0 };
    const double noteLen = 0.5;
    struct Vowel { double f1, f2, f3; };
    const Vowel vowels[] = { { 730, 1090, 2440 }, { 270, 2290, 3010 }, { 530, 1840, 2480 }, { 570, 840, 2410 } };

    using namespace vs::dsp;
    SvfState f1s, f2s, f3s, sibS, sib2S, breathS;
    double phase = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / sr;
        const int noteIdx = (int) (t / noteLen) % 8;
        const double inNote = std::fmod (t, noteLen) / noteLen;
        const double vib = 1.0 + 0.012 * std::sin (2.0 * kPi * 5.5 * t) * std::min (1.0, inNote * 3.0);
        const double f0 = notes[noteIdx] * vib;

        // band-limited sawtooth-ish glottal source (additive, -12 dB/oct)
        phase += f0 / sr;
        if (phase >= 1.0) phase -= 1.0;
        double src = 0.0;
        const int maxH = (int) std::min (40.0, (0.45 * sr) / f0);
        for (int h = 1; h <= maxH; ++h)
            src += std::sin (2.0 * kPi * h * phase) / ((double) h * std::sqrt ((double) h));

        const Vowel& v = vowels[noteIdx % 4];
        const auto c1 = designSvf (SvfType::BandPass, sr, v.f1, 5.0);
        const auto c2 = designSvf (SvfType::BandPass, sr, v.f2, 8.0);
        const auto c3 = designSvf (SvfType::BandPass, sr, v.f3, 10.0);
        const float s = (float) src;
        float voiced = 1.0f * f1s.process (c1, s) + 0.6f * f2s.process (c2, s) + 0.3f * f3s.process (c3, s) + 0.05f * s;

        // syllable envelope: attack/decay within each note
        const float env = (float) (std::min (1.0, inNote * 12.0) * std::pow (1.0 - inNote, 0.4));
        voiced *= env;

        // sibilant "s" at the start of every other note (6-10 kHz noise burst, 70 ms)
        float sib = 0.0f;
        const double sibT = std::fmod (t, 2.0 * noteLen);
        if (sibT < 0.07)
        {
            const float w = uni (rng);
            const auto hs = designSvf (SvfType::BandPass, sr, std::min (7500.0, 0.4 * sr), 1.5);
            const auto hs2 = designSvf (SvfType::HighPass, sr, std::min (5000.0, 0.4 * sr), 0.7);
            sib = 2.2f * sib2S.process (hs2, sibS.process (hs, w)) * (float) std::sin (kPi * sibT / 0.07);
        }

        // breath noise and rumble
        const auto bc = designSvf (SvfType::BandPass, sr, 2500.0, 0.6);
        const float breath = 0.02f * breathS.process (bc, uni (rng));
        const float rumble = 0.03f * (float) std::sin (2.0 * kPi * 45.0 * t);
        // plosive thump every 2 s
        const double pt = std::fmod (t, 2.0);
        const float plosive = pt < 0.03 ? 0.4f * (float) (std::sin (2.0 * kPi * 60.0 * pt) * (1.0 - pt / 0.03)) : 0.0f;

        const float y = level * (voiced * 0.9f + sib + breath) + rumble + plosive * level;
        for (int ch = 0; ch < channels; ++ch)
            out[(size_t) ch][(size_t) i] = ch == 0 ? y : y * 0.97f; // slight L/R difference
    }
    return out;
}

//==============================================================================
inline float peak (const Buffer& b, size_t from = 0)
{
    float p = 0.0f;
    for (auto& ch : b) for (size_t i = from; i < ch.size(); ++i) p = std::max (p, std::abs (ch[i]));
    return p;
}

inline double rms (const std::vector<float>& x, size_t from = 0, size_t to = 0)
{
    if (to == 0 || to > x.size()) to = x.size();
    double s = 0.0;
    for (size_t i = from; i < to; ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / std::max<size_t> (1, to - from));
}

inline double toDb (double g) { return 20.0 * std::log10 (std::max (g, 1.0e-12)); }

inline bool allFinite (const Buffer& b)
{
    for (auto& ch : b) for (float s : ch) if (! std::isfinite (s)) return false;
    return true;
}

/** Goertzel amplitude estimate of a sinusoid at freq (Hann windowed). */
inline double toneAmplitude (const std::vector<float>& x, double sr, double freq, size_t from, size_t len)
{
    const double w = 2.0 * vs::dsp::kPi * freq / sr;
    const double coeff = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0, wsum = 0;
    for (size_t i = 0; i < len; ++i)
    {
        const double win = 0.5 - 0.5 * std::cos (2.0 * vs::dsp::kPi * (double) i / (double) (len - 1));
        const double s0 = x[from + i] * win + coeff * s1 - s2;
        s2 = s1; s1 = s0; wsum += win;
    }
    const double re = s1 - s2 * std::cos (w), im = s2 * std::sin (w);
    return 2.0 * std::sqrt (re * re + im * im) / wsum;
}

/** Energy (dB) of a signal in a frequency band, via a bank of Goertzel probes. */
inline double bandEnergyDb (const std::vector<float>& x, double sr, double f0, double f1, size_t from, size_t len)
{
    double e = 0.0; int n = 0;
    for (double f = f0; f <= f1; f *= 1.06, ++n) { const double a = toneAmplitude (x, sr, f, from, len); e += a * a; }
    return 10.0 * std::log10 (std::max (e / std::max (1, n), 1.0e-24));
}

//==============================================================================
/** Process a buffer through the chain in blocks of the given size. */
inline void processChain (vs::dsp::VocalChain& chain, Buffer& b, int blockSize)
{
    const int n = (int) b[0].size();
    const int chs = (int) b.size();
    std::vector<float*> ptrs ((size_t) chs);
    for (int start = 0; start < n; start += blockSize)
    {
        const int len = std::min (blockSize, n - start);
        for (int c = 0; c < chs; ++c) ptrs[(size_t) c] = b[(size_t) c].data() + start;
        chain.process (ptrs.data(), chs, len);
    }
}

inline vs::ParamValues allModulesOff()
{
    auto v = vs::defaultParamValues();
    using namespace vs;
    for (int i : { pHpfOn, pEqOn, pCompOn, pDeessOn, pSatOn, pExciteOn, pDoubleOn, pLimiterOn }) v[(size_t) i] = 0.0f;
    for (int i : { pSweeten, pTone, pWarmth, pPresence, pControl }) v[(size_t) i] = 0.0f;
    return v;
}

inline void prepareWith (vs::dsp::VocalChain& chain, double sr, const vs::ParamValues& v, int block = 512)
{
    chain.prepare (sr, block);
    chain.setParams (vs::computeEffective (v));
    chain.reset();
}

//==============================================================================
/** 32-bit float WAV writer. */
inline bool writeWav (const std::string& path, const Buffer& b, int sr)
{
    std::ofstream f (path, std::ios::binary);
    if (! f) return false;
    const uint16_t chs = (uint16_t) b.size();
    const uint32_t n = (uint32_t) b[0].size();
    const uint32_t dataBytes = n * chs * 4;
    auto u32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto u16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    f.write ("RIFF", 4); u32 (36 + dataBytes); f.write ("WAVE", 4);
    f.write ("fmt ", 4); u32 (16); u16 (3); u16 (chs); u32 ((uint32_t) sr); u32 ((uint32_t) sr * chs * 4); u16 ((uint16_t) (chs * 4)); u16 (32);
    f.write ("data", 4); u32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i)
        for (uint16_t c = 0; c < chs; ++c) { float s = b[c][i]; f.write ((const char*) &s, 4); }
    return (bool) f;
}

//==============================================================================
struct TestResult { std::string name; bool pass; std::string detail; };

class Registry
{
public:
    void check (const std::string& name, bool pass, const std::string& detail)
    {
        results.push_back ({ name, pass, detail });
        std::printf ("[%s] %-58s %s\n", pass ? "PASS" : "FAIL", name.c_str(), detail.c_str());
        std::fflush (stdout);
    }
    int failures() const { int f = 0; for (auto& r : results) f += r.pass ? 0 : 1; return f; }
    const std::vector<TestResult>& all() const { return results; }

    void writeReport (const std::string& path, const std::string& title) const
    {
        std::ofstream f (path);
        f << "# " << title << "\n\n";
        f << "| # | Test | Result | Measurement |\n|---|---|---|---|\n";
        int i = 1;
        for (auto& r : results)
            f << "| " << i++ << " | " << r.name << " | " << (r.pass ? "PASS" : "**FAIL**") << " | " << r.detail << " |\n";
        f << "\n**Total: " << results.size() << ", passed: " << results.size() - (size_t) failures()
          << ", failed: " << failures() << "**\n";
    }

private:
    std::vector<TestResult> results;
};

inline std::string fmt (const char* f, double a) { char b[128]; std::snprintf (b, sizeof b, f, a); return b; }
inline std::string fmt (const char* f, double a, double c) { char b[160]; std::snprintf (b, sizeof b, f, a, c); return b; }
inline std::string fmt (const char* f, double a, double c, double d) { char b[200]; std::snprintf (b, sizeof b, f, a, c, d); return b; }

} // namespace vt
