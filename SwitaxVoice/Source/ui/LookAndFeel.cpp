#include "LookAndFeel.h"
#include "BinaryData.h"

namespace sx::ui
{
juce::Typeface::Ptr getTypeface (int style)
{
    static juce::Typeface::Ptr faces[4] = {
        juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize),
        juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize),
        juce::Typeface::createSystemTypefaceFor (BinaryData::SpaceGroteskMedium_ttf, BinaryData::SpaceGroteskMedium_ttfSize),
        juce::Typeface::createSystemTypefaceFor (BinaryData::SpaceGroteskBold_ttf, BinaryData::SpaceGroteskBold_ttfSize)
    };
    return faces[juce::jlimit (0, 3, style)];
}

juce::Font font (float height, int style)
{
    return juce::Font (juce::FontOptions (getTypeface (style)).withHeight (height));
}

void drawStripeBand (juce::Graphics& g, juce::Rectangle<float> r, float alpha)
{
    // repeating geometric blocks: gold / coral / emerald / ink, with small diamonds
    const juce::Colour cols[] = { colours::gold, colours::coral, colours::emerald, colours::cyan };
    const float w = r.getHeight() * 2.2f;
    int k = 0;
    for (float x = r.getX(); x < r.getRight(); x += w, ++k)
    {
        auto cell = juce::Rectangle<float> (x, r.getY(), juce::jmin (w, r.getRight() - x), r.getHeight());
        g.setColour (cols[k % 4].withAlpha (0.85f * alpha));
        g.fillRect (cell.removeFromLeft (cell.getWidth() * 0.55f));
        g.setColour (colours::bg0.withAlpha (alpha));
        g.fillRect (cell);
        const auto c = juce::Point<float> (x + w * 0.78f, r.getCentreY());
        juce::Path d;
        const float s = r.getHeight() * 0.32f;
        d.addQuadrilateral (c.x, c.y - s, c.x + s, c.y, c.x, c.y + s, c.x - s, c.y);
        g.setColour (cols[(k + 2) % 4].withAlpha (0.9f * alpha));
        g.fillPath (d);
    }
}

void drawLogoMark (juce::Graphics& g, juce::Rectangle<float> r)
{
    // diamond with a stylised voice wave crossing it
    const auto c = r.getCentre();
    const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    juce::Path diamond;
    diamond.addQuadrilateral (c.x, c.y - s, c.x + s, c.y, c.x, c.y + s, c.x - s, c.y);
    g.setGradientFill (accentGradient (r.getTopLeft(), r.getBottomRight()));
    g.fillPath (diamond);
    juce::Path inner;
    const float si = s * 0.62f;
    inner.addQuadrilateral (c.x, c.y - si, c.x + si, c.y, c.x, c.y + si, c.x - si, c.y);
    g.setColour (colours::bg0);
    g.fillPath (inner);
    juce::Path wave;
    for (int i = 0; i <= 48; ++i)
    {
        const float t = (float) i / 48.0f;
        const float x = c.x - s * 0.9f + t * s * 1.8f;
        const float env = std::sin (t * juce::MathConstants<float>::pi);
        const float y = c.y - env * s * 0.38f * std::sin (t * juce::MathConstants<float>::twoPi * 2.0f);
        i == 0 ? wave.startNewSubPath (x, y) : wave.lineTo (x, y);
    }
    g.setColour (colours::gold);
    g.strokePath (wave, juce::PathStrokeType (s * 0.11f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
SwitaxLookAndFeel::SwitaxLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::bg0);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::backgroundColourId, colours::bg2);
    setColour (juce::ComboBox::outlineColourId, colours::line);
    setColour (juce::ComboBox::arrowColourId, colours::textDim);
    setColour (juce::PopupMenu::backgroundColourId, colours::bg2);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::gold.withAlpha (0.18f));
    setColour (juce::PopupMenu::highlightedTextColourId, colours::text);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, colours::bg0);
    setColour (juce::TextEditor::backgroundColourId, colours::bg2);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextEditor::outlineColourId, colours::line);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::gold);
    setColour (juce::AlertWindow::backgroundColourId, colours::bg1);
    setColour (juce::AlertWindow::textColourId, colours::text);
    setColour (juce::AlertWindow::outlineColourId, colours::line);
    setColour (juce::ScrollBar::thumbColourId, colours::bg3);
    setColour (juce::TooltipWindow::backgroundColourId, colours::bg3);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, colours::line);
}

juce::Typeface::Ptr SwitaxLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    return getTypeface (f.isBold() ? 1 : 0);
}

