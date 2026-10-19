#pragma once
// SwitaxVoice processing chain (framework independent, real-time safe). Built from the shared
// VocalSweet DSP primitives (TPT SVF filters, compressor, de-esser, ADAA saturation, exciter,
// doubler, look-ahead limiter), which are reused unmodified from VocalSweet/Source/dsp.
//
//  in -> input gain -> HPF (12/24 dB) -> low-mid bell -> box bell -> harsh bell (narrow)
//     -> compressor -> saturation -> presence bell -> air shelf -> exciter -> de-esser -> doubler
//     -> dry/wet -> output gain -> look-ahead limiter (ceiling) -> bypass crossfade -> out
//
// All memory is allocated in prepare(). process() performs no allocation, locking or I/O.
// The same class is used by the plugin (real time) and by the analysis advisor (offline
// simulation of a recommendation on captured audio), so predictions use the exact DSP.

#include "Params.h"
#include "dsp/Dynamics.h"
#include "dsp/Tone.h"

namespace sx::dsp
{
using namespace vs::dsp;

class Chain
{
public:
    struct Meters
    {
        float inPeak[kMaxChannels] {}, outPeak[kMaxChannels] {};
        float inRms[kMaxChannels] {}, outRms[kMaxChannels] {};
        float compGrDb = 0, deessGrDb = 0, limGrDb = 0;
    };

    static constexpr int kSubBlock = 16;

    void prepare (double newSampleRate, int /*maxBlockSize*/)
    {
        sampleRate = newSampleRate;
        comp.prepare (sampleRate);
        deess.prepare (sampleRate);
        sat.prepare (sampleRate);
        excite.prepare (sampleRate);
        doubler.prepare (sampleRate);
        limiter.prepare (sampleRate, 1.0);
        bypassDelay.prepare (limiter.getLatency());
        bypassDelay.setDelay (limiter.getLatency());

        for (auto& s : slow) s.prepare (sampleRate / kSubBlock, 40.0);
        for (auto& s : fast) s.prepare (sampleRate, 20.0);
        for (int f : { fBypass, fHpfOn, fHpfSlope, fCompOn, fSatOn, fExciteOn, fDeessOn })
            fast[(size_t) f].prepareLinear (sampleRate, 20.0);   // on/off crossfades: exact 20 ms ramps
        reset();
        prepared = true;
    }

    void reset()
    {
        for (auto& ch : filt) for (auto& f : ch) f.reset();
        comp.reset(); deess.reset(); sat.reset(); excite.reset(); doubler.reset();
        limiter.reset(); bypassDelay.reset();
        applyTargets (true);
        updateCoefficients (true);
    }

    int getLatencySamples() const noexcept { return limiter.getLatency(); }
    double getSampleRate() const noexcept { return sampleRate; }

    /** Set new parameter targets (call once per block before process()). */
    void setParams (const ParamValues& v) noexcept
    {
        params = v;
        applyTargets (false);
    }

    /** Gain (dB) applied to the bypass path only - used for loudness-matched A/B comparison.
        0 dB keeps bypass bit-exact. */
    void setBypassGainDb (float db) noexcept { bypassGainTarget = db == 0.0f ? 1.0f : dbToGain (std::clamp (db, -24.0f, 24.0f)); }

