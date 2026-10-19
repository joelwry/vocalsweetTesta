#pragma once
// Measurement of a captured signal (framework independent, not real-time: runs on a background
// thread or in tests). Everything here is a *measurement*; interpretation lives in Advisor.h.
//
// Activity gating: the signal is cut into 100 ms blocks; a block is "active" when its RMS is
// above max(-60 dBFS, loud-block level - 30 dB). Spectral, pitch and sibilance statistics only
// use active blocks, so pauses and room noise do not skew them. When a processed version of the
// same capture is measured, the activity map of the raw capture is reused (shifted by the
// processing latency) so before/after numbers compare exactly the same moments.

#include "Loudness.h"
#include "Spectrum.h"
#include "dsp/Svf.h"

#include <numeric>
#include <string>

namespace sx::analysis
{
using Buffer = std::vector<std::vector<float>>;   // [channel][sample]

//==============================================================================
struct Activity
{
    double sampleRate = 48000.0;
    int blockSize = 4800;                 // 100 ms
    std::vector<float> blockDb;           // RMS per block (mono mix)
    std::vector<bool> active;
    double threshold = -60.0;
    int activeBlocks = 0;

    bool isActiveAt (long long sample) const noexcept
    {
        if (sample < 0) return false;
        const auto b = (size_t) (sample / blockSize);
        return b < active.size() && active[b];
    }
    double activeSeconds() const noexcept { return activeBlocks * (double) blockSize / sampleRate; }
};

inline std::vector<float> monoMix (const Buffer& b)
{
    if (b.empty()) return {};
    std::vector<float> m (b[0].size(), 0.0f);
    const float g = 1.0f / (float) b.size();
    for (auto& ch : b) for (size_t i = 0; i < m.size() && i < ch.size(); ++i) m[i] += g * ch[i];
    return m;
}

inline double percentileOf (std::vector<double> v, double p)
{
    if (v.empty()) return 0.0;
    std::sort (v.begin(), v.end());
    const double idx = std::clamp (p, 0.0, 1.0) * (double) (v.size() - 1);
    const size_t i0 = (size_t) idx;
    const size_t i1 = std::min (i0 + 1, v.size() - 1);
    return v[i0] + (idx - (double) i0) * (v[i1] - v[i0]);
}

inline Activity detectActivity (const std::vector<float>& mono, double sr)
{
    Activity a;
    a.sampleRate = sr;
    a.blockSize = std::max (1, (int) std::lround (0.1 * sr));
    const size_t nb = mono.size() / (size_t) a.blockSize;
    a.blockDb.resize (nb);
    std::vector<double> levels;
    for (size_t b = 0; b < nb; ++b)
    {
        double s = 0.0;
        for (int i = 0; i < a.blockSize; ++i) { const double x = mono[b * (size_t) a.blockSize + (size_t) i]; s += x * x; }
        a.blockDb[b] = (float) powToDb (s / a.blockSize);
        if (a.blockDb[b] > -90.0f) levels.push_back (a.blockDb[b]);
    }
    const double loud = levels.empty() ? -120.0 : percentileOf (levels, 0.95);
    a.threshold = std::max (-60.0, loud - 30.0);
    a.active.resize (nb);
    for (size_t b = 0; b < nb; ++b)
    {
        a.active[b] = a.blockDb[b] > a.threshold;
        a.activeBlocks += a.active[b] ? 1 : 0;
    }
    return a;
}

//==============================================================================
/** Sibilance event map detected on the raw capture: 10 ms windows whose 4.5-11 kHz energy
    exceeds their 150 Hz-2.5 kHz "body" energy (S - B > 0 dB) while the signal is active. */
struct SibilanceMap
{
    int window = 480;
    std::vector<bool> event;              // per window
    int numEvents = 0;                    // number of separate events (runs)

    /** Fraction of the 10 ms windows in [start, start + len) that are sibilant events. */
    double eventFraction (long long start, int len) const noexcept
    {
        if (event.empty()) return 0.0;
        const long long w0 = std::max (0LL, start / window), w1 = std::min ((long long) event.size(), (start + len) / window);
        if (w1 <= w0) return 0.0;
        int e = 0;
        for (long long w = w0; w < w1; ++w) e += event[(size_t) w] ? 1 : 0;
        return (double) e / (double) (w1 - w0);
    }
};

struct SibilanceStats
{
    int events = 0;
    double eventsPerMinute = 0.0;
    double peakRelBodyDb = -100.0;        // median event peak (sibilant band) - body reference (P90 of body band)
    double eventPeakDb = -100.0;          // median event peak level in the sibilant band (dBFS)
    double bodyRefDb = -100.0;
    double nonEventHfDb = -100.0;         // mean sibilant-band level in active non-event windows
    double centroidHz = 6500.0;           // strongest sibilant sub-band during events
};

class SibilanceAnalyzer
{
public:
    static constexpr int kSubBands = 5;
    static constexpr double kSubHz[kSubBands] = { 4500.0, 5600.0, 7000.0, 8500.0, 10500.0 };

