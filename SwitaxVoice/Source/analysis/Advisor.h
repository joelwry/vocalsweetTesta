#pragma once
// SwitaxVoice Assist: explainable recommendation engine (framework independent).
//
// Pipeline (runs on a background thread, never on the audio thread):
//   1. Measure the raw capture (Analyzer.h).
//   2. Render the capture offline through the plugin's own DSP chain with the CURRENT settings
//      and measure that ("what you hear now").
//   3. Rules compare those measurements with the chosen target (house target or a measured
//      reference) and propose bounded parameter changes. Rules never auto-apply anything.
//   4. Every proposal is SIMULATED: the capture is rendered again with the proposed settings and
//      the targeted metric is re-measured, so the card shows a predicted, measured outcome on the
//      user's own audio rather than a generic promise.
//
// Every recommendation separates: measured facts (evidence), a heuristic interpretation (likely
// issue), a subjective suggestion (proposal), a confidence value with its basis, and limitations.

#include "../core/Chain.h"
#include "Targets.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <functional>
#include <memory>

namespace sx::analysis
{
enum class Basis { Measured, Heuristic, Simulated, Suggestion };

inline const char* basisLabel (Basis b)
{
    switch (b)
    {
        case Basis::Measured:   return "MEASURED";
        case Basis::Heuristic:  return "HEURISTIC";
        case Basis::Simulated:  return "SIMULATED";
        case Basis::Suggestion: return "SUGGESTION";
    }
    return "";
}

struct EvidenceLine { Basis basis; std::string text; };
struct ParamChange { int index; float from, to; };

struct Recommendation
{
    std::string key;                       // stable id ("lowend", "lowmid", "deess", ...)
    std::string category;                  // Low end / Tone / Dynamics / Sibilance / Level / Stereo / Source / Mix
    std::string title;
    int severity = 1;                      // 0 info, 1 minor, 2 moderate, 3 major
    std::vector<EvidenceLine> evidence;    // measured facts
    std::string issue;                     // heuristic interpretation
    std::string proposal;                  // human-readable proposed change (suggestion)
    std::vector<ParamChange> changes;      // empty -> information only
    std::vector<std::string> predicted;    // simulated outcome on the captured audio
    double confidence = 0.5;
    std::string confidenceBasis;
    std::string limitations;

    bool isActionable() const { return ! changes.empty(); }
};

struct Report
{
    bool valid = false;
    std::string status;                    // one-line summary or reason for no analysis
    Context context = Context::IsolatedVocal;
    std::string targetName;
    bool targetMeasured = false;
    std::vector<std::string> signalNotes;  // what was (and was not) available to analyse

    Measurements raw, current, predicted, sidechain;
    bool hasSidechain = false;
    Target target;
    ParamValues analysedWith {};           // settings at analysis time
    ParamValues plan {};                   // settings with every recommendation applied
    std::vector<Recommendation> recs;
    double compGrTypicalNow = 0, compGrTypicalPlan = 0;
    double analysisSeconds = 0;            // wall time spent (for diagnostics)
};

struct AdvisorInput
{
    const Buffer* raw = nullptr;           // captured plugin input
    const Buffer* sidechain = nullptr;     // captured sidechain (beat), optional
    double sampleRate = 48000.0;
    ParamValues current = defaultParamValues();
    Context context = Context::IsolatedVocal;
    Target target = genreTarget (Genre::AfrobeatLead);
    std::function<void (float)> progress;  // 0..1, may be empty
    const std::atomic<bool>* cancel = nullptr;
};

//==============================================================================
namespace detail
{
inline std::string fmt (const char* f, ...)
{
    char b[512];
    va_list a;
    va_start (a, f);
    std::vsnprintf (b, sizeof b, f, a);
    va_end (a);
    return b;
}

inline std::string hz (double f) { return f >= 1000.0 ? fmt ("%.1f kHz", f / 1000.0) : fmt ("%.0f Hz", f); }

struct RenderResult
{
    Buffer out;
    int latency = 0;
    double grTypical = 0, grP95 = 0, deessTypical = 0;   // dB, over active 10 ms blocks
};

/** Render a capture through the real DSP chain (offline, same code as the plugin). */
inline RenderResult render (const Buffer& in, double sr, const ParamValues& v, const Activity* act)
{
    RenderResult r;
    r.out = in;
    const int chs = (int) std::min<size_t> (in.size(), 2);
    sx::dsp::Chain chain;
    chain.prepare (sr, 512);
    chain.setParams (v);
    chain.reset();
    r.latency = chain.getLatencySamples();
    const int n = (int) in[0].size();
    const int block = std::max (16, (int) std::lround (0.01 * sr));
    std::vector<double> gr, ds;
    for (int start = 0; start < n; start += block)
    {
        const int len = std::min (block, n - start);
        float* ptrs[2] = { r.out[0].data() + start, r.out[(size_t) (chs - 1)].data() + start };
        chain.process (ptrs, chs, len);
        if (act == nullptr || act->isActiveAt (start))
        {
            gr.push_back (chain.getMeters().compGrDb);
            ds.push_back (chain.getMeters().deessGrDb);
        }
    }
    if (! gr.empty())
    {
        double s = 0; for (double g : gr) s += g;
        r.grTypical = s / (double) gr.size();
        r.grP95 = percentileOf (gr, 0.95);
        double d = 0; for (double g : ds) d += g;
        r.deessTypical = d / (double) ds.size();
    }
    return r;
}

inline AnalysisOptions optsFor (const Activity& a, const SibilanceMap& s, int latency, bool full)
{
    AnalysisOptions o;
    o.activity = &a;
    o.sibilanceMap = &s;
    o.latencySamples = latency;
    o.clipping = false;
    o.pitch = full;
    o.spectrum = full;
    o.sibilance = full;
    return o;
}

inline double clamp01 (double x) { return std::clamp (x, 0.0, 1.0); }

/** Smoothed (1/3-octave) deviation of a measurement from the target. The level offset between
    the two curves is removed with the MEDIAN deviation over 100 Hz - 12 kHz, so a large local
    problem (e.g. +8 dB of mud) cannot drag the reference level and hide itself. */
inline BandArray deviation (const Measurements& m, const Target& t)
{
    BandArray d {};
    std::vector<double> mid;
    for (int b = 0; b < kNumBands; ++b)
    {
        d[(size_t) b] = m.relDb[(size_t) b] - t.relDb[(size_t) b];
        if (m.bandValid[(size_t) b] && bandCentreHz (b) >= 100.0 && bandCentreHz (b) <= 12000.0) mid.push_back (d[(size_t) b]);
    }
    const double offset = percentileOf (mid, 0.5);
    for (auto& x : d) x -= offset;
    return smoothBands (d, m.bandValid, 1);
}

/** Deviation of one frame (already normalised like relDb) with the same offset rule. */
inline double frameRegionDeviation (const BandArray& f, const Target& t, const std::array<bool, kNumBands>& valid, double lo, double hi)
{
    std::vector<double> mid;
    for (int b = 0; b < kNumBands; ++b)
        if (valid[(size_t) b] && bandCentreHz (b) >= 100.0 && bandCentreHz (b) <= 12000.0) mid.push_back (f[(size_t) b] - t.relDb[(size_t) b]);
    const double offset = percentileOf (mid, 0.5);
    double s = 0; int n = 0;
    for (int b = 0; b < kNumBands; ++b)
        if (valid[(size_t) b] && bandCentreHz (b) >= lo && bandCentreHz (b) <= hi) { s += f[(size_t) b] - t.relDb[(size_t) b] - offset; ++n; }
    return n > 0 ? s / n : 0.0;
}

struct RegionStat { double mean = 0, peak = -1e9, trough = 1e9; int peakBand = -1, troughBand = -1; };

inline RegionStat region (const BandArray& d, const std::array<bool, kNumBands>& valid, double lo, double hi)
{
    RegionStat r; int n = 0;
    for (int b = 0; b < kNumBands; ++b)
    {
        const double f = bandCentreHz (b);
        if (! valid[(size_t) b] || f < lo || f > hi) continue;
        r.mean += d[(size_t) b]; ++n;
        if (d[(size_t) b] > r.peak) { r.peak = d[(size_t) b]; r.peakBand = b; }
        if (d[(size_t) b] < r.trough) { r.trough = d[(size_t) b]; r.troughBand = b; }
    }
    if (n > 0) r.mean /= n;
    return r;
}

inline float roundTo (float v, float step) { return std::round (v / step) * step; }
} // namespace detail

//==============================================================================
class Advisor
{
public:
    explicit Advisor (AdvisorInput in) : input (std::move (in)) {}

