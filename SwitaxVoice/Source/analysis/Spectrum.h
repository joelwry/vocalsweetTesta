#pragma once
// Spectral analysis primitives (framework independent): radix-2 FFT and 1/6-octave band
// powers calibrated so that the sum over all bands equals the signal's mean-square value
// (Parseval with Hann-window power normalisation). Pink noise therefore reads flat.

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

namespace sx::analysis
{
constexpr int kBandsPerOctave = 6;
constexpr int kNumBands = 60;                   // 20 Hz .. 20.48 kHz
constexpr double kBandBaseHz = 20.0;

inline double bandLowHz (int b) noexcept { return kBandBaseHz * std::pow (2.0, (double) b / kBandsPerOctave); }
inline double bandHighHz (int b) noexcept { return bandLowHz (b + 1); }
inline double bandCentreHz (int b) noexcept { return kBandBaseHz * std::pow (2.0, ((double) b + 0.5) / kBandsPerOctave); }
inline int bandForFreq (double f) noexcept
{
    return std::clamp ((int) std::floor (std::log2 (std::max (f, kBandBaseHz) / kBandBaseHz) * kBandsPerOctave), 0, kNumBands - 1);
}

using BandArray = std::array<double, kNumBands>;

inline double powToDb (double p) noexcept { return 10.0 * std::log10 (std::max (p, 1.0e-20)); }
inline double dbToPow (double db) noexcept { return std::pow (10.0, db / 10.0); }

/** In-place iterative radix-2 complex FFT. */
class Fft
{
public:
    explicit Fft (int order = 12) { setOrder (order); }

    void setOrder (int order)
    {
        n = 1 << order;
        twiddle.resize ((size_t) n / 2);
        for (int k = 0; k < n / 2; ++k)
            twiddle[(size_t) k] = std::polar (1.0, -2.0 * 3.14159265358979323846 * k / n);
        rev.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < order; ++b) if (i & (1 << b)) r |= 1 << (order - 1 - b);
            rev[(size_t) i] = r;
        }
    }

    int size() const noexcept { return n; }

    void perform (std::vector<std::complex<double>>& x) const
    {
        for (int i = 0; i < n; ++i)
            if (i < rev[(size_t) i]) std::swap (x[(size_t) i], x[(size_t) rev[(size_t) i]]);
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int k = 0; k < half; ++k)
                {
                    const auto t = twiddle[(size_t) (k * step)] * x[(size_t) (i + k + half)];
                    x[(size_t) (i + k + half)] = x[(size_t) (i + k)] - t;
                    x[(size_t) (i + k)] += t;
                }
        }
    }

private:
    int n = 0;
    std::vector<std::complex<double>> twiddle;
    std::vector<int> rev;
};

/** Computes calibrated 1/6-octave band powers (mean-square units) of one windowed frame. */
class BandAnalyzer
{
public:
    void prepare (double sr, int order = 12)
    {
        sampleRate = sr;
        fft.setOrder (order);
        const int n = fft.size();
        window.resize ((size_t) n);
        double wsq = 0.0;
        for (int i = 0; i < n; ++i)
        {
            window[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * i / n);   // periodic Hann
            wsq += window[(size_t) i] * window[(size_t) i];
        }
        norm = 2.0 / ((double) n * wsq);
        work.assign ((size_t) n, {});
        binPow.assign ((size_t) n / 2 + 1, 0.0);

        const double binHz = sr / n;
        for (int b = 0; b < kNumBands; ++b)
        {
            const double lo = bandLowHz (b), hi = bandHighHz (b);
            valid[(size_t) b] = hi <= 0.5 * sr * 0.98;
            firstBin[(size_t) b] = (int) std::ceil (lo / binHz);
            lastBin[(size_t) b] = std::min ((int) std::ceil (hi / binHz) - 1, n / 2);
            widthBins[(size_t) b] = (hi - lo) / binHz;
        }
    }

    int frameSize() const noexcept { return fft.size(); }
    bool isBandValid (int b) const noexcept { return valid[(size_t) b]; }
    double getSampleRate() const noexcept { return sampleRate; }

    /** frame: frameSize() samples. Writes per-band mean-square power. */
    void analyse (const float* frame, BandArray& bands)
    {
        const int n = fft.size();
        for (int i = 0; i < n; ++i) work[(size_t) i] = { (double) frame[i] * window[(size_t) i], 0.0 };
        fft.perform (work);
        for (int k = 0; k <= n / 2; ++k)
            binPow[(size_t) k] = std::norm (work[(size_t) k]) * norm * ((k == 0 || k == n / 2) ? 0.5 : 1.0);

        for (int b = 0; b < kNumBands; ++b)
        {
            if (! valid[(size_t) b]) { bands[(size_t) b] = 0.0; continue; }
            const int f0 = firstBin[(size_t) b], f1 = lastBin[(size_t) b];
            if (f1 >= f0)
            {
                double s = 0.0;
                for (int k = f0; k <= f1; ++k) s += binPow[(size_t) k];
                bands[(size_t) b] = s;
            }
            else
            {
                // band narrower than one bin: interpolate the density at the band centre
                const double pos = bandCentreHz (b) / (sampleRate / n);
                const int k0 = std::clamp ((int) pos, 0, n / 2 - 1);
                const double frac = pos - k0;
                bands[(size_t) b] = (binPow[(size_t) k0] * (1.0 - frac) + binPow[(size_t) k0 + 1] * frac) * widthBins[(size_t) b];
            }
        }
    }

private:
    double sampleRate = 48000.0, norm = 1.0;
    Fft fft;
    std::vector<double> window, binPow;
    std::vector<std::complex<double>> work;
    std::array<bool, kNumBands> valid {};
    std::array<int, kNumBands> firstBin {}, lastBin {};
    std::array<double, kNumBands> widthBins {};
};

/** Smooths a dB band curve with a window of +/- `halfWidth` bands (only valid bands). */
inline BandArray smoothBands (const BandArray& db, const std::array<bool, kNumBands>& valid, int halfWidth)
{
    BandArray out {};
    for (int b = 0; b < kNumBands; ++b)
    {
        double s = 0.0; int n = 0;
        for (int k = std::max (0, b - halfWidth); k <= std::min (kNumBands - 1, b + halfWidth); ++k)
            if (valid[(size_t) k]) { s += db[(size_t) k]; ++n; }
        out[(size_t) b] = n > 0 ? s / n : db[(size_t) b];
    }
    return out;
}

/** Mean of a dB curve over a frequency range (valid bands only). */
inline double meanOverRange (const BandArray& db, const std::array<bool, kNumBands>& valid, double fLo, double fHi)
{
    double s = 0.0; int n = 0;
    for (int b = 0; b < kNumBands; ++b)
        if (valid[(size_t) b] && bandCentreHz (b) >= fLo && bandCentreHz (b) <= fHi) { s += db[(size_t) b]; ++n; }
    return n > 0 ? s / n : 0.0;
}

} // namespace sx::analysis
