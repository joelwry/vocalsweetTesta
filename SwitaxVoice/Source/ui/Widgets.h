#pragma once

#include "LookAndFeel.h"
#include "../AnalysisEngine.h"

class SwitaxVoiceAudioProcessor;

namespace sx::ui
{
//==============================================================================
/** Rotary control bound to a parameter, with its name above and live value below. */
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& title,
          juce::Colour accent = colours::gold, bool bipolar = false, bool big = false);

    void paint (juce::Graphics&) override;
    void resized() override;
    void setDimmed (bool d);
    void setHighlighted (bool h) { if (h != highlighted) { highlighted = h; repaint(); } }

    juce::Slider slider;
    const juce::String paramId;

private:
    juce::RangedAudioParameter* param = nullptr;
    juce::String title;
    juce::Colour accent;
    bool big = false, dimmed = false, highlighted = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
/** Compact stereo peak/RMS bar meter with peak hold and clip indicator. */
class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (juce::String label) : name (std::move (label)) {}
    void setLevels (float peakL, float peakR, float rmsL, float rmsR);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { clipped = false; holdDb[0] = holdDb[1] = -90.0f; repaint(); }

private:
    juce::String name;
    float peakDb[2] { -90.0f, -90.0f }, rmsDb[2] { -90.0f, -90.0f }, holdDb[2] { -90.0f, -90.0f };
    int holdTicks[2] {};
    bool clipped = false;
};

//==============================================================================
/** BS.1770 loudness readouts, true peak, gain-reduction bars and phase correlation. */
class LoudnessPanel : public juce::Component
{
public:
    void update (const AnalysisEngine::LiveMeters& m, float compGr, float deessGr, float limGr);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onReset) onReset(); }
    std::function<void()> onReset;

private:
    AnalysisEngine::LiveMeters meters;
    float gr[3] {};
};

//==============================================================================
/** Channel-strip EQ display: the static response of the chain's filters (computed from the
    same values the DSP uses) over live input/output spectra. */
class EqDisplay : public juce::Component
{
public:
    explicit EqDisplay (SwitaxVoiceAudioProcessor& p);
    void tick();
    void paint (juce::Graphics&) override;
    void resized() override { curveDirty = true; }

private:
    juce::Rectangle<float> plot() const;
    float xFor (float f) const;
    float yFor (float db) const;

    SwitaxVoiceAudioProcessor& proc;
    sx::ParamValues last {};
    double lastRate = 0.0;
    bool curveDirty = true, lastAudition = false;
    juce::Path curve;
    std::array<float, AnalysisEngine::kDisplayPoints> specIn {}, specOut {};
};

//==============================================================================
/** Titled card holding a module's power switch and its controls (grid of knobs). */
class ModuleCard : public juce::Component
{
public:
    ModuleCard (juce::AudioProcessorValueTreeState& state, juce::String title, const juce::String& powerParamId,
                int columns, juce::Colour accent);

    Knob& addKnob (const juce::String& paramId, const juce::String& name, bool bipolar = false);
    juce::ToggleButton& addToggle (const juce::String& paramId, const juce::String& text);
    void refreshPowerState();
    Knob* findKnob (const juce::String& paramId);
    void setHighlightedParams (const juce::StringArray& ids);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String title, powerId;
    juce::Colour accent;
    int columns;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> toggleAttachments;
};

/** Small coloured label chip ("MEASURED", "HEURISTIC", ...). Returns its width. */
float drawChip (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, juce::Colour c, bool filled = false);

} // namespace sx::ui
