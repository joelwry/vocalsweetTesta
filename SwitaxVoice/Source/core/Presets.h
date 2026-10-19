#pragma once
// SwitaxVoice factory presets. Each preset is the default parameter set plus explicit overrides,
// so every preset is a complete, deterministic state. They are starting points; the Assist page
// is meant to refine them for the actual recording.

#include "Params.h"
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace sx
{
struct Preset
{
    std::string name;
    ParamValues values;
};

inline Preset makePreset (const char* name, std::initializer_list<std::pair<ParamIndex, float>> overrides)
{
    Preset p { name, defaultParamValues() };
    for (const auto& [index, value] : overrides)
        p.values[(size_t) index] = clampParam (index, value);
    return p;
}

inline const std::vector<Preset>& factoryPresets()
{
    // clang-format off
    static const std::vector<Preset> presets {
        // Neutral starting point for the Assist page: gentle clean-up only.
        makePreset ("Clean Start", {}),

        // Forward, bright, controlled: sits on top of busy percussion.
        makePreset ("Afrobeat Lead", {
            { pHpfFreq, 90 }, { pLowFreq, 280 }, { pLowGain, -2.0f }, { pBoxFreq, 800 }, { pBoxGain, -1.5f },
            { pPresFreq, 3800 }, { pPresGain, 2.5f }, { pAirFreq, 11500 }, { pAirGain, 3.0f },
            { pCompThresh, -24 }, { pCompRatio, 3.0f }, { pCompAttack, 6 }, { pCompRelease, 90 },
            { pDeessFreq, 7000 }, { pDeessThresh, -32 }, { pDeessRange, 7 },
            { pSatDrive, 18 }, { pExciteOn, 1 }, { pExciteFreq, 5000 }, { pExciteAmount, 12 } }),

        // Warmer and smoother with an open top.
        makePreset ("Afrofusion Lead", {
            { pHpfFreq, 80 }, { pLowFreq, 260 }, { pLowGain, -1.0f }, { pBoxFreq, 700 }, { pBoxGain, -1.0f },
            { pPresFreq, 3200 }, { pPresGain, 1.0f }, { pAirFreq, 12500 }, { pAirGain, 3.0f },
            { pCompThresh, -24 }, { pCompRatio, 2.6f }, { pCompAttack, 10 }, { pCompRelease, 140 },
            { pDeessFreq, 6500 }, { pDeessThresh, -32 }, { pDeessRange, 7 },
            { pSatDrive, 24 } }),

        // Airy and lighter in the lows so log drums and bass have room.
        makePreset ("Amapiano Vocal", {
            { pHpfFreq, 120 }, { pHpfSlope, 1 }, { pLowFreq, 300 }, { pLowGain, -2.5f }, { pBoxGain, -1.0f },
            { pPresFreq, 3500 }, { pPresGain, 1.0f }, { pAirFreq, 10500 }, { pAirGain, 4.0f },
            { pCompThresh, -24 }, { pCompRatio, 2.8f }, { pCompAttack, 12 }, { pCompRelease, 160 },
            { pDeessFreq, 7000 }, { pDeessThresh, -34 }, { pDeessRange, 8 },
            { pSatDrive, 12 }, { pExciteOn, 1 }, { pExciteFreq, 6000 }, { pExciteAmount, 15 } }),

        // Dense, upfront, firmly controlled with some grit.
        makePreset ("Melodic Rap", {
            { pHpfFreq, 95 }, { pLowFreq, 300 }, { pLowGain, -1.5f }, { pBoxFreq, 1000 }, { pBoxGain, -2.0f },
            { pPresFreq, 3000 }, { pPresGain, 2.5f }, { pAirGain, 1.5f },
            { pCompThresh, -26 }, { pCompRatio, 4.0f }, { pCompAttack, 3 }, { pCompRelease, 70 },
            { pDeessFreq, 6500 }, { pDeessThresh, -32 }, { pDeessRange, 7 },
            { pSatDrive, 35 } }),

        // Glossy, present, extended air.
        makePreset ("Afro-Pop Bright", {
            { pHpfFreq, 100 }, { pHpfSlope, 1 }, { pLowFreq, 320 }, { pLowGain, -2.5f }, { pBoxFreq, 850 }, { pBoxGain, -2.0f },
            { pPresFreq, 4000 }, { pPresGain, 3.0f }, { pAirFreq, 12000 }, { pAirGain, 4.5f },
            { pCompThresh, -25 }, { pCompRatio, 3.5f }, { pCompAttack, 5 }, { pCompRelease, 80 },
            { pDeessFreq, 7500 }, { pDeessThresh, -35 }, { pDeessRange, 9 },
            { pSatDrive, 15 }, { pExciteOn, 1 }, { pExciteFreq, 6000 }, { pExciteAmount, 20 } }),

        // Thin, soft, wide and controlled so stacks sit behind the lead.
        makePreset ("Background Stack", {
            { pHpfFreq, 150 }, { pHpfSlope, 1 }, { pLowFreq, 300 }, { pLowGain, -4.0f }, { pBoxGain, -2.0f },
            { pPresGain, -1.5f }, { pAirGain, 2.0f },
            { pCompThresh, -28 }, { pCompRatio, 3.5f }, { pCompAttack, 12 }, { pCompRelease, 200 },
            { pDeessThresh, -38 }, { pDeessRange, 10 },
            { pSatDrive, 20 }, { pDoubleOn, 1 }, { pDoubleAmount, 40 }, { pDoubleWidth, 85 }, { pOutputGain, -3 } }),

        // Ad-libs: narrower band, more grit, wide double.
        makePreset ("Ad-lib Spice", {
            { pHpfFreq, 200 }, { pHpfSlope, 1 }, { pLowFreq, 400 }, { pLowGain, -3.0f },
            { pPresFreq, 2500 }, { pPresGain, 4.0f }, { pAirGain, -2.0f },
            { pCompThresh, -28 }, { pCompRatio, 5.0f }, { pCompAttack, 2 }, { pCompRelease, 60 },
            { pSatDrive, 55 }, { pDoubleOn, 1 }, { pDoubleAmount, 60 }, { pDoubleWidth, 100 }, { pOutputGain, -4 } }),

        // Transparent: use SwitaxVoice as an analyser / meter only (e.g. on a mix bus).
        makePreset ("Analysis Only", {
            { pHpfOn, 0 }, { pEqOn, 0 }, { pCompOn, 0 }, { pDeessOn, 0 }, { pSatOn, 0 }, { pExciteOn, 0 },
            { pDoubleOn, 0 }, { pLimiterOn, 0 }, { pMix, 0 } }),
    };
    // clang-format on
    return presets;
}

} // namespace sx
