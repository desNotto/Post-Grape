#include "Widgets.h"

namespace pg::ui
{
namespace
{
constexpr float kStart = juce::MathConstants<float>::pi * 1.25f; // -135 degrees
constexpr float kEnd = juce::MathConstants<float>::pi * 2.75f;   // +135 degrees

juce::Image greyCopy (const juce::Image& src)
{
    auto img = src.createCopy();
    img.desaturate();
    return img;
}
} // namespace

// ---------------------------------------------------------------- Knob
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& l, bool decorated)
    : apvts (state), label (l), deco (decorated)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (kStart, kEnd, true);
    slider.setMouseDragSensitivity (220);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    bind (paramId);
}

void Knob::bind (const juce::String& paramId)
{
    attachment.reset();
    param = apvts.getParameter (paramId);
    jassert (param != nullptr);
    if (param == nullptr)
        return;
    const auto range = param->getNormalisableRange();
    slider.getProperties().set ("bipolar", range.start < 0.0f && range.end > 0.0f);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    slider.setTooltip (param->getName (64));
    repaint();
}

void Knob::setGeometry (float diameter, float zIn)
{
    diam = diameter;
    z = zIn;
}

juce::Point<int> Knob::preferredSize() const
{
    // Label (with letter spacing) and value text must fit without clipping.
    const float labelW = textWidth (font (10.0f * z, Weight::Black).withExtraKerningFactor (0.1f), label) + 8.0f * z;
    const float valueW = textWidth (font (10.5f * z, Weight::Bold), "00.00 kHz") + 4.0f * z;
    const float w = juce::jmax (diam, labelW, valueW);
    const float h = diam + 2.0f * z + 14.0f * z + 15.0f * z;
    return { juce::roundToInt (w), juce::roundToInt (h) };
}

void Knob::resized()
{
    const int d = juce::roundToInt (diam);
    slider.setBounds ((getWidth() - d) / 2, 0, d, d);
}

void Knob::paint (juce::Graphics& g)
{
    const float d = diam;
    auto r = getLocalBounds().toFloat();
    g.setColour (col::plum);
    auto lf = font (10.0f * z, Weight::Black);
    g.setFont (lf.withExtraKerningFactor (0.1f));
    g.drawText (label, juce::Rectangle<float> (0, d + 2.0f * z, r.getWidth(), 14.0f * z), juce::Justification::centred, false);

    g.setColour (col::muted);
    g.setFont (font (10.5f * z, Weight::Bold));
    const auto text = param != nullptr ? param->getCurrentValueAsText() : juce::String();
    g.drawText (text, juce::Rectangle<float> (0, d + 16.0f * z, r.getWidth(), 14.0f * z), juce::Justification::centred, false);
}

void Knob::paintOverChildren (juce::Graphics& g)
{
    if (! deco)
        return;
    const auto& im = Images::get();
    const float s = diam;
    const float x0 = (float) slider.getX();
    drawImageW (g, im.grapes, x0 - s * 0.05f, s * 0.60f, s * 0.36f);
    drawImageW (g, im.vine, x0 + s - s * 0.28f, s * 0.62f, s * 0.40f, 0.9f);
}

// ---------------------------------------------------------------- PowerButton
PowerButton::PowerButton()
{
    greyGrapes = greyCopy (Images::get().grapes);
    setTooltip ("Module on / off");
}

void PowerButton::paintButton (juce::Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (d, d);
    const bool on = getToggleState();

    if (on)
    {
        // Glow
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (juce::Colour (0xffef8fcf).withAlpha (0.12f));
            g.fillEllipse (r.expanded ((float) i * 1.3f));
        }
        juce::ColourGradient gr (juce::Colours::white, r.getX() + d * 0.4f, r.getY() + d * 0.35f,
                                 juce::Colour (0xffe48cc6), r.getX() + d * 0.4f + d * 0.7f, r.getY() + d * 0.35f, true);
        gr.addColour (0.45, juce::Colour (0xfff6c6e4));
        g.setGradientFill (gr);
        g.fillEllipse (r);
        g.setColour (juce::Colour (0xffd07ab4));
        g.drawEllipse (r, 1.0f);
    }
    else
    {
        juce::ColourGradient gr (juce::Colour (0xfff9f2f7), r.getX() + d * 0.4f, r.getY() + d * 0.35f,
                                 juce::Colour (0xffdccbd8), r.getX() + d * 0.4f + d * 0.7f, r.getY() + d * 0.35f, true);
        g.setGradientFill (gr);
        g.fillEllipse (r);
        g.setColour (juce::Colour (0xffc3a9bf));
        g.drawEllipse (r, 1.0f);
    }
    if (over)
    {
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.fillEllipse (r);
    }
    const float iw = d * 0.64f;
    const auto& img = on ? Images::get().grapes : greyGrapes;
    const float ih = iw * (float) img.getHeight() / (float) img.getWidth();
    drawImageW (g, img, r.getCentreX() - iw * 0.5f, r.getCentreY() - ih * 0.5f, iw, on ? 1.0f : 0.65f);
}

