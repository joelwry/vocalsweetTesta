#pragma once
// Test signals for SwitaxVoice. The synthetic voice is NOT a real singer: it is a formant-filtered
// harmonic source with phrases, pauses, vowel changes and sibilant noise bursts. Its purpose is to
// inject *known* problems (mud, harshness, rumble, hum, sibilance, uneven phrases, noise, clipping)
// so tests can check that the analyser flags exactly what was injected and nothing else.

#include "analysis/Advisor.h"

#include <cstdio>
#include <cstdint>
#include <fstream>
#include <random>
#include <string>

namespace sxt
{
using sx::analysis::Buffer;
using namespace vs::dsp;

struct VoiceSpec
{
    double seconds = 12.0;
    double sampleRate = 48000.0;
    int channels = 1;
    double baseHz = 196.0;             // lowest note of the melody
    double sibilance = 0.25;           // sibilant burst amplitude (relative)
    double mudDb = 0.0, mudHz = 300.0;     // injected bell
    double harshDb = 0.0, harshHz = 3200.0;
    double rumble = 0.0;               // 38 Hz rumble amplitude (absolute)
    double hum = 0.0;                  // 50 Hz + harmonics hum amplitude (absolute)
    double phraseSwingDb = 0.0;        // alternate phrases +/- swing/2
    double noise = 0.0;                // white noise floor amplitude
    double targetPeak = 0.35;          // normalisation of the voice before faults
    double clipGain = 0.0;             // >0: drive into hard clip at +/-1 after gain
    bool invertRight = false;          // stereo: right = -left
    unsigned seed = 11;
};

inline Buffer voice (const VoiceSpec& s)
{
    const double sr = s.sampleRate;
    const int n = (int) (sr * s.seconds);
    std::vector<float> v ((size_t) n, 0.0f);
    std::mt19937 rng (s.seed);
    std::uniform_real_distribution<float> uni (-1.0f, 1.0f);

    const double semis[] = { 0, 2, 4, 5, 7, 5, 4, 2 };
    struct Vowel { double f1, f2, f3, f4; };
    const Vowel vowels[] = { { 730, 1090, 2440, 3400 }, { 300, 2200, 2950, 3700 }, { 530, 1840, 2480, 3500 }, { 570, 840, 2410, 3300 }, { 440, 1020, 2240, 3300 } };
    const double phraseLen = 2.0, gap = 0.45, noteLen = (phraseLen - gap) / 4.0;

    SvfState f1s, f2s, f3s, f4s, sibA, sibB, br;
    double phase = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / sr;
        const int phrase = (int) (t / phraseLen);
        const double inPhrase = std::fmod (t, phraseLen);
        if (inPhrase >= phraseLen - gap) { v[(size_t) i] = 0.0f; continue; }
        const int noteIdx = (int) (inPhrase / noteLen);
        const double inNote = std::fmod (inPhrase, noteLen) / noteLen;
        const int seqIdx = (phrase * 4 + noteIdx) % 8;
        const double vib = 1.0 + 0.010 * std::sin (2.0 * kPi * 5.3 * t) * std::min (1.0, inNote * 2.5);
        const double f0 = s.baseHz * std::pow (2.0, semis[seqIdx] / 12.0) * vib;

        phase += f0 / sr;
        if (phase >= 1.0) phase -= 1.0;
        double src = 0.0;
        const int maxH = (int) std::min (60.0, (0.45 * sr) / f0);
        for (int h = 1; h <= maxH; ++h) src += std::sin (2.0 * kPi * h * phase) / std::pow ((double) h, 1.1);

        const Vowel& w = vowels[(phrase + noteIdx) % 5];
        const float x = (float) src;
        float voiced = 1.0f * f1s.process (designSvf (SvfType::BandPass, sr, w.f1, 4.0), x)
                     + 0.55f * f2s.process (designSvf (SvfType::BandPass, sr, w.f2, 6.0), x)
                     + 0.30f * f3s.process (designSvf (SvfType::BandPass, sr, w.f3, 8.0), x)
                     + 0.18f * f4s.process (designSvf (SvfType::BandPass, sr, w.f4, 8.0), x)
                     + 0.04f * x;
        const float env = (float) (std::min (1.0, inNote * 14.0) * std::pow (1.0 - 0.6 * inNote, 0.5));
        voiced *= env * 0.5f;

        // sibilant at the start of notes 0 and 2 of each phrase (60 ms)
        float sib = 0.0f;
        const double nt = std::fmod (inPhrase, noteLen);
        if ((noteIdx == 0 || noteIdx == 2) && nt < 0.06)
        {
            const float wn = uni (rng);
            const float b = sibB.process (designSvf (SvfType::HighPass, sr, std::min (5000.0, 0.42 * sr), 0.7),
                                          sibA.process (designSvf (SvfType::BandPass, sr, std::min (7200.0, 0.42 * sr), 1.3), wn));
            sib = (float) s.sibilance * 3.0f * b * (float) std::sin (kPi * nt / 0.06);
        }
        const float breath = 0.006f * br.process (designSvf (SvfType::BandPass, sr, 3000.0, 0.5), uni (rng)) * env;
        const double swing = (phrase % 2 == 0 ? 0.5 : -0.5) * s.phraseSwingDb;
        v[(size_t) i] = (float) ((voiced + sib + breath) * std::pow (10.0, swing / 20.0));
    }

