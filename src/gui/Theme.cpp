#include "Theme.h"
#include "PostGrapeData.h"

namespace pg::ui
{
namespace
{
juce::Typeface::Ptr typeface (Weight w)
{
    static juce::Typeface::Ptr bold = juce::Typeface::createSystemTypefaceFor (PostGrapeData::NunitoBold_ttf, PostGrapeData::NunitoBold_ttfSize);
    static juce::Typeface::Ptr extra = juce::Typeface::createSystemTypefaceFor (PostGrapeData::NunitoExtraBold_ttf, PostGrapeData::NunitoExtraBold_ttfSize);
    static juce::Typeface::Ptr black = juce::Typeface::createSystemTypefaceFor (PostGrapeData::NunitoBlack_ttf, PostGrapeData::NunitoBlack_ttfSize);
    switch (w)
    {
        case Weight::ExtraBold: return extra;
        case Weight::Black: return black;
        default: return bold;
    }
}

juce::Point<float> polar (juce::Point<float> c, float r, float angle)
{
    return { c.x + r * std::sin (angle), c.y - r * std::cos (angle) };
}
} // namespace

juce::Font font (float height, Weight w)
{
    // 'height' is the CSS font size (em size) as in the mockup.
    return juce::Font (juce::FontOptions (typeface (w)).withPointHeight (height));
}

float textWidth (const juce::Font& f, const juce::String& s)
{
    return juce::GlyphArrangement::getStringWidth (f, s);
}

const Images& Images::get()
{
    static const Images images = [] {
        Images i;
        i.grapes = juce::ImageCache::getFromMemory (PostGrapeData::grapes_png, PostGrapeData::grapes_pngSize);
        i.leaf = juce::ImageCache::getFromMemory (PostGrapeData::leaf_png, PostGrapeData::leaf_pngSize);
        i.leafSmall = juce::ImageCache::getFromMemory (PostGrapeData::leafsmall_png, PostGrapeData::leafsmall_pngSize);
        i.swirl = juce::ImageCache::getFromMemory (PostGrapeData::swirl_png, PostGrapeData::swirl_pngSize);
        i.vine = juce::ImageCache::getFromMemory (PostGrapeData::vine_png, PostGrapeData::vine_pngSize);
        return i;
    }();
    return images;
}

void drawImageW (juce::Graphics& g, const juce::Image& img, float x, float y, float w, float alpha)
{
    if (! img.isValid() || w <= 0.0f)
        return;
    const float h = w * (float) img.getHeight() / (float) img.getWidth();
    g.setOpacity (alpha);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (img, { x, y, w, h }, juce::RectanglePlacement::stretchToFit);
    g.setOpacity (1.0f);
}

void drawChip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, float fontH)
{
    g.setColour (juce::Colour (0xd9fff8fc));
    g.fillRoundedRectangle (r, fontH * 0.7f);
    g.setColour (col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), fontH * 0.7f, 1.0f);
    g.setColour (col::plum);
    g.setFont (font (fontH, Weight::ExtraBold));
    g.drawText (text, r, juce::Justification::centred, false);
}

