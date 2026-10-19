#pragma once
// Analysis contexts and comparison targets.
//
// IMPORTANT: the genre targets below are SwitaxVoice "house targets". They are heuristic
// starting points written from general vocal-production practice for each style. They were
// NOT measured from commercial recordings, and a vocal that deviates from them is not
// necessarily wrong. For a measured comparison, load a reference file (the "Reference" target),
// which is analysed with exactly the same measurement code as your vocal.

#include "Analyzer.h"

#include <string>
#include <utility>
#include <vector>

namespace sx::analysis
{
/** What the plugin's input actually carries. SwitaxVoice can only analyse its own input
    (plus an optional sidechain); this tells the advisor how to interpret that signal. */
enum class Context
{
    IsolatedVocal = 0,   // one dry vocal take on its own track
    VocalWithBleed,      // vocal recorded with beat/room bleed
    VocalBus,            // several vocal tracks summed (leads, doubles, ad-libs)
    FullMix,             // plugin on the master / mix bus
    Instrumental,        // beat only (e.g. to inspect the beat before recording)
    kNumContexts
};

inline const char* contextName (Context c)
{
    switch (c)
    {
        case Context::IsolatedVocal:  return "Isolated vocal";
        case Context::VocalWithBleed: return "Vocal with bleed";
        case Context::VocalBus:       return "Vocal bus";
        case Context::FullMix:        return "Full mix";
        case Context::Instrumental:   return "Instrumental";
        default:                      return "?";
    }
}

inline bool isVocalContext (Context c) { return c == Context::IsolatedVocal || c == Context::VocalWithBleed || c == Context::VocalBus; }

/** Confidence multiplier: how well the context supports vocal-specific conclusions. */
inline double contextConfidence (Context c)
{
    switch (c)
    {
        case Context::IsolatedVocal:  return 1.0;
        case Context::VocalWithBleed: return 0.75;
        case Context::VocalBus:       return 0.8;
        default:                      return 0.6;
    }
}

struct Target
{
    std::string name;
    bool measured = false;                 // true for a reference file, false for a house target
    bool pitchAdaptive = false;            // house vocal targets: low/mid part follows the singer's register
    double adaptedF0 = 0.0;                // f0 used by adaptToPitch (0 = not adapted)
    std::string description;
    BandArray relDb {};                    // target spectrum, same normalisation as Measurements::relDb
    BandArray upperDb {};                  // absolute-frequency curve (formants, presence, air) for adaptation
    double toleranceDb = 3.5;              // deviations within +/- tolerance are not reported
    double levelRangeMin = 6.0, levelRangeMax = 9.0;   // momentary P95 - P10 (dB)
    double sibilanceMaxRelDb = -8.0;       // sibilant event peak relative to vocal body
    double integratedLufs = -18.0;         // only used for full-mix information
};

/** Linear interpolation of (Hz, dB) anchors on a log-frequency axis. */
inline BandArray curveFromAnchors (const std::vector<std::pair<double, double>>& anchors)
{
    BandArray out {};
    for (int b = 0; b < kNumBands; ++b)
    {
        const double f = bandCentreHz (b);
        if (f <= anchors.front().first) { out[(size_t) b] = anchors.front().second; continue; }
        if (f >= anchors.back().first) { out[(size_t) b] = anchors.back().second; continue; }
        for (size_t k = 1; k < anchors.size(); ++k)
        {
            if (f <= anchors[k].first)
            {
                const double t = std::log (f / anchors[k - 1].first) / std::log (anchors[k].first / anchors[k - 1].first);
                out[(size_t) b] = anchors[k - 1].second + t * (anchors[k].second - anchors[k - 1].second);
                break;
            }
        }
    }
    return out;
}

/** Re-normalise a target curve to 0 dB mean over 200 Hz - 5 kHz (as Measurements::relDb). */
inline BandArray normaliseCurve (BandArray c)
{
    std::array<bool, kNumBands> all {};
    all.fill (true);
    const double ref = meanOverRange (c, all, 200.0, 5000.0);
    for (auto& v : c) v -= ref;
    return c;
}

enum class Genre { AfrobeatLead = 0, AfrofusionLead, AmapianoVocal, MelodicRap, AfroPopBright, NaturalVocal, kNumGenres };

inline std::vector<std::string> genreNames()
{
    return { "Afrobeat Lead", "Afrofusion Lead", "Amapiano Vocal", "Melodic Rap", "Afro-Pop Bright", "Natural Vocal" };
}

/** House target for a vocal in the given style (heuristic, see file comment). */
inline Target genreTarget (Genre g)
{
    // Base shape of a finished lead vocal, 1/6-octave band power relative to the
    // 200 Hz - 5 kHz mean (pink-referenced). Genre variants add broad offsets.
    using A = std::vector<std::pair<double, double>>;
    const A base { { 20, -40 }, { 60, -24 }, { 100, -12 }, { 160, -4 }, { 250, 1.5 }, { 400, 2.5 }, { 700, 1.5 },
                   { 1200, -0.5 }, { 2000, -2.5 }, { 3500, -3.5 }, { 6000, -7.5 }, { 10000, -12.5 }, { 16000, -19 }, { 20000, -26 } };
    auto offsets = [] (const A& o) { return curveFromAnchors (o); };

    Target t;
    BandArray off {};
    switch (g)
    {
        case Genre::AfrobeatLead:
            t.name = "Afrobeat Lead";
            t.description = "Forward and bright so the voice cuts through busy percussion; controlled low-mids.";
            off = offsets ({ { 20, 0 }, { 200, -0.5 }, { 400, -1 }, { 1000, 0 }, { 2500, 1.2 }, { 4000, 1.5 }, { 8000, 2 }, { 14000, 2 }, { 20000, 2 } });
            t.levelRangeMin = 5.0; t.levelRangeMax = 8.5; t.sibilanceMaxRelDb = -8.0;
            break;
        case Genre::AfrofusionLead:
            t.name = "Afrofusion Lead";
            t.description = "Smoother and warmer than Afrobeat, with an open airy top and relaxed dynamics.";
            off = offsets ({ { 20, 0 }, { 200, 0.5 }, { 400, 0 }, { 1000, 0 }, { 3000, 0.5 }, { 8000, 1.5 }, { 14000, 2 }, { 20000, 2 } });
            t.levelRangeMin = 6.0; t.levelRangeMax = 9.5; t.sibilanceMaxRelDb = -9.0;
            break;
        case Genre::AmapianoVocal:
            t.name = "Amapiano Vocal";
            t.description = "Airy and slightly lighter in the lows, leaving room for log drums and bass.";
            off = offsets ({ { 20, -3 }, { 150, -2 }, { 300, -0.5 }, { 1000, 0 }, { 3000, 0.5 }, { 8000, 2.5 }, { 14000, 3 }, { 20000, 3 } });
            t.levelRangeMin = 6.0; t.levelRangeMax = 9.5; t.sibilanceMaxRelDb = -8.5;
            break;
        case Genre::MelodicRap:
            t.name = "Melodic Rap";
            t.description = "Dense and upfront: a little more low-mid weight and presence, tighter dynamics.";
            off = offsets ({ { 20, 0 }, { 200, 0.8 }, { 400, 1 }, { 1000, 0 }, { 2500, 1.5 }, { 4000, 1.5 }, { 8000, 0.5 }, { 20000, 0 } });
            t.levelRangeMin = 4.0; t.levelRangeMax = 7.5; t.sibilanceMaxRelDb = -7.5;
            break;
        case Genre::AfroPopBright:
            t.name = "Afro-Pop Bright";
            t.description = "Glossy and present with extended air; firm level control.";
            off = offsets ({ { 20, 0 }, { 200, -1 }, { 400, -1 }, { 1000, 0 }, { 2500, 1.5 }, { 4000, 2 }, { 8000, 2.5 }, { 14000, 3 }, { 20000, 3 } });
            t.levelRangeMin = 5.0; t.levelRangeMax = 8.0; t.sibilanceMaxRelDb = -7.5;
            break;
        case Genre::NaturalVocal:
        default:
            t.name = "Natural Vocal";
            t.description = "Minimal treatment: natural tone and dynamics, clean-up only.";
            off = offsets ({ { 20, 0 }, { 20000, 0 } });
            t.levelRangeMin = 6.0; t.levelRangeMax = 12.0; t.sibilanceMaxRelDb = -7.0;
            t.toleranceDb = 4.5;
            break;
    }
    const auto b = curveFromAnchors (base);
    for (int i = 0; i < kNumBands; ++i) t.upperDb[(size_t) i] = b[(size_t) i] + off[(size_t) i];
    t.relDb = normaliseCurve (t.upperDb);
    t.pitchAdaptive = true;
    return t;
}

/** Below ~1.2 kHz a voice's long-term spectrum is dominated by its own fundamentals and low
    harmonics, so it depends on the register sung, not on tone quality. A soprano would look
    "boxy" and a baritone "muddy" against any fixed curve. This re-centres the low/mid part of a
    house target on the measured median f0 (template in octaves relative to f0), keeps the
    absolute curve above 2 kHz (formants, presence, air do not move with the note) and
    cross-fades in between. Measured reference targets are never modified. */
inline void adaptToPitch (Target& t, double f0Low, double f0Median, double f0High)
{
    if (! t.pitchAdaptive || f0Median < 60.0 || f0Median > 1000.0) return;
    const std::vector<std::pair<double, double>> rel { { -3.0, -40 }, { -2.0, -30 }, { -1.0, -18 }, { -0.4, -6 }, { 0.0, 0 },
                                                       { 1.0, 1 }, { 1.585, 0 }, { 2.0, -1.5 }, { 4.0, -4.5 } };
    auto templ = [&rel] (double x)
    {
        if (x <= rel.front().first) return rel.front().second;
        for (size_t k = 1; k < rel.size(); ++k)
            if (x <= rel[k].first)
            {
                const double u = (x - rel[k - 1].first) / (rel[k].first - rel[k - 1].first);
                return rel[k - 1].second + u * (rel[k].second - rel[k - 1].second);
            }
        return rel.back().second - 1.5 * (x - rel.back().first);
    };
    // the long-term spectrum of a melody is the template averaged over the notes sung
    const double f0s[3] = { std::clamp (f0Low, 60.0, f0Median), f0Median, std::clamp (f0High, f0Median, 1000.0) };
    const double w[3] = { 0.25, 0.5, 0.25 };
    BandArray low {};
    for (int b = 0; b < kNumBands; ++b)
    {
        double p = 0.0;
        for (int k = 0; k < 3; ++k) p += w[k] * dbToPow (templ (std::log2 (bandCentreHz (b) / f0s[k])));
        low[(size_t) b] = powToDb (p);
    }
    // align the two curves at 1.2-2 kHz, then cross-fade (log frequency)
    std::array<bool, kNumBands> all {};
    all.fill (true);
    const double offset = meanOverRange (t.upperDb, all, 1200.0, 2000.0) - meanOverRange (low, all, 1200.0, 2000.0);
    for (int b = 0; b < kNumBands; ++b)
    {
        const double f = bandCentreHz (b);
        const double w = std::clamp (std::log (f / 1200.0) / std::log (2000.0 / 1200.0), 0.0, 1.0);
        t.relDb[(size_t) b] = (1.0 - w) * (low[(size_t) b] + offset) + w * t.upperDb[(size_t) b];
    }
    t.relDb = normaliseCurve (t.relDb);
    t.adaptedF0 = f0Median;
}

/** House target for a full mix / instrumental (information only). */
inline Target mixTarget (Genre g)
{
    using A = std::vector<std::pair<double, double>>;
    Target t;
    const bool amapiano = g == Genre::AmapianoVocal;
    t.name = std::string (amapiano ? "Amapiano" : "Afrobeat / Afro-pop") + " mix (house target)";
    t.description = "Broad tonal balance typical of the style. Information only: SwitaxVoice's channel strip is a vocal chain.";
    t.relDb = normaliseCurve (curveFromAnchors (amapiano
        ? A { { 20, -2 }, { 45, 7 }, { 70, 7.5 }, { 120, 4.5 }, { 250, 1 }, { 600, -0.5 }, { 1500, -2.5 }, { 4000, -5 }, { 8000, -7.5 }, { 14000, -12 }, { 20000, -20 } }
        : A { { 20, -6 }, { 50, 4 }, { 80, 5 }, { 150, 3 }, { 300, 1 }, { 800, -1 }, { 2000, -3 }, { 5000, -5.5 }, { 10000, -8.5 }, { 16000, -14 }, { 20000, -20 } }));
    t.toleranceDb = 4.5;
    t.levelRangeMin = 3.0; t.levelRangeMax = 8.0;
    t.integratedLufs = -9.0;
    return t;
}

/** Measured target from a reference analysis. */
inline Target referenceTarget (const Measurements& ref, const std::string& name)
{
    Target t;
    t.name = "Reference: " + name;
    t.measured = true;
    t.description = "Measured from your reference file with the same analysis as your vocal.";
    t.relDb = ref.relDb;
    t.toleranceDb = 3.0;
    t.levelRangeMin = std::max (2.0, ref.levelRange - 2.0);
    t.levelRangeMax = ref.levelRange + 2.0;
    t.sibilanceMaxRelDb = ref.sibilance.events > 0 ? ref.sibilance.peakRelBodyDb + 2.0 : -8.0;
    t.integratedLufs = ref.integratedLufs;
    return t;
}

} // namespace sx::analysis
