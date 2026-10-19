#include "Widgets.h"
#include "../PluginProcessor.h"

namespace vs::ui
{
//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& t,
            Style s, juce::Colour accent, bool bipolar)
    : title (t), style (s)
{
    param = state.getParameter (paramId);
    jassert (param != nullptr);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (style == Style::Hero ? 320 : 220);
    slider.setVelocityBasedMode (false);
    slider.getProperties().set ("hero", style == Style::Hero);
    slider.getProperties().set ("bipolar", bipolar);
    slider.getProperties().set ("accent", (juce::int64) accent.getARGB());
    slider.setPopupMenuEnabled (false);
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
    slider.setTooltip (param->getName (64) + "  (double-click to reset)");
}

void Knob::resized()
{
    auto r = getLocalBounds();
    switch (style)
    {
        case Style::Hero:  slider.setBounds (r.removeFromTop (r.getHeight() - 30)); break;
        case Style::Macro: r.removeFromTop (20); slider.setBounds (r.removeFromTop (r.getHeight() - 20)); break;
        case Style::Small:
        {
            r.removeFromTop (15);
            auto area = r.removeFromTop (r.getHeight() - 15);
            const int d = juce::jmin (area.getWidth(), area.getHeight(), 66);
            slider.setBounds (area.withSizeKeepingCentre (d, d));
            break;
        }
    }
}

void Knob::paint (juce::Graphics& g)
{
    const auto valueText = param->getCurrentValueAsText();
    const auto textCol = dimmed ? colours::textFaint : colours::text;
    auto r = getLocalBounds();

    if (style == Style::Hero)
    {
        g.setFont (font (15.0f, 2));
        g.setColour (colours::honey);
        g.drawText (title.toUpperCase(), r.removeFromBottom (26), juce::Justification::centred);
        return;
    }

    // title directly above and value directly below the knob itself
    const bool macro = style == Style::Macro;
    const int th = macro ? 20 : 15;
    const auto sb = slider.getBounds();
    g.setFont (font (macro ? 13.5f : 11.0f, 1));
    g.setColour (dimmed ? colours::textFaint : (macro ? colours::text : colours::textDim));
    g.drawText (macro ? title.toUpperCase() : title, juce::Rectangle<int> (0, juce::jmax (0, sb.getY() - th), r.getWidth(), th),
                juce::Justification::centred);
    g.setFont (font (macro ? 12.5f : 10.5f));
    g.setColour (dimmed ? colours::textFaint : colours::textDim.brighter (0.2f));
    g.drawText (valueText, juce::Rectangle<int> (0, juce::jmin (r.getBottom() - th, sb.getBottom()), r.getWidth(), th),
                juce::Justification::centred);
}

void Knob::paintOverChildren (juce::Graphics& g)
{
    if (style != Style::Hero) return;
    // value readout inside the hero knob, below the pointer hub
    auto knobArea = slider.getBounds().toFloat();
    const auto c = knobArea.getCentre();
    g.setFont (font (knobArea.getHeight() * 0.13f, 2));
    g.setColour (dimmed ? colours::textFaint : colours::text);
    g.drawText (param->getCurrentValueAsText(), juce::Rectangle<float> (knobArea.getWidth() * 0.5f, knobArea.getHeight() * 0.16f)
                    .withCentre ({ c.x, c.y + knobArea.getHeight() * 0.17f }), juce::Justification::centred);
}

