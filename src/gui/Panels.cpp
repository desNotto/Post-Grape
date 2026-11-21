#include "Panels.h"
#include "Params.h"

namespace pg::ui
{
namespace
{
// Level meter scale: -60 dBFS .. 0 dBFS -> 0 .. 1.
float levelToMeter (float lin) { return juce::jlimit (0.0f, 1.0f, 1.0f + juce::Decibels::gainToDecibels (lin, -100.0f) / 60.0f); }

// Peak hold with fall-back for meters (called at 30 Hz).
float ballistic (float shown, float target, float fall = 0.82f) { return target > shown ? target : shown * fall + target * (1.0f - fall); }
} // namespace

// ================================================================ ModulePanel
ModulePanel::ModulePanel (juce::AudioProcessorValueTreeState& state, const juce::String& t, const juce::String& tg,
                          const juce::String& powerParam, bool leaf)
    : apvts (state), title (t), tag (tg), leafDeco (leaf)
{
    addAndMakeVisible (power);
    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, powerParam, power);
    power.setTooltip (t + " on / off");
}

void ModulePanel::addBody (juce::Component& c)
{
    addAndMakeVisible (c);
    body.push_back (&c);
}

void ModulePanel::refresh()
{
    const bool on = power.getToggleState();
    if (on != lastOn)
    {
        lastOn = on;
        for (auto* c : body)
            c->setAlpha (on ? 1.0f : 0.38f);
    }
}

juce::Point<int> ModulePanel::rowSize (std::initializer_list<Knob*> knobs, float diameter, float zz, float gap)
{
    int w = 0, h = 0;
    for (auto* k : knobs)
    {
        k->setGeometry (diameter, zz);
        const auto s = k->preferredSize();
        w += s.x;
        h = juce::jmax (h, s.y);
    }
    w += juce::roundToInt (gap * (float) (knobs.size() - 1));
    return { w, h };
}

int ModulePanel::layoutRow (std::initializer_list<Knob*> knobs, juce::Rectangle<int> area, int y, float diameter, float gap)
{
    const auto size = rowSize (knobs, diameter, z, gap);
    float x = (float) area.getCentreX() - (float) size.x * 0.5f;
    for (auto* k : knobs)
    {
        const auto s = k->preferredSize();
        k->setBounds (juce::roundToInt (x), y + size.y - s.y, s.x, s.y); // align on the bottom
        x += (float) s.x + gap;
    }
    return size.y;
}

void ModulePanel::resized()
{
    const int pw = juce::roundToInt (28.0f * z);
    power.setBounds (juce::roundToInt (18.0f * z), juce::roundToInt (9.0f * z), pw, pw);
    auto b = getLocalBounds();
    b.removeFromTop (juce::roundToInt (40.0f * z));
    b = b.reduced (juce::roundToInt (12.0f * z), 0);
    b.removeFromBottom (juce::roundToInt (10.0f * z));
    layoutBody (b);
}

void ModulePanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float rad = 14.0f * z;
    juce::Path frame;
    frame.addRoundedRectangle (r, rad);
    juce::DropShadow (col::plum.withAlpha (0.10f), juce::roundToInt (8.0f * z), { 0, juce::roundToInt (3.0f * z) }).drawForPath (g, frame);
    g.setGradientFill (juce::ColourGradient (col::panelTop, 0, r.getY(), col::panelBottom, 0, r.getBottom(), false));
    g.fillPath (frame);
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.drawRoundedRectangle (r.reduced (2.0f), rad - 2.0f, 3.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (r, rad, 1.0f);

    // Screws
    for (float sx : { r.getX() + 8.0f * z, r.getRight() - 15.0f * z })
    {
        auto s = juce::Rectangle<float> (sx, r.getY() + 8.0f * z, 7.0f * z, 7.0f * z);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, s.getX() + s.getWidth() * 0.35f, s.getY() + s.getHeight() * 0.35f,
                                                 juce::Colour (0xffd8b9d2), s.getRight(), s.getBottom(), true));
        g.fillEllipse (s);
        g.setColour (juce::Colour (0xffc9a3c3));
        g.drawEllipse (s, 1.0f);
    }

    // Title and tag
    const float hy = 9.0f * z, hh = 28.0f * z;
    g.setColour (col::deep);
    g.setFont (font (13.0f * z, Weight::Black).withExtraKerningFactor (0.12f));
    g.drawText (title.toUpperCase(), juce::Rectangle<float> (54.0f * z, hy, r.getWidth() * 0.6f, hh), juce::Justification::centredLeft, false);
    if (tag.isNotEmpty())
    {
        g.setColour (col::lav2);
        g.setFont (font (10.0f * z, Weight::ExtraBold).withExtraKerningFactor (0.08f));
        g.drawText (tag, juce::Rectangle<float> (r.getWidth() * 0.3f, hy, r.getWidth() * 0.7f - 28.0f * z, hh), juce::Justification::centredRight, false);
    }

    if (leafDeco)
        drawImageW (g, Images::get().leaf, r.getRight() - 32.0f * z, r.getBottom() - 30.0f * z, 30.0f * z, 0.9f);
}

// ================================================================ EQ
EqPanel::EqPanel (juce::AudioProcessorValueTreeState& s, std::function<double()> sampleRate)
    : ModulePanel (s, "Bus EQ", juce::String::fromUTF8 ("TPT SVF \xc2\xb7 64-BIT \xc2\xb7 MIN-PHASE"), "eq_on", true),
      display (s, std::move (sampleRate)),
      freq (s, params::bandId (0, "freq"), "FREQ"), gain (s, params::bandId (0, "gain"), "GAIN"), q (s, params::bandId (0, "q"), "Q"),
      hp (s, "eq_hp", "HPF"), lp (s, "eq_lp", "LPF"), out (s, "eq_out", "OUTPUT")
{
    for (juce::Component* c : { (juce::Component*) &display, (juce::Component*) &bandSel, (juce::Component*) &target,
                                (juce::Component*) &type, (juce::Component*) &slope, (juce::Component*) &freq, (juce::Component*) &gain,
                                (juce::Component*) &q, (juce::Component*) &hp, (juce::Component*) &lp, (juce::Component*) &out })
        addBody (*c);

    slope.bind (s, "eq_hp_slope");
    bandSel.onChange = [this] (int b) { selectBand (b); };
    display.onSelectBand = [this] (int b) { selectBand (b); };
    selectBand (0);
}

void EqPanel::selectBand (int b)
{
    band = juce::jlimit (0, 3, b);
    bandSel.setSelected (band, false);
    display.setSelectedBand (band);
    freq.bind (params::bandId (band, "freq"));
    gain.bind (params::bandId (band, "gain"));
    q.bind (params::bandId (band, "q"));
    target.bind (apvts, params::bandId (band, "target"));
    type.bind (apvts, params::bandId (band, "type"));
    freq.setLabel ("FREQ " + juce::String (band + 1));
    gain.setLabel ("GAIN " + juce::String (band + 1));
    q.setLabel ("Q " + juce::String (band + 1));
}

void EqPanel::refresh()
{
    ModulePanel::refresh();
    display.refresh();
}

