#include "Displays.h"
#include "Params.h"
#include "dsp/BusEQ.h"

namespace pg::ui
{
namespace
{
const juce::Colour targetColour[3] = { col::hot, col::purple, col::teal };
constexpr double kMinF = 20.0, kMaxF = 20000.0, kRangeDb = 20.0;
} // namespace

// ================================================================ EqDisplay
EqDisplay::EqDisplay (juce::AudioProcessorValueTreeState& state, std::function<double()> sampleRate)
    : apvts (state), getFs (std::move (sampleRate))
{
    setRepaintsOnMouseActivity (false);
}

float EqDisplay::value (const juce::String& id) const
{
    auto* v = apvts.getRawParameterValue (id);
    return v != nullptr ? v->load() : 0.0f;
}

void EqDisplay::refresh()
{
    std::vector<float> now;
    for (auto* id : { "eq_hp", "eq_hp_slope", "eq_lp", "eq_on" })
        now.push_back (value (id));
    for (int b = 0; b < 4; ++b)
        for (auto* n : { "freq", "gain", "q", "type", "target" })
            now.push_back (value (params::bandId (b, n)));
    if (now != snapshot)
    {
        snapshot = std::move (now);
        repaint();
    }
}

float EqDisplay::xForFreq (double f) const
{
    return (float) (std::log (f / kMinF) / std::log (kMaxF / kMinF)) * (float) getWidth();
}

double EqDisplay::freqForX (float x) const
{
    return kMinF * std::pow (kMaxF / kMinF, (double) x / juce::jmax (1, getWidth()));
}

float EqDisplay::yForDb (double db) const
{
    const float h = (float) getHeight();
    return h * 0.5f - (float) (db / kRangeDb) * (h * 0.5f - 10.0f * z);
}

double EqDisplay::dbForY (float y) const
{
    const float h = (float) getHeight();
    return (h * 0.5f - y) / (h * 0.5f - 10.0f * z) * kRangeDb;
}

juce::Point<float> EqDisplay::nodePos (int b) const
{
    const int type = juce::roundToInt (value (params::bandId (b, "type")));
    const double gain = type == 1 ? -15.0 : value (params::bandId (b, "gain"));
    return { xForFreq (value (params::bandId (b, "freq"))), yForDb (gain) };
}

int EqDisplay::hitNode (juce::Point<float> p) const
{
    int best = -1;
    float bestD = 18.0f * z;
    for (int b = 0; b < 4; ++b)
    {
        const float d = nodePos (b).getDistanceFrom (p);
        if (d < bestD)
        {
            bestD = d;
            best = b;
        }
    }
    return best;
}

void EqDisplay::setParam (const juce::String& id, float v)
{
    if (auto* p = param (id))
        p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void EqDisplay::paint (juce::Graphics& g)
{
    const float W = (float) getWidth(), H = (float) getHeight();
    const double fs = juce::jmax (22050.0, getFs());

    // Grid
    g.setFont (font (9.0f * z, Weight::Bold));
    for (double f : { 30.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = xForFreq (f);
        g.setColour (juce::Colour (0xffe8d3e3));
        g.drawVerticalLine (juce::roundToInt (x), 0.0f, H);
        g.setColour (col::faint);
        const auto t = f >= 1000.0 ? juce::String (f / 1000.0, 0) + "k" : juce::String ((int) f);
        g.drawText (t, juce::Rectangle<float> (x + 3.0f * z, H - 14.0f * z, 30.0f * z, 12.0f * z), juce::Justification::centredLeft, false);
    }
    for (int d : { -12, -6, 0, 6, 12 })
    {
        const float y = yForDb (d);
        g.setColour (d == 0 ? juce::Colour (0xffdcc0d6) : col::grid);
        g.drawHorizontalLine (juce::roundToInt (y), 0.0f, W);
        if (d != 0)
        {
            g.setColour (col::faint);
            g.drawText ((d > 0 ? "+" : "") + juce::String (d), juce::Rectangle<float> (4.0f * z, y - 13.0f * z, 30.0f * z, 12.0f * z),
                        juce::Justification::centredLeft, false);
        }
    }

    // Filters
    std::array<pg::SvfCoefs, 4> bc;
    std::array<int, 4> target {};
    for (int b = 0; b < 4; ++b)
    {
        pg::EqBandSettings s;
        s.freq = value (params::bandId (b, "freq"));
        s.gainDb = value (params::bandId (b, "gain"));
        s.q = value (params::bandId (b, "q"));
        s.type = (pg::EqBandType) juce::jlimit (0, 3, juce::roundToInt (value (params::bandId (b, "type"))));
        target[(size_t) b] = juce::jlimit (0, 2, juce::roundToInt (value (params::bandId (b, "target"))));
        pg::BusEQ::designBand (s, fs, bc[(size_t) b]);
    }
    const bool steep = value ("eq_hp_slope") > 0.5f;
    pg::SvfCoefs hpA, hpB, lp;
    if (steep)
    {
        hpA.design (pg::SvfType::HighPass, fs, value ("eq_hp"), pg::kQ24a);
        hpB.design (pg::SvfType::HighPass, fs, value ("eq_hp"), pg::kQ24b);
    }
    else
        hpA.design (pg::SvfType::HighPass, fs, value ("eq_hp"), pg::kQ12);
    lp.design (pg::SvfType::LowPass, fs, value ("eq_lp"), pg::kQ12);

    auto curve = [&] (bool side) {
        juce::Path p;
        const float step = juce::jmax (1.5f, 2.0f * z);
        for (float x = 0.0f; x <= W + step; x += step)
        {
            const double f = freqForX (juce::jmin (x, W));
            double mag = std::abs (hpA.response (f, fs)) * std::abs (lp.response (f, fs));
            if (steep) mag *= std::abs (hpB.response (f, fs));
            for (size_t b = 0; b < 4; ++b)
                if (target[b] == 0 || target[b] == (side ? 2 : 1))
                    mag *= std::abs (bc[b].response (f, fs));
            const double db = juce::jlimit (-kRangeDb - 1.0, kRangeDb + 1.0, pg::gainToDb (mag));
            const float y = yForDb (db);
            if (x == 0.0f) p.startNewSubPath (x, y);
            else p.lineTo (juce::jmin (x, W), y);
        }
        return p;
    };

    const bool on = value ("eq_on") > 0.5f;
    auto mainCurve = curve (false);
    auto sideCurve = curve (true);

    // Fill under the main curve
    {
        juce::Path fill (mainCurve);
        fill.lineTo (W, H);
        fill.lineTo (0, H);
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0x55e98cc9), 0, 0, juce::Colour (0x05e98cc9), 0, H, false));
        g.fillPath (fill);
    }
    // Side curve (dashed)
    {
        juce::Path dashed;
        const float dashes[] = { 4.0f * z, 3.0f * z };
        juce::PathStrokeType (1.4f * z).createDashedStroke (dashed, sideCurve, dashes, 2);
        g.setColour (col::teal.withAlpha (0.6f));
        g.fillPath (dashed);
    }
    // Main curve with glow
    g.setColour (juce::Colour (0xffe47bc0).withAlpha (0.18f));
    g.strokePath (mainCurve, juce::PathStrokeType (7.0f * z, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xffe47bc0).withAlpha (0.25f));
    g.strokePath (mainCurve, juce::PathStrokeType (4.0f * z, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (on ? col::hot : col::muted);
    g.strokePath (mainCurve, juce::PathStrokeType (2.4f * z, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // HP / LP markers
    g.setFont (font (9.0f * z, Weight::Black));
    for (auto [t, f] : { std::pair<const char*, float> { "HP", value ("eq_hp") }, { "LP", value ("eq_lp") } })
    {
        const float x = xForFreq (f);
        g.setColour (col::purple);
        g.drawText (t, juce::Rectangle<float> (x - 12.0f * z, 4.0f * z, 24.0f * z, 11.0f * z), juce::Justification::centred, false);
        g.fillRect (x - 0.5f, 17.0f * z, 1.0f, 8.0f * z);
    }

    // Nodes
    for (int b = 0; b < 4; ++b)
    {
        const auto p = nodePos (b);
        const auto c = targetColour[target[(size_t) b]];
        const float r = (b == selected ? 9.0f : 7.5f) * z;
        g.setColour (b == selected ? c : juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (p));
        g.setColour (c);
        g.drawEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (p), 2.0f * z);
        g.setColour (b == selected ? juce::Colours::white : c);
        g.setFont (font (10.0f * z, Weight::Black));
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (r * 2, r * 2).withCentre (p), juce::Justification::centred, false);
    }

    // Legend
    g.setColour (col::muted);
    g.setFont (font (9.0f * z, Weight::ExtraBold));
    g.drawText (juce::String::fromUTF8 ("\xe2\x80\x94 MID / STEREO     - - SIDE"),
                juce::Rectangle<float> (W - 210.0f * z, H - 30.0f * z, 202.0f * z, 12.0f * z), juce::Justification::centredRight, false);
}

void EqDisplay::mouseDown (const juce::MouseEvent& e)
{
    dragging = hitNode (e.position);
    if (dragging < 0)
        return;
    selected = dragging;
    if (onSelectBand)
        onSelectBand (dragging);
    for (auto* n : { "freq", "gain" })
        if (auto* p = param (params::bandId (dragging, n)))
            p->beginChangeGesture();
    repaint();
}

void EqDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;
    setParam (params::bandId (dragging, "freq"), (float) juce::jlimit (20.0, 20000.0, freqForX (e.position.x)));
    if (juce::roundToInt (value (params::bandId (dragging, "type"))) != 1)
        setParam (params::bandId (dragging, "gain"), (float) juce::jlimit (-18.0, 18.0, dbForY (e.position.y)));
    refresh();
}

void EqDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging < 0)
        return;
    for (auto* n : { "freq", "gain" })
        if (auto* p = param (params::bandId (dragging, n)))
            p->endChangeGesture();
    dragging = -1;
}

void EqDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int b = hitNode (e.position);
    if (b < 0)
        return;
    if (auto* p = param (params::bandId (b, "gain")))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (0.0f));
        p->endChangeGesture();
    }
}

void EqDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int b = hitNode (e.position);
    if (b < 0) b = selected;
    if (auto* p = param (params::bandId (b, "q")))
    {
        const float q = value (params::bandId (b, "q")) * std::pow (1.15f, w.deltaY * 4.0f);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (0.1f, 20.0f, q)));
        p->endChangeGesture();
    }
}

// ================================================================ LimiterScope
void LimiterScope::refresh()
{
    const uint32_t count = source.count();
    if (count - readCount > (uint32_t) pg::ScopeFifo::kSize)
        readCount = count - (uint32_t) pg::ScopeFifo::kSize;
    bool any = false;
    while (readCount != count)
    {
        cols[head] = source.get (readCount++);
        head = (head + 1) % cols.size();
        any = true;
    }
    if (any)
    {
        const auto& last = cols[(head + cols.size() - 1) % cols.size()];
        lastGrDb = (float) -pg::gainToDb (last.gain);
        repaint();
    }
}

void LimiterScope::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    drawScreen (g, bounds, z);
    juce::Path clip;
    clip.addRoundedRectangle (bounds.reduced (1.0f), 12.0f * z);
    g.saveState();
    g.reduceClipRegion (clip);

    const float W = bounds.getWidth(), H = bounds.getHeight();
    const float mid = H * 0.5f, sc = H * 0.5f - 14.0f * z;

    g.setColour (col::grid);
    for (int i = 1; i < 10; ++i) g.drawVerticalLine (juce::roundToInt ((float) i * W / 10.0f), 0.0f, H);
    for (int i = 1; i < 6; ++i) g.drawHorizontalLine (juce::roundToInt ((float) i * H / 6.0f), 0.0f, W);

    const float ceil = (float) pg::dbToGain (ceilingDb);
    const float yc = mid - ceil * sc;
    {
        const float dashes[] = { 5.0f * z, 4.0f * z };
        g.setColour (col::hot.withAlpha (0.55f));
        g.drawDashedLine ({ 0.0f, yc, W, yc }, dashes, 2, 1.0f);
        g.drawDashedLine ({ 0.0f, H - yc, W, H - yc }, dashes, 2, 1.0f);
    }

    const size_t N = cols.size();
    const float dx = W / (float) (N - 1);
    auto envelope = [&] (bool out) {
        juce::Path p;
        for (size_t i = 0; i < N; ++i)
        {
            const auto& c = cols[(head + i) % N];
            const float v = juce::jmin (1.2f, out ? c.outPeak : c.inPeak);
            const float y = mid - v * sc;
            if (i == 0) p.startNewSubPath (0, y);
            else p.lineTo ((float) i * dx, y);
        }
        for (size_t k = N; k-- > 0;)
        {
            const auto& c = cols[(head + k) % N];
            const float v = juce::jmin (1.2f, out ? c.outPeak : c.inPeak);
            p.lineTo ((float) k * dx, mid + v * sc);
        }
        p.closeSubPath();
        return p;
    };

    g.setColour (juce::Colour (0x55b39ab8));
    g.fillPath (envelope (false));

    auto outPath = envelope (true);
    g.setColour (juce::Colour (0xffe47bc0).withAlpha (0.25f));
    g.strokePath (outPath, juce::PathStrokeType (4.0f * z));
    juce::ColourGradient og (juce::Colour (0xffe06fb6), 0, 0, juce::Colour (0xffe06fb6), 0, H, false);
    og.addColour (0.5, juce::Colour (0xfff7c4e4));
    g.setGradientFill (og);
    g.fillPath (outPath);

    // Gain reduction line (0 dB at the top, 12 dB at 45 % height)
    {
        juce::Path gr;
        for (size_t i = 0; i < N; ++i)
        {
            const auto& c = cols[(head + i) % N];
            const float db = (float) -pg::gainToDb (c.gain);
            const float y = 4.0f * z + juce::jmin (db, 14.0f) / 12.0f * H * 0.45f;
            if (i == 0) gr.startNewSubPath (0, y);
            else gr.lineTo ((float) i * dx, y);
        }
        g.setColour (col::plum);
        g.strokePath (gr, juce::PathStrokeType (2.0f * z, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.restoreState();

    // Legend chips
    const float fh = 10.0f * z, ch = 19.0f * z;
    {
        const juce::String legend = juce::String::fromUTF8 ("INPUT \xe2\x96\xac   OUTPUT \xe2\x96\xac   GAIN REDUCTION \xe2\x96\xac");
        const float w = textWidth (font (fh, Weight::ExtraBold), legend) + 18.0f * z;
        auto r = juce::Rectangle<float> (10.0f * z, 8.0f * z, w, ch);
        g.setColour (juce::Colour (0xd9fff8fc));
        g.fillRoundedRectangle (r, 7.0f * z);
        g.setColour (col::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 7.0f * z, 1.0f);
        // Coloured text parts
        g.setFont (font (fh, Weight::ExtraBold));
        float x = r.getX() + 9.0f * z;
        auto part = [&] (const juce::String& s, juce::Colour c) {
            g.setColour (c);
            const float pw = textWidth (font (fh, Weight::ExtraBold), s);
            g.drawText (s, juce::Rectangle<float> (x, r.getY(), pw + 2.0f, ch), juce::Justification::centredLeft, false);
            x += pw;
        };
        const auto bar = juce::String::fromUTF8 ("\xe2\x96\xac");
        part ("INPUT ", col::plum); part (bar, juce::Colour (0xffb39ab8));
        part ("   OUTPUT ", col::plum); part (bar, juce::Colour (0xffe06fb6));
        part ("   GAIN REDUCTION ", col::plum); part (bar, col::plum);
    }
    {
        const auto t = "CEILING " + juce::String (ceilingDb, 1) + " dBTP  " + juce::String::fromUTF8 ("\xc2\xb7")
                     + "  GR " + (lastGrDb >= 0.05f ? "-" + juce::String (lastGrDb, 1) : juce::String ("0.0")) + " dB";
        const float w = textWidth (font (fh, Weight::ExtraBold), t) + 18.0f * z;
        drawChip (g, juce::Rectangle<float> (W - w - 10.0f * z, 8.0f * z, w, ch), t, fh);
    }

    const auto& im = Images::get();
    drawImageW (g, im.grapes, -6.0f * z, H - 40.0f * z, 40.0f * z, 0.95f);
    drawImageW (g, im.leaf, W - 28.0f * z, H - 26.0f * z, 30.0f * z);
}

// ================================================================ MonoDisplay
void MonoDisplay::setValues (float f, float w)
{
    if (f != freq || w != width)
    {
        freq = f;
        width = w;
        repaint();
    }
}

void MonoDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawScreen (g, r, z);
    const float W = r.getWidth(), H = r.getHeight();
    const float x = (float) (std::log (freq / kMinF) / std::log (kMaxF / kMinF)) * W;
    juce::Path clip;
    clip.addRoundedRectangle (r.reduced (1.0f), 12.0f * z);
    g.saveState();
    g.reduceClipRegion (clip);
    g.setGradientFill (juce::ColourGradient (col::ledP.withAlpha (0.33f), 0, 0, col::ledP.withAlpha (0.13f), x, 0, false));
    g.fillRect (0.0f, 0.0f, x, H);
    g.restoreState();
    g.setColour (col::purple);
    g.fillRect (x - 1.0f, 2.0f, 2.0f, H - 4.0f);
    g.setFont (font (9.0f * z, Weight::Black));
    g.setColour (col::plum);
    g.drawText ("MONO " + juce::String (juce::roundToInt (100.0f - width)) + "%", juce::Rectangle<float> (6.0f * z, 0, x, H),
                juce::Justification::centredLeft, false);
    g.setColour (col::faint);
    g.drawText ("STEREO", juce::Rectangle<float> (x + 6.0f * z, 0, W - x, H), juce::Justification::centredLeft, false);
}

// ================================================================ PanDisplay
void PanDisplay::setValues (float p, float s)
{
    if (p != pan || s != spread)
    {
        pan = p;
        spread = s;
        repaint();
    }
}

void PanDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawScreen (g, r, z);
    const float W = r.getWidth(), H = r.getHeight();
    const float R = juce::jmin (W * 0.5f - 20.0f * z, H - 16.0f * z);
    const juce::Point<float> c (W * 0.5f, H - 7.0f * z);
    const float pi = juce::MathConstants<float>::pi;

    auto arc = [&] (float radius, float a0, float a1) {
        juce::Path p;
        p.addCentredArc (c.x, c.y, radius, radius, 0.0f, a0, a1, true);
        return p;
    };
    g.setColour (col::line);
    g.strokePath (arc (R, -pi * 0.5f, pi * 0.5f), juce::PathStrokeType (1.2f));
    g.strokePath (arc (R * 0.5f, -pi * 0.5f, pi * 0.5f), juce::PathStrokeType (1.2f));

    g.setFont (font (9.0f * z, Weight::Black));
    // Small ticks at the far left, centre and far right.
    g.setColour (col::faint);
    for (float pos : { -1.0f, 0.0f, 1.0f })
    {
        const float a = pos * pi * 0.5f;
        g.drawLine (c.x + (R + 3.0f * z) * std::sin (a), c.y - (R + 3.0f * z) * std::cos (a),
                    c.x + (R + 7.0f * z) * std::sin (a), c.y - (R + 7.0f * z) * std::cos (a), 1.2f);
    }

    const double sp = spread * 0.01, pn = pan * 0.01;
    const float posL = (float) juce::jlimit (-1.0, 1.0, pn - sp), posR = (float) juce::jlimit (-1.0, 1.0, pn + sp);
    // Position -1..+1 -> angle -90..+90 degrees (0 = top).
    auto angle = [&] (float pos) { return pos * pi * 0.5f; };
    auto pt = [&] (float pos) { return juce::Point<float> (c.x + R * std::sin (angle (pos)), c.y - R * std::cos (angle (pos))); };

    g.setColour (col::arc);
    g.strokePath (arc (R, angle (posL), angle (posR)), juce::PathStrokeType (3.0f * z, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    for (auto [pos, t, colr] : { std::tuple<float, const char*, juce::Colour> { posL, "L", col::purple }, { posR, "R", col::hot } })
    {
        const auto p = pt (pos);
        const auto dot = juce::Rectangle<float> (16.0f * z, 16.0f * z).withCentre (p);
        g.setColour (colr.withAlpha (0.3f));
        g.fillEllipse (dot.expanded (2.5f * z));
        g.setColour (colr);
        g.fillEllipse (dot);
        g.setColour (juce::Colours::white);
        g.drawText (t, dot, juce::Justification::centred, false);
    }
}
} // namespace pg::ui