//==============================================================================
void LevelMeter::setLevels (float pL, float pR, float rL, float rR)
{
    const float in[2] = { pL, pR }, rm[2] = { rL, rR };
    for (int c = 0; c < 2; ++c)
    {
        const float pk = juce::Decibels::gainToDecibels (in[c], -90.0f);
        const float rs = juce::Decibels::gainToDecibels (rm[c], -90.0f);
        peakDb[c] = pk > peakDb[c] ? pk : juce::jmax (pk, peakDb[c] - 1.6f);   // ~48 dB/s fall
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
    g.setColour (colours::bg1);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (colours::lineSoft);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    auto top = r.removeFromTop (44.0f);
    g.setFont (font (11.0f, 1));
    g.setColour (colours::textDim);
    g.drawText (name, top.removeFromTop (22.0f).translated (0, 4), juce::Justification::centred);
    const float maxHold = juce::jmax (holdDb[0], holdDb[1]);
    g.setFont (font (12.5f, 1));
    g.setColour (clipped ? colours::meterHigh : colours::text);
    g.drawText (maxHold <= -89.0f ? juce::String ("-inf") : juce::String (maxHold, 1), top, juce::Justification::centred);

    auto bars = r.reduced (14.0f, 10.0f).withTrimmedBottom (4.0f);
    auto scale = bars.removeFromRight (24.0f);
    const float minDb = -60.0f;
    auto yFor = [&] (float db) { return juce::jmap (juce::jlimit (minDb, 6.0f, db), minDb, 6.0f, bars.getBottom(), bars.getY()); };

    // scale
    g.setFont (font (9.5f));
    for (float db : { 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f, -60.0f })
    {
        const float y = yFor (db);
        g.setColour (db == 0.0f ? colours::textDim : colours::textFaint);
        g.drawText (db > 0 ? "+" + juce::String ((int) db) : juce::String ((int) db),
                    scale.withY (y - 6.0f).withHeight (12.0f), juce::Justification::centredRight);
        g.setColour (colours::lineSoft);
        g.drawHorizontalLine ((int) y, bars.getX(), bars.getRight());
    }

    const float barW = (bars.getWidth() - 6.0f) * 0.5f;
    for (int c = 0; c < 2; ++c)
    {
        auto bar = juce::Rectangle<float> (bars.getX() + (float) c * (barW + 6.0f), bars.getY(), barW, bars.getHeight());
        g.setColour (colours::bg0);
        g.fillRoundedRectangle (bar, 3.0f);

        juce::ColourGradient grad (colours::meterLow, 0.0f, bar.getBottom(), colours::meterHigh, 0.0f, bar.getY(), false);
        grad.addColour (juce::jmap (-18.0f, minDb, 6.0f, 0.0f, 1.0f), colours::meterLow);
        grad.addColour (juce::jmap (-6.0f, minDb, 6.0f, 0.0f, 1.0f), colours::meterMid);
        grad.addColour (juce::jmap (0.0f, minDb, 6.0f, 0.0f, 1.0f), colours::meterHigh);

        // peak (dim) and RMS (bright) bars
        auto peakBar = bar.withTop (yFor (peakDb[c]));
        g.setGradientFill (grad);
        g.setOpacity (0.35f);
        g.fillRoundedRectangle (peakBar, 3.0f);
        g.setOpacity (1.0f);
        g.fillRoundedRectangle (bar.withTop (yFor (rmsDb[c])), 3.0f);

        if (holdDb[c] > minDb)
        {
            g.setColour (holdDb[c] > -0.1f ? colours::meterHigh : colours::text.withAlpha (0.85f));
            g.fillRect (bar.withY (yFor (holdDb[c]) - 1.0f).withHeight (2.0f));
        }
    }
}

//==============================================================================
EqDisplay::EqDisplay (VocalSweetAudioProcessor& p) : proc (p)
{
    spectrum.fill (-100.0f);
}

juce::Rectangle<float> EqDisplay::plotArea() const
{
    return getLocalBounds().toFloat().reduced (14.0f, 12.0f).withTrimmedTop (18.0f).withTrimmedBottom (14.0f).withTrimmedLeft (18.0f);
}

float EqDisplay::xForFreq (float f) const
{
    auto a = plotArea();
    return a.getX() + a.getWidth() * (std::log (f / 20.0f) / std::log (1000.0f));
}

float EqDisplay::yForDb (float db) const
{
    auto a = plotArea();
    return juce::jmap (juce::jlimit (-18.0f, 18.0f, db), -18.0f, 18.0f, a.getBottom(), a.getY());
}

void EqDisplay::tick()
{
    // --- EQ curve from the same effective parameters the DSP is smoothing towards
    const auto values = proc.getParamValues();
    const double rate = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
    if (values != lastValues || rate != lastRate || curveDirty)
    {
        lastValues = values;
        lastRate = rate;
        eff = vs::computeEffective (values);
        curve.clear();
        auto a = plotArea();
        const int pts = juce::jmax (64, (int) a.getWidth());
        for (int i = 0; i <= pts; ++i)
        {
            const float x = a.getX() + a.getWidth() * (float) i / (float) pts;
            const float f = 20.0f * std::pow (1000.0f, (float) i / (float) pts);
            const float y = yForDb ((float) vs::dsp::VocalChain::responseDb (eff, rate, f));
            i == 0 ? curve.startNewSubPath (x, y) : curve.lineTo (x, y);
        }
        curveDirty = false;
    }

    // --- spectrum analyzer
    const int got = proc.analyzerFifo.pull (pullBuf.data(), (int) pullBuf.size());
    for (int i = 0; i < got; ++i)
    {
        ring[(size_t) ringPos] = pullBuf[(size_t) i];
        ringPos = (ringPos + 1) % fftSize;
    }

    std::fill (fftData.begin(), fftData.end(), 0.0f);
    for (int i = 0; i < fftSize; ++i) fftData[(size_t) i] = ring[(size_t) ((ringPos + i) % fftSize)];
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const float binHz = (float) rate / (float) fftSize;
    const float norm = 4.0f / (float) fftSize;  // Hann coherent gain compensation (~0 dBFS sine -> 0 dB)
    for (int b = 0; b < numBands; ++b)
    {
        const float f0 = 20.0f * std::pow (1000.0f, (float) b / numBands);
        const float f1 = 20.0f * std::pow (1000.0f, (float) (b + 1) / numBands);
        int k0 = juce::jlimit (1, fftSize / 2 - 1, (int) (f0 / binHz));
        int k1 = juce::jlimit (k0, fftSize / 2 - 1, (int) (f1 / binHz));
        float m = 0.0f;
        for (int k = k0; k <= k1; ++k) m = juce::jmax (m, fftData[(size_t) k]);
        const float db = juce::Decibels::gainToDecibels (m * norm, -100.0f);
        spectrum[(size_t) b] = db > spectrum[(size_t) b] ? db : juce::jmax (db, spectrum[(size_t) b] - 2.5f);
    }
    repaint();
}

void EqDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::bg1);
    g.fillRoundedRectangle (bounds, 12.0f);
    g.setColour (colours::lineSoft);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 12.0f, 1.0f);

    g.setFont (font (11.0f, 1));
    g.setColour (colours::textDim);
    g.drawText ("RESPONSE", bounds.reduced (16.0f, 10.0f).removeFromTop (14.0f), juce::Justification::topLeft);
    g.setColour (colours::textFaint);
    g.drawText ("EQ + TONE  |  OUTPUT SPECTRUM", bounds.reduced (16.0f, 10.0f).removeFromTop (14.0f), juce::Justification::topRight);

    auto a = plotArea();
    g.saveState();
    g.reduceClipRegion (a.toNearestInt().expanded (1));

    // grid
    g.setFont (font (9.5f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        g.setColour (colours::lineSoft);
        g.drawVerticalLine ((int) xForFreq (f), a.getY(), a.getBottom());
    }
    for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
    {
        g.setColour (db == 0.0f ? colours::line : colours::lineSoft);
        g.drawHorizontalLine ((int) yForDb (db), a.getX(), a.getRight());
    }

    // spectrum (-90..-6 dBFS mapped to the plot height)
    {
        juce::Path sp;
        sp.startNewSubPath (a.getX(), a.getBottom());
        for (int b = 0; b < numBands; ++b)
        {
            const float f = 20.0f * std::pow (1000.0f, ((float) b + 0.5f) / numBands);
            const float y = juce::jmap (juce::jlimit (-90.0f, -6.0f, spectrum[(size_t) b]), -90.0f, -6.0f, a.getBottom(), a.getY() + a.getHeight() * 0.1f);
            sp.lineTo (xForFreq (f), y);
        }
        sp.lineTo (a.getRight(), a.getBottom());
        sp.closeSubPath();
        g.setGradientFill (juce::ColourGradient (colours::violet.withAlpha (0.28f), 0, a.getY(),
                                                 colours::violet.withAlpha (0.04f), 0, a.getBottom(), false));
        g.fillPath (sp);
    }

    // EQ curve with fill to 0 dB
    {
        juce::Path fill (curve);
        fill.lineTo (a.getRight(), yForDb (0.0f));
        fill.lineTo (a.getX(), yForDb (0.0f));
        fill.closeSubPath();
        g.setColour (colours::mint.withAlpha (0.10f));
        g.fillPath (fill);
        g.setColour (colours::mint.withAlpha (0.25f));
        g.strokePath (curve, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colours::mint);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // band markers
    const double rate = lastRate > 0 ? lastRate : 48000.0;
    struct Mark { float f; const char* label; bool on; };
    const Mark marks[] = { { eff.hpfFreq, "HPF", eff.hpfOn }, { eff.mudFreq, "MUD", eff.eqOn }, { eff.boxFreq, "BOX", eff.eqOn },
                           { eff.presFreq, "PRES", eff.eqOn }, { eff.airFreq, "AIR", eff.eqOn } };
    for (const auto& m : marks)
    {
        if (! m.on) continue;
        const float x = xForFreq (m.f);
        const float y = yForDb ((float) vs::dsp::VocalChain::responseDb (eff, rate, m.f));
        g.setColour (colours::bg0);
        g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre ({ x, y }));
        g.setColour (colours::mint);
        g.drawEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ x, y }), 2.0f);
        g.setFont (font (9.5f, 1));
        g.setColour (colours::text.withAlpha (0.75f));
        g.drawText (m.label, juce::Rectangle<float> (40.0f, 12.0f).withCentre ({ x, y - 14.0f }), juce::Justification::centred);
    }

    // de-esser band indicator
    if (eff.deessOn)
    {
        const float x0 = xForFreq (eff.deessFreq * 0.75f), x1 = xForFreq (juce::jmin (19000.0f, eff.deessFreq * 2.2f));
        g.setColour (colours::rose.withAlpha (0.10f));
        g.fillRect (juce::Rectangle<float> (x0, a.getBottom() - 6.0f, x1 - x0, 6.0f));
    }
    g.restoreState();

    // axis labels
    g.setFont (font (9.5f));
    g.setColour (colours::textFaint);
    for (auto [f, s] : { std::pair { 50.0f, "50" }, { 100.0f, "100" }, { 200.0f, "200" }, { 500.0f, "500" },
                         { 1000.0f, "1k" }, { 2000.0f, "2k" }, { 5000.0f, "5k" }, { 10000.0f, "10k" } })
        g.drawText (s, juce::Rectangle<float> (30.0f, 12.0f).withCentre ({ xForFreq (f), a.getBottom() + 9.0f }), juce::Justification::centred);
    for (float db : { -12.0f, 0.0f, 12.0f })
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (a.getX() - 22.0f, yForDb (db) - 6.0f, 18.0f, 12.0f),
                    juce::Justification::centredRight);
}