    /** Per-window band energies (dB) of a mono signal. */
    struct Profile
    {
        std::vector<float> sDb, bDb;
        std::vector<std::array<float, kSubBands>> subDb;
    };

    static Profile profile (const std::vector<float>& mono, double sr)
    {
        using namespace vs::dsp;
        Profile p;
        const int w = std::max (1, (int) std::lround (0.01 * sr));
        const size_t nw = mono.size() / (size_t) w;
        p.sDb.resize (nw); p.bDb.resize (nw); p.subDb.resize (nw);
        const double nyq = 0.45 * sr;
        const auto sHp = designSvf (SvfType::HighPass, sr, std::min (4500.0, nyq), 0.707);
        const auto sLp = designSvf (SvfType::LowPass, sr, std::min (11000.0, nyq), 0.707);
        const auto bHp = designSvf (SvfType::HighPass, sr, 150.0, 0.707);
        const auto bLp = designSvf (SvfType::LowPass, sr, 2500.0, 0.707);
        SvfCoeffs sub[kSubBands];
        for (int k = 0; k < kSubBands; ++k) sub[k] = designSvf (SvfType::BandPass, sr, std::min (kSubHz[k], nyq), 2.5);
        SvfState s1, s2, s3, b1, b2, b3, subS[kSubBands];

        for (size_t win = 0; win < nw; ++win)
        {
            double es = 0, eb = 0, esub[kSubBands] {};
            for (int i = 0; i < w; ++i)
            {
                const float x = mono[win * (size_t) w + (size_t) i];
                const float hs = s3.process (sLp, s2.process (sHp, s1.process (sHp, x)));
                const float hb = b3.process (bLp, b2.process (bHp, b1.process (bLp, x)));
                es += (double) hs * hs;
                eb += (double) hb * hb;
                for (int k = 0; k < kSubBands; ++k) { const float y = subS[k].process (sub[k], x); esub[k] += (double) y * y; }
            }
            p.sDb[win] = (float) powToDb (es / w);
            p.bDb[win] = (float) powToDb (eb / w);
            for (int k = 0; k < kSubBands; ++k) p.subDb[win][(size_t) k] = (float) powToDb (esub[k] / w);
        }
        return p;
    }

    static SibilanceMap detect (const Profile& p, const Activity& act, double sr)
    {
        SibilanceMap m;
        m.window = std::max (1, (int) std::lround (0.01 * sr));
        m.event.assign (p.sDb.size(), false);
        // the sibilant band must also be reasonably loud relative to the programme
        std::vector<double> active;
        for (size_t w = 0; w < p.sDb.size(); ++w)
            if (act.isActiveAt ((long long) w * m.window)) active.push_back (std::max (p.sDb[w], p.bDb[w]));
        const double loud = active.empty() ? -120.0 : percentileOf (active, 0.95);
        bool prev = false;
        for (size_t w = 0; w < p.sDb.size(); ++w)
        {
            const bool e = act.isActiveAt ((long long) w * m.window) && p.sDb[w] - p.bDb[w] > 0.0f && p.sDb[w] > loud - 30.0;
            m.event[w] = e;
            if (e && ! prev) ++m.numEvents;
            prev = e;
        }
        return m;
    }

