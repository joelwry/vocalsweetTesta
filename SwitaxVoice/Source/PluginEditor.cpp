#include "PluginEditor.h"

using namespace sx::ui;

//==============================================================================
SwitaxVoiceAudioProcessorEditor::Content::Content (SwitaxVoiceAudioProcessor& p)
    : proc (p), assist (p), channel (p)
{
    addAndMakeVisible (presetBox);
    presetBox.setTooltip ("Factory and user presets");
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

    for (auto* b : { &assistTab, &channelTab })
    {
        b->getProperties().set ("tab", true);
        b->getProperties().set ("accent", (juce::int64) colours::gold.getARGB());
        addAndMakeVisible (*b);
    }
    assistTab.setToggleState (true, juce::dontSendNotification);

    compareButton.getProperties().set ("accent", (juce::int64) colours::cyan.getARGB());
    compareButton.setClickingTogglesState (true);
    compareButton.setTooltip ("A/B: hear the unprocessed input, level-matched to the processed output (measured loudness difference), so you judge the processing and not the volume");
    compareButton.onClick = [this] { proc.setCompare (compareButton.getToggleState(), compareGain); };

    bypassButton.getProperties().set ("accent", (juce::int64) colours::coral.getARGB());
    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass all processing (latency compensated, click-free)");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "bypass", bypassButton);

    for (juce::Component* c : std::initializer_list<juce::Component*> { &prevButton, &nextButton, &saveButton, &compareButton, &bypassButton,
                                                                         &inMeter, &outMeter, &loudness, &assist })
        addAndMakeVisible (c);
    addChildComponent (channel);
    loudness.onReset = [this] { proc.analysis.resetIntegrated(); };
    assist.onHighlight = [this] (const juce::StringArray& ids) { channel.highlight (ids); };
}

void SwitaxVoiceAudioProcessorEditor::Content::rebuildPresetMenu()
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

void SwitaxVoiceAudioProcessorEditor::Content::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (colours::bg0.brighter (0.04f), 0, 0, colours::bg0.darker (0.25f), 0, r.getHeight(), false));
    g.fillAll();

    // header
    auto header = r.removeFromTop (62.0f);
    g.setColour (colours::bg1);
    g.fillRect (header);
    drawStripeBand (g, juce::Rectangle<float> (0.0f, header.getBottom(), r.getWidth(), 4.0f), 0.9f);
    drawLogoMark (g, juce::Rectangle<float> (18.0f, 12.0f, 38.0f, 38.0f));
    g.setFont (font (24.0f, 3));
    g.setColour (colours::text);
    g.drawText ("SWITAX", juce::Rectangle<float> (66.0f, 10.0f, 120.0f, 28.0f), juce::Justification::centredLeft);
    const float sw = juce::GlyphArrangement::getStringWidth (font (24.0f, 3), "SWITAX");
    g.setFont (font (24.0f, 2));
    g.setColour (colours::gold);
    g.drawText ("VOICE", juce::Rectangle<float> (66.0f + sw + 6.0f, 10.0f, 100.0f, 28.0f), juce::Justification::centredLeft);
    g.setFont (font (9.5f, 2));
    g.setColour (colours::textFaint.brighter (0.2f));
    g.drawText ("ANALYSE  .  EXPLAIN  .  SWEETEN", juce::Rectangle<float> (67.0f, 37.0f, 220.0f, 14.0f), juce::Justification::centredLeft);

    // tab background
    g.setColour (colours::bg0);
    g.fillRoundedRectangle (assistTab.getBounds().getUnion (channelTab.getBounds()).toFloat().expanded (3.0f), 8.0f);

    // right rail
    auto rail = juce::Rectangle<float> (1012.0f, 76.0f, 152.0f, (float) getHeight() - 76.0f - 44.0f);
    g.setColour (colours::bg1);
    g.fillRoundedRectangle (rail, 10.0f);
    g.setColour (colours::lineSoft);
    g.drawRoundedRectangle (rail.reduced (0.5f), 10.0f, 1.0f);

    // compare gain hint
    if (std::abs (compareGain) > 0.05f || compareButton.getToggleState())
    {
        g.setFont (font (9.5f, 1));
        g.setColour (colours::cyan);
        g.drawText ("match " + juce::String (compareGain, 1) + " dB", compareButton.getBounds().translated (0, 21).withHeight (12).toFloat(),
                    juce::Justification::centred);
    }

    // footer
    auto footer = getLocalBounds().toFloat().removeFromBottom (32.0f);
    g.setColour (colours::bg1.withAlpha (0.85f));
    g.fillRect (footer);
    g.setFont (font (10.5f));
    g.setColour (colours::textFaint.brighter (0.2f));
    g.drawText (footerText, footer.reduced (18.0f, 0.0f), juce::Justification::centredLeft);
    const juce::String format = proc.wrapperType == juce::AudioProcessor::wrapperType_Undefined
                                    ? juce::String ("Preview")
                                    : juce::String (juce::AudioProcessor::getWrapperTypeDescription (proc.wrapperType));
    g.drawText ("SwitaxVoice " JucePlugin_VersionString "  |  " + format + "  |  built on the VocalSweet engine", footer.reduced (18.0f, 0.0f), juce::Justification::centredRight);
}