// ---------------------------------------------------------------- LofiSwitch
LofiSwitch::LofiSwitch()
{
    greyGrapes = greyCopy (Images::get().grapes);
    greySwirl = greyCopy (Images::get().swirl);
    setTooltip ("LOFI on / off (automatable)");
}

void LofiSwitch::paintButton (juce::Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const float h = r.getHeight(), rad = h * 0.5f;
    const bool on = getToggleState();
    const auto& im = Images::get();

    if (on)
    {
        g.setColour (juce::Colour (0xffef8fcf).withAlpha (0.25f));
        g.fillRoundedRectangle (r.expanded (2.0f), rad + 2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff8dcef), 0, r.getY(), juce::Colour (0xffefc0e0), 0, r.getBottom(), false));
    }
    else
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeee2ec), 0, r.getY(), juce::Colour (0xffdccbd8), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, rad);
    g.setColour (on ? juce::Colour (0xffd692c1) : juce::Colour (0xffc3a9bf));
    g.drawRoundedRectangle (r, rad, 1.0f);

    // Swirl on the free side
    const float sw = r.getWidth() * 0.48f;
    const float sx = on ? r.getX() + h * 0.30f : r.getRight() - h * 0.30f - sw;
    const auto& swImg = on ? im.swirl : greySwirl;
    const float sh = sw * (float) swImg.getHeight() / (float) swImg.getWidth();
    drawImageW (g, swImg, sx, r.getCentreY() - sh * 0.5f, sw, on ? 0.95f : 0.6f);

    // Nub
    const float nd = h - 4.0f;
    auto nub = juce::Rectangle<float> (nd, nd).withCentre ({ on ? r.getRight() - 2.0f - nd * 0.5f : r.getX() + 2.0f + nd * 0.5f, r.getCentreY() });
    juce::DropShadow (col::plum.withAlpha (0.25f), 4, { 0, 2 }).drawForRectangle (g, nub.toNearestInt());
    juce::ColourGradient ng (on ? juce::Colours::white : juce::Colour (0xfffbf6fa), nub.getX() + nd * 0.4f, nub.getY() + nd * 0.35f,
                             on ? juce::Colour (0xffde86c0) : juce::Colour (0xffd9c6d6), nub.getX() + nd * 1.1f, nub.getY() + nd * 0.35f, true);
    if (on) ng.addColour (0.55, juce::Colour (0xfff5c3e3));
    g.setGradientFill (ng);
    g.fillEllipse (nub);
    g.setColour (on ? juce::Colour (0xffc975ae) : juce::Colour (0xffbda1b9));
    g.drawEllipse (nub, 1.0f);
    const float gw = nd * 0.62f;
    const auto& gImg = on ? im.grapes : greyGrapes;
    const float gh = gw * (float) gImg.getHeight() / (float) gImg.getWidth();
    drawImageW (g, gImg, nub.getCentreX() - gw * 0.5f, nub.getCentreY() - gh * 0.5f, gw, on ? 1.0f : 0.6f);

    if (over)
    {
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRoundedRectangle (r, rad);
    }
}

// ---------------------------------------------------------------- Segmented
Segmented::Segmented (juce::StringArray i) : items (std::move (i)) {}

void Segmented::bind (juce::AudioProcessorValueTreeState& state, const juce::String& paramId)
{
    attachment.reset();
    if (auto* p = state.getParameter (paramId))
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v) { setSelected (juce::roundToInt (v), false); });
        attachment->sendInitialUpdate();
    }
}

void Segmented::unbind() { attachment.reset(); }

void Segmented::setSelected (int index, bool notify)
{
    index = juce::jlimit (0, items.size() - 1, index);
    if (index == selected && ! notify)
        return;
    selected = index;
    repaint();
    if (notify)
    {
        if (attachment != nullptr)
            attachment->setValueAsCompleteGesture ((float) index);
        if (onChange)
            onChange (index);
    }
}

void Segmented::setZ (float zIn)
{
    z = zIn;
    repaint();
}

std::vector<float> Segmented::itemWidths() const
{
    std::vector<float> w;
    const auto f = font (11.0f * z, Weight::ExtraBold);
    for (const auto& s : items)
        w.push_back (textWidth (f, s) * 1.06f + 20.0f * z);
    return w;
}

int Segmented::preferredWidth() const
{
    float total = 2.0f;
    for (float w : itemWidths())
        total += w;
    return juce::roundToInt (total);
}