    /** Process in place. numChannels 1 or 2 (extra channels are left untouched). */
    void process (float* const* channels, int numChannels, int numSamples) noexcept
    {
        numChannels = std::min (numChannels, kMaxChannels);
        if (! prepared || numChannels <= 0) return;

        Meters m;
        double inSq[kMaxChannels] {}, outSq[kMaxChannels] {};
        float compGrMax = 0.0f, deessGrMax = 0.0f;

        for (int start = 0; start < numSamples; start += kSubBlock)
        {
            const int n = std::min (kSubBlock, numSamples - start);
            for (auto& s : slow) s.skip (1);
            updateCoefficients (false);

            for (int i = start; i < start + n; ++i)
            {
                float frame[kMaxChannels], dry[kMaxChannels], byp[kMaxChannels];

                const float inGain = fast[fInGain].next();
                const float outGain = fast[fOutGain].next();
                const float makeup = fast[fMakeup].next();
                const float mix = fast[fMix].next();
                const float bypass = fast[fBypass].next();
                const float eHpf = fast[fHpfOn].next();
                const float hpfSlope = fast[fHpfSlope].next();
                const float eComp = fast[fCompOn].next();
                const float eSat = fast[fSatOn].next();
                const float eExc = fast[fExciteOn].next();
                const float eDeess = fast[fDeessOn].next();
                // bypass-path gain: exact 1.0 when not comparing so bypass stays bit-exact
                bypassGain = bypassGainTarget == 1.0f && std::abs (bypassGain - 1.0f) < 1.0e-4f
                                 ? 1.0f : bypassGainTarget + bypassCoeff * (bypassGain - bypassGainTarget);

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    float x = channels[ch][i];
                    if (! std::isfinite (x)) x = 0.0f;   // never let NaN/Inf enter the filters
                    x = std::clamp (x, -64.0f, 64.0f);   // +36 dBFS absolute input guard
                    m.inPeak[ch] = std::max (m.inPeak[ch], std::abs (x));
                    inSq[ch] += (double) x * x;
                    dry[ch] = x;
                    byp[ch] = bypassDelay.process (ch, x);

                    auto& f = filt[(size_t) ch];
                    float s = x * inGain;

                    // high-pass (two cascaded 2nd-order stages, slope crossfaded)
                    const float h1 = f[kHpf1].process (cHpf, s);
                    const float h2 = f[kHpf2].process (cHpf, h1);
                    const float hp = h1 + hpfSlope * (h2 - h1);
                    s += eHpf * (hp - s);

                    // corrective EQ (before the compressor so it does not react to the problem)
                    s = f[kLow].process (cLow, s);
                    s = f[kBox].process (cBox, s);
                    s = f[kHarsh].process (cHarsh, s);
                    frame[ch] = s;
                }
                bypassDelay.advance();

                // compressor (stereo linked)
                {
                    float det = 0.0f;
                    for (int ch = 0; ch < numChannels; ++ch) det = std::max (det, std::abs (frame[ch]));
                    const float g = comp.processSample (det) * makeup;
                    for (int ch = 0; ch < numChannels; ++ch) frame[ch] += eComp * (frame[ch] * g - frame[ch]);
                    compGrMax = std::max (compGrMax, comp.getGainReductionDb() * eComp);
                }

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    auto& f = filt[(size_t) ch];
                    float s = frame[ch];
                    s += eSat * (sat.process (ch, s) - s);
                    s = f[kPres].process (cPres, s);
                    s = f[kAir].process (cAir, s);
                    s += eExc * (excite.process (ch, s) - s);
                    frame[ch] = s;
                }

                // de-esser (split band, linked)
                {
                    float d[kMaxChannels];
                    for (int ch = 0; ch < numChannels; ++ch) d[ch] = frame[ch];
                    deess.processFrame (d, numChannels);
                    for (int ch = 0; ch < numChannels; ++ch) frame[ch] += eDeess * (d[ch] - frame[ch]);
                    deessGrMax = std::max (deessGrMax, deess.getGainReductionDb() * eDeess);
                }

                doubler.processFrame (frame, numChannels);

                for (int ch = 0; ch < numChannels; ++ch)
                    frame[ch] = (dry[ch] + mix * (frame[ch] - dry[ch])) * outGain;

                limiter.processFrame (frame, numChannels);

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const float b = byp[ch] * bypassGain;
                    // written so that bypass == 1 is the bit-exact (latency-compensated) dry signal
                    const float y = bypass >= 1.0f ? b : (1.0f - bypass) * frame[ch] + bypass * b;
                    channels[ch][i] = y;
                    m.outPeak[ch] = std::max (m.outPeak[ch], std::abs (y));
                    outSq[ch] += (double) y * y;
                }
            }
        }

        // stability watchdog: if anything blew up, reset and output silence for this block
        if (! isStateFinite())
        {
            reset();
            for (int ch = 0; ch < numChannels; ++ch)
                std::fill (channels[ch], channels[ch] + numSamples, 0.0f);
            ++watchdogResets;
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            m.inRms[ch] = (float) std::sqrt (inSq[ch] / std::max (1, numSamples));
            m.outRms[ch] = (float) std::sqrt (outSq[ch] / std::max (1, numSamples));
        }
        if (numChannels == 1)
        {
            m.inPeak[1] = m.inPeak[0]; m.outPeak[1] = m.outPeak[0];
            m.inRms[1] = m.inRms[0];   m.outRms[1] = m.outRms[0];
        }
        m.compGrDb = compGrMax;
        m.deessGrDb = deessGrMax;
        m.limGrDb = limiter.popGainReductionDb();
        meters = m;
    }

    const Meters& getMeters() const noexcept { return meters; }
    int getWatchdogResets() const noexcept { return watchdogResets; }

    /** Static magnitude response (dB) of the linear EQ section for the given parameter values -
        used by the editor's curve display and by the advisor, so both show what the DSP does. */
    static double responseDb (const ParamValues& v, double sr, double freq)
    {
        double db = 0.0;
        if (isOn (v, pHpfOn))
        {
            const auto c = designSvf (SvfType::HighPass, sr, v[pHpfFreq], 0.707);
            db += svfMagnitudeDb (c, sr, freq) * (isOn (v, pHpfSlope) ? 2.0 : 1.0);
        }
        if (isOn (v, pEqOn))
        {
            db += svfMagnitudeDb (designSvf (SvfType::Bell, sr, v[pLowFreq], kLowQ, v[pLowGain]), sr, freq);
            db += svfMagnitudeDb (designSvf (SvfType::Bell, sr, v[pBoxFreq], kBoxQ, v[pBoxGain]), sr, freq);
            db += svfMagnitudeDb (designSvf (SvfType::Bell, sr, v[pHarshFreq], kHarshQ, v[pHarshGain]), sr, freq);
            db += svfMagnitudeDb (designSvf (SvfType::Bell, sr, v[pPresFreq], kPresQ, v[pPresGain]), sr, freq);
            db += svfMagnitudeDb (designSvf (SvfType::HighShelf, sr, v[pAirFreq], 0.707, v[pAirGain]), sr, freq);
        }
        return db;
    }

    static constexpr double kLowQ = 1.0, kBoxQ = 1.4, kHarshQ = 2.5, kPresQ = 0.9;

