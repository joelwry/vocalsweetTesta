#pragma once

#include "Widgets.h"

namespace sx::ui
{
/** The full channel strip: EQ/spectrum display plus every module's controls. */
class ChannelPage : public juce::Component
{
public:
    explicit ChannelPage (SwitaxVoiceAudioProcessor& p);
    void tick() { eq.tick(); }
    void resized() override;
    /** Highlights the controls a recommendation changes (empty = none). */
    void highlight (const juce::StringArray& paramIds);

    EqDisplay eq;
    ModuleCard hpf, eqCard, comp, deess, sat, excite, dbl, output;
};

} // namespace sx::ui