    /** Statistics of `p` (possibly a processed signal) at the event positions of `map`
        (detected on the raw signal), shifted by `latencySamples`. */
    static SibilanceStats measure (const Profile& p, const SibilanceMap& map, const Activity& act, int latencySamples)
    {
        SibilanceStats st;
        const int shift = (int) std::lround ((double) latencySamples / map.window);
        std::vector<double> eventPeaks, body, nonEvent;
        double subSum[kSubBands] {};
        // distance (in windows) to the nearest event, so "between sibilants" excludes release tails
        std::vector<int> dist (map.event.size(), 1 << 20);
        for (int pass = 0; pass < 2; ++pass)
        {
            int d = 1 << 20;
            for (size_t k = 0; k < map.event.size(); ++k)
            {
                const size_t w = pass == 0 ? k : map.event.size() - 1 - k;
                d = map.event[w] ? 0 : std::min (d + 1, 1 << 20);
                dist[w] = std::min (dist[w], d);
            }
        }
        double runPeak = -200.0; bool inRun = false;

        for (size_t w = 0; w < map.event.size(); ++w)
        {
            const size_t pw = w + (size_t) std::max (0, shift);
            if (pw >= p.sDb.size()) break;
            const bool active = act.isActiveAt ((long long) w * map.window);
            if (map.event[w])
            {
                runPeak = std::max (runPeak, (double) p.sDb[pw]);
                inRun = true;
                for (int k = 0; k < kSubBands; ++k) subSum[k] += dbToPow (p.subDb[pw][(size_t) k]);
            }
            else
            {
                if (inRun) { eventPeaks.push_back (runPeak); runPeak = -200.0; inRun = false; }
                if (active) body.push_back (p.bDb[pw]);
                if (active && dist[w] >= 8) nonEvent.push_back (dbToPow (p.sDb[pw]));
            }
        }
        if (inRun) eventPeaks.push_back (runPeak);

        st.events = (int) eventPeaks.size();
        st.eventsPerMinute = act.activeSeconds() > 0 ? st.events * 60.0 / act.activeSeconds() : 0.0;
        st.bodyRefDb = body.empty() ? -100.0 : percentileOf (body, 0.90);
        st.eventPeakDb = eventPeaks.empty() ? -100.0 : percentileOf (eventPeaks, 0.5);
        st.peakRelBodyDb = eventPeaks.empty() ? -100.0 : st.eventPeakDb - st.bodyRefDb;
        st.nonEventHfDb = nonEvent.empty() ? -100.0 : powToDb (std::accumulate (nonEvent.begin(), nonEvent.end(), 0.0) / (double) nonEvent.size());
        int best = 2;
        for (int k = 0; k < kSubBands; ++k) if (subSum[k] > subSum[best]) best = k;
        st.centroidHz = kSubHz[best];
        return st;
    }
};

//==============================================================================
/** YIN fundamental-frequency estimator (de Cheveigne & Kawahara 2002), run on a decimated
    copy of the signal (~16 kHz) for efficiency. */
class PitchTracker
{
public:
    struct Result { double voicedFraction = 0, p5 = 0, p50 = 0, p95 = 0; int voicedFrames = 0; };

    static Result track (const std::vector<float>& mono, double sr, const Activity& act, double fMin = 65.0, double fMax = 1000.0)
    {
        using namespace vs::dsp;
        Result r;
        const int dec = std::max (1, (int) std::floor (sr / 16000.0));
        const double dsr = sr / dec;
        // anti-alias low-pass (2 x 2nd order) then decimate
        const auto lp = designSvf (SvfType::LowPass, sr, std::min (0.4 * dsr, 6000.0), 0.707);
        SvfState a, b;
        std::vector<float> d;
        d.reserve (mono.size() / (size_t) dec + 1);
        for (size_t i = 0; i < mono.size(); ++i)
        {
            const float y = b.process (lp, a.process (lp, mono[i]));
            if (i % (size_t) dec == 0) d.push_back (y);
        }

        const int W = (int) std::lround (0.030 * dsr);
        const int hop = (int) std::lround (0.010 * dsr);
        const int tauMin = std::max (2, (int) std::floor (dsr / fMax));
        const int tauMax = (int) std::ceil (dsr / fMin);
        std::vector<double> diff ((size_t) tauMax + 2, 0.0), cmnd ((size_t) tauMax + 2, 1.0);
        std::vector<double> f0s;
        int activeFrames = 0;

        for (size_t start = 0; start + (size_t) (W + tauMax + 1) < d.size(); start += (size_t) hop)
        {
            if (! act.isActiveAt ((long long) ((start + (size_t) W / 2) * (size_t) dec))) continue;
            ++activeFrames;
            for (int tau = 1; tau <= tauMax; ++tau)
            {
                double s = 0.0;
                const float* x = d.data() + start;
                for (int j = 0; j < W; ++j) { const double v = (double) x[j] - x[j + tau]; s += v * v; }
                diff[(size_t) tau] = s;
            }
            double run = 0.0;
            cmnd[0] = 1.0;
            for (int tau = 1; tau <= tauMax; ++tau)
            {
                run += diff[(size_t) tau];
                cmnd[(size_t) tau] = run > 0.0 ? diff[(size_t) tau] * tau / run : 1.0;
            }
            int best = -1;
            for (int tau = tauMin; tau < tauMax; ++tau)
            {
                if (cmnd[(size_t) tau] < 0.15)
                {
                    while (tau + 1 < tauMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau]) ++tau;
                    best = tau;
                    break;
                }
            }
            if (best < 0) continue;   // unvoiced / unreliable frame
            // parabolic interpolation
            const double y0 = cmnd[(size_t) best - 1], y1 = cmnd[(size_t) best], y2 = cmnd[(size_t) best + 1];
            const double den = y0 - 2.0 * y1 + y2;
            const double t = (double) best + (std::abs (den) > 1e-12 ? 0.5 * (y0 - y2) / den : 0.0);
            f0s.push_back (dsr / t);
        }
        r.voicedFrames = (int) f0s.size();
        r.voicedFraction = activeFrames > 0 ? (double) f0s.size() / activeFrames : 0.0;
        if (! f0s.empty())
        {
            r.p5 = percentileOf (f0s, 0.05);
            r.p50 = percentileOf (f0s, 0.50);
            r.p95 = percentileOf (f0s, 0.95);
        }
        return r;
    }
};