void EqPanel::layoutBody (juce::Rectangle<int> b)
{
    const float d = 52.0f * z, gap = 8.0f * z;
    const auto rs = rowSize ({ &freq, &gain, &q }, d, z, gap);
    auto right = b.removeFromRight (rs.x + juce::roundToInt (10.0f * z));
    b.removeFromRight (juce::roundToInt (10.0f * z));

    // Right column: band knobs, filter knobs, HPF slope.
    for (auto* s : { &bandSel, &target, &type, &slope })
        s->setZ (z);
    const int segH = slope.preferredHeight();
    const int colH = rs.y * 2 + juce::roundToInt (6.0f * z) + segH + juce::roundToInt (8.0f * z);
    int y = right.getY() + juce::jmax (0, (right.getHeight() - colH) / 2);
    y += layoutRow ({ &freq, &gain, &q }, right, y, d, gap) + juce::roundToInt (6.0f * z);
    y += layoutRow ({ &hp, &lp, &out }, right, y, d, gap) + juce::roundToInt (8.0f * z);
    const int sw = slope.preferredWidth();
    slope.setBounds (hp.getBounds().getCentreX() - juce::roundToInt (16.0f * z), y, sw, segH);

    // Left: display + selector row.
    auto ctl = b.removeFromBottom (segH);
    b.removeFromBottom (juce::roundToInt (6.0f * z));
    display.setZ (z);
    display.setBounds (b);
    int x = ctl.getX();
    for (auto* s : { &bandSel, &target, &type })
    {
        const int w = s->preferredWidth();
        s->setBounds (x, ctl.getY(), w, segH);
        x += w + juce::roundToInt (8.0f * z);
    }
}

// ================================================================ Compressor
CompPanel::CompPanel (juce::AudioProcessorValueTreeState& s, std::atomic<float>& grDb)
    : ModulePanel (s, "Bus Compressor", "LOW BAND ONLY", "comp_on"),
      split (s, "comp_split", "SPLIT", true), thresh (s, "comp_thresh", "THRESH", true), ratio (s, "comp_ratio", "RATIO"),
      makeup (s, "comp_makeup", "MAKEUP"), attack (s, "comp_attack", "ATTACK"), release (s, "comp_release", "RELEASE"),
      sc (s, "comp_sc", "SC HPF"), link (s, "comp_link", "LINK"), grSource (grDb)
{
    for (auto* k : { &split, &thresh, &ratio, &makeup, &attack, &release, &sc, &link })
        addBody (*k);
    addBody (gr);
}

void CompPanel::refresh()
{
    ModulePanel::refresh();
    grShown = ballistic (grShown, grSource.load());
    gr.setValue (grShown / 12.0f);
}

void CompPanel::layoutBody (juce::Rectangle<int> b)
{
    auto m = b.removeFromRight (juce::roundToInt (18.0f * z));
    gr.setBounds (m.withTrimmedTop (juce::roundToInt (4.0f * z)).withTrimmedBottom (juce::roundToInt (4.0f * z)));
    b.removeFromRight (juce::roundToInt (8.0f * z));

    const float d1 = 58.0f * z, d2 = 46.0f * z, gap = 10.0f * z;
    const auto r1 = rowSize ({ &split, &thresh, &ratio, &makeup }, d1, z, gap);
    const auto r2 = rowSize ({ &attack, &release, &sc, &link }, d2, z, gap);
    const int total = r1.y + r2.y + juce::roundToInt (8.0f * z);
    int y = b.getY() + juce::jmax (0, (b.getHeight() - total) / 2);
    y += layoutRow ({ &split, &thresh, &ratio, &makeup }, b, y, d1, gap) + juce::roundToInt (8.0f * z);
    layoutRow ({ &attack, &release, &sc, &link }, b, y, d2, gap);
}

// ================================================================ Wider
WiderPanel::WiderPanel (juce::AudioProcessorValueTreeState& s, std::atomic<float>& corr)
    : ModulePanel (s, "Wider", "", "wide_on"),
      width (s, "wide_width", "WIDTH", true), air (s, "wide_air", "SIDE AIR"), cut (s, "wide_cut", "SIDE CUT"), corrSource (corr)
{
    for (auto* k : { &width, &air, &cut })
        addBody (*k);
    addBody (corrMeter);
}

void WiderPanel::refresh()
{
    ModulePanel::refresh();
    corrMeter.setValue (corrSource.load());
}

