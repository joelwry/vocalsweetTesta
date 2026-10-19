#include "ChannelPage.h"
#include "../PluginProcessor.h"

namespace sx::ui
{
ChannelPage::ChannelPage (SwitaxVoiceAudioProcessor& p)
    : eq (p),
      hpf (p.apvts, "High-Pass", "hpfOn", 1, colours::cyan),
      eqCard (p.apvts, "Tone EQ", "eqOn", 5, colours::gold),
      comp (p.apvts, "Compressor", "compOn", 3, colours::coral),
      deess (p.apvts, "De-Esser", "deessOn", 3, colours::violet),
      sat (p.apvts, "Saturate", "satOn", 1, colours::coral),
      excite (p.apvts, "Exciter", "exciteOn", 2, colours::gold),
      dbl (p.apvts, "Doubler", "doubleOn", 2, colours::cyan),
      output (p.apvts, "Output", "", 4, colours::emerald)
{
    hpf.addKnob ("hpfFreq", "Freq");
    hpf.addToggle ("hpfSlope", "24 dB");
    // EQ grid: frequencies on the first row, gains on the second
    eqCard.addKnob ("lowFreq", "Low-mid Hz");
    eqCard.addKnob ("boxFreq", "Box Hz");
    eqCard.addKnob ("harshFreq", "Harsh Hz");
    eqCard.addKnob ("presFreq", "Presence Hz");
    eqCard.addKnob ("airFreq", "Air Hz");
    eqCard.addKnob ("lowGain", "Low-mid", true);
    eqCard.addKnob ("boxGain", "Box", true);
    eqCard.addKnob ("harshGain", "Harsh", true);
    eqCard.addKnob ("presGain", "Presence", true);
    eqCard.addKnob ("airGain", "Air", true);
    comp.addKnob ("compThresh", "Threshold");
    comp.addKnob ("compRatio", "Ratio");
    comp.addKnob ("compMakeup", "Makeup");
    comp.addKnob ("compAttack", "Attack");
    comp.addKnob ("compRelease", "Release");
    comp.addToggle ("compAuto", "Auto makeup");
    deess.addKnob ("deessFreq", "Freq");
    deess.addKnob ("deessThresh", "Threshold");
    deess.addKnob ("deessRange", "Range");
    sat.addKnob ("satDrive", "Drive");
    excite.addKnob ("exciteFreq", "Freq");
    excite.addKnob ("exciteAmount", "Amount");
    dbl.addKnob ("doubleAmount", "Amount");
    dbl.addKnob ("doubleWidth", "Width");
    output.addKnob ("inputGain", "Input", true);
    output.addKnob ("outputGain", "Output", true);
    output.addKnob ("mix", "Dry/Wet");
    output.addKnob ("ceiling", "Ceiling");
    output.addToggle ("limiterOn", "Limiter");

    for (juce::Component* c : std::initializer_list<juce::Component*> { &eq, &hpf, &eqCard, &comp, &deess, &sat, &excite, &dbl, &output })
        addAndMakeVisible (c);
}

void ChannelPage::resized()
{
    auto r = getLocalBounds();
    eq.setBounds (r.removeFromTop (226));
    r.removeFromTop (10);
    const int gap = 10;
    const int rowH = (r.getHeight() - gap) / 2;
    auto row1 = r.removeFromTop (rowH);
    r.removeFromTop (gap);
    auto row2 = r;
    auto place = [gap] (juce::Rectangle<int>& row, juce::Component& c, int w) { c.setBounds (row.removeFromLeft (w)); row.removeFromLeft (gap); };
    place (row1, hpf, 104);
    place (row1, eqCard, 400);
    place (row1, comp, row1.getWidth());
    place (row2, deess, 236);
    place (row2, sat, 104);
    place (row2, excite, 168);
    place (row2, dbl, 168);
    place (row2, output, row2.getWidth());
}

void ChannelPage::highlight (const juce::StringArray& ids)
{
    for (auto* m : { &hpf, &eqCard, &comp, &deess, &sat, &excite, &dbl, &output })
        m->setHighlightedParams (ids);
}

} // namespace sx::ui