//==============================================================================
struct Measurements
{
    bool valid = false;
    double sampleRate = 48000.0;
    int channels = 1;
    double totalSeconds = 0, activeSeconds = 0;

    // loudness & dynamics
    double integratedLufs = -200, loudnessRange = 0, maxMomentary = -200, maxShortTerm = -200;
    double momentaryP10 = -200, momentaryP50 = -200, momentaryP95 = -200, levelRange = 0;
    double truePeakDb = -200, samplePeakDb = -200, activeRmsDb = -200, crestDb = 0, plrDb = 0;
    double noiseFloorDb = -120, activeLevelDb = -120;

    // clipping (raw signal)
    int clipRuns = 0, flatTopRuns = 0;
    std::vector<double> clipTimes;        // seconds, first few

    // spectrum (active frames only)
    BandArray ltasDb {};                  // 1/6-octave band power, dBFS (mean-square)
    BandArray relDb {};                   // ltas normalised to its 200 Hz - 5 kHz mean
    BandArray peakinessDb {};             // ltas minus its 1-octave smoothed trend
    BandArray persistence {};             // fraction of active frames with the band > 3 dB above local trend
    std::array<bool, kNumBands> bandValid {};
    int spectralFrames = 0, sibilantFramesSkipped = 0;
    std::vector<BandArray> frameRelDb;    // per active frame, normalised like relDb (for consistency checks)

    // pitch
    PitchTracker::Result pitch;

    // sibilance
    SibilanceStats sibilance;

    // stereo
    bool stereo = false;
    double correlation = 1.0, lowCorrelation = 1.0, sideToMidDb = -120.0, monoLossDb = 0.0;

    // low end below the lowest sustained fundamental
    double lowCutoffHz = 80.0, lowEnergyRelDb = -120.0;
};

struct AnalysisOptions
{
    const Activity* activity = nullptr;          // reuse a raw activity map (for processed signals)
    const SibilanceMap* sibilanceMap = nullptr;  // reuse raw sibilance events
    int latencySamples = 0;                      // processing latency of the measured signal
    bool pitch = true, sibilance = true, spectrum = true, clipping = true;
};

