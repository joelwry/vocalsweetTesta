#include "PluginEditor.h"

using namespace vs::ui;

//==============================================================================
VocalSweetAudioProcessorEditor::Content::Content (VocalSweetAudioProcessor& p)
    : proc (p), apvts (p.apvts), eqDisplay (p),
      inputKnob (apvts, "inputGain", "Input", Knob::Style::Small, colours::mint, true),
      mixKnob (apvts, "mix", "Dry / Wet", Knob::Style::Small, colours::honey),
      outputKnob (apvts, "outputGain", "Output", Knob::Style::Small, colours::mint, true),
      ceilingKnob (apvts, "ceiling", "Ceiling", Knob::Style::Small, colours::violet),
      sweetenKnob (apvts, "sweeten", "Sweeten", Knob::Style::Hero, colours::honey),
      toneKnob (apvts, "tone", "Tone", Knob::Style::Macro, colours::mint, true),
      warmthKnob (apvts, "warmth", "Warmth", Knob::Style::Macro, colours::honey),
      presenceKnob (apvts, "presence", "Presence", Knob::Style::Macro, colours::rose),
      controlKnob (apvts, "control", "Control", Knob::Style::Macro, colours::violet),
      hpfPanel (apvts, "High-Pass", "hpfOn", 1, colours::mint),
      eqLowPanel (apvts, "EQ  Clean-up", "eqOn", 2, colours::mint),
      eqHighPanel (apvts, "EQ  Presence / Air", "eqOn", 2, colours::mint),
      compPanel (apvts, "Compressor", "compOn", 3, colours::honey),
      deessPanel (apvts, "De-Esser", "deessOn", 3, colours::rose),
      satPanel (apvts, "Saturate", "satOn", 1, colours::honey),
      excitePanel (apvts, "Exciter", "exciteOn", 2, colours::rose),
      doublePanel (apvts, "Doubler", "doubleOn", 2, colours::violet)
{
    // header
    addAndMakeVisible (presetBox);
    presetBox.setJustificationType (juce::Justification::centredLeft);
    presetBox.setTooltip ("Choose a starting-point preset");
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != proc.presets.getCurrentIndex())
        {
            proc.presets.loadPreset (idx);
            proc.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withProgramChanged (true));
        }
    };
    rebuildPresetMenu();

    auto step = [this] (int delta)
    {
        const int n = proc.presets.getNumPresets();
        const int idx = (proc.presets.getCurrentIndex() + delta + n) % n;
        proc.presets.loadPreset (idx);
        presetBox.setSelectedId (idx + 1, juce::dontSendNotification);
        proc.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withProgramChanged (true));
    };
    prevButton.onClick = [step] { step (-1); };
    nextButton.onClick = [step] { step (+1); };
    prevButton.setTooltip ("Previous preset");
    nextButton.setTooltip ("Next preset");

    saveButton.setTooltip ("Save the current settings as a user preset");
    saveButton.onClick = [this]
    {
        saveDialog = std::make_unique<juce::AlertWindow> ("Save Preset", "Name for the new user preset:", juce::MessageBoxIconType::NoIcon, this);
        saveDialog->addTextEditor ("name", proc.presets.getCurrentName() + " (My)", {});
        saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
        {
            if (result == 1 && saveDialog != nullptr)
            {
                const int idx = proc.presets.saveUserPreset (saveDialog->getTextEditorContents ("name"));
                rebuildPresetMenu();
                if (idx >= 0) presetBox.setSelectedId (idx + 1, juce::dontSendNotification);
            }
            saveDialog.reset();
        }));
    };

    for (auto* b : { &mainTab, &advTab })
    {
        b->getProperties().set ("tab", true);
        b->setClickingTogglesState (false);
        addAndMakeVisible (*b);
    }
    mainTab.setToggleState (true, juce::dontSendNotification);

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass all processing (latency compensated, click-free)");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "bypass", bypassButton);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &prevButton, &nextButton, &saveButton, &bypassButton, &inMeter, &outMeter, &eqDisplay, &grDisplay,
             &inputKnob, &mixKnob, &outputKnob, &ceilingKnob, &limiterToggle,
             &sweetenKnob, &toneKnob, &warmthKnob, &presenceKnob, &controlKnob })
        addAndMakeVisible (c);

    limiterToggle.setTooltip ("Output limiter / clip protection");
    limiterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "limiterOn", limiterToggle);

    // advanced panels
    hpfPanel.addKnob ("hpfFreq", "Freq");
    hpfPanel.addToggle ("hpfSlope", "24 dB");
    eqLowPanel.addKnob ("mudFreq", "Mud Freq");
    eqLowPanel.addKnob ("mudGain", "Mud", true);
    eqLowPanel.addKnob ("boxFreq", "Box Freq");
    eqLowPanel.addKnob ("boxGain", "Box", true);
    eqHighPanel.addKnob ("presFreq", "Pres Freq");
    eqHighPanel.addKnob ("presGain", "Presence", true);
    eqHighPanel.addKnob ("airFreq", "Air Freq");
    eqHighPanel.addKnob ("airGain", "Air", true);
    compPanel.addKnob ("compThresh", "Threshold");
    compPanel.addKnob ("compRatio", "Ratio");
    compPanel.addKnob ("compMakeup", "Makeup");
    compPanel.addKnob ("compAttack", "Attack");
    compPanel.addKnob ("compRelease", "Release");
    deessPanel.addKnob ("deessFreq", "Freq");
    deessPanel.addKnob ("deessThresh", "Threshold");
    deessPanel.addKnob ("deessRange", "Range");
    satPanel.addKnob ("satDrive", "Drive");
    excitePanel.addKnob ("exciteFreq", "Freq");
    excitePanel.addKnob ("exciteAmount", "Amount");
    doublePanel.addKnob ("doubleAmount", "Amount");
    doublePanel.addKnob ("doubleWidth", "Width");
    for (auto* panel : { &hpfPanel, &eqLowPanel, &eqHighPanel, &compPanel, &deessPanel, &satPanel, &excitePanel, &doublePanel })
        addChildComponent (panel);
}