//==============================================================================
void GainReductionDisplay::push (float c, float d, float l)
{
    comp[(size_t) writePos] = c;
    deess[(size_t) writePos] = d;
    lim[(size_t) writePos] = l;
    writePos = (writePos + 1) % historySize;
    compNow = c > compNow ? c : compNow + 0.25f * (c - compNow);
    deessNow = d > deessNow ? d : deessNow + 0.25f * (d - deessNow);
    limNow = l > limNow ? l : limNow + 0.25f * (l - limNow);
    repaint();
}

void GainReductionDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::bg1);
    g.fillRoundedRectangle (bounds, 12.0f);
    g.setColour (colours::lineSoft);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 12.0f, 1.0f);

    auto r = bounds.reduced (16.0f, 10.0f);
    g.setFont (font (11.0f, 1));
    g.setColour (colours::textDim);
    g.drawText ("GAIN REDUCTION", r.removeFromTop (14.0f), juce::Justification::topLeft);

    auto readouts = r.removeFromBottom (54.0f);
    r.removeFromBottom (6.0f);
    auto plot = r.reduced (0.0f, 6.0f).withTrimmedLeft (20.0f);
    const float maxDb = 18.0f;
    auto yFor = [&] (float db) { return plot.getY() + plot.getHeight() * juce::jlimit (0.0f, 1.0f, db / maxDb); };

    g.setFont (font (9.5f));
    for (float db : { 0.0f, 6.0f, 12.0f, 18.0f })
    {
        g.setColour (colours::lineSoft);
        g.drawHorizontalLine ((int) yFor (db), plot.getX(), plot.getRight());
        g.setColour (colours::textFaint);
        g.drawText (db == 0.0f ? "0" : "-" + juce::String ((int) db), juce::Rectangle<float> (plot.getX() - 22.0f, yFor (db) - 6.0f, 18.0f, 12.0f),
                    juce::Justification::centredRight);
    }

    auto historyPath = [&] (const std::array<float, historySize>& h, bool closed)
    {
        juce::Path p;
        for (int i = 0; i < historySize; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) (historySize - 1);
            const float y = yFor (h[(size_t) ((writePos + i) % historySize)]);
            if (i == 0) { closed ? (p.startNewSubPath (x, plot.getY()), p.lineTo (x, y)) : p.startNewSubPath (x, y); }
            else p.lineTo (x, y);
        }
        if (closed) { p.lineTo (plot.getRight(), plot.getY()); p.closeSubPath(); }
        return p;
    };

    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());
    g.setGradientFill (juce::ColourGradient (colours::honey.withAlpha (0.55f), 0, plot.getY(), colours::honey.withAlpha (0.1f), 0, plot.getBottom(), false));
    g.fillPath (historyPath (comp, true));
    g.setColour (colours::honey);
    g.strokePath (historyPath (comp, false), juce::PathStrokeType (1.5f));
    g.setColour (colours::rose);
    g.strokePath (historyPath (deess, false), juce::PathStrokeType (1.5f));
    g.setColour (colours::violet);
    g.strokePath (historyPath (lim, false), juce::PathStrokeType (1.2f));
    g.restoreState();

    // readouts
    const float w = readouts.getWidth() / 3.0f;
    struct R { const char* label; float v; juce::Colour c; };
    const R rs[] = { { "COMP", compNow, colours::honey }, { "DE-ESS", deessNow, colours::rose }, { "LIMIT", limNow, colours::violet } };
    for (int i = 0; i < 3; ++i)
    {
        auto cell = readouts.withWidth (w).withX (readouts.getX() + w * (float) i).reduced (3.0f, 2.0f);
        g.setColour (colours::bg0.withAlpha (0.7f));
        g.fillRoundedRectangle (cell, 6.0f);
        g.setColour (rs[i].c);
        g.fillRoundedRectangle (cell.withWidth (3.0f).reduced (0.0f, 8.0f), 1.5f);
        auto c2 = cell.reduced (10.0f, 5.0f);
        g.setFont (font (9.5f, 1));
        g.setColour (colours::textDim);
        g.drawText (rs[i].label, c2.removeFromTop (14.0f), juce::Justification::centredLeft);
        g.setFont (font (14.0f, 1));
        g.setColour (colours::text);
        g.drawText (juce::String (rs[i].v < 0.05f ? 0.0f : -rs[i].v, 1) + " dB", c2, juce::Justification::centredLeft);
    }
}