void Segmented::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float rad = juce::jmin (14.0f * z, r.getHeight() * 0.5f);
    g.setColour (juce::Colour (0xfff3e3f0));
    g.fillRoundedRectangle (r, rad);

    const auto widths = itemWidths();
    float total = 0.0f;
    for (float w : widths) total += w;
    const float scale = (r.getWidth()) / juce::jmax (1.0f, total);

    juce::Path clip;
    clip.addRoundedRectangle (r, rad);
    g.saveState();
    g.reduceClipRegion (clip);
    float x = r.getX();
    const auto f = font (11.0f * z, Weight::ExtraBold);
    for (int i = 0; i < items.size(); ++i)
    {
        const float w = widths[(size_t) i] * scale;
        auto cell = juce::Rectangle<float> (x, r.getY(), w, r.getHeight());
        if (i == selected)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff3a6d6), 0, r.getY(), juce::Colour (0xffd873b4), 0, r.getBottom(), false));
            g.fillRect (cell);
        }
        g.setColour (i == selected ? juce::Colours::white : col::muted);
        g.setFont (f.withExtraKerningFactor (0.05f));
        g.drawText (items[i], cell, juce::Justification::centred, false);
        x += w;
    }
    g.restoreState();
    g.setColour (col::line2);
    g.drawRoundedRectangle (r, rad, 1.0f);
}

void Segmented::mouseDown (const juce::MouseEvent& e)
{
    const auto widths = itemWidths();
    float total = 0.0f;
    for (float w : widths) total += w;
    const float scale = (float) getWidth() / juce::jmax (1.0f, total);
    float x = 0.0f;
    for (int i = 0; i < items.size(); ++i)
    {
        x += widths[(size_t) i] * scale;
        if ((float) e.x < x)
        {
            setSelected (i, true);
            return;
        }
    }
}

// ---------------------------------------------------------------- IconButton
IconButton::IconButton (Icon i, const juce::String& tip) : juce::Button (tip), icon (i)
{
    setTooltip (tip);
}