void VocalSweetAudioProcessorEditor::Content::rebuildPresetMenu()
{
    proc.presets.rescanUserPresets();
    presetBox.clear (juce::dontSendNotification);
    const int nf = proc.presets.getNumFactoryPresets();
    presetBox.addSectionHeading ("Factory");
    for (int i = 0; i < nf; ++i) presetBox.addItem (proc.presets.getPresetName (i), i + 1);
    if (proc.presets.getNumPresets() > nf)
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("User");
        for (int i = nf; i < proc.presets.getNumPresets(); ++i) presetBox.addItem (proc.presets.getPresetName (i), i + 1);
    }
    presetBox.setSelectedId (proc.presets.getCurrentIndex() + 1, juce::dontSendNotification);
}

void VocalSweetAudioProcessorEditor::Content::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (colours::bg0.brighter (0.03f), 0, 0, colours::bg0.darker (0.3f), 0, r.getHeight(), false));
    g.fillAll();

    // header
    auto header = r.removeFromTop (64.0f);
    g.setColour (colours::bg1.withAlpha (0.9f));
    g.fillRect (header);
    g.setColour (colours::lineSoft);
    g.drawHorizontalLine ((int) header.getBottom(), 0.0f, header.getRight());

    // logo mark: rounded square with a stylised waveform
    auto mark = juce::Rectangle<float> (22.0f, 14.0f, 36.0f, 36.0f);
    g.setGradientFill (accentGradient (mark.getTopLeft(), mark.getBottomRight()));
    g.fillRoundedRectangle (mark, 10.0f);
    juce::Path wave;
    for (int i = 0; i <= 40; ++i)
    {
        const float t = (float) i / 40.0f;
        const float x = mark.getX() + 6.0f + t * (mark.getWidth() - 12.0f);
        const float env = std::sin (t * juce::MathConstants<float>::pi);
        const float y = mark.getCentreY() - env * 9.0f * std::sin (t * juce::MathConstants<float>::twoPi * 2.5f);
        i == 0 ? wave.startNewSubPath (x, y) : wave.lineTo (x, y);
    }
    g.setColour (colours::bg0);
    g.strokePath (wave, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setFont (font (25.0f, 1));
    g.setColour (colours::text);
    g.drawText ("Vocal", juce::Rectangle<float> (68.0f, 12.0f, 80.0f, 30.0f), juce::Justification::centredLeft);
    const float vocalW = juce::GlyphArrangement::getStringWidth (font (25.0f, 1), "Vocal");
    juce::GlyphArrangement ga;
    ga.addLineOfText (font (25.0f, 2), "Sweet", 68.0f + vocalW + 1.0f, 36.0f);
    juce::Path sweetPath;
    ga.createPath (sweetPath);
    g.setGradientFill (accentGradient ({ 68.0f + vocalW, 14.0f }, { 68.0f + vocalW + 80.0f, 40.0f }));
    g.fillPath (sweetPath);
    g.setFont (font (9.5f, 1));
    g.setColour (colours::textFaint);
    g.drawText ("VOCAL ENHANCER", juce::Rectangle<float> (69.0f, 39.0f, 160.0f, 14.0f), juce::Justification::centredLeft);

    // tab background
    g.setColour (colours::bg0);
    g.fillRoundedRectangle (mainTab.getBounds().getUnion (advTab.getBounds()).toFloat().expanded (3.0f), 8.0f);

    // macro card (main page)
    if (! advanced)
    {
        auto card = juce::Rectangle<float> (140.0f, 314.0f, 800.0f, 342.0f);
        g.setColour (colours::bg1);
        g.fillRoundedRectangle (card, 14.0f);
        g.setColour (colours::lineSoft);
        g.drawRoundedRectangle (card.reduced (0.5f), 14.0f, 1.0f);

        const auto c = sweetenKnob.getBounds().toFloat().getCentre().translated (0.0f, -14.0f);
        juce::ColourGradient glow (colours::rose.withAlpha (0.10f), c, colours::rose.withAlpha (0.0f), c.translated (190.0f, 0.0f), true);
        g.setGradientFill (glow);
        g.fillEllipse (juce::Rectangle<float> (380.0f, 380.0f).withCentre (c));

        g.setFont (font (11.0f, 1));
        g.setColour (colours::textDim);
        g.drawText ("MACROS", card.reduced (18.0f, 12.0f).removeFromTop (14.0f), juce::Justification::topLeft);
        g.setFont (font (11.0f));
        g.setColour (colours::textFaint);
        g.drawText ("Sweeten coordinates clean-up EQ, compression, de-essing, harmonics and air. Double-click any knob to reset.",
                    card.reduced (18.0f, 10.0f).removeFromBottom (16.0f), juce::Justification::centred);
    }

    // footer
    auto footer = getLocalBounds().toFloat().removeFromBottom (36.0f);
    g.setColour (colours::bg1.withAlpha (0.8f));
    g.fillRect (footer);
    g.setColour (colours::lineSoft);
    g.drawHorizontalLine ((int) footer.getY(), 0.0f, footer.getRight());
    g.setFont (font (11.0f));
    g.setColour (colours::textFaint);
    g.drawText (footerText, footer.reduced (22.0f, 0.0f), juce::Justification::centredLeft);
    const juce::String format = proc.wrapperType == juce::AudioProcessor::wrapperType_Undefined
                                    ? juce::String ("Preview")
                                    : juce::String (juce::AudioProcessor::getWrapperTypeDescription (proc.wrapperType));
    g.drawText ("VocalSweet " JucePlugin_VersionString "  |  " + format, footer.reduced (22.0f, 0.0f), juce::Justification::centredRight);

    // column labels under meters
    g.setFont (font (10.0f, 1));
    g.setColour (colours::textFaint);
}