void WiderPanel::layoutBody (juce::Rectangle<int> b)
{
    auto m = b.removeFromRight (juce::roundToInt (30.0f * z));
    const int mh = juce::jmin (m.getHeight() - juce::roundToInt (18.0f * z), juce::roundToInt (110.0f * z));
    corrMeter.setBounds (m.getCentreX() - juce::roundToInt (9.0f * z), m.getY() + (m.getHeight() - mh - juce::roundToInt (14.0f * z)) / 2,
                         juce::roundToInt (18.0f * z), mh);
    const float d = 50.0f * z, gap = 6.0f * z;
    const auto rs = rowSize ({ &width, &air, &cut }, d, z, gap);
    layoutRow ({ &width, &air, &cut }, b, b.getY() + (b.getHeight() - rs.y) / 2, d, gap);
}

void WiderPanel::paintOverChildren (juce::Graphics& g)
{
    g.setColour (col::muted);
    g.setFont (font (10.0f * z, Weight::Bold));
    auto r = corrMeter.getBounds().toFloat();
    g.drawText ("CORR", juce::Rectangle<float> (r.getCentreX() - 20.0f * z, r.getBottom() + 2.0f * z, 40.0f * z, 13.0f * z),
                juce::Justification::centred, false);
}

// ================================================================ Mono-Maker
MonoPanel::MonoPanel (juce::AudioProcessorValueTreeState& s)
    : ModulePanel (s, "Mono-Maker", "", "mono_on"),
      freq (s, "mono_freq", "FREQ", true), width (s, "mono_width", "LOW WIDTH")
{
    addBody (freq);
    addBody (width);
    addBody (display);
}

void MonoPanel::refresh()
{
    ModulePanel::refresh();
    display.setValues (apvts.getRawParameterValue ("mono_freq")->load(), apvts.getRawParameterValue ("mono_width")->load());
}

void MonoPanel::layoutBody (juce::Rectangle<int> b)
{
    const float d = 54.0f * z, gap = 18.0f * z;
    const auto rs = rowSize ({ &freq, &width }, d, z, gap);
    layoutRow ({ &freq, &width }, b, b.getY(), d, gap);
    b.removeFromTop (rs.y + juce::roundToInt (6.0f * z));
    display.setZ (z);
    display.setBounds (b.withHeight (juce::jmin (b.getHeight(), juce::roundToInt (48.0f * z))));
}

// ================================================================ True Pan
PanPanel::PanPanel (juce::AudioProcessorValueTreeState& s)
    : ModulePanel (s, "True Stereo Pan", "", "pan_on"),
      pan (s, "pan_pan", "PAN"), spread (s, "pan_spread", "SPREAD")
{
    addBody (display);
    addBody (pan);
    addBody (spread);
}

void PanPanel::refresh()
{
    ModulePanel::refresh();
    display.setValues (apvts.getRawParameterValue ("pan_pan")->load(), apvts.getRawParameterValue ("pan_spread")->load());
}

void PanPanel::layoutBody (juce::Rectangle<int> b)
{
    const float d = 50.0f * z, gap = 18.0f * z;
    const auto rs = rowSize ({ &pan, &spread }, d, z, gap);
    layoutRow ({ &pan, &spread }, b, b.getBottom() - rs.y, d, gap);
    b.removeFromBottom (rs.y + juce::roundToInt (6.0f * z));
    display.setZ (z);
    display.setBounds (b);
}

// ================================================================ Lofi
LofiPanel::LofiPanel (juce::AudioProcessorValueTreeState& s)
    : ModulePanel (s, "Lofi", "AUTOMATE ON / OFF", "lofi_on"),
      mix (s, "lofi_mix", "MIX"), tone (s, "lofi_tone", "TONE"), bits (s, "lofi_bits", "BITS"), rate (s, "lofi_rate", "RATE"),
      noise (s, "lofi_noise", "NOISE"), wobble (s, "lofi_wobble", "WOBBLE"), drive (s, "lofi_drive", "DRIVE")
{
    addBody (sw);
    swAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, "lofi_active", sw);
    sw.onStateChange = [this] { repaint(); };
    for (auto* k : { &mix, &tone, &bits, &rate, &noise, &wobble, &drive })
        addBody (*k);
}

