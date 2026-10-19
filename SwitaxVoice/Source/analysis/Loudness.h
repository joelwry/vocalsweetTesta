#pragma once
// Loudness and peak measurement (framework independent).
//
//  * K-weighting and gated loudness per ITU-R BS.1770-4: momentary (400 ms), short-term (3 s),
//    integrated (absolute gate -70 LUFS, relative gate -10 LU). Filter coefficients are derived
//    for any sample rate from the analog prototype parameters (the method used by libebur128).
//  * Loudness range (LRA) per EBU Tech 3342: short-term values, absolute gate -70 LUFS,
//    relative gate -20 LU, 10th to 95th percentile.
//  * True peak: 4x oversampling with a windowed-sinc polyphase interpolator (BS.1770-4 Annex 2
//    approach; our own filter design, 64 taps).
//
// Memory is allocated in prepare() only. Gating uses fine (0.01 dB) histograms holding exact
// per-bin energy sums, so the integrated value is exact up to the 0.01 dB gate quantisation and
// memory use does not grow with programme length.

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace sx::analysis
{
constexpr double kPi = 3.14159265358979323846;

inline double energyToLufs (double e) noexcept { return e > 0.0 ? -0.691 + 10.0 * std::log10 (e) : -200.0; }
inline double lufsToEnergy (double l) noexcept { return std::pow (10.0, (l + 0.691) / 10.0); }

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;
    inline double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0; }
};

/** The two-stage K-weighting filter (high-shelf "pre-filter" + RLB high-pass). */
struct KWeighting
{
    Biquad shelf, highpass;

    void prepare (double sr) noexcept
    {
        // stage 1: high shelf (+4 dB above ~1.7 kHz)
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (kPi * f0 / sr);
            const double Vh = std::pow (10.0, G / 20.0);
            const double Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            shelf.b0 = (Vh + Vb * K / Q + K * K) / a0;
            shelf.b1 = 2.0 * (K * K - Vh) / a0;
            shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
            shelf.a1 = 2.0 * (K * K - 1.0) / a0;
            shelf.a2 = (1.0 - K / Q + K * K) / a0;
        }
        // stage 2: RLB high-pass (~38 Hz)
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (kPi * f0 / sr);
            const double a0 = 1.0 + K / Q + K * K;
            highpass.b0 = 1.0; highpass.b1 = -2.0; highpass.b2 = 1.0;
            highpass.a1 = 2.0 * (K * K - 1.0) / a0;
            highpass.a2 = (1.0 - K / Q + K * K) / a0;
        }
        reset();
    }
    void reset() noexcept { shelf.reset(); highpass.reset(); }
    inline double process (double x) noexcept { return highpass.process (shelf.process (x)); }

    /** Magnitude response in dB of the cascade at frequency f (for tests / documentation). */
    double magnitudeDb (double sr, double f) const
    {
        auto mag = [&] (const Biquad& q)
        {
            const double w = 2.0 * kPi * f / sr;
            const double cr = std::cos (w), ci = -std::sin (w), c2r = std::cos (2 * w), c2i = -std::sin (2 * w);
            const double nr = q.b0 + q.b1 * cr + q.b2 * c2r, ni = q.b1 * ci + q.b2 * c2i;
            const double dr = 1.0 + q.a1 * cr + q.a2 * c2r, di = q.a1 * ci + q.a2 * c2i;
            return std::sqrt ((nr * nr + ni * ni) / (dr * dr + di * di));
        };
        return 20.0 * std::log10 (mag (shelf) * mag (highpass));
    }
};

/** Fine loudness histogram (-70 .. +10 LUFS in 0.01 dB bins) with exact energy sums. */
class LoudnessHistogram
{
public:
    static constexpr double kMin = -70.0, kMax = 10.0, kStep = 0.01;
    static constexpr int kBins = (int) ((kMax - kMin) / kStep) + 1;

    void prepare() { counts.assign ((size_t) kBins, 0); energy.assign ((size_t) kBins, 0.0); total = 0; }
    void reset() { std::fill (counts.begin(), counts.end(), 0); std::fill (energy.begin(), energy.end(), 0.0); total = 0; }

