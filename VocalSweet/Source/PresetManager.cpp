#include "PresetManager.h"

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state)
    : apvts (state), factory (vs::factoryPresets())
{
    rescanUserPresets();
}

juce::File PresetManager::getUserPresetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("VocalSweet").getChildFile ("Presets");
}

void PresetManager::rescanUserPresets()
{
    userFiles.clear();
    auto dir = getUserPresetDirectory();
    if (dir.isDirectory())
    {
        userFiles = dir.findChildFiles (juce::File::findFiles, false, "*.vspreset");
        userFiles.sort();
    }
}

juce::String PresetManager::getPresetName (int index) const
{
    if (juce::isPositiveAndBelow (index, (int) factory.size()))
        return factory[(size_t) index].name;
    const int u = index - (int) factory.size();
    if (juce::isPositiveAndBelow (u, userFiles.size()))
        return userFiles[u].getFileNameWithoutExtension();
    return {};
}

juce::StringArray PresetManager::getAllPresetNames() const
{
    juce::StringArray names;
    for (int i = 0; i < getNumPresets(); ++i) names.add (getPresetName (i));
    return names;
}

void PresetManager::applyValues (const vs::ParamValues& values)
{
    for (int i = 0; i < vs::kNumParams; ++i)
    {
        if (i == vs::pBypass) continue;   // loading a preset never toggles bypass
        if (auto* p = apvts.getParameter (vs::kParamDefs[(size_t) i].id))
        {
            const float norm = p->convertTo0to1 (vs::clampParam (i, values[(size_t) i]));
            if (std::abs (p->getValue() - norm) > 1.0e-7f)
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (norm);
                p->endChangeGesture();
            }
        }
    }
}

bool PresetManager::loadPreset (int index)
{
    if (juce::isPositiveAndBelow (index, (int) factory.size()))
    {
        applyValues (factory[(size_t) index].values);
    }
    else
    {
        const int u = index - (int) factory.size();
        if (! juce::isPositiveAndBelow (u, userFiles.size())) return false;

        auto xml = juce::XmlDocument::parse (userFiles[u]);
        if (xml == nullptr || ! xml->hasTagName ("VocalSweetPreset")) return false;

        auto values = vs::defaultParamValues();
        for (int i = 0; i < vs::kNumParams; ++i)
        {
            const char* id = vs::kParamDefs[(size_t) i].id;
            if (xml->hasAttribute (id))
                values[(size_t) i] = vs::clampParam (i, (float) xml->getDoubleAttribute (id));
        }
        applyValues (values);
    }

    currentIndex = index;
    currentName = getPresetName (index);
    apvts.state.setProperty ("presetName", currentName, nullptr);
    return true;
}

int PresetManager::saveUserPreset (const juce::String& rawName)
{
    auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return -1;

    auto dir = getUserPresetDirectory();
    if (! dir.createDirectory()) return -1;

    juce::XmlElement xml ("VocalSweetPreset");
    xml.setAttribute ("name", name);
    xml.setAttribute ("version", 1);
    for (int i = 0; i < vs::kNumParams; ++i)
    {
        if (i == vs::pBypass) continue;
        const char* id = vs::kParamDefs[(size_t) i].id;
        if (auto* v = apvts.getRawParameterValue (id))
            xml.setAttribute (id, (double) v->load());
    }

    auto file = dir.getChildFile (name + ".vspreset");
    if (! xml.writeTo (file)) return -1;

    rescanUserPresets();
    const int idx = (int) factory.size() + userFiles.indexOf (file);
    currentIndex = idx;
    currentName = name;
    apvts.state.setProperty ("presetName", currentName, nullptr);
    return idx;
}

bool PresetManager::deleteUserPreset (int index)
{
    const int u = index - (int) factory.size();
    if (! juce::isPositiveAndBelow (u, userFiles.size())) return false;
    const bool ok = userFiles[u].deleteFile();
    rescanUserPresets();
    return ok;
}

void PresetManager::setCurrentFromState (const juce::String& name)
{
    currentName = name.isNotEmpty() ? name : juce::String ("Default");
    currentIndex = juce::jmax (0, getAllPresetNames().indexOf (currentName));
}
