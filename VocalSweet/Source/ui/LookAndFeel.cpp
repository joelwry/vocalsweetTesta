#include "LookAndFeel.h"
#include "BinaryData.h"

namespace vs::ui
{
juce::Typeface::Ptr getTypeface (int weight)
{
    static juce::Typeface::Ptr regular  = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf,  BinaryData::InterRegular_ttfSize);
    static juce::Typeface::Ptr semibold = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize);
    static juce::Typeface::Ptr bold     = juce::Typeface::createSystemTypefaceFor (BinaryData::InterBold_ttf,     BinaryData::InterBold_ttfSize);
    return weight >= 2 ? bold : (weight == 1 ? semibold : regular);
}

juce::Font font (float height, int weight)
{
    return juce::Font (juce::FontOptions (getTypeface (weight)).withHeight (height));
}

VocalSweetLookAndFeel::VocalSweetLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::bg0);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::backgroundColourId, colours::bg2);
    setColour (juce::ComboBox::outlineColourId, colours::line);
    setColour (juce::ComboBox::arrowColourId, colours::textDim);
    setColour (juce::PopupMenu::backgroundColourId, colours::bg2);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::honey.withAlpha (0.18f));
    setColour (juce::PopupMenu::highlightedTextColourId, colours::text);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, colours::bg0);
    setColour (juce::TextEditor::backgroundColourId, colours::bg2);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextEditor::outlineColourId, colours::line);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::honey);
    setColour (juce::AlertWindow::backgroundColourId, colours::bg1);
    setColour (juce::AlertWindow::textColourId, colours::text);
    setColour (juce::AlertWindow::outlineColourId, colours::line);
}

juce::Typeface::Ptr VocalSweetLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    return getTypeface (f.isBold() ? 2 : 0);
}

//==============================================================================
void VocalSweetLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                              float startAngle, float endAngle, juce::Slider& s)
{
    const bool hero = s.getProperties().getWithDefault ("hero", false);
    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const auto accent = s.getProperties().contains ("accent")
                            ? juce::Colour ((juce::uint32) (juce::int64) s.getProperties()["accent"])
                            : colours::honey;

    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto area = bounds.withSizeKeepingCentre (size, size);
    const auto c = area.getCentre();
    const float radius = size * 0.5f;
    const float trackW = hero ? juce::jmax (6.0f, radius * 0.085f) : juce::jmax (3.0f, radius * 0.11f);
    const float arcR = radius - trackW * 0.5f - (hero ? 6.0f : 1.0f);
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool enabled = s.isEnabled();

    // hero: tick ring
    if (hero)
    {
        for (int i = 0; i <= 40; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * (float) i / 40.0f;
            const bool major = i % 5 == 0;
            const float r0 = radius - 1.0f, r1 = radius - (major ? 6.0f : 3.5f);
            const bool lit = (float) i / 40.0f <= pos + 1.0e-4f;
            g.setColour (lit ? accent.interpolatedWith (colours::rose, (float) i / 40.0f).withAlpha (0.9f)
                             : colours::line);
            g.drawLine (c.x + r0 * std::sin (a), c.y - r0 * std::cos (a),
                        c.x + r1 * std::sin (a), c.y - r1 * std::cos (a), major ? 1.6f : 1.0f);
        }
    }

    // track
    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::bg0.brighter (0.06f));
    g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        juce::ColourGradient grad (accent, c.translated (-radius, radius), hero ? colours::rose : accent.brighter (0.25f),
                                   c.translated (radius, -radius), false);
        if (! enabled) grad = juce::ColourGradient (colours::textFaint, c, colours::textFaint, c.translated (1, 1), false);

        if (hero && enabled)
        {
            // soft glow underneath
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (colours::rose.withAlpha (0.06f));
                g.strokePath (arc, juce::PathStrokeType (trackW + (float) i * 5.0f, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
            }
        }
        g.setGradientFill (grad);
        g.strokePath (arc, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // knob body
    const float bodyR = arcR - trackW * 0.5f - (hero ? radius * 0.07f : radius * 0.12f);
    auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (body.translated (0.0f, bodyR * 0.08f).expanded (bodyR * 0.04f));

    juce::ColourGradient bodyGrad (colours::bg3.brighter (0.18f), body.getTopLeft(),
                                   colours::bg1, body.getBottomRight(), false);
    g.setGradientFill (bodyGrad);
    g.fillEllipse (body);

    if (hero)
    {
        juce::ColourGradient inner (colours::bg3.brighter (0.05f), c.translated (0, -bodyR * 0.6f),
                                    colours::bg1.darker (0.2f), c.translated (0, bodyR), false);
        g.setGradientFill (inner);
        g.fillEllipse (body.reduced (bodyR * 0.12f));
    }

    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawEllipse (body.reduced (0.5f), 1.0f);

    // pointer
    const float p0 = bodyR * (hero ? 0.55f : 0.35f), p1 = bodyR * 0.88f;
    juce::Line<float> pointer (c.x + p0 * std::sin (angle), c.y - p0 * std::cos (angle),
                               c.x + p1 * std::sin (angle), c.y - p1 * std::cos (angle));
    g.setColour (enabled ? colours::text : colours::textFaint);
    g.drawLine (pointer, hero ? 3.5f : 2.2f);
}

//==============================================================================
void VocalSweetLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                                  bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const bool isTab = b.getProperties().getWithDefault ("tab", false);
    const float corner = 6.0f;

    if (isTab)
    {
        if (on)
        {
            g.setColour (colours::bg3);
            g.fillRoundedRectangle (r, corner);
        }
        else if (over)
        {
            g.setColour (colours::bg2.brighter (0.05f));
            g.fillRoundedRectangle (r, corner);
        }
        return;
    }

    if (on)
    {
        g.setGradientFill (accentGradient (r.getTopLeft(), r.getBottomRight()));
        g.fillRoundedRectangle (r, corner);
    }
    else
    {
        g.setColour (down ? colours::bg3.brighter (0.1f) : (over ? colours::bg3 : colours::bg2));
        g.fillRoundedRectangle (r, corner);
        g.setColour (colours::line);
        g.drawRoundedRectangle (r, corner, 1.0f);
    }
}

void VocalSweetLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool on = b.getToggleState();
    const bool isTab = b.getProperties().getWithDefault ("tab", false);
    g.setFont (font (isTab ? 12.5f : 13.0f, 1));
    g.setColour (isTab ? (on ? colours::text : colours::textDim) : (on ? colours::bg0 : colours::text));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void VocalSweetLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    // small pill switch used for module power buttons
    const bool on = b.getToggleState();
    auto r = b.getLocalBounds().toFloat();
    auto pill = r.withSizeKeepingCentre (juce::jmin (30.0f, r.getWidth()), juce::jmin (16.0f, r.getHeight()));
    pill.setX (r.getRight() - pill.getWidth());

    g.setColour (on ? colours::honey.withAlpha (0.22f) : colours::bg0);
    g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
    g.setColour (on ? colours::honey.withAlpha (0.8f) : (over ? colours::textDim : colours::line));
    g.drawRoundedRectangle (pill.reduced (0.5f), pill.getHeight() * 0.5f, 1.0f);

    const float d = pill.getHeight() - 6.0f;
    auto knob = juce::Rectangle<float> (d, d).withCentre ({ on ? pill.getRight() - 3.0f - d * 0.5f
                                                               : pill.getX() + 3.0f + d * 0.5f,
                                                            pill.getCentreY() });
    g.setColour (on ? colours::honey : colours::textFaint);
    g.fillEllipse (knob);

    if (b.getButtonText().isNotEmpty())
    {
        g.setFont (font (12.0f, 1));
        g.setColour (on ? colours::text : colours::textDim);
        g.drawText (b.getButtonText(), r.withRight (pill.getX() - 6.0f), juce::Justification::centredRight);
    }
}

//==============================================================================
void VocalSweetLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& cb)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (colours::bg2);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (cb.isMouseOver (true) ? colours::honey.withAlpha (0.6f) : colours::line);
    g.drawRoundedRectangle (r, 7.0f, 1.0f);

    // chevron
    const float cx = (float) w - 18.0f, cy = (float) h * 0.5f;
    juce::Path p;
    p.startNewSubPath (cx - 5.0f, cy - 2.5f);
    p.lineTo (cx, cy + 2.5f);
    p.lineTo (cx + 5.0f, cy - 2.5f);
    g.setColour (colours::textDim);
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void VocalSweetLookAndFeel::positionComboBoxText (juce::ComboBox& cb, juce::Label& label)
{
    label.setBounds (12, 1, cb.getWidth() - 40, cb.getHeight() - 2);
    label.setFont (getComboBoxFont (cb));
    label.setJustificationType (juce::Justification::centredLeft);
}

void VocalSweetLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (colours::bg2);
    g.setColour (colours::line);
    g.drawRect (0, 0, w, h);
}

void VocalSweetLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator,
                                               bool isActive, bool isHighlighted, bool isTicked, bool,
                                               const juce::String& text, const juce::String&, const juce::Drawable*,
                                               const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour (colours::line);
        g.fillRect (area.reduced (10, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    auto r = area.reduced (4, 1);
    if (isHighlighted && isActive)
    {
        g.setColour (colours::honey.withAlpha (0.16f));
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
    }
    if (isTicked)
    {
        g.setColour (colours::honey);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ (float) r.getX() + 12.0f, (float) r.getCentreY() }));
    }
    g.setColour (isActive ? colours::text : colours::textFaint);
    g.setFont (getPopupMenuFont());
    g.drawText (text, r.withTrimmedLeft (24), juce::Justification::centredLeft);
}

void VocalSweetLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area,
                                                        const juce::String& name)
{
    g.setFont (font (11.0f, 1));
    g.setColour (colours::honey.withAlpha (0.85f));
    g.drawText (name.toUpperCase(), area.reduced (12, 0), juce::Justification::bottomLeft);
}

} // namespace vs::ui