    Report run()
    {
        using namespace detail;
        Report rep;
        rep.context = input.context;
        rep.target = input.target;
        rep.targetName = input.target.name;
        rep.targetMeasured = input.target.measured;
        rep.analysedWith = input.current;
        rep.plan = input.current;
        sr = input.sampleRate;

        if (input.raw == nullptr || input.raw->empty() || input.raw->front().size() < (size_t) (sr * 1.0))
        {
            rep.status = "Nothing captured yet. Press LEARN and play at least 5 seconds of the part.";
            return rep;
        }
        const Buffer& raw = *input.raw;
        const int chs = (int) std::min<size_t> (raw.size(), 2);

        // ---- 1. raw measurement
        tick (0.02f);
        rep.raw = analyse (raw, sr, AnalysisOptions {}, &act, &sibMap);
        if (input.target.pitchAdaptive && rep.raw.pitch.voicedFrames > 50)
        {
            adaptToPitch (input.target, rep.raw.pitch.p5, rep.raw.pitch.p50, rep.raw.pitch.p95);
            rep.target = input.target;
        }
        rep.signalNotes.push_back (fmt ("Analysed: this plugin's input (before SwitaxVoice processing), %s, %.1f kHz, "
                                        "%.1f s with signal out of %.1f s captured.",
                                        chs == 2 ? "stereo" : "mono", sr / 1000.0, rep.raw.activeSeconds, rep.raw.totalSeconds));
        rep.signalNotes.push_back ("Simulated: your current SwitaxVoice settings and every proposal, rendered offline through the plugin's own DSP.");
        rep.signalNotes.push_back (isVocalContext (input.context)
            ? "Not available: other tracks, the beat and the master bus. Conclusions are about this track only."
            : "Context is a mix/instrumental: only valid if SwitaxVoice is inserted on that bus. Vocal processing proposals are disabled.");

        if (rep.raw.activeSeconds < 3.0)
        {
            rep.status = fmt ("Only %.1f s of signal was captured. Play at least 5 seconds of the part and try again.", rep.raw.activeSeconds);
            return rep;
        }

        if (input.sidechain != nullptr && ! input.sidechain->empty())
        {
            const auto scMono = monoMix (*input.sidechain);
            double e = 0; for (float x : scMono) e += (double) x * x;
            if (! scMono.empty() && powToDb (e / (double) scMono.size()) > -60.0)
            {
                rep.sidechain = analyse (*input.sidechain, sr, AnalysisOptions {});
                rep.hasSidechain = rep.sidechain.valid && rep.sidechain.activeSeconds > 3.0;
            }
        }
        rep.signalNotes.push_back (rep.hasSidechain
            ? fmt ("Sidechain (beat): %.1f s with signal captured; used for vocal-vs-beat level and masking checks.", rep.sidechain.activeSeconds)
            : "Sidechain (beat): no signal. Route the beat to SwitaxVoice's sidechain input to enable vocal-vs-beat checks.");

        // ---- 2. current settings
        tick (0.08f);
        auto now = render (raw, sr, input.current, &act);
        latency = now.latency;
        rep.current = analyse (now.out, sr, optsFor (act, sibMap, latency, true));
        rep.compGrTypicalNow = now.grTypical;
        tick (0.15f);

        ParamValues plan = input.current;
        auto& recs = rep.recs;

        sourceChecks (rep);
        if (isVocalContext (input.context))
        {
            gainStaging (rep, plan);                  tick (0.2f);
            lowEnd (rep, plan);                       tick (0.3f);
            if (cancelled()) return cancel (rep);
            tone (rep, plan);                         tick (0.42f);
            if (cancelled()) return cancel (rep);
            dynamics (rep, plan, now.grTypical);      tick (0.6f);
            if (cancelled()) return cancel (rep);
            sibilance (rep, plan);                    tick (0.75f);
            if (cancelled()) return cancel (rep);
            saturation (rep, plan);                   tick (0.82f);
            stereo (rep, plan);                       tick (0.86f);
        }
        else
        {
            mixInformation (rep, plan);
        }
        if (cancelled()) return cancel (rep);

        // ---- final: predicted result of the whole plan, peak safety and level
        auto fin = render (raw, sr, plan, &act);
        rep.predicted = analyse (fin.out, sr, optsFor (act, sibMap, fin.latency, true));
        rep.compGrTypicalPlan = fin.grTypical;
        tick (0.93f);
        if (isVocalContext (input.context))
        {
            peakSafety (rep, plan);
            if (rep.hasSidechain) beatBalance (rep, plan);
            else levelMatch (rep, plan);
        }
        rep.plan = plan;

        // order: actionable first by severity, information after
        std::stable_sort (recs.begin(), recs.end(), [] (const Recommendation& a, const Recommendation& b)
        {
            if (a.isActionable() != b.isActionable()) return a.isActionable();
            return a.severity > b.severity;
        });

        int actionable = 0;
        for (auto& r : recs) actionable += r.isActionable() ? 1 : 0;
        rep.status = recs.empty()
            ? fmt ("No issues beyond tolerance against %s. Your current settings already fit this target.", rep.targetName.c_str())
            : fmt ("%d proposal%s and %d note%s against %s.", actionable, actionable == 1 ? "" : "s",
                   (int) recs.size() - actionable, (int) recs.size() - actionable == 1 ? "" : "s", rep.targetName.c_str());
        rep.valid = true;
        tick (1.0f);
        return rep;
    }

private:
    //==========================================================================
    double confidence (double magnitude01, double consistency01, std::string& basis) const
    {
        const double dur = detail::clamp01 (activeSeconds() / 15.0) * 0.6 + 0.4;
        const double ctx = contextConfidence (input.context);
        const double tgt = input.target.measured ? 1.0 : 0.8;
        const double c = std::clamp (dur * (0.5 * magnitude01 + 0.5 * consistency01) * ctx * tgt, 0.05, 0.95);
        basis = detail::fmt ("%.0f s of signal analysed; evidence strength %.0f %%, consistency %.0f %%", activeSeconds(),
                             magnitude01 * 100.0, consistency01 * 100.0);
        if (! input.target.measured) basis += "; compared with a house target (heuristic), not a measured reference";
        if (ctx < 1.0) basis += detail::fmt ("; context '%s' lowers confidence", contextName (input.context));
        return c;
    }

    double activeSeconds() const { return act.activeSeconds(); }
    bool cancelled() const { return input.cancel != nullptr && input.cancel->load(); }
    Report& cancel (Report& r) { r.valid = false; r.status = "Analysis cancelled."; r.recs.clear(); return r; }
    void tick (float p) const { if (input.progress) input.progress (p); }

    static void addChange (Recommendation& r, ParamValues& plan, int index, float to)
    {
        to = clampParam (index, to);
        if (std::abs (plan[(size_t) index] - to) < 1.0e-4f) return;
        r.changes.push_back ({ index, plan[(size_t) index], to });
        plan[(size_t) index] = to;
    }

