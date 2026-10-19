#pragma once

#include "PluginProcessor.h"
#include "ui/Widgets.h"

class VocalSweetAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int designWidth = 1080, designHeight = 700;

    explicit VocalSweetAudioProcessorEditor (VocalSweetAudioProcessor&);
    ~VocalSweetAudioProcessorEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    /** Switch between the MAIN (macro) and ADVANCED pages. */
    void showAdvanced (bool advanced);
    bool isShowingAdvanced() const { return advancedShown; }
    /** Updates meters/analyzer immediately (used by the screenshot tool). */
    void refreshNow() { timerCallback(); }
    juce::ComboBox& getPresetBox() { return content.presetBox; }

private:
    void timerCallback() override;

    struct Content : public juce::Component
    {
        explicit Content (VocalSweetAudioProcessor& p);
        void paint (juce::Graphics&) override;
        void resized() override;
        void rebuildPresetMenu();

        VocalSweetAudioProcessor& proc;
        juce::AudioProcessorValueTreeState& apvts;

        juce::ComboBox presetBox;
        juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" };
        juce::TextButton mainTab { "MAIN" }, advTab { "ADVANCED" }, bypassButton { "BYPASS" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

        vs::ui::LevelMeter inMeter { "INPUT" }, outMeter { "OUTPUT" };
        vs::ui::EqDisplay eqDisplay;
        vs::ui::GainReductionDisplay grDisplay;

        vs::ui::Knob inputKnob, mixKnob, outputKnob, ceilingKnob;
        vs::ui::Knob sweetenKnob, toneKnob, warmthKnob, presenceKnob, controlKnob;
        juce::ToggleButton limiterToggle { "LIMIT" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> limiterAttachment;

        vs::ui::ModulePanel hpfPanel, eqLowPanel, eqHighPanel, compPanel, deessPanel, satPanel, excitePanel, doublePanel;

        juce::String footerText;
        bool advanced = false;
        std::unique_ptr<juce::AlertWindow> saveDialog;
    };

    vs::ui::VocalSweetLookAndFeel lnf;
    VocalSweetAudioProcessor& processor;
    Content content;
    juce::TooltipWindow tooltips { this, 600 };
    bool advancedShown = false;
    int lastPresetIndex = -1;
    juce::String lastPresetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalSweetAudioProcessorEditor)
};