    // normalise the clean voice
    float pk = 0.0f;
    for (float x : v) pk = std::max (pk, std::abs (x));
    const float g = pk > 0 ? (float) s.targetPeak / pk : 1.0f;
    for (auto& x : v) x *= g;

    // injected tonal faults
    if (s.mudDb != 0.0 || s.harshDb != 0.0)
    {
        SvfState a, b;
        const auto ca = designSvf (SvfType::Bell, sr, s.mudHz, 1.2, s.mudDb);
        const auto cb = designSvf (SvfType::Bell, sr, s.harshHz, 3.0, s.harshDb);
        for (auto& x : v) x = b.process (cb, a.process (ca, x));
    }
    std::normal_distribution<float> gauss (0.0f, 1.0f);
    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / sr;
        double add = 0.0;
        if (s.rumble > 0) add += s.rumble * (std::sin (2.0 * kPi * 38.0 * t) + 0.5 * std::sin (2.0 * kPi * 23.0 * t + 1.0));
        if (s.hum > 0) add += s.hum * (std::sin (2.0 * kPi * 50.0 * t) + 0.4 * std::sin (2.0 * kPi * 100.0 * t) + 0.2 * std::sin (2.0 * kPi * 150.0 * t));
        if (s.noise > 0) add += s.noise * gauss (rng);
        v[(size_t) i] += (float) add;
    }
    if (s.clipGain > 0)
        for (auto& x : v) x = std::clamp (x * (float) s.clipGain, -1.0f, 1.0f);

    Buffer out ((size_t) s.channels, v);
    if (s.channels == 2 && s.invertRight)
        for (auto& x : out[1]) x = -x;
    return out;
}

inline Buffer sine (double sr, double seconds, double freq, double ampDb, int channels = 2, double phase = 0.0)
{
    const int n = (int) (sr * seconds);
    const float a = (float) std::pow (10.0, ampDb / 20.0);
    Buffer b ((size_t) channels, std::vector<float> ((size_t) n));
    for (auto& ch : b)
        for (int i = 0; i < n; ++i) ch[(size_t) i] = a * (float) std::sin (phase + 2.0 * kPi * freq * i / sr);
    return b;
}

inline Buffer concat (std::initializer_list<Buffer> parts)
{
    Buffer out (parts.begin()->size());
    for (auto& p : parts)
        for (size_t c = 0; c < out.size(); ++c) out[c].insert (out[c].end(), p[c].begin(), p[c].end());
    return out;
}

/** Minimal WAV reader (PCM 16/24/32-bit and float 32) for test material. */
inline bool readWav (const std::string& path, Buffer& out, double& sr)
{
    std::ifstream f (path, std::ios::binary);
    if (! f) return false;
    char id[4]; uint32_t sz; f.read (id, 4); f.read ((char*) &sz, 4); f.read (id, 4);
    uint16_t fmt = 0, chs = 0, bits = 0; uint32_t rate = 0;
    std::vector<char> data;
    while (f.read (id, 4) && f.read ((char*) &sz, 4))
    {
        if (std::string (id, 4) == "fmt ")
        {
            std::vector<char> c (sz); f.read (c.data(), sz);
            fmt = *(uint16_t*) &c[0]; chs = *(uint16_t*) &c[2]; rate = *(uint32_t*) &c[4]; bits = *(uint16_t*) &c[14];
            if (fmt == 0xFFFE && sz >= 26) fmt = *(uint16_t*) &c[24];
        }
        else if (std::string (id, 4) == "data") { data.resize (sz); f.read (data.data(), sz); break; }
        else f.seekg (sz + (sz & 1), std::ios::cur);
    }
    if (chs == 0 || data.empty()) return false;
    const int bps = bits / 8;
    const size_t frames = data.size() / ((size_t) bps * chs);
    out.assign (chs, std::vector<float> (frames));
    for (size_t i = 0; i < frames; ++i)
        for (int c = 0; c < chs; ++c)
        {
            const char* p = &data[(i * chs + (size_t) c) * (size_t) bps];
            float v = 0;
            if (fmt == 3 && bits == 32) v = *(const float*) p;
            else if (bits == 16) v = *(const int16_t*) p / 32768.0f;
            else if (bits == 24) { int32_t x = (uint8_t) p[0] | ((uint8_t) p[1] << 8) | ((int8_t) p[2] << 16); v = x / 8388608.0f; }
            else if (bits == 32) v = (float) (*(const int32_t*) p / 2147483648.0);
            out[(size_t) c][i] = v;
        }
    sr = rate;
    return true;
}

} // namespace sxt
