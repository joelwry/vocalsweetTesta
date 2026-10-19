#pragma once

#include "PluginProcessor.h"
#include "ui/AssistPage.h"
#include "ui/ChannelPage.h"

class SwitaxVoiceAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int designWidth = 1180, designHeight = 760;

    explicit SwitaxVoiceAudioProcessorEditor (SwitaxVoiceAudioProcessor&);
    ~SwitaxVoiceAudioProcessorEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    enum class Page { Assist, Channel };
    void showPage (Page p);
    Page getPage() const { return page; }
    void refreshNow() { timerCallback(); }

    sx::ui::AssistPage& getAssistPage() { return content.assist; }
    sx::ui::ChannelPage& getChannelPage() { return content.channel; }
    juce::ComboBox& getPresetBox() { return content.presetBox; }

private:
    void timerCallback() override;

    struct Content : public juce::Component
    {
        explicit Content (SwitaxVoiceAudioProcessor& p);
        void paint (juce::Graphics&) override;
        void resized() override;
        void rebuildPresetMenu();

        SwitaxVoiceAudioProcessor& proc;
        juce::ComboBox presetBox;
        juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" };
        juce::TextButton assistTab { "ASSIST" }, channelTab { "CHANNEL" }, compareButton { "A/B" }, bypassButton { "BYPASS" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
        sx::ui::LevelMeter inMeter { "IN" }, outMeter { "OUT" };
        sx::ui::LoudnessPanel loudness;
        sx::ui::AssistPage assist;
        sx::ui::ChannelPage channel;
        juce::String footerText;
        float compareGain = 0.0f;
        std::unique_ptr<juce::AlertWindow> saveDialog;
    };

    sx::ui::SwitaxLookAndFeel lnf;
    SwitaxVoiceAudioProcessor& processor;
    Content content;
    juce::TooltipWindow tooltips { this, 600 };
    Page page = Page::Assist;
    int lastPresetIndex = -1;
    juce::String lastPresetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwitaxVoiceAudioProcessorEditor)
};
