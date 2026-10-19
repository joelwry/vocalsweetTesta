#include "Widgets.h"
#include "../PluginProcessor.h"

namespace sx::ui
{
float drawChip (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, juce::Colour c, bool filled)
{
    const auto f = font (9.5f, 3);
    const float w = juce::GlyphArrangement::getStringWidth (f, text) + 12.0f;
    auto r = area.withWidth (w).withSizeKeepingCentre (w, juce::jmin (16.0f, area.getHeight()));
    g.setColour (filled ? c : c.withAlpha (0.14f));
    g.fillRoundedRectangle (r, 3.0f);
    if (! filled)
    {
        g.setColour (c.withAlpha (0.55f));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    }
    g.setFont (f);
    g.setColour (filled ? colours::bg0 : c);
    g.drawText (text, r, juce::Justification::centred);
    return w;
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& t,
            juce::Colour a, bool bipolar, bool b)
    : paramId (id), title (t), accent (a), big (b)
{
    param = state.getParameter (id);
    jassert (param != nullptr);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (big ? 300 : 220);
    slider.getProperties().set ("big", big);
    slider.getProperties().set ("bipolar", bipolar);
    slider.getProperties().set ("accent", (juce::int64) accent.getARGB());
    slider.setPopupMenuEnabled (false);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
    slider.setTooltip (param->getName (64) + "  (double-click to reset)");
}

void Knob::setDimmed (bool d)
{
    if (d == dimmed) return;
    dimmed = d;
    slider.getProperties().set ("dimmed", d);
    slider.repaint();
    repaint();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (16);
    r.removeFromBottom (16);
    const int d = juce::jmin (r.getWidth(), r.getHeight(), big ? 200 : 64);
    slider.setBounds (r.withSizeKeepingCentre (d, d));
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    if (highlighted)
    {
        g.setColour (colours::emerald.withAlpha (0.12f));
        g.fillRoundedRectangle (r.toFloat().reduced (1.0f), 6.0f);
        g.setColour (colours::emerald.withAlpha (0.6f));
        g.drawRoundedRectangle (r.toFloat().reduced (1.0f), 6.0f, 1.0f);
    }
    const auto sb = slider.getBounds();
    g.setFont (font (10.5f, 2));
    g.setColour (dimmed ? colours::textFaint : colours::textDim);
    g.drawText (title.toUpperCase(), juce::Rectangle<int> (0, sb.getY() - 16, r.getWidth(), 15), juce::Justification::centred);
    g.setFont (font (11.0f, 1));
    g.setColour (dimmed ? colours::textFaint : colours::text);
    g.drawText (param->getCurrentValueAsText(), juce::Rectangle<int> (0, sb.getBottom() + 1, r.getWidth(), 15), juce::Justification::centred);
}

//==============================================================================
void LevelMeter::setLevels (float pL, float pR, float rL, float rR)
{
    const float in[2] = { pL, pR }, rm[2] = { rL, rR };
    for (int c = 0; c < 2; ++c)
    {
        const float pk = juce::Decibels::gainToDecibels (in[c], -90.0f);
        const float rs = juce::Decibels::gainToDecibels (rm[c], -90.0f);
        peakDb[c] = pk > peakDb[c] ? pk : juce::jmax (pk, peakDb[c] - 1.6f);
        rmsDb[c] = rs > rmsDb[c] ? rmsDb[c] + 0.5f * (rs - rmsDb[c]) : juce::jmax (rs, rmsDb[c] - 1.0f);
        if (pk >= holdDb[c]) { holdDb[c] = pk; holdTicks[c] = 45; }
        else if (--holdTicks[c] <= 0) holdDb[c] = juce::jmax (-90.0f, holdDb[c] - 1.2f);
        if (pk > -0.1f) clipped = true;
    }
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setFont (font (10.5f, 3));
    g.setColour (colours::textDim);
    g.drawText (name, r.removeFromTop (14.0f), juce::Justification::centred);
    const float maxHold = juce::jmax (holdDb[0], holdDb[1]);
    g.setFont (font (11.5f, 2));
    g.setColour (clipped ? colours::meterHigh : colours::text);
    g.drawText (maxHold <= -89.0f ? juce::String ("-inf") : juce::String (maxHold, 1), r.removeFromBottom (16.0f), juce::Justification::centred);

    auto bars = r.reduced (2.0f, 4.0f);
    const float minDb = -60.0f;
    auto yFor = [&] (float db) { return juce::jmap (juce::jlimit (minDb, 6.0f, db), minDb, 6.0f, bars.getBottom(), bars.getY()); };
    const float barW = (bars.getWidth() - 3.0f) * 0.5f;
    for (int c = 0; c < 2; ++c)
    {
        auto bar = juce::Rectangle<float> (bars.getX() + (float) c * (barW + 3.0f), bars.getY(), barW, bars.getHeight());
        g.setColour (colours::bg0);
        g.fillRoundedRectangle (bar, 2.0f);
        juce::ColourGradient grad (colours::meterLow, 0.0f, bar.getBottom(), colours::meterHigh, 0.0f, bar.getY(), false);
        grad.addColour (juce::jmap (-18.0f, minDb, 6.0f, 0.0f, 1.0f), colours::meterLow);
        grad.addColour (juce::jmap (-6.0f, minDb, 6.0f, 0.0f, 1.0f), colours::meterMid);
        grad.addColour (juce::jmap (0.0f, minDb, 6.0f, 0.0f, 1.0f), colours::meterHigh);
        g.setGradientFill (grad);
        g.setOpacity (0.35f);
        g.fillRoundedRectangle (bar.withTop (yFor (peakDb[c])), 2.0f);
        g.setOpacity (1.0f);
        g.fillRoundedRectangle (bar.withTop (yFor (rmsDb[c])), 2.0f);
        if (holdDb[c] > minDb)
        {
            g.setColour (holdDb[c] > -0.1f ? colours::meterHigh : colours::text.withAlpha (0.85f));
            g.fillRect (bar.withY (yFor (holdDb[c]) - 1.0f).withHeight (2.0f));
        }
    }
    g.setColour (colours::bg0.withAlpha (0.7f));
    for (float db : { 0.0f, -12.0f, -24.0f, -36.0f, -48.0f })
        g.fillRect (bars.withY (yFor (db)).withHeight (1.0f));
}

//==============================================================================
void LoudnessPanel::update (const AnalysisEngine::LiveMeters& m, float compGr, float deessGr, float limGr)
{
    meters = m;
    auto smooth = [] (float& v, float t) { v = t > v ? t : v + 0.25f * (t - v); };
    smooth (gr[0], compGr); smooth (gr[1], deessGr); smooth (gr[2], limGr);
    repaint();
}

void LoudnessPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto lufs = [] (double v) { return v < -99.0 ? juce::String ("--") : juce::String (v, 1); };

