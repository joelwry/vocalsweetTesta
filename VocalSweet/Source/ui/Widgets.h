#pragma once

#include "LookAndFeel.h"
#include "../dsp/VocalChain.h"

class VocalSweetAudioProcessor;

namespace vs::ui
{
//==============================================================================
/** Rotary control bound to a parameter, with its name and live value text. */
class Knob : public juce::Component
{
public:
    enum class Style { Hero, Macro, Small };

    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& title,
          Style style = Style::Small, juce::Colour accent = colours::honey, bool bipolar = false);

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void setDimmed (bool d) { if (d != dimmed) { dimmed = d; slider.setAlpha (d ? 0.4f : 1.0f); repaint(); } }

    juce::Slider slider;

private:
    juce::RangedAudioParameter* param = nullptr;
    juce::String title;
    Style style;
    bool dimmed = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
/** Stereo peak/RMS meter with peak hold and clip indicator. */
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
/** Real-time EQ response curve (computed from the same effective parameters the DSP uses)
    over an FFT spectrum of the plugin's output. */
class EqDisplay : public juce::Component
{
public:
    explicit EqDisplay (VocalSweetAudioProcessor& p);
    void tick();
    void paint (juce::Graphics&) override;
    void resized() override { curveDirty = true; }

private:
    float xForFreq (float f) const;
    float yForDb (float db) const;
    juce::Rectangle<float> plotArea() const;

    VocalSweetAudioProcessor& proc;
    vs::EffectiveParams eff;
    vs::ParamValues lastValues {};
    double lastRate = 0.0;
    bool curveDirty = true;
    juce::Path curve;

    static constexpr int fftOrder = 12, fftSize = 1 << fftOrder, numBands = 220;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> ring = std::vector<float> ((size_t) fftSize, 0.0f), fftData = std::vector<float> ((size_t) fftSize * 2, 0.0f),
                       pullBuf = std::vector<float> (16384, 0.0f);
    int ringPos = 0;
    std::array<float, numBands> spectrum {};
};

//==============================================================================
/** Scrolling gain-reduction history for compressor, de-esser and limiter. */
class GainReductionDisplay : public juce::Component
{
public:
    void push (float compDb, float deessDb, float limDb);
    void paint (juce::Graphics&) override;

private:
    static constexpr int historySize = 180;
    std::array<float, historySize> comp {}, deess {}, lim {};
    int writePos = 0;
    float compNow = 0, deessNow = 0, limNow = 0;
};

//==============================================================================
/** Titled card holding a module's power switch and its detailed controls. */
class ModulePanel : public juce::Component
{
public:
    ModulePanel (juce::AudioProcessorValueTreeState& state, juce::String title, const juce::String& powerParamId,
                 int columns, juce::Colour accent = colours::honey);

    Knob& addKnob (const juce::String& paramId, const juce::String& name, bool bipolar = false);
    juce::ToggleButton& addToggle (const juce::String& paramId, const juce::String& text);
    void refreshPowerState();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String title;
    juce::Colour accent;
    int columns;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> toggleAttachments;
};

} // namespace vs::ui
