#include "Params.h"

namespace pg::params
{
namespace
{
using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;
using APB = juce::AudioParameterBool;
using Range = juce::NormalisableRange<float>;

// Logarithmic range (for frequency, time, Q, ratio).
Range logRange (float lo, float hi)
{
    return Range (lo, hi,
                  [] (float a, float b, float v) { return a * std::pow (b / a, v); },
                  [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
                  [] (float a, float b, float v) { return juce::jlimit (a, b, v); });
}

juce::String fmtNum (float v)
{
    const float a = std::abs (v);
    return juce::String (v, a >= 100.0f ? 0 : (a >= 10.0f ? 1 : 2));
}

juce::String fmtHz (float v, int)
{
    if (v >= 1000.0f)
        return juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz";
    return fmtNum (v) + " Hz";
}

juce::String fmtDb (float v, int) { return fmtNum (v) + " dB"; }
juce::String fmtDbSigned (float v, int) { return (v > 0.005f ? "+" : "") + fmtNum (v) + " dB"; }
juce::String fmtPct (float v, int) { return juce::String (v, 1) + " %"; }
juce::String fmtMs (float v, int) { return fmtNum (v) + " ms"; }

float parseNumber (const juce::String& t) { return t.retainCharacters ("0123456789.-+").getFloatValue(); }

float parseHz (const juce::String& t)
{
    const float v = parseNumber (t);
    return t.containsIgnoreCase ("k") ? v * 1000.0f : v;
}

std::unique_ptr<APF> fl (const juce::String& id, const juce::String& name, Range r, float def,
                          std::function<juce::String (float, int)> toText,
                          std::function<float (const juce::String&)> fromText = parseNumber)
{
    return std::make_unique<APF> (juce::ParameterID { id, 1 }, name, r, def,
                                  juce::AudioParameterFloatAttributes()
                                      .withStringFromValueFunction (std::move (toText))
                                      .withValueFromStringFunction (std::move (fromText)));
}

std::unique_ptr<APB> sw (const juce::String& id, const juce::String& name, bool def)
{
    return std::make_unique<APB> (juce::ParameterID { id, 1 }, name, def);
}

std::unique_ptr<APC> ch (const juce::String& id, const juce::String& name, juce::StringArray items, int def)
{
    return std::make_unique<APC> (juce::ParameterID { id, 1 }, name, items, def);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ---- 1. Bus EQ ----
    auto eq = std::make_unique<juce::AudioProcessorParameterGroup> ("eq", "Bus EQ", "|");
    eq->addChild (sw ("eq_on", "EQ On", true));
    eq->addChild (fl ("eq_hp", "EQ HPF", logRange (40.0f, 150.0f), 40.0f, fmtHz, parseHz));
    eq->addChild (ch ("eq_hp_slope", "EQ HPF Slope", { "12 dB/oct", "24 dB/oct" }, 0));
    eq->addChild (fl ("eq_lp", "EQ LPF", logRange (6000.0f, 16000.0f), 16000.0f, fmtHz, parseHz));
    eq->addChild (fl ("eq_out", "EQ Output", Range (-12.0f, 12.0f), 0.0f, fmtDbSigned));
    const float defF[4] = { 100.0f, 400.0f, 2500.0f, 6000.0f };
    const int defT[4] = { 0, 0, 0, 3 };
    for (int b = 0; b < 4; ++b)
    {
        const auto n = "EQ Band " + juce::String (b + 1) + " ";
        eq->addChild (fl (bandId (b, "freq"), n + "Freq", logRange (20.0f, 20000.0f), defF[b], fmtHz, parseHz));
        eq->addChild (fl (bandId (b, "gain"), n + "Gain", Range (-18.0f, 18.0f), 0.0f, fmtDbSigned));
        eq->addChild (fl (bandId (b, "q"), n + "Q", logRange (0.1f, 20.0f), 1.0f,
                          [] (float v, int) { return fmtNum (v); }));
        eq->addChild (ch (bandId (b, "type"), n + "Type", { "Bell", "Notch", "Low Shelf", "High Shelf" }, defT[b]));
        eq->addChild (ch (bandId (b, "target"), n + "Target", { "Stereo", "Mid", "Side" }, 0));
    }
    layout.add (std::move (eq));

    // ---- 2. Bus Compressor ----
    auto comp = std::make_unique<juce::AudioProcessorParameterGroup> ("comp", "Bus Compressor", "|");
    comp->addChild (sw ("comp_on", "Comp On", true));
    comp->addChild (fl ("comp_split", "Comp Split", logRange (80.0f, 350.0f), 160.0f, fmtHz, parseHz));
    comp->addChild (fl ("comp_thresh", "Comp Threshold", Range (-48.0f, 0.0f), 0.0f, fmtDb));
    comp->addChild (fl ("comp_ratio", "Comp Ratio", logRange (1.0f, 20.0f), 3.0f,
                        [] (float v, int) { return juce::String (v, v >= 10.0f ? 1 : 2) + ":1"; }));
    comp->addChild (fl ("comp_attack", "Comp Attack", logRange (0.1f, 100.0f), 10.0f, fmtMs));
    comp->addChild (fl ("comp_release", "Comp Release", logRange (10.0f, 1000.0f), 120.0f, fmtMs));
    comp->addChild (fl ("comp_sc", "Comp SC HPF", logRange (20.0f, 300.0f), 20.0f,
                        [] (float v, int i) { return v <= 20.5f ? juce::String ("OFF") : fmtHz (v, i); }, parseHz));
    comp->addChild (fl ("comp_link", "Comp Stereo Link", Range (0.0f, 100.0f), 100.0f, fmtPct));
    comp->addChild (fl ("comp_makeup", "Comp Makeup", Range (0.0f, 12.0f), 0.0f, fmtDbSigned));
    layout.add (std::move (comp));

    // ---- 3. Wider ----
    auto wide = std::make_unique<juce::AudioProcessorParameterGroup> ("wide", "Wider", "|");
    wide->addChild (sw ("wide_on", "Wider On", true));
    wide->addChild (fl ("wide_width", "Wider Width", Range (0.0f, 200.0f), 100.0f, fmtPct));
    wide->addChild (fl ("wide_air", "Wider Side Air", Range (0.0f, 9.0f), 0.0f, fmtDbSigned));
    wide->addChild (fl ("wide_cut", "Wider Side Cut", logRange (20.0f, 400.0f), 20.0f,
                        [] (float v, int i) { return v <= 20.5f ? juce::String ("OFF") : fmtHz (v, i); }, parseHz));
    layout.add (std::move (wide));

    // ---- 4. Mono-Maker ----
    auto mono = std::make_unique<juce::AudioProcessorParameterGroup> ("mono", "Mono-Maker", "|");
    mono->addChild (sw ("mono_on", "Mono On", true));
    mono->addChild (fl ("mono_freq", "Mono Freq", logRange (60.0f, 250.0f), 120.0f, fmtHz, parseHz));
    mono->addChild (fl ("mono_width", "Mono Low Width", Range (0.0f, 100.0f), 100.0f, fmtPct));
    layout.add (std::move (mono));

    // ---- 5. True Stereo Pan ----
    auto pan = std::make_unique<juce::AudioProcessorParameterGroup> ("pan", "True Stereo Pan", "|");
    pan->addChild (sw ("pan_on", "Pan On", true));
    pan->addChild (fl ("pan_pan", "Pan", Range (-100.0f, 100.0f), 0.0f,
                       [] (float v, int) {
                           const int r = juce::roundToInt (v);
                           return r == 0 ? juce::String ("C") : (r < 0 ? "L" + juce::String (-r) : "R" + juce::String (r));
                       },
                       [] (const juce::String& t) {
                           const float v = std::abs (parseNumber (t));
                           return t.trim().startsWithIgnoreCase ("L") ? -v : v;
                       }));
    pan->addChild (fl ("pan_spread", "Pan Spread", Range (0.0f, 100.0f), 100.0f, fmtPct));
    layout.add (std::move (pan));

    // ---- 6. Lofi ----
    auto lofi = std::make_unique<juce::AudioProcessorParameterGroup> ("lofi", "Lofi", "|");
    lofi->addChild (sw ("lofi_on", "Lofi Module On", true));
    lofi->addChild (sw ("lofi_active", "Lofi", false));
    lofi->addChild (fl ("lofi_tone", "Lofi Tone", logRange (300.0f, 6000.0f), 1800.0f, fmtHz, parseHz));
    lofi->addChild (fl ("lofi_bits", "Lofi Bits", Range (4.0f, 16.0f), 10.0f,
                        [] (float v, int) { return juce::String (v, 1) + " bit"; }));
    lofi->addChild (fl ("lofi_rate", "Lofi Rate", logRange (2000.0f, 48000.0f), 16000.0f,
                        [] (float v, int) { return juce::String (v / 1000.0f, 1) + " kHz"; }, parseHz));
    lofi->addChild (fl ("lofi_noise", "Lofi Noise", Range (0.0f, 100.0f), 15.0f, fmtPct));
    lofi->addChild (fl ("lofi_wobble", "Lofi Wobble", Range (0.0f, 100.0f), 20.0f, fmtPct));
    lofi->addChild (fl ("lofi_drive", "Lofi Drive", Range (0.0f, 24.0f), 6.0f, fmtDb));
    lofi->addChild (fl ("lofi_mix", "Lofi Mix", Range (0.0f, 100.0f), 100.0f, fmtPct));
    layout.add (std::move (lofi));

    // ---- 7. Bus Limiter ----
    auto lim = std::make_unique<juce::AudioProcessorParameterGroup> ("lim", "Bus Limiter", "|");
    lim->addChild (sw ("lim_on", "Limiter On", true));
    lim->addChild (fl ("lim_gain", "Limiter Gain", Range (0.0f, 18.0f), 0.0f, fmtDb));
    lim->addChild (fl ("lim_clip", "Limiter Soft Clip", Range (0.0f, 100.0f), 0.0f, fmtPct));
    lim->addChild (fl ("lim_ceiling", "Limiter Ceiling", Range (-12.0f, 0.0f), -0.3f, fmtDb));
    lim->addChild (fl ("lim_release", "Limiter Release", logRange (5.0f, 500.0f), 60.0f, fmtMs));
    lim->addChild (fl ("lim_adapt", "Limiter Adapt", Range (0.0f, 100.0f), 70.0f, fmtPct));
    lim->addChild (fl ("lim_out", "Limiter Output", Range (-12.0f, 0.0f), 0.0f, fmtDb));
    lim->addChild (ch ("lim_look", "Limiter Lookahead", { "LIVE 0", "1.5 ms", "5 ms" }, 0));
    lim->addChild (ch ("lim_os", "Limiter Oversampling", { "4x", "8x", "16x" }, 0));
    layout.add (std::move (lim));

    // ---- 8. Auto-Gain ----
    auto ag = std::make_unique<juce::AudioProcessorParameterGroup> ("ag", "Auto-Gain", "|");
    ag->addChild (sw ("ag_on", "Auto-Gain On", false));
    ag->addChild (fl ("ag_speed", "Auto-Gain Speed", logRange (0.3f, 10.0f), 3.0f,
                      [] (float v, int) { return juce::String (v, v >= 10.0f ? 1 : 2) + " s"; }));
    ag->addChild (fl ("ag_range", "Auto-Gain Range", Range (0.0f, 24.0f), 12.0f, fmtDb));
    ag->addChild (fl ("ag_trim", "Auto-Gain Trim", Range (-12.0f, 12.0f), 0.0f, fmtDbSigned));
    ag->addChild (ch ("ag_detect", "Auto-Gain Detector", { "LUFS", "RMS" }, 0));
    ag->addChild (sw ("ag_hold", "Auto-Gain Hold", false));
    layout.add (std::move (ag));

    return layout;
}

Reader::Reader (juce::AudioProcessorValueTreeState& apvts) : state (apvts)
{
    for (int m = 0; m < kNumModules; ++m)
        on[(size_t) m] = p (kModuleOn[m]);
    eqHp = p ("eq_hp"); eqSlope = p ("eq_hp_slope"); eqLp = p ("eq_lp"); eqOut = p ("eq_out");
    for (int b = 0; b < 4; ++b)
    {
        bFreq[(size_t) b] = p (bandId (b, "freq"));
        bGain[(size_t) b] = p (bandId (b, "gain"));
        bQ[(size_t) b] = p (bandId (b, "q"));
        bType[(size_t) b] = p (bandId (b, "type"));
        bTarget[(size_t) b] = p (bandId (b, "target"));
    }
    cSplit = p ("comp_split"); cThr = p ("comp_thresh"); cRatio = p ("comp_ratio"); cAtt = p ("comp_attack");
    cRel = p ("comp_release"); cSc = p ("comp_sc"); cLink = p ("comp_link"); cMk = p ("comp_makeup");
    wWidth = p ("wide_width"); wAir = p ("wide_air"); wCut = p ("wide_cut");
    mFreq = p ("mono_freq"); mWidth = p ("mono_width");
    pPan = p ("pan_pan"); pSpread = p ("pan_spread");
    lActive = p ("lofi_active"); lTone = p ("lofi_tone"); lBits = p ("lofi_bits"); lRate = p ("lofi_rate");
    lNoise = p ("lofi_noise"); lWobble = p ("lofi_wobble"); lDrive = p ("lofi_drive"); lMix = p ("lofi_mix");
    limGain = p ("lim_gain"); limClip = p ("lim_clip"); limCeil = p ("lim_ceiling"); limRel = p ("lim_release");
    limAdapt = p ("lim_adapt"); limOut = p ("lim_out"); limLook = p ("lim_look"); limOs = p ("lim_os");
    agSpeed = p ("ag_speed"); agRange = p ("ag_range"); agTrim = p ("ag_trim"); agDetect = p ("ag_detect"); agHold = p ("ag_hold");
}

std::atomic<float>* Reader::p (const juce::String& id) const
{
    auto* v = state.getRawParameterValue (id);
    jassert (v != nullptr);
    return v;
}

void Reader::read (EngineSettings& s) const noexcept
{
    auto v = [] (std::atomic<float>* a) { return (double) a->load (std::memory_order_relaxed); };
    auto i = [] (std::atomic<float>* a) { return juce::roundToInt (a->load (std::memory_order_relaxed)); };

    for (size_t m = 0; m < (size_t) kNumModules; ++m)
        s.on[m] = v (on[m]) > 0.5;

    s.eq.hpFreq = v (eqHp);
    s.eq.hpSteep = i (eqSlope) == 1;
    s.eq.lpFreq = v (eqLp);
    s.eq.outDb = v (eqOut);
    for (size_t b = 0; b < 4; ++b)
    {
        auto& d = s.eq.bands[b];
        d.freq = v (bFreq[b]);
        d.gainDb = v (bGain[b]);
        d.q = v (bQ[b]);
        d.type = (EqBandType) juce::jlimit (0, 3, i (bType[b]));
        d.target = (EqTarget) juce::jlimit (0, 2, i (bTarget[b]));
    }

    s.comp.splitHz = v (cSplit);
    s.comp.thresholdDb = v (cThr);
    s.comp.ratio = v (cRatio);
    s.comp.attackMs = v (cAtt);
    s.comp.releaseMs = v (cRel);
    s.comp.scHpfHz = v (cSc);
    s.comp.link = v (cLink) * 0.01;
    s.comp.makeupDb = v (cMk);

    s.wide.width = v (wWidth) * 0.01;
    s.wide.airDb = v (wAir);
    s.wide.sideCutHz = v (wCut);

    s.mono.freqHz = v (mFreq);
    s.mono.width = v (mWidth) * 0.01;

    s.pan.pan = v (pPan) * 0.01;
    s.pan.spread = v (pSpread) * 0.01;

    s.lofi.active = v (lActive) > 0.5;
    s.lofi.toneHz = v (lTone);
    s.lofi.bits = v (lBits);
    s.lofi.rateHz = v (lRate);
    s.lofi.noise = v (lNoise) * 0.01;
    s.lofi.wobble = v (lWobble) * 0.01;
    s.lofi.driveDb = v (lDrive);
    s.lofi.mix = v (lMix) * 0.01;

    s.lim.inGainDb = v (limGain);
    s.lim.clip = v (limClip) * 0.01;
    s.lim.ceilingDb = v (limCeil);
    s.lim.releaseMs = v (limRel);
    s.lim.adapt = v (limAdapt) * 0.01;
    s.lim.outDb = v (limOut);
    s.lim.lookaheadIndex = i (limLook);
    s.lim.osIndex = i (limOs);

    s.ag.speedSec = v (agSpeed);
    s.ag.rangeDb = v (agRange);
    s.ag.trimDb = v (agTrim);
    s.ag.lufs = i (agDetect) == 0;
    s.ag.hold = v (agHold) > 0.5;
}
} // namespace pg::params