    auto row = [&] (const char* label, const juce::String& value, const char* unit, juce::Colour c, float h = 30.0f)
    {
        auto line = r.removeFromTop (h);
        g.setFont (font (9.5f, 3));
        g.setColour (colours::textFaint);
        g.drawText (label, line.removeFromTop (11.0f), juce::Justification::centredLeft);
        g.setFont (font (16.0f, 3));
        g.setColour (c);
        const float w = juce::GlyphArrangement::getStringWidth (font (16.0f, 3), value);
        g.drawText (value, line.withWidth (w + 2), juce::Justification::centredLeft);
        g.setFont (font (10.0f, 1));
        g.setColour (colours::textDim);
        g.drawText (unit, line.withTrimmedLeft (w + 5), juce::Justification::centredLeft);
    };

    g.setFont (font (10.5f, 3));
    g.setColour (colours::gold);
    g.drawText ("LOUDNESS OUT", r.removeFromTop (16.0f), juce::Justification::centredLeft);
    row ("MOMENTARY", lufs (meters.outMomentary), "LUFS", colours::text);
    row ("SHORT-TERM", lufs (meters.outShortTerm), "LUFS", colours::text);
    row ("INTEGRATED", lufs (meters.outIntegrated), "LUFS", colours::gold);
    row ("INPUT INTEGRATED", lufs (meters.inIntegrated), "LUFS", colours::cyan);
    row ("TRUE PEAK", lufs (meters.outTruePeak), "dBTP", meters.outTruePeak > -1.0 ? colours::meterHigh : colours::text);
    row ("LOUDNESS RANGE", juce::String (meters.outLra, 1), "LU", colours::text);
    r.removeFromTop (4.0f);