void LofiPanel::refresh() { ModulePanel::refresh(); }

void LofiPanel::layoutBody (juce::Rectangle<int> b)
{
    const float d = 42.0f * z, gap = 6.0f * z;
    auto left = b.removeFromLeft (juce::roundToInt (122.0f * z));
    b.removeFromLeft (juce::roundToInt (6.0f * z));

    // Left column: switch, label, mix.
    const int swW = juce::roundToInt (114.0f * z), swH = juce::roundToInt (42.0f * z);
    const auto mixSize = rowSize ({ &mix }, d, z, gap);
    const int colH = swH + juce::roundToInt (18.0f * z) + mixSize.y;
    int y = left.getY() + juce::jmax (0, (left.getHeight() - colH) / 2);
    sw.setBounds (left.getCentreX() - swW / 2, y, swW, swH);
    labelArea = juce::Rectangle<int> (left.getX(), y + swH, left.getWidth(), juce::roundToInt (16.0f * z));
    layoutRow ({ &mix }, left, y + swH + juce::roundToInt (18.0f * z), d, gap);

    // Right: 3 x 2 grid.
    const auto r1 = rowSize ({ &tone, &bits, &rate }, d, z, gap);
    const int total = r1.y * 2 + juce::roundToInt (4.0f * z);
    int yy = b.getY() + juce::jmax (0, (b.getHeight() - total) / 2);
    yy += layoutRow ({ &tone, &bits, &rate }, b, yy, d, gap) + juce::roundToInt (4.0f * z);
    layoutRow ({ &noise, &wobble, &drive }, b, yy, d, gap);
}

void LofiPanel::paintOverChildren (juce::Graphics& g)
{
    g.setColour (col::plum.withAlpha (sw.getAlpha()));
    g.setFont (font (10.0f * z, Weight::Black).withExtraKerningFactor (0.1f));
    g.drawText (sw.getToggleState() ? "LOFI ON" : "LOFI OFF", labelArea, juce::Justification::centred, false);
}

// ================================================================ Limiter
LimiterPanel::LimiterPanel (juce::AudioProcessorValueTreeState& s, pg::Limiter& limiter)
    : ModulePanel (s, "Bus Limiter", juce::String::fromUTF8 ("SOFT CLIP \xe2\x86\x92 TRUE-PEAK BRICKWALL \xc2\xb7 OVERSAMPLED"), "lim_on"),
      lim (limiter), scope (limiter.scope),
      gain (s, "lim_gain", "GAIN", true), clip (s, "lim_clip", "SOFT CLIP", true), ceiling (s, "lim_ceiling", "CEILING", true),
      release (s, "lim_release", "RELEASE"), adapt (s, "lim_adapt", "ADAPT"), output (s, "lim_out", "OUTPUT")
{
    addBody (scope);
    for (auto* k : { &gain, &clip, &ceiling, &release, &adapt, &output })
        addBody (*k);
    addBody (look);
    addBody (os);
    for (auto* m : { &inL, &inR, &grM, &outL, &outR })
        addBody (*m);
    look.bind (s, "lim_look");
    os.bind (s, "lim_os");
}

void LimiterPanel::refresh()
{
    ModulePanel::refresh();
    scope.setCeilingDb (apvts.getRawParameterValue ("lim_ceiling")->load());
    scope.refresh();

    const float vals[5] = { lim.inPeak[0].exchange (0.0f), lim.inPeak[1].exchange (0.0f), lim.grDb.load(),
                            lim.outPeak[0].exchange (0.0f), lim.outPeak[1].exchange (0.0f) };
    for (int i = 0; i < 5; ++i)
        shown[i] = ballistic (shown[i], vals[i]);
    inL.setValue (levelToMeter (shown[0]));
    inR.setValue (levelToMeter (shown[1]));
    grM.setValue (shown[2] / 12.0f);
    outL.setValue (levelToMeter (shown[3]));
    outR.setValue (levelToMeter (shown[4]));
}

