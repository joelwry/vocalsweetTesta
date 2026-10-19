#pragma once
// Single source of truth for every SwitaxVoice parameter (ID, range, default). Framework
// independent: the DSP, the analysis advisor, the plugin, the UI and the tests all share it.
//
// Parameter convention used across the VocalSweet / SwitaxVoice family:
//   * IDs are lowerCamelCase, stable forever (they are stored in DAW projects).
//   * Plain values are stored in natural units (dB, Hz, ms, %, :1); booleans are 0/1.
//   * Every module has an <module>On switch so it can be bypassed click-free.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace sx
{
enum class ParamKind { Float, Bool, Choice };

struct ParamDef
{
    const char* id;
    const char* name;
    ParamKind kind;
    float min, max, def;
    float centre;       // skew centre for Float (0 = linear)
    const char* unit;
    const char* group;
};

// clang-format off
enum ParamIndex : int
{
    pInputGain,
    pHpfOn, pHpfFreq, pHpfSlope,
    pEqOn, pLowFreq, pLowGain, pBoxFreq, pBoxGain, pHarshFreq, pHarshGain, pPresFreq, pPresGain, pAirFreq, pAirGain,
    pCompOn, pCompThresh, pCompRatio, pCompAttack, pCompRelease, pCompMakeup, pCompAuto,
    pDeessOn, pDeessFreq, pDeessThresh, pDeessRange,
    pSatOn, pSatDrive,
    pExciteOn, pExciteFreq, pExciteAmount,
    pDoubleOn, pDoubleAmount, pDoubleWidth,
    pOutputGain, pLimiterOn, pCeiling, pMix, pBypass,
    kNumParams
};

inline constexpr std::array<ParamDef, kNumParams> kParamDefs {{
    { "inputGain",   "Input Gain",        ParamKind::Float,  -24.0f,  24.0f,    0.0f,     0.0f, "dB", "IO" },
    { "hpfOn",       "HPF On",            ParamKind::Bool,     0.0f,   1.0f,    1.0f,     0.0f, "",   "HPF" },
    { "hpfFreq",     "HPF Frequency",     ParamKind::Float,   20.0f, 400.0f,   70.0f,   100.0f, "Hz", "HPF" },
    { "hpfSlope",    "HPF Slope",         ParamKind::Choice,   0.0f,   1.0f,    0.0f,     0.0f, "",   "HPF" },  // 0 = 12, 1 = 24 dB/oct
    { "eqOn",        "EQ On",             ParamKind::Bool,     0.0f,   1.0f,    1.0f,     0.0f, "",   "EQ" },
    { "lowFreq",     "Low-Mid Frequency", ParamKind::Float,  100.0f, 600.0f,  250.0f,   250.0f, "Hz", "EQ" },
    { "lowGain",     "Low-Mid Gain",      ParamKind::Float,  -12.0f,   6.0f,    0.0f,     0.0f, "dB", "EQ" },
    { "boxFreq",     "Box Frequency",     ParamKind::Float,  400.0f,1800.0f,  800.0f,   800.0f, "Hz", "EQ" },
    { "boxGain",     "Box Gain",          ParamKind::Float,  -12.0f,   6.0f,    0.0f,     0.0f, "dB", "EQ" },
    { "harshFreq",   "Harsh Frequency",   ParamKind::Float, 1800.0f,6000.0f, 3200.0f,  3200.0f, "Hz", "EQ" },
    { "harshGain",   "Harsh Gain",        ParamKind::Float,  -12.0f,   3.0f,    0.0f,     0.0f, "dB", "EQ" },
    { "presFreq",    "Presence Frequency",ParamKind::Float, 1500.0f,7000.0f, 4000.0f,  3500.0f, "Hz", "EQ" },
    { "presGain",    "Presence Gain",     ParamKind::Float,   -6.0f,   9.0f,    0.0f,     0.0f, "dB", "EQ" },
    { "airFreq",     "Air Frequency",     ParamKind::Float, 6000.0f,18000.0f,11000.0f,10500.0f, "Hz", "EQ" },
    { "airGain",     "Air Gain",          ParamKind::Float,   -6.0f,   9.0f,    0.0f,     0.0f, "dB", "EQ" },
    { "compOn",      "Compressor On",     ParamKind::Bool,     0.0f,   1.0f,    1.0f,     0.0f, "",   "COMP" },
    { "compThresh",  "Comp Threshold",    ParamKind::Float,  -60.0f,   0.0f,  -18.0f,     0.0f, "dB", "COMP" },
    { "compRatio",   "Comp Ratio",        ParamKind::Float,    1.0f,  20.0f,    2.5f,     4.0f, ":1", "COMP" },
    { "compAttack",  "Comp Attack",       ParamKind::Float,    0.1f, 100.0f,    8.0f,    10.0f, "ms", "COMP" },
    { "compRelease", "Comp Release",      ParamKind::Float,   10.0f,1000.0f,  120.0f,   150.0f, "ms", "COMP" },
    { "compMakeup",  "Comp Makeup",       ParamKind::Float,   -6.0f,  18.0f,    0.0f,     0.0f, "dB", "COMP" },
    { "compAuto",    "Comp Auto Makeup",  ParamKind::Bool,     0.0f,   1.0f,    1.0f,     0.0f, "",   "COMP" },
    { "deessOn",     "De-Esser On",       ParamKind::Bool,     0.0f,   1.0f,    1.0f,     0.0f, "",   "DEESS" },
    { "deessFreq",   "De-Ess Frequency",  ParamKind::Float, 3000.0f,12000.0f,6500.0f,  6500.0f, "Hz", "DEESS" },
    { "deessThresh", "De-Ess Threshold",  ParamKind::Float,  -60.0f,   0.0f,  -30.0f,     0.0f, "dB", "DEESS" },
    { "deessRange",  "De-Ess Range",      ParamKind::Float,    0.0f,  18.0f,    6.0f,     0.0f, "dB", "DEESS" },
    { "satOn",       "Saturation On",     ParamKind::Bool,     0.0f,   1.0f,    1.0f,     0.0f, "",   "SAT" },
    { "satDrive",    "Saturation Drive",  ParamKind::Float,    0.0f, 100.0f,   10.0f,     0.0f, "%",  "SAT" },
    { "exciteOn",    "Exciter On",        ParamKind::Bool,     0.0f,   1.0f,    0.0f,     0.0f, "",   "EXCITE" },
    { "exciteFreq",  "Exciter Frequency", ParamKind::Float, 2000.0f,10000.0f,5000.0f,  4500.0f, "Hz", "EXCITE" },
    { "exciteAmount","Exciter Amount",    ParamKind::Float,    0.0f, 100.0f,   15.0f,     0.0f, "%",  "EXCITE" },
    { "doubleOn",    "Doubler On",        ParamKind::Bool,     0.0f,   1.0f,    0.0f,     0.0f, "",   "DOUBLE" },
    { "doubleAmount","Doubler Amount",    ParamKind::Float,    0.0f, 100.0f,   40.0f,     0.0f, "%",  "DOUBLE" },
    { "doubleWidth", "Doubler Width",     ParamKind::Float,    0.0f, 100.0f,   70.0f,     0.0f, "%",  "DOUBLE" },
    { "outputGain",  "Output Gain",       ParamKind::Float,  -24.0f,  24.0f,    0.0f,     0.0f, "dB", "IO" },
    { "limiterOn",   "Limiter On",        ParamKind::Bool,     0.0f,   1.0f,    1.0f,     0.0f, "",   "IO" },
    { "ceiling",     "Output Ceiling",    ParamKind::Float,  -12.0f,   0.0f,   -1.0f,     0.0f, "dB", "IO" },
    { "mix",         "Dry/Wet",           ParamKind::Float,    0.0f, 100.0f,  100.0f,     0.0f, "%",  "IO" },
    { "bypass",      "Bypass",            ParamKind::Bool,     0.0f,   1.0f,    0.0f,     0.0f, "",   "IO" },
}};
// clang-format on

inline int findParamIndex (const char* id) noexcept
{
    for (int i = 0; i < kNumParams; ++i)
        if (std::strcmp (kParamDefs[(size_t) i].id, id) == 0)
            return i;
    return -1;
}

using ParamValues = std::array<float, kNumParams>;

inline ParamValues defaultParamValues() noexcept
{
    ParamValues v {};
    for (int i = 0; i < kNumParams; ++i) v[(size_t) i] = kParamDefs[(size_t) i].def;
    return v;
}

inline float clampParam (int index, float value) noexcept
{
    const auto& d = kParamDefs[(size_t) index];
    if (! std::isfinite (value)) return d.def;
    value = std::clamp (value, d.min, d.max);
    if (d.kind != ParamKind::Float) value = std::round (value);
    return value;
}

inline bool isOn (const ParamValues& v, int index) noexcept { return v[(size_t) index] > 0.5f; }

/** Static compressor curve (6 dB soft knee): dB of gain reduction for a given level. */
inline float staticGainReductionDb (float levelDb, float thresholdDb, float ratio) noexcept
{
    const float slope = 1.0f - 1.0f / std::max (ratio, 1.0f), knee = 6.0f, over = levelDb - thresholdDb;
    if (2.0f * over <= -knee) return 0.0f;
    if (2.0f * over >= knee) return slope * over;
    const float x = over + knee * 0.5f;
    return slope * x * x / (2.0f * knee);
}

/** Total compressor makeup in dB. The automatic part compensates 80 % of the static gain
    reduction a typical vocal peak (-14 dBFS) receives, so threshold/ratio moves stay roughly
    loudness-neutral; the Makeup knob adds on top. */
inline float effectiveMakeupDb (const ParamValues& v) noexcept
{
    const float autoPart = isOn (v, pCompAuto)
                               ? 0.8f * staticGainReductionDb (-14.0f, v[pCompThresh], v[pCompRatio])
                               : 0.0f;
    return std::clamp (v[pCompMakeup] + autoPart, -6.0f, 30.0f);
}

} // namespace sx