    void add (double blockEnergy) noexcept
    {
        const double l = energyToLufs (blockEnergy);
        if (l < kMin) return;   // absolute gate (-70 LUFS) is applied here
        const int b = std::clamp ((int) ((l - kMin) / kStep), 0, kBins - 1);
        counts[(size_t) b] += 1;
        energy[(size_t) b] += blockEnergy;
        ++total;
    }

    long long count() const noexcept { return total; }

    /** BS.1770 integrated loudness: blocks above the absolute gate, then relative gate. */
    double integrated (double relativeGateLu = -10.0) const noexcept
    {
        if (total == 0) return -200.0;
        double sum = 0; long long n = 0;
        for (int b = 0; b < kBins; ++b) { sum += energy[(size_t) b]; n += counts[(size_t) b]; }
        const double gate = energyToLufs (sum / (double) n) + relativeGateLu;
        const int first = std::clamp ((int) std::ceil ((gate - kMin) / kStep), 0, kBins);
        sum = 0; n = 0;
        for (int b = first; b < kBins; ++b) { sum += energy[(size_t) b]; n += counts[(size_t) b]; }
        return n > 0 ? energyToLufs (sum / (double) n) : -200.0;
    }

    /** Loudness value at a percentile (0..1) of the blocks above an optional relative gate. */
    double percentile (double p, double relativeGateLu = -20.0) const noexcept
    {
        if (total == 0) return -200.0;
        double sum = 0; long long n = 0;
        for (int b = 0; b < kBins; ++b) { sum += energy[(size_t) b]; n += counts[(size_t) b]; }
        const double gate = energyToLufs (sum / (double) n) + relativeGateLu;
        const int first = std::clamp ((int) std::ceil ((gate - kMin) / kStep), 0, kBins);
        long long m = 0;
        for (int b = first; b < kBins; ++b) m += counts[(size_t) b];
        if (m == 0) return -200.0;
        const long long target = (long long) std::floor (p * (double) (m - 1));
        long long acc = 0;
        for (int b = first; b < kBins; ++b)
        {
            acc += counts[(size_t) b];
            if (acc > target) return kMin + ((double) b + 0.5) * kStep;
        }
        return kMax;
    }

    /** EBU Tech 3342 loudness range (use with short-term values). */
    double range() const noexcept
    {
        if (total < 2) return 0.0;
        return std::max (0.0, percentile (0.95, -20.0) - percentile (0.10, -20.0));
    }

private:
    std::vector<long long> counts;
    std::vector<double> energy;
    long long total = 0;
};

/** Gated loudness meter: momentary, short-term, integrated, LRA. Channels 1 or 2 (G = 1.0). */
class LoudnessMeter
{
public:
    void prepare (double sr, int numChannels)
    {
        sampleRate = sr;
        channels = std::clamp (numChannels, 1, 2);
        hop = std::max (1, (int) std::lround (0.1 * sr));
        for (auto& k : kw) k.prepare (sr);
        momentaryHist.prepare();
        shortTermHist.prepare();
        reset();
    }

    void reset()
    {
        for (auto& k : kw) k.reset();
        ring.fill (0.0);
        ringCount = 0; ringPos = 0; acc = 0.0; accN = 0;
        momentaryHist.reset();
        shortTermHist.reset();
        lastMomentary = lastShortTerm = -200.0;
        maxMomentary = maxShortTerm = -200.0;
    }

    /** ch[c][i]; numChannels must match prepare() (mono may also be passed to a stereo meter). */
    void process (const float* const* ch, int numChannels, int numSamples) noexcept
    {
        numChannels = std::min (numChannels, channels);
        for (int i = 0; i < numSamples; ++i)
        {
            double e = 0.0;
            for (int c = 0; c < numChannels; ++c)
            {
                const double y = kw[(size_t) c].process ((double) ch[c][i]);
                e += y * y;
            }
            acc += e;
            if (++accN >= hop) completeHop();
        }
    }

    double momentary() const noexcept { return lastMomentary; }
    double shortTerm() const noexcept { return lastShortTerm; }
    double integrated() const noexcept { return momentaryHist.integrated(); }
    double loudnessRange() const noexcept { return shortTermHist.range(); }
    double maxMomentaryLufs() const noexcept { return maxMomentary; }
    double maxShortTermLufs() const noexcept { return maxShortTerm; }
    const LoudnessHistogram& momentaryHistogram() const noexcept { return momentaryHist; }
    const LoudnessHistogram& shortTermHistogram() const noexcept { return shortTermHist; }
    double getSampleRate() const noexcept { return sampleRate; }