    static std::string changeText (const ParamChange& c)
    {
        const auto& d = kParamDefs[(size_t) c.index];
        auto val = [&] (float v) -> std::string
        {
            if (d.kind != ParamKind::Float) return v > 0.5f ? "on" : "off";
            const std::string u = d.unit;
            if (u == "Hz") return detail::hz (v);
            if (u == "dB") return detail::fmt ("%+.1f dB", v);
            if (u == ":1") return detail::fmt ("%.1f:1", v);
            if (u == "%") return detail::fmt ("%.0f %%", v);
            return detail::fmt ("%.1f %s", v, d.unit);
        };
        return std::string (d.name) + " " + val (c.from) + " -> " + val (c.to);
    }

    static void describeChanges (Recommendation& r)
    {
        std::string s;
        for (auto& c : r.changes) s += (s.empty() ? "" : "; ") + changeText (c);
        r.proposal = r.proposal.empty() ? s : r.proposal + "  [" + s + "]";
    }

    Measurements simulate (const ParamValues& p, bool full, detail::RenderResult* rr = nullptr) const
    {
        auto res = detail::render (*input.raw, sr, p, &act);
        auto m = analyse (res.out, sr, detail::optsFor (act, sibMap, res.latency, full));
        if (rr != nullptr) *rr = std::move (res);
        return m;
    }

    Measurements simulateSibilance (const ParamValues& p) const
    {
        auto res = detail::render (*input.raw, sr, p, &act);
        auto o = detail::optsFor (act, sibMap, res.latency, false);
        o.sibilance = true;
        return analyse (res.out, sr, o);
    }

    //==========================================================================
    void sourceChecks (Report& rep)
    {
        using namespace detail;
        const auto& m = rep.raw;
        if (m.clipRuns > 0 || m.flatTopRuns > 3)
        {
            Recommendation r;
            r.key = "clipping"; r.category = "Source"; r.severity = m.clipRuns > 5 ? 3 : 2;
            r.title = "Clipping found in the recording";
            if (m.clipRuns > 0)
            {
                std::string times;
                for (double t : m.clipTimes) times += (times.empty() ? "" : ", ") + fmt ("%d:%05.2f", (int) (t / 60), std::fmod (t, 60.0));
                r.evidence.push_back ({ Basis::Measured, fmt ("%d run%s of 3+ consecutive samples at full scale (>= -0.004 dBFS); first at %s.",
                                                              m.clipRuns, m.clipRuns == 1 ? "" : "s", times.c_str()) });
            }
            if (m.flatTopRuns > 3)
                r.evidence.push_back ({ Basis::Measured, fmt ("%d flat-topped runs (4+ identical samples) below full scale.", m.flatTopRuns) });
            r.evidence.push_back ({ Basis::Measured, fmt ("Sample peak %.2f dBFS, true peak %.2f dBTP.", m.samplePeakDb, m.truePeakDb) });
            r.issue = m.clipRuns > 0 ? "The take was clipped before it reached SwitaxVoice (recording level or an earlier plugin)."
                                     : "Flat-topped waveforms usually mean clipping earlier in the chain at a lower gain (heuristic).";
            r.proposal = "Clipped peaks cannot be restored by EQ or compression. Re-record with about 6 dB less input gain, or use a dedicated de-clipper before SwitaxVoice.";
            r.confidence = confidence (clamp01 (m.clipRuns / 5.0 + m.flatTopRuns / 20.0), 1.0, r.confidenceBasis);
            if (m.clipRuns > 0) r.confidence = std::max (r.confidence, 0.85);
            r.limitations = "Deliberately hard-clipped effects and some synthetic sounds also produce flat tops.";
            rep.recs.push_back (r);
        }
        if (m.crestDb > 0 && m.crestDb < 9.0 && m.plrDb < 9.5 && isVocalContext (input.context))
        {
            Recommendation r;
            r.key = "dense"; r.category = "Source"; r.severity = 0;
            r.title = "Source is already heavily limited or saturated";
            r.evidence.push_back ({ Basis::Measured, fmt ("Crest factor %.1f dB (peak vs active RMS); peak-to-loudness ratio %.1f dB.", m.crestDb, m.plrDb) });
            r.issue = "Unprocessed vocals usually show 14-20 dB crest factor. This take was probably compressed, limited or saturated before SwitaxVoice (heuristic).";
            r.proposal = "Keep extra compression and saturation light; more would flatten the performance further.";
            r.confidence = confidence (clamp01 ((9.0 - m.crestDb) / 4.0), 0.7, r.confidenceBasis);
            r.limitations = "Some delivery styles (shouted, chanted ad-libs) are naturally dense.";
            rep.recs.push_back (r);
        }
        const double snr = m.activeLevelDb - m.noiseFloorDb;
        if (m.noiseFloorDb > -55.0 && snr < 30.0 && isVocalContext (input.context))
        {
            Recommendation r;
            r.key = "noise"; r.category = "Source"; r.severity = snr < 20 ? 2 : 1;
            r.title = "Background noise is high";
            r.evidence.push_back ({ Basis::Measured, fmt ("Noise floor about %.1f dBFS (10th percentile of 100 ms levels); typical vocal level %.1f dBFS; difference %.1f dB.",
                                                          m.noiseFloorDb, m.activeLevelDb, snr) });
            r.issue = "Room tone, headphone bleed or electrical noise is close to the voice. Compression and saturation will make it more audible in pauses (heuristic).";
            r.proposal = "Clean the noise before SwitaxVoice (noise reduction, gate or clip editing); SwitaxVoice has no gate.";
            r.confidence = confidence (clamp01 ((30.0 - snr) / 15.0), 0.6, r.confidenceBasis);
            r.limitations = "If the take has very few pauses, the 'noise floor' may include quiet singing.";
            rep.recs.push_back (r);
        }
    }

    //==========================================================================
    void gainStaging (Report& rep, ParamValues& plan)
    {
        using namespace detail;
        const auto& m = rep.raw;
        if (m.integratedLufs > -32.0) return;
        Recommendation r;
        r.key = "gain"; r.category = "Level"; r.severity = 1;
        r.title = "Input level is low for the processors";
        r.evidence.push_back ({ Basis::Measured, fmt ("Input integrated loudness %.1f LUFS; sample peak %.1f dBFS.", m.integratedLufs, m.samplePeakDb) });
        r.issue = "The compressor, de-esser and saturation are calibrated for vocals around -20 LUFS; at this level they barely react.";
        const float gain = roundTo ((float) std::clamp (-20.0 - m.integratedLufs, 0.0, 24.0), 0.5f);
        r.proposal = "Raise Input Gain (the level-match step below compensates the output).";
        addChange (r, plan, pInputGain, plan[pInputGain] + gain);
        if (r.changes.empty()) return;
        describeChanges (r);
        r.predicted.push_back (fmt ("Level into the processors: about %.1f LUFS.", m.integratedLufs + gain));
        r.confidence = confidence (clamp01 ((-32.0 - m.integratedLufs) / 10.0 + 0.4), 1.0, r.confidenceBasis);
        r.limitations = "Gain staging only; it does not change tone. Leaves headroom: peaks after gain stay below 0 dBFS inside the plugin's float path.";
        rep.recs.push_back (r);
    }