    g.setFont (font (10.5f, 3));
    g.setColour (colours::gold);
    g.drawText ("GAIN REDUCTION", r.removeFromTop (16.0f), juce::Justification::centredLeft);
    const char* names[3] = { "COMP", "DE-ESS", "LIMIT" };
    const juce::Colour cols[3] = { colours::gold, colours::coral, colours::violet };
    for (int i = 0; i < 3; ++i)
    {
        auto line = r.removeFromTop (17.0f);
        g.setFont (font (9.5f, 3));
        g.setColour (colours::textDim);
        g.drawText (names[i], line.removeFromLeft (42.0f), juce::Justification::centredLeft);
        auto val = line.removeFromRight (30.0f);
        g.setFont (font (10.0f, 1));
        g.drawText (juce::String (gr[i], 1), val, juce::Justification::centredRight);
        auto bar = line.reduced (2.0f, 5.0f);
        g.setColour (colours::bg0);
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (cols[i]);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, gr[i] / 18.0f)), 2.0f);
    }
    r.removeFromTop (6.0f);

    g.setFont (font (10.5f, 3));
    g.setColour (colours::gold);
    g.drawText ("CORRELATION", r.removeFromTop (16.0f), juce::Justification::centredLeft);
    auto bar = r.removeFromTop (14.0f).reduced (0.0f, 3.0f);
    g.setColour (colours::bg0);
    g.fillRoundedRectangle (bar, 2.0f);
    const float cx = bar.getCentreX();
    const float x = juce::jmap ((float) juce::jlimit (-1.0, 1.0, meters.correlation), -1.0f, 1.0f, bar.getX(), bar.getRight());
    g.setColour (meters.correlation < 0.0 ? colours::meterHigh : colours::emerald);
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (cx, x), bar.getY(), juce::jmax (cx, x), bar.getBottom()));
    g.setColour (colours::textFaint);
    g.fillRect (bar.withX (cx - 0.5f).withWidth (1.0f));
    g.setFont (font (9.5f, 1));
    g.drawText ("-1", r.removeFromTop (12.0f), juce::Justification::centredLeft);
    g.drawText ("+1", r.translated (0, -12.0f).removeFromTop (12.0f), juce::Justification::centredRight);
    g.setColour (colours::text);
    g.drawText (juce::String (meters.correlation, 2), r.translated (0, -12.0f).removeFromTop (12.0f), juce::Justification::centred);

    r.removeFromTop (4.0f);
    g.setFont (font (9.5f, 1));
    g.setColour (meters.sidechainActive ? colours::emerald : colours::textFaint);
    g.drawFittedText (meters.sidechainActive ? juce::String ("Sidechain: ") + lufs (meters.scShortTerm) + " LUFS-S"
                                             : juce::String ("Sidechain: no signal"), r.removeFromTop (14.0f).toNearestInt(),
                      juce::Justification::centredLeft, 1);
    g.setColour (colours::textFaint);
    g.drawFittedText ("Click to reset integrated", r.removeFromTop (14.0f).toNearestInt(), juce::Justification::centredLeft, 1);
}

