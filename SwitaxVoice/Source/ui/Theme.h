#pragma once

#include <JuceHeader.h>

namespace sx::ui
{
namespace colours
{
    const juce::Colour bg0       { 0xff0a0f1c };  // window (deep ink)
    const juce::Colour bg1       { 0xff111829 };  // panel
    const juce::Colour bg2       { 0xff172036 };  // raised panel
    const juce::Colour bg3       { 0xff212c47 };  // control body
    const juce::Colour line      { 0xff2a3656 };
    const juce::Colour lineSoft  { 0xff1b2540 };
    const juce::Colour text      { 0xffeef1f7 };
    const juce::Colour textDim   { 0xff94a0bb };
    const juce::Colour textFaint { 0xff5d6886 };
    const juce::Colour gold      { 0xffffc23d };  // primary accent
    const juce::Colour coral     { 0xffff6a4d };  // secondary accent
    const juce::Colour emerald   { 0xff2ee59d };
    const juce::Colour cyan      { 0xff3cc8ff };
    const juce::Colour violet    { 0xffa78bfa };
    const juce::Colour meterLow  { 0xff2ee59d };
    const juce::Colour meterMid  { 0xffffc23d };
    const juce::Colour meterHigh { 0xffff4d5e };

    // evidence labels: measured facts / heuristics / simulations / suggestions
    const juce::Colour measured   = cyan;
    const juce::Colour heuristic  = gold;
    const juce::Colour simulated  = emerald;
    const juce::Colour suggestion = coral;
} // namespace colours

/** Fonts: 0 = Inter regular, 1 = Inter semibold, 2 = Space Grotesk medium, 3 = Space Grotesk bold. */
juce::Typeface::Ptr getTypeface (int style);
juce::Font font (float height, int style = 0);

inline juce::ColourGradient accentGradient (juce::Point<float> a, juce::Point<float> b)
{
    return juce::ColourGradient (colours::gold, a, colours::coral, b, false);
}

/** Small decorative stripe band (kente-inspired geometric pattern) used in the header. */
void drawStripeBand (juce::Graphics& g, juce::Rectangle<float> r, float alpha = 1.0f);

/** The SwitaxVoice logo mark. */
void drawLogoMark (juce::Graphics& g, juce::Rectangle<float> r);

} // namespace sx::ui