    //==========================================================================
    void lowEnd (Report& rep, ParamValues& plan)
    {
        using namespace detail;
        const auto& m = rep.current;
        const bool pitchOk = m.pitch.voicedFrames > 20 && m.pitch.p5 > 0;
        const double fc = m.lowCutoffHz;

        // mains hum indicator on the raw signal (persistent narrow peak at 50/60 Hz or 100/120 Hz)
        int humBand = -1;
        for (double f : { 50.0, 60.0, 100.0, 120.0 })
        {
            const int b = bandForFreq (f);
            if (rep.raw.persistence[(size_t) b] > 0.8 && rep.raw.peakinessDb[(size_t) b] > 6.0) { humBand = b; break; }
        }

        if (m.lowEnergyRelDb < -28.0 && humBand < 0) return;

        Recommendation r;
        r.key = "lowend"; r.category = "Low end"; r.severity = m.lowEnergyRelDb > -18.0 ? 3 : 2;
        r.title = humBand >= 0 ? "Low-frequency hum or rumble" : "Rumble below the voice";
        r.evidence.push_back ({ Basis::Measured, fmt ("Energy below %s is %.1f dB relative to the whole signal (after your current settings).", hz (fc).c_str(), m.lowEnergyRelDb) });
        if (pitchOk)
            r.evidence.push_back ({ Basis::Measured, fmt ("Lowest sustained fundamental about %s (5th percentile of %d voiced frames, YIN pitch tracking).", hz (m.pitch.p5).c_str(), m.pitch.voicedFrames) });
        else
            r.evidence.push_back ({ Basis::Measured, "Not enough clearly pitched frames to locate the lowest fundamental; using 80 Hz." });
        if (humBand >= 0)
            r.evidence.push_back ({ Basis::Measured, fmt ("Narrow peak at about %s, %.1f dB above the local spectrum, present in %.0f %% of frames.",
                                                          hz (bandCentreHz (humBand)).c_str(), rep.raw.peakinessDb[(size_t) humBand], rep.raw.persistence[(size_t) humBand] * 100.0) });
        r.issue = "Energy below the singer's lowest fundamental is not part of the voice (handling noise, plosives, traffic, AC hum). It eats headroom and makes the compressor pump (heuristic).";

        const float target = roundTo ((float) fc, 5.0f);
        const bool hpfOn = isOn (plan, pHpfOn);
        if (! hpfOn || plan[pHpfFreq] < target * 0.9f)
        {
            addChange (r, plan, pHpfOn, 1.0f);
            addChange (r, plan, pHpfFreq, std::max (plan[pHpfFreq], target));
            r.proposal = "High-pass just below the lowest note.";
        }
        else if (! isOn (plan, pHpfSlope))
        {
            addChange (r, plan, pHpfSlope, 1.0f);
            r.proposal = "The high-pass is already at the right frequency; make it steeper.";
        }
        else
        {
            r.proposal = "The high-pass is already set; the remaining low end may be part of the performance or needs cleaning at the source.";
        }
        describeChanges (r);
        if (! r.changes.empty())
        {
            const auto s = simulate (plan, true);
            r.predicted.push_back (fmt ("Energy below %s: %.1f dB -> %.1f dB.", hz (fc).c_str(), m.lowEnergyRelDb, s.lowEnergyRelDb));
            r.predicted.push_back (fmt ("Change at the lowest fundamental (%s): %.1f dB.", hz (pitchOk ? m.pitch.p5 : 100.0).c_str(),
                                        sx::dsp::Chain::responseDb (plan, sr, pitchOk ? m.pitch.p5 : 100.0) - sx::dsp::Chain::responseDb (rep.analysedWith, sr, pitchOk ? m.pitch.p5 : 100.0)));
        }
        r.confidence = confidence (clamp01 ((m.lowEnergyRelDb + 28.0) / 12.0 + (humBand >= 0 ? 0.4 : 0.0)), pitchOk ? 0.9 : 0.5, r.confidenceBasis);
        r.limitations = "Deep male voices, intentional sub-heavy ad-lib effects or bleed from the beat can also put energy here. Listen while auditioning.";
        rep.recs.push_back (r);
    }