void SwitaxVoiceAudioProcessorEditor::Content::resized()
{
    prevButton.setBounds (430, 15, 32, 32);
    presetBox.setBounds (466, 15, 230, 32);
    nextButton.setBounds (700, 15, 32, 32);
    saveButton.setBounds (738, 15, 58, 32);
    assistTab.setBounds (818, 16, 80, 30);
    channelTab.setBounds (900, 16, 86, 30);
    compareButton.setBounds (1004, 15, 72, 22);
    bypassButton.setBounds (1086, 15, 76, 32);

    auto page = juce::Rectangle<int> (16, 76, 984, getHeight() - 76 - 44);
    assist.setBounds (page);
    channel.setBounds (page);

    inMeter.setBounds (1022, 86, 62, 210);
    outMeter.setBounds (1092, 86, 62, 210);
    loudness.setBounds (1024, 304, 130, getHeight() - 304 - 52);
}

//==============================================================================
SwitaxVoiceAudioProcessorEditor::SwitaxVoiceAudioProcessorEditor (SwitaxVoiceAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), content (p)
{
    setLookAndFeel (&lnf);
    content.setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.setBounds (0, 0, designWidth, designHeight);
    content.assistTab.onClick = [this] { showPage (Page::Assist); };
    content.channelTab.onClick = [this] { showPage (Page::Channel); };

    setResizable (true, true);
    setResizeLimits (designWidth * 2 / 3, designHeight * 2 / 3, designWidth * 2, designHeight * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) designWidth / (double) designHeight);
    setSize (designWidth, designHeight);
    startTimerHz (30);
}

SwitaxVoiceAudioProcessorEditor::~SwitaxVoiceAudioProcessorEditor()
{
    stopTimer();
    processor.stopAudition();
    processor.setCompare (false, 0.0f);
    content.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void SwitaxVoiceAudioProcessorEditor::resized()
{
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) designWidth));
}

void SwitaxVoiceAudioProcessorEditor::showPage (Page p)
{
    page = p;
    content.assistTab.setToggleState (p == Page::Assist, juce::dontSendNotification);
    content.channelTab.setToggleState (p == Page::Channel, juce::dontSendNotification);
    content.assist.setVisible (p == Page::Assist);
    content.channel.setVisible (p == Page::Channel);
    content.repaint();
}

void SwitaxVoiceAudioProcessorEditor::timerCallback()
{
    auto& p = processor;
    content.inMeter.setLevels (p.inPeak[0].read(), p.inPeak[1].read(), p.inRms[0].load(), p.inRms[1].load());
    content.outMeter.setLevels (p.outPeak[0].read(), p.outPeak[1].read(), p.outRms[0].load(), p.outRms[1].load());
    const auto live = p.analysis.getLiveMeters();
    content.loudness.update (live, p.compGr.read(), p.deessGr.read(), p.limGr.read());

    // level-matched compare gain: processed minus input loudness (short-term, else integrated)
    float match = 0.0f;
    if (! p.isComparing())
    {
        if (live.outIntegrated > -70 && live.inIntegrated > -70) match = (float) (live.outIntegrated - live.inIntegrated);
        else if (live.outShortTerm > -70 && live.inShortTerm > -70) match = (float) (live.outShortTerm - live.inShortTerm);
        match = juce::jlimit (-24.0f, 24.0f, match);
        if (std::abs (match - content.compareGain) > 0.05f) { content.compareGain = match; content.repaint (content.compareButton.getBounds().expanded (10, 20)); }
    }

    if (page == Page::Assist) content.assist.tick();
    else content.channel.tick();

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
    footer << "   |   " << (p.getMainBusNumInputChannels() == 1 ? "Mono" : "Stereo");
    if (p.isAuditioning()) footer << "   |   AUDITIONING (parameters unchanged)";
    if (p.isComparing()) footer << "   |   A/B: unprocessed input, level-matched";
    if (live.droppedFrames > 0) footer << "   |   analyser skipped " << (int) live.droppedFrames << " samples";
    if (footer != content.footerText)
    {
        content.footerText = footer;
        content.repaint (0, designHeight - 32, designWidth, 32);
    }
}
