#pragma once

#include <JuceHeader.h>
#include "dsp/Presets.h"

/** Factory + user preset handling. Factory presets come from dsp/Presets.h; user presets are
    XML files (*.vspreset) stored in <user app data>/VocalSweet/Presets. All methods are
    message-thread only (never called from the audio callback). */
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& state);

    int getNumPresets() const { return (int) factory.size() + userFiles.size(); }
    int getNumFactoryPresets() const { return (int) factory.size(); }
    juce::String getPresetName (int index) const;
    juce::StringArray getAllPresetNames() const;

    /** Applies the preset to all parameters (notifying the host, so it is undoable/automatable). */
    bool loadPreset (int index);
    /** Saves the current parameter values as a user preset. Returns the new preset index or -1. */
    int saveUserPreset (const juce::String& name);
    bool deleteUserPreset (int index);

    int getCurrentIndex() const { return currentIndex; }
    juce::String getCurrentName() const { return currentName; }
    void setCurrentFromState (const juce::String& name);

    void rescanUserPresets();
    static juce::File getUserPresetDirectory();

    /** Applies a full set of plain values to the parameters. */
    void applyValues (const vs::ParamValues& values);

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::vector<vs::Preset> factory;
    juce::Array<juce::File> userFiles;
    int currentIndex = 0;
    juce::String currentName { "Default" };
};