    //==========================================================================
    void tone (Report& rep, ParamValues& plan)
    {
        using namespace detail;
        const auto& m = rep.current;
        if (m.spectralFrames < 20) return;
        const auto& t = input.target;
        const auto d = deviation (m, t);
        const double tol = t.toleranceDb;

        struct Pending { Recommendation r; int band; double before; double lo, hi; bool cut; };
        std::vector<Pending> pend;

        auto consistency = [&] (double lo, double hi, bool over)
        {
            // fraction of active frames whose own spectrum deviates in the same direction (the
            // long-term average alone can be dominated by a few loud moments)
            if (m.frameRelDb.empty()) return 0.5;
            int hit = 0;
            for (auto& f : m.frameRelDb)
            {
                const double dv = frameRegionDeviation (f, t, m.bandValid, lo, hi);
                if (over ? dv > tol * 0.5 : dv < -tol * 0.5) ++hit;
            }
            return (double) hit / (double) m.frameRelDb.size();
        };

        auto bandCut = [&] (const char* key, const char* title, const char* issue, double lo, double hi, int freqIdx, int gainIdx,
                            float maxCut, float freqMin, float freqMax, double peakMinForResonance)
        {
            const auto rs = region (d, m.bandValid, lo, hi);
            if (rs.peakBand < 0) return;
            const bool broad = rs.mean > tol;
            // "persistent resonance" is only claimed where 1/6-octave bands are wide compared with
            // the voice's harmonic spacing; below ~1.8 kHz the fundamental and low harmonics of the
            // melody themselves form persistent peaks, so only broad build-up counts there.
            const bool resonant = peakMinForResonance > 0.0 && rs.peak > tol + 1.0
                                  && m.peakinessDb[(size_t) rs.peakBand] > peakMinForResonance
                                  && m.persistence[(size_t) rs.peakBand] > 0.6;
            if (! broad && ! resonant) return;
            if (plan[(size_t) gainIdx] <= -8.0f) return;   // already cut a lot: do not stack more

            Pending p;
            auto& r = p.r;
            r.key = key; r.category = "Tone"; r.title = title;
            r.severity = rs.mean > tol + 3.0 || rs.peak > tol + 5.0 ? 2 : 1;
            r.evidence.push_back ({ Basis::Measured, fmt ("%s-%s averages %+.1f dB versus the target (tolerance +/-%.1f dB); largest at %s (%+.1f dB).",
                                                          hz (lo).c_str(), hz (hi).c_str(), rs.mean, tol, hz (bandCentreHz (rs.peakBand)).c_str(), rs.peak) });
            if (resonant)
                r.evidence.push_back ({ Basis::Measured, fmt ("At %s the spectrum stands %.1f dB above its local trend in %.0f %% of active frames (a persistent resonance rather than moving notes).",
                                                              hz (bandCentreHz (rs.peakBand)).c_str(), m.peakinessDb[(size_t) rs.peakBand], m.persistence[(size_t) rs.peakBand] * 100.0) });
            r.issue = issue;
            // correct about 65 % of the measured peak deviation, never more than maxCut per pass
            const float cut = (float) std::clamp (0.65 * std::max (rs.mean, rs.peak), 1.0, (double) maxCut);
            const float f = std::clamp ((float) bandCentreHz (rs.peakBand), freqMin, freqMax);
            r.proposal = "Gentle cut at the measured frequency (kept small on purpose).";
            addChange (r, plan, freqIdx, roundTo (f, f >= 1000.0f ? 50.0f : 5.0f));
            addChange (r, plan, gainIdx, roundTo (plan[(size_t) gainIdx] - cut, 0.5f));
            describeChanges (r);
            const double cons = std::max (consistency (lo, hi, true), resonant ? m.persistence[(size_t) rs.peakBand] : 0.0);
            r.confidence = confidence (clamp01 ((std::max (rs.mean, rs.peak - 1.5) - tol) / 4.0 + 0.3), cons, r.confidenceBasis);
            r.limitations = input.target.measured
                ? "Compared with your reference; mic, singer and arrangement differences are normal. Trust your ears."
                : "Compared with a house target. Voices differ: a deep or breathy voice can legitimately sit outside it.";
            p.band = rs.peakBand; p.before = rs.mean; p.lo = lo; p.hi = hi; p.cut = true;
            pend.push_back (std::move (p));
        };

        auto bandBoost = [&] (const char* key, const char* title, const char* issue, double lo, double hi, int freqIdx, int gainIdx,
                              float maxBoost, float freqMin, float freqMax, bool shelf)
        {
            const auto rs = region (d, m.bandValid, lo, hi);
            if (rs.troughBand < 0 || rs.mean > -tol) return;
            if (plan[(size_t) gainIdx] >= 6.0f) return;
            Pending p;
            auto& r = p.r;
            r.key = key; r.category = "Tone"; r.title = title;
            r.severity = rs.mean < -tol - 3.0 ? 2 : 1;
            r.evidence.push_back ({ Basis::Measured, fmt ("%s-%s averages %+.1f dB versus the target (tolerance +/-%.1f dB).",
                                                          hz (lo).c_str(), hz (hi).c_str(), rs.mean, tol) });
            r.issue = issue;
            const float boost = (float) std::clamp (-0.5 * rs.mean, 1.0, (double) maxBoost);
            const float f = shelf ? plan[(size_t) freqIdx] : std::clamp ((float) bandCentreHz (rs.troughBand), freqMin, freqMax);
            r.proposal = "Moderate boost (limited so it cannot turn harsh; the de-esser check below runs with it applied).";
            if (! shelf) addChange (r, plan, freqIdx, roundTo (f, 50.0f));
            addChange (r, plan, gainIdx, roundTo (plan[(size_t) gainIdx] + boost, 0.5f));
            describeChanges (r);
            r.confidence = confidence (clamp01 ((-rs.mean - tol) / 4.0 + 0.3), consistency (lo, hi, false), r.confidenceBasis);
            r.limitations = "Boosting cannot add what the microphone did not capture; a dull mic or a distant take may need re-recording rather than EQ.";
            p.band = rs.troughBand; p.before = rs.mean; p.lo = lo; p.hi = hi; p.cut = false;
            pend.push_back (std::move (p));
        };

        bandCut ("lowmid", "Low-mid build-up (muddiness)",
                 "Too much 150-500 Hz energy makes a vocal sound muddy or 'woolly' and masks the beat's low-mids (heuristic).",
                 150.0, 500.0, pLowFreq, pLowGain, 4.0f, 100.0f, 600.0f, 0.0);
        bandCut ("box", "Boxy / nasal midrange",
                 "Excess 500 Hz-1.5 kHz often comes from a small room or close mic and sounds boxy or nasal (heuristic).",
                 500.0, 1500.0, pBoxFreq, pBoxGain, 3.5f, 400.0f, 1800.0f, 0.0);
        bandCut ("harsh", "Harsh upper-mid resonance",
                 "A persistent peak in 2-5 kHz is perceived as harshness or 'ear fatigue', especially at high playback levels (heuristic).",
                 2000.0, 5000.0, pHarshFreq, pHarshGain, 4.0f, 1800.0f, 6000.0f, 4.0);
        bandBoost ("presence", "Lacks presence",
                   "Too little 2-5 kHz energy pushes a vocal behind the beat, especially against dense Afrobeat percussion (heuristic).",
                   2000.0, 5000.0, pPresFreq, pPresGain, 3.0f, 2500.0f, 5000.0f, false);
        bandBoost ("air", "Lacks air", "Too little energy above 8 kHz sounds dull or distant compared with modern Afro vocals (heuristic).",
                   8000.0, 16000.0, pAirFreq, pAirGain, 3.0f, 0.0f, 0.0f, true);
        {
            const auto rs = region (d, m.bandValid, 8000.0, 16000.0);
            if (rs.peakBand >= 0 && rs.mean > tol + 1.0 && plan[pAirGain] > -4.0f)
            {
                Pending p;
                auto& r = p.r;
                r.key = "airx"; r.category = "Tone"; r.title = "Top end is brighter than the target"; r.severity = 1;
                r.evidence.push_back ({ Basis::Measured, fmt ("8-16 kHz averages %+.1f dB versus the target.", rs.mean) });
                r.issue = "An overly bright top end can sound thin or hissy and exaggerates breaths (heuristic).";
                addChange (r, plan, pAirGain, roundTo (plan[pAirGain] - (float) std::clamp ((rs.mean - tol * 0.5) * 0.6, 1.0, 3.0), 0.5f));
                describeChanges (r);
                r.confidence = confidence (clamp01 ((rs.mean - tol) / 4.0 + 0.3), consistency (8000.0, 16000.0, true), r.confidenceBasis);
                r.limitations = "Bright tops are a stylistic choice in Afro-pop; reject this if you like the sparkle.";
                p.band = rs.peakBand; p.before = rs.mean; p.lo = 8000.0; p.hi = 16000.0; p.cut = true;
                pend.push_back (std::move (p));
            }
        }
        if (pend.empty()) return;

        // one combined simulation verifies all EQ proposals (bands barely interact)
        if (! isOn (plan, pEqOn)) { for (auto& p : pend) addChange (p.r, plan, pEqOn, 1.0f); }
        const auto s = simulate (plan, true);
        const auto d2 = deviation (s, t);
        for (auto& p : pend)
        {
            const auto rs2 = region (d2, s.bandValid, p.lo, p.hi);
            p.r.predicted.push_back (fmt ("%s-%s versus target: %+.1f dB -> %+.1f dB (all tone proposals applied).",
                                          hz (p.lo).c_str(), hz (p.hi).c_str(), p.before, rs2.mean));
            rep.recs.push_back (std::move (p.r));
        }
    }