void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float rad = r.getWidth() * 0.29f;
    juce::DropShadow (col::plum.withAlpha (0.12f), 3, { 0, 2 }).drawForRectangle (g, r.toNearestInt());
    g.setGradientFill (juce::ColourGradient (over ? juce::Colours::white : juce::Colour (0xfffff9fd), 0, r.getY(),
                                             over ? juce::Colour (0xfff7e6f3) : juce::Colour (0xfff1dfee), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, rad);
    if (down)
    {
        g.setColour (col::plum.withAlpha (0.06f));
        g.fillRoundedRectangle (r, rad);
    }
    g.setColour (col::line2);
    g.drawRoundedRectangle (r, rad, 1.0f);

    // Icon in a 20 x 20 box.
    const float s = r.getWidth() * 0.53f / 20.0f;
    const juce::Point<float> o (r.getCentreX() - 10.0f * s, r.getCentreY() - 10.0f * s);
    auto P = [&] (float x, float y) { return juce::Point<float> (o.x + x * s, o.y + y * s); };
    const juce::Colour ink = col::plum, fillC (0xffe7c6e3);
    const float lw = 1.6f * s;
    juce::Path p;

    switch (icon)
    {
        case Icon::Prev:
        case Icon::Next:
        {
            const bool prev = icon == Icon::Prev;
            p.startNewSubPath (P (prev ? 12.5f : 7.5f, 4.0f));
            p.lineTo (P (prev ? 6.5f : 13.5f, 10.0f));
            p.lineTo (P (prev ? 12.5f : 7.5f, 16.0f));
            g.setColour (ink);
            g.strokePath (p, juce::PathStrokeType (2.4f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            break;
        }
        case Icon::Save:
        {
            p.startNewSubPath (P (4, 3)); p.lineTo (P (14, 3)); p.lineTo (P (17, 6)); p.lineTo (P (17, 17)); p.lineTo (P (4, 17)); p.closeSubPath();
            g.setColour (fillC); g.fillPath (p);
            g.setColour (ink); g.strokePath (p, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            juce::Rectangle<float> a (P (7, 3), P (13, 7)), b (P (6.5f, 11), P (14.5f, 17));
            g.setColour (juce::Colours::white); g.fillRect (a); g.fillRoundedRectangle (b, 1.0f * s);
            g.setColour (ink); g.drawRect (a, 1.4f * s); g.drawRoundedRectangle (b, 1.0f * s, 1.4f * s);
            break;
        }
        case Icon::Rename:
        {
            p.startNewSubPath (P (4, 16)); p.lineTo (P (5, 12)); p.lineTo (P (13, 4)); p.lineTo (P (16, 7)); p.lineTo (P (8, 15)); p.closeSubPath();
            g.setColour (fillC); g.fillPath (p);
            g.setColour (ink); g.strokePath (p, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            break;
        }
        case Icon::Delete:
        {
            p.startNewSubPath (P (5, 6)); p.lineTo (P (15, 6)); p.lineTo (P (14, 17)); p.lineTo (P (6, 17)); p.closeSubPath();
            g.setColour (fillC); g.fillPath (p);
            g.setColour (ink); g.strokePath (p, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            juce::Path t;
            t.startNewSubPath (P (3.5f, 6)); t.lineTo (P (16.5f, 6));
            t.startNewSubPath (P (8, 6)); t.lineTo (P (8, 4)); t.lineTo (P (12, 4)); t.lineTo (P (12, 6));
            g.strokePath (t, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            break;
        }
        case Icon::Folder:
        {
            p.startNewSubPath (P (3, 5)); p.lineTo (P (8, 5)); p.lineTo (P (10, 7)); p.lineTo (P (17, 7)); p.lineTo (P (17, 16)); p.lineTo (P (3, 16)); p.closeSubPath();
            g.setColour (fillC); g.fillPath (p);
            g.setColour (ink); g.strokePath (p, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            break;
        }
    }
}

// ---------------------------------------------------------------- LedMeter
void LedMeter::setValue (float v)
{
    v = juce::jlimit (0.0f, 1.0f, v);
    if (std::abs (v - value) > 0.001f)
    {
        value = v;
        repaint();
    }
}

void LedMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float rad = r.getWidth() * 0.5f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff3e3ef), 0, r.getY(), juce::Colour (0xffead6e6), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, rad);
    g.setColour (col::line2);
    g.drawRoundedRectangle (r, rad, 1.0f);

    const float pad = juce::jmax (2.0f, r.getWidth() * 0.18f), gap = juce::jmax (1.5f, r.getWidth() * 0.12f);
    auto inner = r.reduced (pad);
    const float segH = (inner.getHeight() - gap * (float) (n - 1)) / (float) n;
    for (int i = 0; i < n; ++i)
    {
        const float pos = (float) i / (float) n;
        const bool lit = pos < value;
        // Level mode: i = 0 at the bottom. GR mode: i = 0 at the top.
        const float y = gr ? inner.getY() + (float) i * (segH + gap) : inner.getBottom() - (float) (i + 1) * segH - (float) i * gap;
        auto seg = juce::Rectangle<float> (inner.getX(), y, inner.getWidth(), segH);
        juce::Colour c = col::ledOff;
        if (lit)
        {
            if (gr) c = col::pink2;
            else c = pos < 0.55f ? col::ledG : (pos < 0.75f ? col::ledY : (pos < 0.88f ? col::ledO : (pos < 0.96f ? col::ledR : col::ledP)));
            g.setColour (c.withAlpha (0.35f));
            g.fillRoundedRectangle (seg.expanded (1.2f), 3.0f);
        }
        g.setColour (c);
        g.fillRoundedRectangle (seg, juce::jmin (3.0f, segH * 0.4f));
        g.setColour (lit ? c.darker (0.15f) : juce::Colour (0xffd9c4d5));
        g.drawRoundedRectangle (seg, juce::jmin (3.0f, segH * 0.4f), 0.8f);
    }
}

// ---------------------------------------------------------------- CorrMeter
void CorrMeter::setValue (float v)
{
    v = juce::jlimit (-1.0f, 1.0f, v);
    if (std::abs (v - value) > 0.002f)
    {
        value = v;
        repaint();
    }
}

void CorrMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float rad = r.getWidth() * 0.5f;
    g.setColour (juce::Colour (0xfff3e3ef));
    g.fillRoundedRectangle (r, rad);
    g.setColour (col::line2);
    g.drawRoundedRectangle (r, rad, 1.0f);
    // Centre line (correlation 0)
    g.fillRect (r.getX(), r.getCentreY() - 0.75f, r.getWidth(), 1.5f);

    const float y = r.getY() + (1.0f - value) * 0.5f * r.getHeight();
    const float h = juce::jmax (6.0f, r.getHeight() * 0.09f);
    const auto c = value > 0.3f ? col::ledG : (value > -0.1f ? col::ledY : col::ledR);
    auto ind = juce::Rectangle<float> (r.getX() + 3.0f, juce::jlimit (r.getY() + 2.0f, r.getBottom() - h - 2.0f, y - h * 0.5f),
                                       r.getWidth() - 6.0f, h);
    g.setColour (c.withAlpha (0.4f));
    g.fillRoundedRectangle (ind.expanded (1.5f), 3.0f);
    g.setColour (c);
    g.fillRoundedRectangle (ind, 2.5f);
}
} // namespace pg::ui