void VocalSweetAudioProcessorEditor::Content::resized()
{
    // header
    presetBox.setBounds (400, 15, 280, 34);
    prevButton.setBounds (362, 15, 34, 34);
    nextButton.setBounds (684, 15, 34, 34);
    saveButton.setBounds (724, 15, 62, 34);
    mainTab.setBounds (820, 17, 72, 30);
    advTab.setBounds (894, 17, 86, 30);
    bypassButton.setBounds (990, 15, 72, 34);

    // side columns
    inMeter.setBounds (20, 80, 104, 340);
    outMeter.setBounds (956, 80, 104, 340);
    inputKnob.setBounds (22, 428, 100, 100);
    mixKnob.setBounds (22, 540, 100, 100);
    outputKnob.setBounds (958, 428, 100, 100);
    ceilingKnob.setBounds (958, 532, 100, 96);
    limiterToggle.setBounds (966, 632, 84, 22);

    // centre
    const int topH = advanced ? 150 : 222;
    eqDisplay.setBounds (140, 80, 530, topH);
    grDisplay.setBounds (682, 80, 258, topH);

    sweetenKnob.setBounds (405, 330, 270, 300);
    toneKnob.setBounds (170, 400, 118, 156);
    warmthKnob.setBounds (296, 400, 118, 156);
    presenceKnob.setBounds (666, 400, 118, 156);
    controlKnob.setBounds (792, 400, 118, 156);

    const int y1 = 80 + topH + 12;
    const int rowH = (656 - y1 - 10) / 2;
    const int y2 = y1 + rowH + 10;
    auto place = [] (std::initializer_list<std::pair<juce::Component*, int>> items, int y, int h)
    {
        int x = 140;
        for (auto& [c, w] : items) { c->setBounds (x, y, w, h); x += w + 10; }
    };
    place ({ { &hpfPanel, 130 }, { &eqLowPanel, 190 }, { &eqHighPanel, 190 }, { &compPanel, 260 } }, y1, rowH);
    place ({ { &deessPanel, 250 }, { &satPanel, 130 }, { &excitePanel, 190 }, { &doublePanel, 200 } }, y2, rowH);

    for (auto* c : std::initializer_list<juce::Component*> { &sweetenKnob, &toneKnob, &warmthKnob, &presenceKnob, &controlKnob })
        c->setVisible (! advanced);
    for (auto* panel : { &hpfPanel, &eqLowPanel, &eqHighPanel, &compPanel, &deessPanel, &satPanel, &excitePanel, &doublePanel })
        panel->setVisible (advanced);
}