    //==========================================================================
    void dynamics (Report& rep, ParamValues& plan, double grNow)
    {
        using namespace detail;
        const auto& t = input.target;
        // measure with the plan so far (EQ changes alter what the compressor sees)
        detail::RenderResult base;
        const auto m = simulate (plan, false, &base);
        const double range = m.levelRange;
        if (m.integratedLufs < -150) return;

        if (range > t.levelRangeMax + 1.0)
        {
            Recommendation r;
            r.key = "comp"; r.category = "Dynamics"; r.title = "Level jumps between words and phrases";
            r.severity = range > t.levelRangeMax + 4.0 ? 2 : 1;
            r.evidence.push_back ({ Basis::Measured, fmt ("Momentary loudness range %.1f dB (10th-95th percentile of 400 ms blocks); target %.1f-%.1f dB.",
                                                          range, t.levelRangeMin, t.levelRangeMax) });
            r.evidence.push_back ({ Basis::Measured, fmt ("Compressor currently reduces %.1f dB on average while you sing (%.1f dB on the loudest 5 %%).", base.grTypical, base.grP95) });
            r.issue = "Large level differences make quiet words disappear under the beat while loud words jump out (heuristic for this style).";
            ParamValues trial = plan;
            trial[pCompOn] = 1.0f;
            trial[pCompRatio] = std::max (plan[pCompRatio], isOn (plan, pCompOn) ? 3.0f : 3.0f);
            const float hi = isOn (plan, pCompOn) ? plan[pCompThresh] : -6.0f;
            // bisection on the threshold: a lower threshold gives a smaller loudness range
            float tLo = -50.0f, tHi = hi, best = hi;
            double bestRange = range, bestGr = base.grTypical;
            bool limitedByGr = false;
            for (int it = 0; it < 7; ++it)
            {
                const float mid = 0.5f * (tLo + tHi);
                trial[pCompThresh] = mid;
                detail::RenderResult rr;
                const auto sm = simulate (trial, false, &rr);
                if (rr.grTypical > 6.0) { tLo = mid; limitedByGr = true; continue; }
                best = mid; bestRange = sm.levelRange; bestGr = rr.grTypical;
                if (sm.levelRange > t.levelRangeMax - 0.5) tHi = mid; else tLo = mid;
            }
            if (bestRange >= range - 0.5) return;   // compression cannot help meaningfully
            r.proposal = "More level control from the compressor (bounded so it never averages more than about 6 dB of gain reduction).";
            addChange (r, plan, pCompOn, 1.0f);
            addChange (r, plan, pCompRatio, roundTo (trial[pCompRatio], 0.1f));
            addChange (r, plan, pCompThresh, roundTo (best, 0.5f));
            describeChanges (r);
            r.predicted.push_back (fmt ("Momentary loudness range %.1f dB -> %.1f dB.", range, bestRange));
            r.predicted.push_back (fmt ("Average gain reduction while singing %.1f dB -> %.1f dB.", base.grTypical, bestGr));
            if (limitedByGr && bestRange > t.levelRangeMax)
                r.predicted.push_back ("Stopped short of the target to avoid over-compression. Ride the remaining phrase-level differences with clip gain or fader automation.");
            r.confidence = confidence (clamp01 ((range - t.levelRangeMax) / 5.0 + 0.3), 0.8, r.confidenceBasis);
            r.limitations = "The loudness range includes deliberate dynamics (soft verses, belted hooks). If the variation is musical, keep it.";
            rep.recs.push_back (r);
        }
        else if (range < t.levelRangeMin - 1.0 && grNow > 3.0 && isOn (plan, pCompOn))
        {
            Recommendation r;
            r.key = "compless"; r.category = "Dynamics"; r.title = "Compression is flattening the performance";
            r.severity = 1;
            r.evidence.push_back ({ Basis::Measured, fmt ("Momentary loudness range %.1f dB; target %.1f-%.1f dB.", range, t.levelRangeMin, t.levelRangeMax) });
            r.evidence.push_back ({ Basis::Measured, fmt ("Compressor reduces %.1f dB on average while you sing.", base.grTypical) });
            r.issue = "Very little level variation can make a vocal sound flat and tiring (heuristic).";
            ParamValues trial = plan;
            float tLo = plan[pCompThresh], tHi = 0.0f, best = plan[pCompThresh];
            double bestRange = range, bestGr = base.grTypical;
            for (int it = 0; it < 6; ++it)
            {
                const float mid = 0.5f * (tLo + tHi);
                trial[pCompThresh] = mid;
                detail::RenderResult rr;
                const auto sm = simulate (trial, false, &rr);
                best = mid; bestRange = sm.levelRange; bestGr = rr.grTypical;
                if (sm.levelRange < t.levelRangeMin) tLo = mid; else tHi = mid;
            }
            addChange (r, plan, pCompThresh, roundTo (best, 0.5f));
            describeChanges (r);
            r.proposal = "Ease off the compressor. " + r.proposal;
            r.predicted.push_back (fmt ("Momentary loudness range %.1f dB -> %.1f dB; gain reduction %.1f dB -> %.1f dB.", range, bestRange, base.grTypical, bestGr));
            r.confidence = confidence (clamp01 ((t.levelRangeMin - range) / 4.0 + 0.3), 0.7, r.confidenceBasis);
            r.limitations = "Some styles (chanted hooks, melodic rap) are meant to be very even; reject if this is intentional.";
            rep.recs.push_back (r);
        }
    }

    //==========================================================================
    void sibilance (Report& rep, ParamValues& plan)
    {
        using namespace detail;
        const auto& t = input.target;
        const auto m = simulateSibilance (plan);
        const auto& s = m.sibilance;
        if (s.events < 3 || s.eventsPerMinute < 4.0) return;
        const double excess = s.peakRelBodyDb - t.sibilanceMaxRelDb;

        if (excess > 1.5)
        {
            Recommendation r;
            r.key = "deess"; r.category = "Sibilance"; r.title = "Sibilance is prominent";
            r.severity = excess > 5.0 ? 3 : 2;
            r.evidence.push_back ({ Basis::Measured, fmt ("%d sibilant events (%.0f per minute). Their peaks in 4.5-11 kHz sit %+.1f dB relative to the vocal body (150 Hz-2.5 kHz); target %+.1f dB or lower.",
                                                          s.events, s.eventsPerMinute, s.peakRelBodyDb, t.sibilanceMaxRelDb) });
            r.evidence.push_back ({ Basis::Measured, fmt ("Strongest sibilant energy around %s.", hz (rep.raw.sibilance.centroidHz).c_str()) });
            r.issue = "Loud 's', 'sh' and 't' sounds become piercing, especially after presence/air boosts and on earbuds (heuristic).";
            ParamValues trial = plan;
            trial[pDeessOn] = 1.0f;
            trial[pDeessFreq] = (float) rep.raw.sibilance.centroidHz;
            trial[pDeessRange] = std::clamp (std::max (plan[pDeessRange], (float) excess + 3.0f), 0.0f, 14.0f);
            float tLo = -60.0f, tHi = isOn (plan, pDeessOn) ? plan[pDeessThresh] : -10.0f, best = tHi;
            double bestRel = s.peakRelBodyDb, bestHf = s.nonEventHfDb;
            for (int it = 0; it < 7; ++it)
            {
                const float mid = 0.5f * (tLo + tHi);
                trial[pDeessThresh] = mid;
                const auto sm = simulateSibilance (trial);
                const bool sideEffect = sm.sibilance.nonEventHfDb < s.nonEventHfDb - 2.0;   // dulls vowels too much
                if (sideEffect) { tLo = mid; continue; }
                best = mid; bestRel = sm.sibilance.peakRelBodyDb; bestHf = sm.sibilance.nonEventHfDb;
                if (sm.sibilance.peakRelBodyDb > t.sibilanceMaxRelDb + 0.5) tHi = mid; else tLo = mid;
            }
            if (bestRel > s.peakRelBodyDb - 0.7) return;
            r.proposal = "De-ess the measured sibilance band (only the high band is reduced, and only during sibilants).";
            addChange (r, plan, pDeessOn, 1.0f);
            addChange (r, plan, pDeessFreq, roundTo (trial[pDeessFreq], 50.0f));
            addChange (r, plan, pDeessRange, roundTo (trial[pDeessRange], 0.5f));
            addChange (r, plan, pDeessThresh, roundTo (best, 0.5f));
            describeChanges (r);
            r.predicted.push_back (fmt ("Sibilant peaks relative to the body: %+.1f dB -> %+.1f dB.", s.peakRelBodyDb, bestRel));
            r.predicted.push_back (fmt ("High end between sibilants changes by %+.1f dB (kept above -2 dB so vowels stay open).", bestHf - s.nonEventHfDb));
            r.confidence = confidence (clamp01 (excess / 6.0 + 0.2), clamp01 (s.eventsPerMinute / 20.0), r.confidenceBasis);
            r.limitations = "Detection is by spectrum, not by phoneme: bright hi-hat bleed, breaths or cymbals can count as sibilance. Over-de-essing causes lisping; audition it.";
            rep.recs.push_back (r);
        }
        else if (s.peakRelBodyDb < t.sibilanceMaxRelDb - 7.0 && isOn (plan, pDeessOn))
        {
            const auto rawRel = rep.raw.sibilance.peakRelBodyDb;
            if (rawRel - s.peakRelBodyDb < 5.0) return;
            Recommendation r;
            r.key = "deessless"; r.category = "Sibilance"; r.title = "De-esser may be dulling consonants"; r.severity = 1;
            r.evidence.push_back ({ Basis::Measured, fmt ("Sibilant peaks are %+.1f dB relative to the body after processing (%+.1f dB before). Target about %+.1f dB.",
                                                          s.peakRelBodyDb, rawRel, t.sibilanceMaxRelDb) });
            r.issue = "Too much de-essing makes words lisp and lose clarity (heuristic).";
            addChange (r, plan, pDeessThresh, roundTo (plan[pDeessThresh] + 6.0f, 0.5f));
            describeChanges (r);
            const auto sm = simulateSibilance (plan);
            r.predicted.push_back (fmt ("Sibilant peaks relative to the body: %+.1f dB -> %+.1f dB.", s.peakRelBodyDb, sm.sibilance.peakRelBodyDb));
            r.confidence = confidence (0.5, clamp01 (s.eventsPerMinute / 20.0), r.confidenceBasis);
            r.limitations = "If you prefer soft consonants, keep the current setting.";
            rep.recs.push_back (r);
        }
    }

