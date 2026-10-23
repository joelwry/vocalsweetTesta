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

    // Window sizing. The editor is a scaled copy of a 1180 x 760 design.
    static constexpr double minScale = 0.5, maxScale = 2.0;
    /** Largest scale that still fits the given screen work area (never below minScale). */
    static double maxScaleFor (juce::Rectangle<int> userArea);
    /** Default scale: the design size, shrunk so window + host title bar fit the screen. */
    static double fitScale (juce::Rectangle<int> userArea);
    /** Work area of the smallest display, so the window fits wherever the host opens it. */
    static juce::Rectangle<int> smallestUserArea();
    /** Resizes the window to the given scale (clamped to [minScale, what fits the screen]). */
    void setScale (double scale);
    double getScale() const { return (double) getWidth() / (double) designWidth; }

    enum class Page { Assist, Channel };
    void showPage (Page p);
    Page getPage() const { return page; }
    void refreshNow() { timerCallback(); }

    sx::ui::AssistPage& getAssistPage() { return content.assist; }
    sx::ui::ChannelPage& getChannelPage() { return content.channel; }
    juce::ComboBox& getPresetBox() { return content.presetBox; }
    juce::Component& getRightHandleForTest() { return rightHandle; }
    juce::Component& getBottomHandleForTest() { return bottomHandle; }
    juce::Component& getCornerHandleForTest() { return cornerHandle; }
    juce::ComboBox& getSizeBoxForTest() { return content.sizeBox; }

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
        juce::ComboBox sizeBox;
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

    /** Invisible drag handle on the right edge, bottom edge or bottom-right corner. */
    struct EdgeHandle : public juce::Component
    {
        enum class Kind { Right, Bottom, Corner };
        EdgeHandle (SwitaxVoiceAudioProcessorEditor& o, Kind k);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        SwitaxVoiceAudioProcessorEditor& owner;
        Kind kind;
        int startW = 0, startH = 0;
    };

    sx::ui::SwitaxLookAndFeel lnf;
    SwitaxVoiceAudioProcessor& processor;
    Content content;
    juce::TooltipWindow tooltips { this, 600 };
    EdgeHandle rightHandle { *this, EdgeHandle::Kind::Right }, bottomHandle { *this, EdgeHandle::Kind::Bottom },
               cornerHandle { *this, EdgeHandle::Kind::Corner };
    Page page = Page::Assist;
    int lastPresetIndex = -1;
    juce::String lastPresetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwitaxVoiceAudioProcessorEditor)
};