//==============================================================================
void SwitaxLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, juce::Slider& s)
{
    const bool big = s.getProperties().getWithDefault ("big", false);
    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const auto accent = s.getProperties().contains ("accent")
                            ? juce::Colour ((juce::uint32) (juce::int64) s.getProperties()["accent"])
                            : colours::gold;
    const bool enabled = s.isEnabled() && ! (bool) s.getProperties().getWithDefault ("dimmed", false);

    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto c = bounds.getCentre();
    const float radius = size * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // segmented ring
    const int segments = big ? 36 : 24;
    const float ringR = radius - (big ? 5.0f : 3.0f);
    const float segLen = (endAngle - startAngle) / (float) segments;
    const float centrePos = bipolar ? 0.5f : 0.0f;
    for (int i = 0; i < segments; ++i)
    {
        const float a0 = startAngle + segLen * ((float) i + 0.12f);
        const float a1 = startAngle + segLen * ((float) i + 0.88f);
        const float mid = ((float) i + 0.5f) / (float) segments;
        const bool lit = bipolar ? (mid >= juce::jmin (pos, centrePos) && mid <= juce::jmax (pos, centrePos))
                                 : mid <= pos;
        juce::Path seg;
        seg.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, a0, a1, true);
        juce::Colour col = colours::bg3;
        if (lit && enabled) col = accent.interpolatedWith (colours::coral, big ? mid * 0.8f : 0.0f);
        else if (lit) col = colours::textFaint;
        g.setColour (col);
        g.strokePath (seg, juce::PathStrokeType (big ? 6.0f : juce::jmax (2.5f, radius * 0.1f), juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }

    // body
    const float bodyR = ringR - (big ? 13.0f : radius * 0.24f);
    auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (body.translated (0.0f, bodyR * 0.07f));
    g.setGradientFill (juce::ColourGradient (colours::bg3, body.getTopLeft(), colours::bg2.darker (0.2f), body.getBottomRight(), false));
    g.fillEllipse (body);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawEllipse (body.reduced (0.5f), 1.0f);

    // indicator dot on the body edge
    const float dotR = big ? 5.0f : juce::jmax (2.0f, bodyR * 0.14f);
    const float dr = bodyR - dotR * 2.0f;
    g.setColour (enabled ? accent : colours::textFaint);
    g.fillEllipse (juce::Rectangle<float> (dotR * 2.0f, dotR * 2.0f).withCentre ({ c.x + dr * std::sin (angle), c.y - dr * std::cos (angle) }));
}

void SwitaxLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                          juce::Slider::SliderStyle, juce::Slider& s)
{
    const auto accent = s.getProperties().contains ("accent")
                            ? juce::Colour ((juce::uint32) (juce::int64) s.getProperties()["accent"])
                            : colours::gold;
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    auto track = r.withSizeKeepingCentre (r.getWidth(), 4.0f);
    g.setColour (colours::bg3);
    g.fillRoundedRectangle (track, 2.0f);
    g.setColour (accent.withAlpha (s.isEnabled() ? 1.0f : 0.4f));
    g.fillRoundedRectangle (track.withRight (pos), 2.0f);
    g.setColour (colours::text);
    g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ pos, r.getCentreY() }));
}

//==============================================================================
void SwitaxLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const auto& props = b.getProperties();
    const bool isTab = props.getWithDefault ("tab", false);
    const bool primary = props.getWithDefault ("primary", false);
    const auto accent = props.contains ("accent") ? juce::Colour ((juce::uint32) (juce::int64) props["accent"]) : colours::gold;
    const float corner = props.getWithDefault ("round", false) ? r.getHeight() * 0.5f : 6.0f;
    const float alpha = b.isEnabled() ? 1.0f : 0.4f;

    if (isTab)
    {
        if (on)
        {
            g.setColour (colours::bg3);
            g.fillRoundedRectangle (r, corner);
            g.setColour (accent);
            g.fillRoundedRectangle (r.removeFromBottom (2.5f).reduced (r.getWidth() * 0.25f, 0.0f), 1.0f);
        }
        else if (over)
        {
            g.setColour (colours::bg2.brighter (0.05f));
            g.fillRoundedRectangle (r, corner);
        }
        return;
    }

    if (primary || on)
    {
        g.setColour ((down ? accent.darker (0.2f) : (over ? accent.brighter (0.1f) : accent)).withAlpha (alpha));
        g.fillRoundedRectangle (r, corner);
    }
    else
    {
        g.setColour ((down ? colours::bg3.brighter (0.1f) : (over ? colours::bg3 : colours::bg2)).withAlpha (alpha));
        g.fillRoundedRectangle (r, corner);
        g.setColour ((over ? accent.withAlpha (0.7f) : colours::line).withAlpha (alpha));
        g.drawRoundedRectangle (r, corner, 1.0f);
    }
}

void SwitaxLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool on = b.getToggleState();
    const auto& props = b.getProperties();
    const bool isTab = props.getWithDefault ("tab", false);
    const bool primary = props.getWithDefault ("primary", false);
    g.setFont (font (isTab ? 13.0f : 12.5f, 2));
    juce::Colour c = isTab ? (on ? colours::text : colours::textDim) : ((primary || on) ? colours::bg0 : colours::text);
    if (! b.isEnabled()) c = c.withAlpha (0.45f);
    g.setColour (c);
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void SwitaxLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    const bool on = b.getToggleState();
    auto r = b.getLocalBounds().toFloat();
    auto pill = r.withSizeKeepingCentre (juce::jmin (30.0f, r.getWidth()), juce::jmin (16.0f, r.getHeight()));
    pill.setX (r.getRight() - pill.getWidth());
    const auto accent = b.getProperties().contains ("accent") ? juce::Colour ((juce::uint32) (juce::int64) b.getProperties()["accent"]) : colours::emerald;

    g.setColour (on ? accent.withAlpha (0.25f) : colours::bg0);
    g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
    g.setColour (on ? accent : (over ? colours::textDim : colours::line));
    g.drawRoundedRectangle (pill.reduced (0.5f), pill.getHeight() * 0.5f, 1.0f);
    const float d = pill.getHeight() - 6.0f;
    auto knob = juce::Rectangle<float> (d, d).withCentre ({ on ? pill.getRight() - 3.0f - d * 0.5f : pill.getX() + 3.0f + d * 0.5f, pill.getCentreY() });
    g.setColour (on ? accent : colours::textFaint);
    g.fillEllipse (knob);

    if (b.getButtonText().isNotEmpty())
    {
        g.setFont (font (12.0f, 1));
        g.setColour (on ? colours::text : colours::textDim);
        g.drawText (b.getButtonText(), r.withRight (pill.getX() - 6.0f), juce::Justification::centredRight);
    }
}

//==============================================================================
void SwitaxLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& cb)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (colours::bg2);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (cb.isMouseOver (true) ? colours::gold.withAlpha (0.6f) : colours::line);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    const float cx = (float) w - 16.0f, cy = (float) h * 0.5f;
    juce::Path p;
    p.startNewSubPath (cx - 4.5f, cy - 2.0f);
    p.lineTo (cx, cy + 2.5f);
    p.lineTo (cx + 4.5f, cy - 2.0f);
    g.setColour (colours::gold);
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void SwitaxLookAndFeel::positionComboBoxText (juce::ComboBox& cb, juce::Label& label)
{
    label.setBounds (10, 1, cb.getWidth() - 34, cb.getHeight() - 2);
    label.setFont (getComboBoxFont (cb));
    label.setJustificationType (juce::Justification::centredLeft);
}

void SwitaxLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (colours::bg2);
    g.setColour (colours::line);
    g.drawRect (0, 0, w, h);
}

void SwitaxLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator,
                                           bool isActive, bool isHighlighted, bool isTicked, bool,
                                           const juce::String& text, const juce::String&, const juce::Drawable*, const juce::Colour*)
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
        g.setColour (colours::gold.withAlpha (0.15f));
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
    }
    if (isTicked)
    {
        juce::Path d;
        const auto c = juce::Point<float> ((float) r.getX() + 12.0f, (float) r.getCentreY());
        d.addQuadrilateral (c.x, c.y - 4, c.x + 4, c.y, c.x, c.y + 4, c.x - 4, c.y);
        g.setColour (colours::gold);
        g.fillPath (d);
    }
    g.setColour (isActive ? colours::text : colours::textFaint);
    g.setFont (getPopupMenuFont());
    g.drawText (text, r.withTrimmedLeft (24), juce::Justification::centredLeft);
}

void SwitaxLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name)
{
    g.setFont (font (11.0f, 2));
    g.setColour (colours::gold.withAlpha (0.85f));
    g.drawText (name.toUpperCase(), area.reduced (12, 0), juce::Justification::bottomLeft);
}

void SwitaxLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                       int thumbStart, int thumbSize, bool over, bool)
{
    auto thumb = vertical ? juce::Rectangle<int> (x, thumbStart, w, thumbSize) : juce::Rectangle<int> (thumbStart, y, thumbSize, h);
    g.setColour (over ? colours::line.brighter (0.2f) : colours::line);
    g.fillRoundedRectangle (thumb.toFloat().reduced (1.5f), 3.0f);
}

} // namespace sx::ui