    //==========================================================================
    void saturation (Report& rep, ParamValues& plan)
    {
        using namespace detail;
        if (! isOn (plan, pSatOn) || plan[pSatDrive] < 5.0f) return;
        auto distortion = [&] (const ParamValues& with)
        {
            ParamValues without = with;
            without[pSatOn] = 0.0f;
            const auto a = render (*input.raw, sr, with, &act);
            const auto b = render (*input.raw, sr, without, &act);
            double e = 0, ed = 0;
            for (size_t c = 0; c < a.out.size(); ++c)
                for (size_t i = 0; i < a.out[c].size(); ++i)
                {
                    if (! act.isActiveAt ((long long) i - a.latency)) continue;
                    const double d = (double) a.out[c][i] - b.out[c][i];
                    ed += d * d; e += (double) b.out[c][i] * b.out[c][i];
                }
            return powToDb (ed / std::max (e, 1e-30));
        };
        const double limit = input.target.name.find ("Rap") != std::string::npos ? -16.0 : -20.0;
        const double dNow = distortion (plan);
        if (dNow <= limit) return;
        Recommendation r;
        r.key = "sat"; r.category = "Tone"; r.title = "Saturation is adding a lot of distortion"; r.severity = 1;
        r.evidence.push_back ({ Basis::Simulated, fmt ("Saturation changes the signal by %.1f dB relative to the clean path (about %.0f %% distortion energy).", dNow, 100.0 * std::pow (10.0, dNow / 10.0)) });
        r.issue = "At this amount saturation starts to sound gritty or fizzy rather than warm (heuristic; it may be intended).";
        ParamValues trial = plan;
        float drive = plan[pSatDrive];
        double dBest = dNow;
        for (float f : { 0.7f, 0.5f, 0.35f, 0.2f })
        {
            trial[pSatDrive] = roundTo (plan[pSatDrive] * f, 1.0f);
            dBest = distortion (trial);
            drive = trial[pSatDrive];
            if (dBest <= limit - 4.0) break;
        }
        addChange (r, plan, pSatDrive, drive);
        describeChanges (r);
        r.predicted.push_back (fmt ("Distortion energy %.1f dB -> %.1f dB relative to the clean path.", dNow, dBest));
        r.confidence = confidence (clamp01 ((dNow - limit) / 8.0 + 0.3), 0.7, r.confidenceBasis);
        r.limitations = "Distortion amount is measured; whether it sounds good is a taste decision. Reject it if you want grit.";
        rep.recs.push_back (r);
    }

    //==========================================================================
    void stereo (Report& rep, ParamValues& plan)
    {
        using namespace detail;
        const auto& m = rep.current;
        if (! m.stereo) return;
        if (rep.raw.sideToMidDb < -40.0 && ! isOn (plan, pDoubleOn)) return;   // mono source on a stereo track: nothing to say

        if (m.correlation < 0.3)
        {
            Recommendation r;
            r.key = "stereo"; r.category = "Stereo"; r.title = "Weak mono compatibility"; r.severity = m.correlation < 0.0 ? 3 : 2;
            r.evidence.push_back ({ Basis::Measured, fmt ("L/R correlation %.2f (1 = mono, 0 = unrelated, below 0 = cancelling); level change when summed to mono %.1f dB.",
                                                          m.correlation, m.monoLossDb) });
            r.issue = "A lead vocal that partly cancels in mono gets thin or quiet on phone speakers and club systems.";
            if (isOn (plan, pDoubleOn) && plan[pDoubleWidth] > 50.0f)
            {
                addChange (r, plan, pDoubleWidth, 45.0f);
                describeChanges (r);
                const auto s = simulate (plan, false);
                r.predicted.push_back (fmt ("Correlation %.2f -> %.2f.", m.correlation, s.correlation));
                r.proposal = "Narrow the doubler. " + r.proposal;
            }
            else
            {
                r.proposal = "Check the polarity of stereo mics or any stereo widener earlier in the chain; SwitaxVoice is not widening this signal.";
            }
            r.confidence = confidence (clamp01 ((0.3 - m.correlation) / 0.5 + 0.3), 0.8, r.confidenceBasis);
            r.limitations = "Wide stereo doubles and ad-lib effects are deliberately decorrelated; judge on the lead vocal.";
            rep.recs.push_back (r);
        }
    }

    //==========================================================================
    void peakSafety (Report& rep, ParamValues& plan)
    {
        using namespace detail;
        const auto& p = rep.predicted;
        if (! isOn (plan, pLimiterOn) && p.truePeakDb > -1.0)
        {
            Recommendation r;
            r.key = "limiter"; r.category = "Level"; r.title = "Output peaks reach full scale"; r.severity = 2;
            r.evidence.push_back ({ Basis::Simulated, fmt ("Predicted output true peak %.1f dBTP with the limiter off.", p.truePeakDb) });
            r.issue = "Peaks this high can clip later in your mix chain or in lossy encoding.";
            addChange (r, plan, pLimiterOn, 1.0f);
            addChange (r, plan, pCeiling, -1.0f);
            describeChanges (r);
            r.confidence = 0.9; r.confidenceBasis = "Measured peak level of the simulated output.";
            r.limitations = "If you limit on the mix bus anyway, this is optional.";
            rep.recs.push_back (r);
        }
        else if (isOn (plan, pLimiterOn) && p.truePeakDb > plan[pCeiling] + 0.5 && p.truePeakDb > -0.5)
        {
            Recommendation r;
            r.key = "ceiling"; r.category = "Level"; r.title = "Inter-sample peaks above the ceiling"; r.severity = 1;
            r.evidence.push_back ({ Basis::Simulated, fmt ("Predicted output true peak %.2f dBTP with the ceiling at %.1f dBFS (sample-peak limiter).", p.truePeakDb, plan[pCeiling]) });
            r.issue = "The limiter catches sample peaks; reconstructed (inter-sample) peaks can still exceed 0 dBTP after conversion or encoding.";
            addChange (r, plan, pCeiling, roundTo (plan[pCeiling] - (float) (p.truePeakDb - plan[pCeiling]) - 0.2f, 0.1f));
            describeChanges (r);
            r.confidence = 0.8; r.confidenceBasis = "True peak measured with 4x oversampling on the simulated output.";
            r.limitations = "Matters mostly if this vocal is the last thing before export.";
            rep.recs.push_back (r);
        }
    }

    void levelMatch (Report& rep, ParamValues& plan)
    {
        using namespace detail;
        const double delta = rep.predicted.integratedLufs - rep.raw.integratedLufs;
        if (std::abs (delta) < 1.5 || rep.predicted.integratedLufs < -150) return;
        Recommendation r;
        r.key = "level"; r.category = "Level"; r.title = delta > 0 ? "Processing makes the vocal louder" : "Processing makes the vocal quieter";
        r.severity = 0;
        r.evidence.push_back ({ Basis::Simulated, fmt ("Integrated loudness %.1f LUFS in, %.1f LUFS out with all proposals (%+.1f LU).",
                                                       rep.raw.integratedLufs, rep.predicted.integratedLufs, delta) });
        r.issue = "Louder almost always sounds 'better' at first. Matching the level lets you judge the processing honestly and keeps your fader where it was.";
        r.proposal = "Level-match the output (apply this last).";
        addChange (r, plan, pOutputGain, roundTo (plan[pOutputGain] - (float) delta, 0.5f));
        describeChanges (r);
        r.predicted.push_back (fmt ("Output loudness about %.1f LUFS (same as the input).", rep.raw.integratedLufs));
        r.confidence = 0.85; r.confidenceBasis = "BS.1770 loudness of input and simulated output.";
        r.limitations = "Assumes all proposals above are accepted. The limiter can make the final value differ by a fraction of a dB.";
        rep.recs.push_back (r);
    }

