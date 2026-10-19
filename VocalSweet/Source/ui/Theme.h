#pragma once

#include <JuceHeader.h>

namespace vs::ui
{
namespace colours
{
    const juce::Colour bg0       { 0xff0d0e12 };  // window
    const juce::Colour bg1       { 0xff14161c };  // panel
    const juce::Colour bg2       { 0xff1b1e26 };  // raised panel
    const juce::Colour bg3       { 0xff242833 };  // control body
    const juce::Colour line      { 0xff2b303c };
    const juce::Colour lineSoft  { 0xff20242d };
    const juce::Colour text      { 0xffece8e4 };
    const juce::Colour textDim   { 0xff8b909d };
    const juce::Colour textFaint { 0xff5b606c };
    const juce::Colour honey     { 0xffffa24a };  // primary accent (warm)
    const juce::Colour rose      { 0xffff4f86 };  // secondary accent
    const juce::Colour mint      { 0xff4fe3c6 };  // EQ curve
    const juce::Colour violet    { 0xff9d7cff };
    const juce::Colour meterLow  { 0xff3ad1a8 };
    const juce::Colour meterMid  { 0xffffc24a };
    const juce::Colour meterHigh { 0xffff4a5a };
} // namespace colours

juce::Typeface::Ptr getTypeface (int weight); // 0 regular, 1 semibold, 2 bold
juce::Font font (float height, int weight = 0);

inline juce::ColourGradient accentGradient (juce::Point<float> a, juce::Point<float> b)
{
    return juce::ColourGradient (colours::honey, a, colours::rose, b, false);
}

} // namespace vs::ui