void drawScreen (juce::Graphics& g, juce::Rectangle<float> r, float z)
{
    const float rad = 12.0f * z;
    g.setGradientFill (juce::ColourGradient (col::screenTop, r.getX(), r.getY(), col::screenBottom, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, rad);
    // Inner shadow at the top
    juce::Path p;
    p.addRoundedRectangle (r, rad);
    g.saveState();
    g.reduceClipRegion (p);
    g.setGradientFill (juce::ColourGradient (col::plum.withAlpha (0.10f), r.getX(), r.getY(),
                                             col::plum.withAlpha (0.0f), r.getX(), r.getY() + 8.0f * z, false));
    g.fillRect (r);
    g.restoreState();
    g.setColour (col::line2);
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
}

LookAndFeel::LookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xfffbf3f9));
    setColour (juce::PopupMenu::textColourId, col::deep);
    setColour (juce::PopupMenu::headerTextColourId, col::lav2);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::pink2);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xfffbf3f9));
    setColour (juce::AlertWindow::textColourId, col::deep);
    setColour (juce::AlertWindow::outlineColourId, col::line2);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xfff3e3f0));
    setColour (juce::TextButton::textColourOffId, col::plum);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xfffdf6fb));
    setColour (juce::TextEditor::textColourId, col::deep);
    setColour (juce::TextEditor::highlightColourId, col::pink2.withAlpha (0.5f));
    setColour (juce::TextEditor::outlineColourId, col::line2);
    setColour (juce::TextEditor::focusedOutlineColourId, col::pink);
    setColour (juce::CaretComponent::caretColourId, col::hot);
    setColour (juce::Slider::textBoxTextColourId, col::deep);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xfffdf6fb));
    setColour (juce::Slider::textBoxOutlineColourId, col::line2);
    setColour (juce::Slider::textBoxHighlightColourId, col::pink2.withAlpha (0.5f));
    setColour (juce::Label::textColourId, col::deep);
    setColour (juce::Label::textWhenEditingColourId, col::deep);
    setColour (juce::Label::backgroundWhenEditingColourId, juce::Colour (0xfffdf6fb));
    setColour (juce::Label::outlineWhenEditingColourId, col::pink);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xfffbf3f9));
    setColour (juce::TooltipWindow::textColourId, col::deep);
    setColour (juce::TooltipWindow::outlineColourId, col::line2);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle,
                                     float endAngle, juce::Slider& slider)
{
    // Geometry from the mockup: a 100 x 100 box.
    const float S = (float) juce::jmin (w, h);
    const float u = S / 100.0f;
    const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool bipolar = (bool) slider.getProperties().getWithDefault ("bipolar", false);

    // Ticks
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const bool big = i % 5 == 0;
        g.setColour (col::tick.withAlpha (big ? 0.9f : 0.55f));
        g.drawLine (juce::Line<float> (polar (c, (big ? 41.0f : 42.0f) * u, a), polar (c, 47.0f * u, a)), (big ? 2.2f : 1.5f) * u);
    }
    // Top marker dot
    const auto top = polar (c, 49.5f * u, 0.0f);
    g.setColour (col::hot);
    g.fillEllipse (juce::Rectangle<float> (5.2f * u, 5.2f * u).withCentre (top));

    // Value arc
    {
        const float from = bipolar ? juce::MathConstants<float>::twoPi : startAngle;
        if (std::abs (angle - from) > 0.01f)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, 39.0f * u, 39.0f * u, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
            g.setColour (col::arc.withAlpha (0.9f));
            g.strokePath (arc, juce::PathStrokeType (3.0f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // Outer ring with drop shadow
    juce::Path ring;
    ring.addEllipse (juce::Rectangle<float> (72.0f * u, 72.0f * u).withCentre (c));
    juce::DropShadow (col::plum.withAlpha (0.35f), juce::jmax (2, juce::roundToInt (4.5f * u)), { 0, juce::roundToInt (2.5f * u) }).drawForPath (g, ring);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff1dcec), c.x, c.y - 36.0f * u, juce::Colour (0xffc9a2c2), c.x, c.y + 36.0f * u, false));
    g.fillPath (ring);

    // Knurl (dashed ring)
    {
        juce::Path k;
        k.addEllipse (juce::Rectangle<float> (70.0f * u, 70.0f * u).withCentre (c));
        juce::Path dashed;
        const float dashes[] = { 1.2f * u, 1.6f * u };
        juce::PathStrokeType (3.2f * u).createDashedStroke (dashed, k, dashes, 2);
        g.setColour (juce::Colour (0xffb78bb0).withAlpha (0.55f));
        g.fillPath (dashed);
    }

    // Cap
    const auto capR = juce::Rectangle<float> (60.0f * u, 60.0f * u).withCentre (c);
    juce::ColourGradient cg (juce::Colour (0xfffffbfd), capR.getX() + capR.getWidth() * 0.40f, capR.getY() + capR.getHeight() * 0.32f,
                             juce::Colour (0xffe2c9dc), capR.getX() + capR.getWidth() * 0.40f + 45.0f * u, capR.getY() + capR.getHeight() * 0.32f, true);
    cg.addColour (0.6, juce::Colour (0xfff4e6f0));
    g.setGradientFill (cg);
    g.fillEllipse (capR);
    g.setColour (juce::Colour (0xffd8bdd2));
    g.drawEllipse (capR, 1.0f * u);

    // Pointer
    g.setColour (slider.isEnabled() ? col::hot : col::muted);
    g.drawLine (juce::Line<float> (polar (c, 14.0f * u, angle), polar (c, 27.0f * u, angle)), 3.2f * u);
    g.fillEllipse (juce::Rectangle<float> (3.2f * u, 3.2f * u).withCentre (polar (c, 14.0f * u, angle)));
    g.fillEllipse (juce::Rectangle<float> (3.2f * u, 3.2f * u).withCentre (polar (c, 27.0f * u, angle)));
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (col::line2);
    g.drawRect (0, 0, w, h, 1);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const float rad = r.getHeight() * 0.5f;
    const bool primary = b.getToggleState() || (bool) b.getProperties().getWithDefault ("primary", false);
    if (primary)
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff3a6d6), 0, r.getY(), juce::Colour (0xffd873b4), 0, r.getBottom(), false));
    else
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff9fd), 0, r.getY(), juce::Colour (0xfff1dfee), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, rad);
    if (over || down)
    {
        g.setColour (juce::Colours::white.withAlpha (down ? 0.1f : 0.25f));
        g.fillRoundedRectangle (r, rad);
    }
    g.setColour (primary ? juce::Colour (0xffcc6aa9) : col::line2);
    g.drawRoundedRectangle (r, rad, 1.0f);
    b.setColour (juce::TextButton::textColourOffId, primary ? juce::Colours::white : col::plum);
}

void LookAndFeel::drawAlertBox (juce::Graphics& g, juce::AlertWindow& alert, const juce::Rectangle<int>& textArea, juce::TextLayout& layout)
{
    auto r = alert.getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (col::panelTop, 0, 0, col::panelBottom, 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 14.0f);
    g.setColour (col::line2);
    g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);
    drawImageW (g, Images::get().grapes, 14.0f, 12.0f, 34.0f);
    layout.draw (g, textArea.toFloat().withTrimmedLeft (0));
}

void LookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool, bool)
{
    g.setColour (col::muted.withAlpha (0.7f));
    const float W = (float) w, H = (float) h;
    g.drawLine (W - 2.0f, H * 0.35f, W * 0.35f, H - 2.0f, 1.6f);
    g.drawLine (W - 2.0f, H * 0.68f, W * 0.68f, H - 2.0f, 1.6f);
}

void LookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 8.0f);
}

void LookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? col::pink : col::line2);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 8.0f, 1.2f);
}
} // namespace pg::ui