    void beatBalance (Report& rep, ParamValues& plan)
    {
        using namespace detail;
        const auto& v = rep.predicted;
        const auto& b = rep.sidechain;
        const double rel = v.integratedLufs - b.integratedLufs;
        Recommendation r;
        r.key = "beat"; r.category = "Mix"; r.severity = 0;
        r.title = "Vocal level against the beat";
        r.evidence.push_back ({ Basis::Measured, fmt ("Vocal %.1f LUFS (simulated output) vs beat on the sidechain %.1f LUFS: %+.1f LU.", v.integratedLufs, b.integratedLufs, rel) });
        r.issue = "Lead vocals in Afro styles usually sit roughly level with the beat in loudness terms (heuristic; arrangement-dependent).";
        if (rel < -6.0 || rel > 4.0)
        {
            r.severity = 1;
            const float g = roundTo ((float) std::clamp (-1.0 - rel, -8.0, 8.0), 0.5f);
            addChange (r, plan, pOutputGain, plan[pOutputGain] + g);
            describeChanges (r);
            r.proposal = rel < 0 ? "The vocal may be buried; raise the output. " + r.proposal : "The vocal may be on top of the beat; lower the output. " + r.proposal;
            r.predicted.push_back (fmt ("Vocal about %+.1f LU relative to the beat.", rel + g));
        }
        else
        {
            r.proposal = "Within the usual range; no change suggested.";
        }
        // masking in the presence region
        double vb = 0, bb = 0; int n = 0;
        for (int k = 0; k < kNumBands; ++k)
            if (v.bandValid[(size_t) k] && bandCentreHz (k) >= 2000 && bandCentreHz (k) <= 4500)
            { vb += dbToPow (v.ltasDb[(size_t) k]); bb += dbToPow (b.ltasDb[(size_t) k]); ++n; }
        if (n > 0)
        {
            const double diff = powToDb (bb) - powToDb (vb);
            r.evidence.push_back ({ Basis::Measured, fmt ("In 2-4.5 kHz the beat is %+.1f dB relative to the vocal.", diff) });
            if (diff > 6.0)
                r.limitations = "The beat dominates the vocal's intelligibility region; a dip there on the beat (sidechained to the vocal) helps more than vocal EQ. ";
        }
        r.confidence = 0.4; r.confidenceBasis = "Measured loudness; the 'usual range' is a heuristic.";
        r.limitations += "Only the sidechain signal is considered, not your full mix.";
        rep.recs.push_back (r);
    }

    //==========================================================================
    void mixInformation (Report& rep, ParamValues& plan)
    {
        using namespace detail;
        const auto& m = rep.raw;
        const auto& t = input.target;
        {
            Recommendation r;
            r.key = "mixloud"; r.category = "Mix"; r.severity = 0; r.title = "Loudness and peaks";
            r.evidence.push_back ({ Basis::Measured, fmt ("Integrated %.1f LUFS, short-term max %.1f LUFS, loudness range %.1f LU.", m.integratedLufs, m.maxShortTerm, m.loudnessRange) });
            r.evidence.push_back ({ Basis::Measured, fmt ("True peak %.2f dBTP; peak-to-loudness ratio %.1f dB.", m.truePeakDb, m.plrDb) });
            r.issue = m.truePeakDb > -1.0 ? "True peaks above -1 dBTP can clip after lossy encoding (streaming services recommend -1 dBTP)."
                                          : "Peaks leave encoding headroom.";
            r.proposal = fmt ("Streaming platforms normalise playback (about -14 LUFS on Spotify/YouTube): this master would be turned %s by about %.1f dB.",
                              m.integratedLufs > -14 ? "down" : "up", std::abs (m.integratedLufs + 14.0));
            r.confidence = 0.9; r.confidenceBasis = "BS.1770-4 / EBU R128 measurement.";
            r.limitations = "Information only. Normalisation targets differ per platform and can change.";
            rep.recs.push_back (r);
        }
        if (m.stereo && m.lowCorrelation < 0.5)
        {
            Recommendation r;
            r.key = "mixlow"; r.category = "Stereo"; r.severity = 2; r.title = "Low end is not mono-compatible";
            r.evidence.push_back ({ Basis::Measured, fmt ("Correlation below 150 Hz: %.2f.", m.lowCorrelation) });
            r.issue = "Wide or out-of-phase bass loses energy on mono club systems and phone speakers.";
            r.proposal = "Make the sub/bass and kick mono below about 120 Hz in the mix.";
            r.confidence = 0.8; r.confidenceBasis = "Measured correlation.";
            r.limitations = "Information only; fix this in the mix.";
            rep.recs.push_back (r);
        }
        if (m.spectralFrames >= 20)
        {
            const auto d = deviation (m, t);
            const struct { const char* name; double lo, hi; } regs[] = { { "Sub (30-60 Hz)", 30, 60 }, { "Bass (60-200 Hz)", 60, 200 },
                                                                         { "Low-mids (200-800 Hz)", 200, 800 }, { "Presence (2-6 kHz)", 2000, 6000 },
                                                                         { "Air (8-16 kHz)", 8000, 16000 } };
            std::string lines;
            for (auto& g : regs)
            {
                const auto rs = region (d, m.bandValid, g.lo, g.hi);
                if (std::abs (rs.mean) > t.toleranceDb)
                    lines += (lines.empty() ? "" : "; ") + fmt ("%s %+.1f dB", g.name, rs.mean);
            }
            if (! lines.empty())
            {
                Recommendation r;
                r.key = "mixtone"; r.category = "Mix"; r.severity = 1; r.title = "Tonal balance differs from the target";
                r.evidence.push_back ({ Basis::Measured, "Versus " + t.name + ": " + lines + "." });
                r.issue = "Large broad differences usually translate to other systems as too boomy/thin or dull/harsh (heuristic).";
                r.proposal = "Adjust in your mastering chain with broad, gentle EQ moves.";
                r.confidence = t.measured ? 0.7 : 0.45; r.confidenceBasis = t.measured ? "Measured reference." : "House target (heuristic).";
                r.limitations = "Information only. Arrangement choices legitimately change the balance.";
                rep.recs.push_back (r);
            }
        }
        if (plan[pMix] > 0.5f && (isOn (plan, pCompOn) || isOn (plan, pDeessOn) || isOn (plan, pSatOn) || isOn (plan, pEqOn)))
        {
            Recommendation r;
            r.key = "mixonly"; r.category = "Mix"; r.severity = 2; r.title = "Vocal processing is running on a mix";
            r.evidence.push_back ({ Basis::Measured, "Context is set to a mix/instrumental while SwitaxVoice's vocal channel strip is active." });
            r.issue = "Vocal-oriented compression, de-essing and EQ will change the whole mix.";
            r.proposal = "Use SwitaxVoice as an analysis-only meter on this bus.";
            addChange (r, plan, pMix, 0.0f);
            addChange (r, plan, pLimiterOn, 0.0f);
            describeChanges (r);
            r.predicted.push_back ("Output equals the input (delayed by the plugin's fixed 1 ms latency, which the host compensates).");
            r.confidence = 0.9; r.confidenceBasis = "Follows from the selected context.";
            r.limitations = "Ignore if you deliberately want to process the mix.";
            rep.recs.push_back (r);
        }
    }

    AdvisorInput input;
    double sr = 48000.0;
    int latency = 0;
    Activity act;
    SibilanceMap sibMap;
};

/** Apply a recommendation's changes scaled by `amount` (0 = nothing, 1 = as proposed). */
inline void applyRecommendation (ParamValues& v, const Recommendation& r, float amount)
{
    for (const auto& c : r.changes)
    {
        const auto& d = kParamDefs[(size_t) c.index];
        float to = c.to;
        if (d.kind == ParamKind::Float) to = c.from + (c.to - c.from) * amount;
        else if (amount <= 0.0f) to = c.from;
        v[(size_t) c.index] = clampParam (c.index, to);
    }
}

} // namespace sx::analysis