    int hopsCompleted() const noexcept { return ringCount; }

private:
    void completeHop() noexcept
    {
        ring[(size_t) ringPos] = acc / (double) accN;
        ringPos = (ringPos + 1) % (int) ring.size();
        ++ringCount;
        acc = 0.0; accN = 0;

        auto meanOf = [this] (int blocks)
        {
            double s = 0;
            for (int k = 1; k <= blocks; ++k) s += ring[(size_t) ((ringPos - k + (int) ring.size()) % (int) ring.size())];
            return s / blocks;
        };
        if (ringCount >= 4)
        {
            const double m = meanOf (4);
            lastMomentary = energyToLufs (m);
            maxMomentary = std::max (maxMomentary, lastMomentary);
            momentaryHist.add (m);
        }
        if (ringCount >= 30)
        {
            const double s = meanOf (30);
            lastShortTerm = energyToLufs (s);
            maxShortTerm = std::max (maxShortTerm, lastShortTerm);
            shortTermHist.add (s);
        }
    }

    double sampleRate = 48000.0;
    int channels = 2, hop = 4800;
    std::array<KWeighting, 2> kw;
    std::array<double, 30> ring {};
    int ringCount = 0, ringPos = 0, accN = 0;
    double acc = 0.0;
    LoudnessHistogram momentaryHist, shortTermHist;
    double lastMomentary = -200.0, lastShortTerm = -200.0, maxMomentary = -200.0, maxShortTerm = -200.0;
};

/** 4x oversampling true-peak meter (per channel state, max across channels). */
class TruePeakMeter
{
public:
    static constexpr int kPhases = 4, kTapsPerPhase = 16;

    TruePeakMeter()
    {
        // windowed-sinc low-pass at the original Nyquist, Kaiser window (beta 7)
        const int N = kPhases * kTapsPerPhase;
        const double centre = (N - 1) * 0.5;
        auto bessel0 = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
        const double beta = 7.0;
        for (int n = 0; n < N; ++n)
        {
            const double t = ((double) n - centre) / kPhases;
            const double sinc = std::abs (t) < 1e-12 ? 1.0 : std::sin (kPi * t) / (kPi * t);
            const double r = ((double) n - centre) / centre;
            const double win = bessel0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / bessel0 (beta);
            coeffs[(size_t) (n % kPhases)][(size_t) (n / kPhases)] = sinc * win;
        }
        // normalise each phase to unity DC gain
        for (auto& ph : coeffs)
        {
            double s = 0; for (double c : ph) s += c;
            for (double& c : ph) c /= s;
        }
        reset();
    }

    void reset() noexcept
    {
        for (auto& h : hist) h.fill (0.0);
        pos = 0; peakTrue = 0.0; peakSample = 0.0;
    }

    void process (const float* const* ch, int numChannels, int numSamples) noexcept
    {
        numChannels = std::min (numChannels, 2);
        for (int i = 0; i < numSamples; ++i)
        {
            for (int c = 0; c < numChannels; ++c)
            {
                const double x = (double) ch[c][i];
                peakSample = std::max (peakSample, std::abs (x));
                auto& h = hist[(size_t) c];
                h[(size_t) pos] = x;
                for (int p = 0; p < kPhases; ++p)
                {
                    double y = 0.0;
                    for (int k = 0; k < kTapsPerPhase; ++k)
                        y += coeffs[(size_t) p][(size_t) k] * h[(size_t) ((pos - k + kTapsPerPhase) % kTapsPerPhase)];
                    peakTrue = std::max (peakTrue, std::abs (y));
                }
            }
            pos = (pos + 1) % kTapsPerPhase;
        }
    }

    double truePeakDb() const noexcept { return 20.0 * std::log10 (std::max (std::max (peakTrue, peakSample), 1.0e-10)); }
    double samplePeakDb() const noexcept { return 20.0 * std::log10 (std::max (peakSample, 1.0e-10)); }
    void resetPeaks() noexcept { peakTrue = peakSample = 0.0; }

private:
    std::array<std::array<double, kTapsPerPhase>, kPhases> coeffs {};
    std::array<std::array<double, kTapsPerPhase>, 2> hist {};
    int pos = 0;
    double peakTrue = 0.0, peakSample = 0.0;
};

} // namespace sx::analysis