//==============================================================================
EqDisplay::EqDisplay (SwitaxVoiceAudioProcessor& p) : proc (p)
{
    specIn.fill (-120.0f);
    specOut.fill (-120.0f);
}

juce::Rectangle<float> EqDisplay::plot() const { return getLocalBounds().toFloat().reduced (12.0f, 10.0f).withTrimmedTop (16.0f).withTrimmedLeft (22.0f).withTrimmedBottom (12.0f); }
float EqDisplay::xFor (float f) const { auto a = plot(); return a.getX() + a.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); }
float EqDisplay::yFor (float db) const { auto a = plot(); return juce::jmap (juce::jlimit (-18.0f, 18.0f, db), -18.0f, 18.0f, a.getBottom(), a.getY()); }

void EqDisplay::tick()
{
    const auto values = proc.getParamValues();
    const double rate = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
    if (values != last || rate != lastRate || curveDirty)
    {
        last = values; lastRate = rate; curveDirty = false;
        curve.clear();
        auto a = plot();
        for (float x = a.getX(); x <= a.getRight(); x += 2.0f)
        {
            const float f = 20.0f * std::pow (1000.0f, (x - a.getX()) / a.getWidth());
            const float y = yFor ((float) sx::dsp::Chain::responseDb (values, rate, f));
            curve.isEmpty() ? curve.startNewSubPath (x, y) : curve.lineTo (x, y);
        }
    }
    proc.analysis.getDisplaySpectra (specIn, specOut);
    repaint();
}

void EqDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::bg1);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (colours::lineSoft);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);
    auto a = plot();

    g.setFont (font (10.5f, 3));
    g.setColour (colours::gold);
    g.drawText ("EQ  +  LIVE SPECTRUM", bounds.reduced (14.0f, 8.0f).removeFromTop (14.0f), juce::Justification::topLeft);
    g.setFont (font (10.0f, 1));
    g.setColour (colours::cyan.withAlpha (0.8f));
    g.drawText ("input", bounds.reduced (14.0f, 8.0f).removeFromTop (14.0f).withTrimmedRight (60), juce::Justification::topRight);
    g.setColour (colours::text.withAlpha (0.8f));
    g.drawText ("output", bounds.reduced (14.0f, 8.0f).removeFromTop (14.0f), juce::Justification::topRight);

    g.setFont (font (9.5f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xFor (f);
        g.setColour (colours::lineSoft);
        g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
        g.setColour (colours::textFaint);
        g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), juce::Rectangle<float> (x - 15, a.getBottom() + 1, 30, 11), juce::Justification::centred);
    }
    for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
    {
        const float y = yFor (db);
        g.setColour (db == 0.0f ? colours::line : colours::lineSoft);
        g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
        g.setColour (colours::textFaint);
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (a.getX() - 24, y - 6, 20, 12), juce::Justification::centredRight);
    }

    // spectra (display scale: -90..-10 dB mapped over the plot height)
    auto specPath = [&] (const std::array<float, AnalysisEngine::kDisplayPoints>& s, bool fill)
    {
        juce::Path p;
        for (int i = 0; i < AnalysisEngine::kDisplayPoints; ++i)
        {
            const float x = xFor (AnalysisEngine::displayFreq (i));
            const float y = juce::jmap (juce::jlimit (-90.0f, -10.0f, s[(size_t) i]), -90.0f, -10.0f, a.getBottom(), a.getY());
            i == 0 ? p.startNewSubPath (x, y) : p.lineTo (x, y);
        }
        if (fill) { p.lineTo (a.getRight(), a.getBottom()); p.lineTo (a.getX(), a.getBottom()); p.closeSubPath(); }
        return p;
    };
    g.setColour (colours::cyan.withAlpha (0.10f));
    g.fillPath (specPath (specIn, true));
    g.setColour (colours::cyan.withAlpha (0.45f));
    g.strokePath (specPath (specIn, false), juce::PathStrokeType (1.0f));
    g.setColour (colours::text.withAlpha (0.55f));
    g.strokePath (specPath (specOut, false), juce::PathStrokeType (1.2f));

    // EQ curve
    juce::Path fillP (curve);
    fillP.lineTo (a.getRight(), yFor (0.0f));
    fillP.lineTo (a.getX(), yFor (0.0f));
    fillP.closeSubPath();
    g.setColour (colours::gold.withAlpha (0.10f));
    g.fillPath (fillP);
    g.setColour (proc.isAuditioning() ? colours::emerald : colours::gold);
    g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