//==============================================================================
VocalSweetAudioProcessorEditor::VocalSweetAudioProcessorEditor (VocalSweetAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), content (p)
{
    setLookAndFeel (&lnf);
    content.setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.setBounds (0, 0, designWidth, designHeight);

    content.mainTab.onClick = [this] { showAdvanced (false); };
    content.advTab.onClick = [this] { showAdvanced (true); };

    setResizable (true, true);
    setResizeLimits (designWidth * 3 / 4, designHeight * 3 / 4, designWidth * 2, designHeight * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) designWidth / (double) designHeight);
    setSize (designWidth, designHeight);
    startTimerHz (30);
}

VocalSweetAudioProcessorEditor::~VocalSweetAudioProcessorEditor()
{
    stopTimer();
    content.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void VocalSweetAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) designWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
}

void VocalSweetAudioProcessorEditor::showAdvanced (bool adv)
{
    advancedShown = adv;
    content.advanced = adv;
    content.mainTab.setToggleState (! adv, juce::dontSendNotification);
    content.advTab.setToggleState (adv, juce::dontSendNotification);
    content.resized();
    content.repaint();
}

void VocalSweetAudioProcessorEditor::timerCallback()
{
    auto& p = processor;
    content.inMeter.setLevels (p.inPeak[0].read(), p.inPeak[1].read(), p.inRms[0].load(), p.inRms[1].load());
    content.outMeter.setLevels (p.outPeak[0].read(), p.outPeak[1].read(), p.outRms[0].load(), p.outRms[1].load());
    content.grDisplay.push (p.compGr.read(), p.deessGr.read(), p.limGr.read());
    content.eqDisplay.tick();

    const int idx = p.presets.getCurrentIndex();
    const auto name = p.presets.getCurrentName();
    if (idx != lastPresetIndex || name != lastPresetName || content.presetBox.getNumItems() != p.presets.getNumPresets())
    {
        if (content.presetBox.getNumItems() != p.presets.getNumPresets()) content.rebuildPresetMenu();
        content.presetBox.setSelectedId (idx + 1, juce::dontSendNotification);
        lastPresetIndex = idx;
        lastPresetName = name;
    }

    const double sr = p.getSampleRate();
    const int lat = p.getLatencySamples();
    juce::String footer = sr > 0 ? juce::String (sr / 1000.0, 1) + " kHz" : juce::String ("-- kHz");
    footer << "   |   Latency " << lat << " smp" << (sr > 0 ? " (" + juce::String (1000.0 * lat / sr, 2) + " ms)" : juce::String());
    footer << "   |   " << (p.getTotalNumInputChannels() == 1 ? "Mono" : "Stereo");
    if (footer != content.footerText)
    {
        content.footerText = footer;
        content.repaint (0, designHeight - 36, designWidth, 36);
    }
}