private:
    enum FilterSlot { kHpf1, kHpf2, kLow, kBox, kHarsh, kPres, kAir, kNumFilters };
    enum Slow
    {
        sHpfFreq, sLowFreq, sLowGain, sBoxFreq, sBoxGain, sHarshFreq, sHarshGain,
        sPresFreq, sPresGain, sAirFreq, sAirGain,
        sCompThresh, sCompRatio, sCompAttack, sCompRelease,
        sDeessFreq, sDeessThresh, sDeessRange,
        sSatDrive, sExcFreq, sExcAmount, sDblAmount, sDblWidth, sCeiling,
        kNumSlow
    };
    enum Fast { fInGain, fOutGain, fMakeup, fMix, fBypass, fHpfOn, fHpfSlope, fCompOn, fSatOn, fExciteOn, fDeessOn, kNumFast };

    void applyTargets (bool snap) noexcept
    {
        const auto& v = params;
        const float eq = isOn (v, pEqOn) ? 1.0f : 0.0f;
        const float slowT[kNumSlow] = {
            v[pHpfFreq], v[pLowFreq], eq * v[pLowGain], v[pBoxFreq], eq * v[pBoxGain], v[pHarshFreq], eq * v[pHarshGain],
            v[pPresFreq], eq * v[pPresGain], v[pAirFreq], eq * v[pAirGain],
            v[pCompThresh], v[pCompRatio], v[pCompAttack], v[pCompRelease],
            v[pDeessFreq], v[pDeessThresh], v[pDeessRange],
            v[pSatDrive] * 0.01f, v[pExciteFreq], v[pExciteAmount] * 0.01f,
            isOn (v, pDoubleOn) ? v[pDoubleAmount] * 0.01f : 0.0f, v[pDoubleWidth] * 0.01f, v[pCeiling] };
        const float fastT[kNumFast] = {
            dbToGain (v[pInputGain]), dbToGain (v[pOutputGain]), dbToGain (effectiveMakeupDb (v)), v[pMix] * 0.01f,
            isOn (v, pBypass) ? 1.0f : 0.0f, isOn (v, pHpfOn) ? 1.0f : 0.0f, isOn (v, pHpfSlope) ? 1.0f : 0.0f,
            isOn (v, pCompOn) ? 1.0f : 0.0f, isOn (v, pSatOn) ? 1.0f : 0.0f, isOn (v, pExciteOn) ? 1.0f : 0.0f,
            isOn (v, pDeessOn) ? 1.0f : 0.0f };

        for (int i = 0; i < kNumSlow; ++i) snap ? slow[(size_t) i].snap (slowT[i]) : slow[(size_t) i].setTarget (slowT[i]);
        for (int i = 0; i < kNumFast; ++i) snap ? fast[(size_t) i].snap (fastT[i]) : fast[(size_t) i].setTarget (fastT[i]);
        limiter.setEnabled (isOn (v, pLimiterOn));
        if (snap) bypassGain = bypassGainTarget;
        bypassCoeff = timeToCoeff (sampleRate, 30.0);
    }

    void updateCoefficients (bool force) noexcept
    {
        float v[kNumSlow];
        bool changed[kNumSlow];
        for (int i = 0; i < kNumSlow; ++i)
        {
            v[i] = slow[(size_t) i].current;
            changed[i] = force || v[i] != lastSlow[i];
            lastSlow[i] = v[i];
        }
        auto any = [&] (std::initializer_list<int> ids) { for (int id : ids) if (changed[id]) return true; return false; };
        const double sr = sampleRate;

        if (any ({ sHpfFreq }))                 cHpf   = designSvf (SvfType::HighPass, sr, v[sHpfFreq], 0.707);
        if (any ({ sLowFreq, sLowGain }))       cLow   = designSvf (SvfType::Bell, sr, v[sLowFreq], kLowQ, v[sLowGain]);
        if (any ({ sBoxFreq, sBoxGain }))       cBox   = designSvf (SvfType::Bell, sr, v[sBoxFreq], kBoxQ, v[sBoxGain]);
        if (any ({ sHarshFreq, sHarshGain }))   cHarsh = designSvf (SvfType::Bell, sr, v[sHarshFreq], kHarshQ, v[sHarshGain]);
        if (any ({ sPresFreq, sPresGain }))     cPres  = designSvf (SvfType::Bell, sr, v[sPresFreq], kPresQ, v[sPresGain]);
        if (any ({ sAirFreq, sAirGain }))       cAir   = designSvf (SvfType::HighShelf, sr, v[sAirFreq], 0.707, v[sAirGain]);
        if (any ({ sCompThresh, sCompRatio, sCompAttack, sCompRelease }))
            comp.setParams (v[sCompThresh], v[sCompRatio], v[sCompAttack], v[sCompRelease]);
        if (any ({ sDeessFreq, sDeessThresh, sDeessRange }))
            deess.setParams (v[sDeessFreq], v[sDeessThresh], v[sDeessRange]);
        if (any ({ sSatDrive }))                sat.setParams (v[sSatDrive], 0.12f);
        if (any ({ sExcFreq, sExcAmount }))     excite.setParams (v[sExcFreq], v[sExcAmount]);
        if (any ({ sDblAmount, sDblWidth }))    doubler.setParams (v[sDblAmount], v[sDblWidth]);
        if (any ({ sCeiling }))                 limiter.setCeiling (v[sCeiling]);
    }

    bool isStateFinite() const noexcept
    {
        for (auto& ch : filt) for (auto& f : ch) if (! f.isFinite()) return false;
        return deess.isFinite() && sat.isFinite() && excite.isFinite() && doubler.isFinite()
               && std::isfinite (comp.getGainReductionDb()) && std::isfinite (bypassGain);
    }

    double sampleRate = 48000.0;
    bool prepared = false;
    int watchdogResets = 0;
    ParamValues params = defaultParamValues();

    std::array<std::array<SvfState, kNumFilters>, kMaxChannels> filt;
    SvfCoeffs cHpf, cLow, cBox, cHarsh, cPres, cAir;
    Compressor comp;
    DeEsser deess;
    Saturator sat;
    Exciter excite;
    Doubler doubler;
    Limiter limiter;
    DelayLine bypassDelay;
    float bypassGain = 1.0f, bypassGainTarget = 1.0f, bypassCoeff = 0.0f;

    std::array<Smoother, kNumSlow> slow;
    std::array<Smoother, kNumFast> fast;
    float lastSlow[kNumSlow] {};
    Meters meters;
};

} // namespace sx::dsp