ModuleCard::ModuleCard (juce::AudioProcessorValueTreeState& state, juce::String t, const juce::String& powerParamId,
                        int cols, juce::Colour a)
    : apvts (state), title (std::move (t)), powerId (powerParamId), accent (a), columns (cols)
{
    if (powerParamId.isNotEmpty())
    {
        addAndMakeVisible (power);
        power.getProperties().set ("accent", (juce::int64) accent.getARGB());
        power.setTooltip ("Module on/off (click-free)");
        powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, powerParamId, power);
        power.onStateChange = [this] { refreshPowerState(); };
    }
}

Knob& ModuleCard::addKnob (const juce::String& id, const juce::String& name, bool bipolar)
{
    knobs.push_back (std::make_unique<Knob> (apvts, id, name, accent, bipolar));
    addAndMakeVisible (*knobs.back());
    refreshPowerState();
    return *knobs.back();
}

juce::ToggleButton& ModuleCard::addToggle (const juce::String& id, const juce::String& text)
{
    toggles.push_back (std::make_unique<juce::ToggleButton> (text));
    toggles.back()->getProperties().set ("accent", (juce::int64) accent.getARGB());
    addAndMakeVisible (*toggles.back());
    toggleAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, id, *toggles.back()));
    return *toggles.back();
}

Knob* ModuleCard::findKnob (const juce::String& id)
{
    for (auto& k : knobs) if (k->paramId == id) return k.get();
    return nullptr;
}

void ModuleCard::setHighlightedParams (const juce::StringArray& ids)
{
    for (auto& k : knobs) k->setHighlighted (ids.contains (k->paramId));
}

void ModuleCard::refreshPowerState()
{
    const bool on = powerId.isEmpty() || power.getToggleState();
    for (auto& k : knobs) k->setDimmed (! on);
    repaint();
}

void ModuleCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::bg1);
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (colours::lineSoft);
    g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.0f);
    const bool on = powerId.isEmpty() || power.getToggleState();
    g.setColour (on ? accent : colours::textFaint);
    g.fillRoundedRectangle (juce::Rectangle<float> (12.0f, 12.0f, 3.0f, 12.0f), 1.5f);
    g.setFont (font (11.5f, 3));
    g.setColour (on ? colours::text : colours::textDim);
    g.drawText (title.toUpperCase(), juce::Rectangle<float> (20.0f, 8.0f, r.getWidth() - 70.0f, 20.0f), juce::Justification::centredLeft);
}

void ModuleCard::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    auto header = r.removeFromTop (24);
    power.setBounds (header.removeFromRight (34));
    if (! toggles.empty())
    {
        auto bottom = r.removeFromBottom (22);
        const int tw = bottom.getWidth() / (int) toggles.size();
        for (auto& t : toggles) t->setBounds (bottom.removeFromLeft (tw).reduced (2, 0));
    }
    const int rows = ((int) knobs.size() + columns - 1) / juce::jmax (1, columns);
    const int cellW = r.getWidth() / juce::jmax (1, columns);
    const int cellH = r.getHeight() / juce::jmax (1, rows);
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int row = (int) i / columns, col = (int) i % columns;
        knobs[i]->setBounds (r.getX() + col * cellW, r.getY() + row * cellH, cellW, cellH);
    }
}

} // namespace sx::ui