void LimiterPanel::layoutBody (juce::Rectangle<int> b)
{
    // Meters on the right.
    const int mw = juce::roundToInt (16.0f * z), mg = juce::roundToInt (6.0f * z);
    meterArea = b.removeFromRight (mw * 5 + mg * 4);
    auto mcol = meterArea.withTrimmedBottom (juce::roundToInt (16.0f * z)).withTrimmedTop (juce::roundToInt (2.0f * z));
    int mx = mcol.getX();
    for (auto* m : { &inL, &inR, &grM, &outL, &outR })
    {
        m->setBounds (mx, mcol.getY(), mw, mcol.getHeight());
        mx += mw + mg;
    }
    b.removeFromRight (juce::roundToInt (14.0f * z));

    // Lookahead / oversampling column (sized to its content).
    look.setZ (z);
    os.setZ (z);
    const int segW = juce::jmax (look.preferredWidth(), os.preferredWidth());
    auto ctl = b.removeFromRight (segW);
    b.removeFromRight (juce::roundToInt (14.0f * z));
    const int lh = juce::roundToInt (16.0f * z), segH = look.preferredHeight(), g6 = juce::roundToInt (6.0f * z);
    const int ctlH = (lh + segH) * 2 + g6 * 3;
    int y = ctl.getY() + (ctl.getHeight() - ctlH) / 2;
    lookLabel = { ctl.getX(), y, segW, lh };
    y += lh + g6 / 2;
    look.setBounds (ctl.getX(), y, look.preferredWidth(), segH);
    y += segH + g6 * 2;
    osLabel = { ctl.getX(), y, segW, lh };
    y += lh + g6 / 2;
    os.setBounds (ctl.getX(), y, os.preferredWidth(), segH);

    // Knobs: big row, small row.
    const float d1 = 58.0f * z, d2 = 46.0f * z, gap = 12.0f * z;
    const auto r1 = rowSize ({ &gain, &clip, &ceiling }, d1, z, gap);
    const auto r2 = rowSize ({ &release, &adapt, &output }, d2, z, gap);
    auto knobs = b.removeFromRight (juce::jmax (r1.x, r2.x) + juce::roundToInt (8.0f * z));
    b.removeFromRight (juce::roundToInt (14.0f * z));
    const int total = r1.y + r2.y + juce::roundToInt (4.0f * z);
    int ky = knobs.getY() + juce::jmax (0, (knobs.getHeight() - total) / 2);
    ky += layoutRow ({ &gain, &clip, &ceiling }, knobs, ky, d1, gap) + juce::roundToInt (4.0f * z);
    layoutRow ({ &release, &adapt, &output }, knobs, ky, d2, gap);

    scope.setZ (z);
    scope.setBounds (b);
}

void LimiterPanel::paintOverChildren (juce::Graphics& g)
{
    g.setColour (col::plum.withAlpha (look.getAlpha()));
    g.setFont (font (10.0f * z, Weight::Black).withExtraKerningFactor (0.1f));
    g.drawText ("LOOKAHEAD", lookLabel, juce::Justification::centredLeft, false);
    g.drawText ("OVERSAMPLING", osLabel, juce::Justification::centredLeft, false);

    g.setColour (col::muted.withAlpha (look.getAlpha()));
    g.setFont (font (9.0f * z, Weight::Black));
    const int ly = inL.getBottom() + juce::roundToInt (2.0f * z), lh = juce::roundToInt (13.0f * z);
    g.drawText ("IN", juce::Rectangle<int> (inL.getX(), ly, inR.getRight() - inL.getX(), lh), juce::Justification::centred, false);
    g.drawText ("GR", juce::Rectangle<int> (grM.getX() - 6, ly, grM.getWidth() + 12, lh), juce::Justification::centred, false);
    g.drawText ("OUT", juce::Rectangle<int> (outL.getX() - 4, ly, outR.getRight() - outL.getX() + 8, lh), juce::Justification::centred, false);
}
} // namespace pg::ui