//==============================================================================
ModulePanel::ModulePanel (juce::AudioProcessorValueTreeState& state, juce::String t, const juce::String& powerParamId,
                          int cols, juce::Colour acc)
    : apvts (state), title (std::move (t)), accent (acc), columns (cols)
{
    addAndMakeVisible (power);
    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, powerParamId, power);
    power.onStateChange = [this] { refreshPowerState(); };
    power.setTooltip ("Enable / disable " + title);
}

Knob& ModulePanel::addKnob (const juce::String& paramId, const juce::String& name, bool bipolar)
{
    knobs.push_back (std::make_unique<Knob> (apvts, paramId, name, Knob::Style::Small, accent, bipolar));
    addAndMakeVisible (*knobs.back());
    return *knobs.back();
}

juce::ToggleButton& ModulePanel::addToggle (const juce::String& paramId, const juce::String& text)
{
    toggles.push_back (std::make_unique<juce::ToggleButton> (text));
    addAndMakeVisible (*toggles.back());
    toggleAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, paramId, *toggles.back()));
    return *toggles.back();
}

void ModulePanel::refreshPowerState()
{
    const bool on = power.getToggleState();
    for (auto& k : knobs) k->setDimmed (! on);
    repaint();
}

void ModulePanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::bg1);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (colours::lineSoft);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    const bool on = power.getToggleState();
    g.setColour (on ? accent : colours::textFaint);
    g.fillRoundedRectangle (juce::Rectangle<float> (12.0f, 13.0f, 3.0f, 12.0f), 1.5f);
    g.setFont (font (11.5f, 2));
    g.setColour (on ? colours::text : colours::textDim);
    g.drawText (title.toUpperCase(), juce::Rectangle<float> (21.0f, 8.0f, r.getWidth() - 70.0f, 22.0f), juce::Justification::centredLeft);
}

void ModulePanel::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    auto header = r.removeFromTop (26);
    power.setBounds (header.removeFromRight (40).reduced (0, 2));

    if (! toggles.empty())
    {
        auto row = r.removeFromBottom (22);
        const int w = row.getWidth() / (int) toggles.size();
        for (auto& t : toggles) t->setBounds (row.removeFromLeft (w).reduced (4, 0));
    }

    const int n = (int) knobs.size();
    if (n == 0) return;
    const int cols = juce::jmax (1, juce::jmin (columns, n));
    const int rows = (n + cols - 1) / cols;
    const int cw = r.getWidth() / cols, ch = r.getHeight() / rows;
    for (int i = 0; i < n; ++i)
    {
        const int cx = i % cols, cy = i / cols;
        knobs[(size_t) i]->setBounds (r.getX() + cx * cw, r.getY() + cy * ch, cw, ch);
    }
    refreshPowerState();
}

} // namespace vs::ui