/** Full measurement of a capture. */
inline Measurements analyse (const Buffer& buf, double sr, const AnalysisOptions& opt, Activity* activityOut = nullptr,
                             SibilanceMap* sibOut = nullptr)
{
    Measurements m;
    if (buf.empty() || buf[0].size() < (size_t) (0.5 * sr)) return m;

    m.sampleRate = sr;
    m.channels = (int) std::min<size_t> (buf.size(), 2);
    const size_t n = buf[0].size();
    m.totalSeconds = (double) n / sr;
    const auto mono = monoMix (buf);

    Activity ownAct;
    if (opt.activity == nullptr) ownAct = detectActivity (mono, sr);
    const Activity& act = opt.activity != nullptr ? *opt.activity : ownAct;
    if (activityOut != nullptr) *activityOut = act;
    m.activeSeconds = act.activeSeconds();
    const long long lat = opt.latencySamples;

    // ---------------------------------------------------------------- loudness & peaks
    {
        LoudnessMeter lm;
        lm.prepare (sr, m.channels);
        TruePeakMeter tp;
        const float* ptrs[2] = { buf[0].data(), buf[(size_t) (m.channels - 1)].data() };
        lm.process (ptrs, m.channels, (int) n);
        tp.process (ptrs, m.channels, (int) n);
        m.integratedLufs = lm.integrated();
        m.loudnessRange = lm.loudnessRange();
        m.maxMomentary = lm.maxMomentaryLufs();
        m.maxShortTerm = lm.maxShortTermLufs();
        const auto& mh = lm.momentaryHistogram();
        m.momentaryP10 = mh.percentile (0.10, -20.0);
        m.momentaryP50 = mh.percentile (0.50, -20.0);
        m.momentaryP95 = mh.percentile (0.95, -20.0);
        m.levelRange = mh.count() > 2 ? std::max (0.0, m.momentaryP95 - m.momentaryP10) : 0.0;
        m.truePeakDb = tp.truePeakDb();
        m.samplePeakDb = tp.samplePeakDb();
        m.plrDb = m.integratedLufs > -150 ? m.truePeakDb - m.integratedLufs : 0.0;
    }

    // ---------------------------------------------------------------- active RMS, crest, noise floor
    {
        double s = 0.0; long long cnt = 0;
        std::vector<double> active, all;
        for (size_t b = 0; b < act.blockDb.size(); ++b)
        {
            const size_t start = b * (size_t) act.blockSize + (size_t) std::max (0LL, lat);
            if (start + (size_t) act.blockSize > n) break;
            double e = 0.0;
            for (int i = 0; i < act.blockSize; ++i) { const double x = mono[start + (size_t) i]; e += x * x; }
            const double db = powToDb (e / act.blockSize);
            all.push_back (db);
            if (act.active[b]) { s += e; cnt += act.blockSize; active.push_back (db); }
        }
        m.activeRmsDb = cnt > 0 ? powToDb (s / (double) cnt) : -120.0;
        m.crestDb = m.activeRmsDb > -119 ? m.samplePeakDb - m.activeRmsDb : 0.0;
        m.activeLevelDb = active.empty() ? -120.0 : percentileOf (active, 0.5);
        m.noiseFloorDb = all.empty() ? -120.0 : std::max (-120.0, percentileOf (all, 0.10));
    }

    // ---------------------------------------------------------------- clipping
    if (opt.clipping)
    {
        for (int c = 0; c < m.channels; ++c)
        {
            const auto& x = buf[(size_t) c];
            int runFull = 0, runFlat = 0;
            for (size_t i = 0; i < n; ++i)
            {
                const float a = std::abs (x[i]);
                runFull = a >= 0.9995f ? runFull + 1 : 0;
                if (runFull == 3)
                {
                    ++m.clipRuns;
                    if (m.clipTimes.size() < 6) m.clipTimes.push_back ((double) i / sr);
                }
                runFlat = (i > 0 && a >= 0.1f && a < 0.9995f && x[i] == x[i - 1]) ? runFlat + 1 : 0;
                if (runFlat == 3) ++m.flatTopRuns;   // 4 identical consecutive samples well above the noise
            }
        }
    }

    // ---------------------------------------------------------------- sibilance (before the spectrum:
    // tonal balance is measured on non-sibilant frames, sibilance is assessed separately)
    SibilanceMap ownMap;
    const SibilanceMap* map = opt.sibilanceMap;
    if (opt.sibilance || opt.spectrum)
    {
        const auto prof = SibilanceAnalyzer::profile (mono, sr);
        if (map == nullptr) { ownMap = SibilanceAnalyzer::detect (prof, act, sr); map = &ownMap; }
        if (sibOut != nullptr) *sibOut = *map;
        if (opt.sibilance) m.sibilance = SibilanceAnalyzer::measure (prof, *map, act, opt.latencySamples);
    }

    // ---------------------------------------------------------------- spectrum
    if (opt.spectrum)
    {
        BandAnalyzer ba;
        ba.prepare (sr, sr > 64000 ? 13 : 12);
        const int N = ba.frameSize(), hop = N / 2;
        for (int b = 0; b < kNumBands; ++b) m.bandValid[(size_t) b] = ba.isBandValid (b);
        BandArray acc {}, frame {};
        std::vector<BandArray> frames;
        for (size_t start = (size_t) std::max (0LL, lat); start + (size_t) N <= n; start += (size_t) hop)
        {
            if (! act.isActiveAt ((long long) (start + (size_t) N / 2) - lat)) continue;
            if (map != nullptr && map->eventFraction ((long long) start - lat - map->window * 2, N + map->window * 4) > 0.0) { ++m.sibilantFramesSkipped; continue; }
            ba.analyse (mono.data() + start, frame);
            for (int b = 0; b < kNumBands; ++b) acc[(size_t) b] += frame[(size_t) b];
            BandArray db {};
            for (int b = 0; b < kNumBands; ++b) db[(size_t) b] = powToDb (frame[(size_t) b]);
            frames.push_back (db);
        }
        m.spectralFrames = (int) frames.size();
        if (! frames.empty())
        {
            for (int b = 0; b < kNumBands; ++b) m.ltasDb[(size_t) b] = powToDb (acc[(size_t) b] / (double) frames.size());
            const double ref = meanOverRange (m.ltasDb, m.bandValid, 200.0, 5000.0);
            for (int b = 0; b < kNumBands; ++b) m.relDb[(size_t) b] = m.ltasDb[(size_t) b] - ref;
            const auto trend = smoothBands (m.ltasDb, m.bandValid, 3);
            for (int b = 0; b < kNumBands; ++b) m.peakinessDb[(size_t) b] = m.ltasDb[(size_t) b] - trend[(size_t) b];
            m.frameRelDb.reserve (frames.size());
            for (auto& f : frames)
            {
                const auto t = smoothBands (f, m.bandValid, 3);
                for (int b = 0; b < kNumBands; ++b)
                    if (f[(size_t) b] - t[(size_t) b] > 3.0) m.persistence[(size_t) b] += 1.0;
                BandArray rel = f;
                const double fr = meanOverRange (f, m.bandValid, 200.0, 5000.0);
                for (auto& x : rel) x -= fr;
                m.frameRelDb.push_back (rel);
            }
            for (auto& p : m.persistence) p /= (double) frames.size();
        }
    }

    // ---------------------------------------------------------------- pitch & low end
    if (opt.pitch)
        m.pitch = PitchTracker::track (lat > 0 ? std::vector<float> (mono.begin() + std::min<long long> (lat, (long long) n), mono.end()) : mono, sr, act);
    m.lowCutoffHz = (m.pitch.voicedFrames > 20 && m.pitch.p5 > 0) ? std::clamp (0.75 * m.pitch.p5, 50.0, 140.0) : 80.0;
    if (m.spectralFrames > 0)
    {
        double lowE = 0.0, totE = 0.0;
        for (int b = 0; b < kNumBands; ++b)
        {
            if (! m.bandValid[(size_t) b]) continue;
            const double e = dbToPow (m.ltasDb[(size_t) b]);
            totE += e;
            if (bandHighHz (b) <= m.lowCutoffHz * 1.06) lowE += e;
        }
        m.lowEnergyRelDb = powToDb (lowE / std::max (totE, 1e-30));
    }

    // ---------------------------------------------------------------- stereo
    if (m.channels == 2)
    {
        m.stereo = true;
        using namespace vs::dsp;
        const auto lp = designSvf (SvfType::LowPass, sr, 150.0, 0.707);
        SvfState l1, l2, r1, r2;
        double ll = 0, rr = 0, lr = 0, lll = 0, rrl = 0, lrl = 0, mid = 0, side = 0;
        const auto& L = buf[0];
        const auto& R = buf[1];
        for (size_t i = 0; i < n; ++i)
        {
            const float lf = l2.process (lp, l1.process (lp, L[i]));
            const float rf = r2.process (lp, r1.process (lp, R[i]));
            if (! act.isActiveAt ((long long) i - lat)) continue;
            ll += (double) L[i] * L[i]; rr += (double) R[i] * R[i]; lr += (double) L[i] * R[i];
            lll += (double) lf * lf; rrl += (double) rf * rf; lrl += (double) lf * rf;
            const double md = 0.5 * ((double) L[i] + R[i]), sd = 0.5 * ((double) L[i] - R[i]);
            mid += md * md; side += sd * sd;
        }
        m.correlation = (ll > 0 && rr > 0) ? lr / std::sqrt (ll * rr) : 1.0;
        m.lowCorrelation = (lll > 1e-12 && rrl > 1e-12) ? lrl / std::sqrt (lll * rrl) : 1.0;
        m.sideToMidDb = powToDb (side / std::max (mid, 1e-30));
        // level change when folded to mono (L+R)/2 vs the average channel power
        m.monoLossDb = (ll + rr) > 0 ? powToDb (mid / (0.5 * (ll + rr))) : 0.0;
    }

    m.valid = true;
    return m;
}

} // namespace sx::analysis
