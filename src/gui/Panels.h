// Post-Grape — module panels (one panel for each module).
#pragma once

#include "Displays.h"
#include "dsp/AutoGain.h"
#include "Widgets.h"

class PostGrapeProcessor;

namespace pg::ui
{
// Frame with screws, title, tag and a power button. Subclasses add the body.
class ModulePanel : public juce::Component, public HintSource
{
public:
    ModulePanel (juce::AudioProcessorValueTreeState& state, const juce::String& moduleKey, const juce::String& title,
                 const juce::String& tag, const juce::String& powerParam, bool leaf = false);
    bool getHint (juce::Point<int>, juce::String& key, juce::String& label) const override;

    void setZ (float zIn) { z = zIn; }
    void paint (juce::Graphics&) override;
    void resized() override;
    // Called from the editor timer.
    virtual void refresh();

protected:
    virtual void layoutBody (juce::Rectangle<int> body) = 0;
    void addBody (juce::Component& c);
    // Place same-size knobs in a centred row. Returns the row height.
    int layoutRow (std::initializer_list<Knob*> knobs, juce::Rectangle<int> area, int y, float diameter, float gap);
    static juce::Point<int> rowSize (std::initializer_list<Knob*> knobs, float diameter, float z, float gap);

    juce::AudioProcessorValueTreeState& apvts;
    float z = 1.0f;

private:
    juce::String key, title, tag;
    bool leafDeco;
    PowerButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    std::vector<juce::Component*> body;
    bool lastOn = true;
};

class EqPanel : public ModulePanel
{
public:
    EqPanel (juce::AudioProcessorValueTreeState&, std::function<double()> sampleRate);
    void refresh() override;

private:
    void layoutBody (juce::Rectangle<int>) override;
    void selectBand (int b);

    EqDisplay display;
    Segmented bandSel { { "1", "2", "3", "4" } };
    Segmented target { { "STEREO", "MID", "SIDE" } };
    Segmented type { { "BELL", "NOTCH", "LO SHELF", "HI SHELF" } };
    Segmented slope { { "12 dB", "24 dB" } };
    Knob freq, gain, q, hp, lp, out;
    int band = 0;
};

class CompPanel : public ModulePanel
{
public:
    CompPanel (juce::AudioProcessorValueTreeState&, std::atomic<float>& grDb);
    void refresh() override;

private:
    void layoutBody (juce::Rectangle<int>) override;
    Knob split, thresh, ratio, makeup, attack, release, sc, link;
    LedMeter gr { 14, true };
    std::atomic<float>& grSource;
    float grShown = 0.0f;
};

class WiderPanel : public ModulePanel
{
public:
    WiderPanel (juce::AudioProcessorValueTreeState&, std::atomic<float>& corr);
    void refresh() override;
    void paintOverChildren (juce::Graphics&) override;

private:
    void layoutBody (juce::Rectangle<int>) override;
    Knob width, air, cut;
    CorrMeter corrMeter;
    std::atomic<float>& corrSource;
};

class MonoPanel : public ModulePanel
{
public:
    explicit MonoPanel (juce::AudioProcessorValueTreeState&);
    void refresh() override;

private:
    void layoutBody (juce::Rectangle<int>) override;
    Knob freq, width;
    MonoDisplay display;
};

class PanPanel : public ModulePanel
{
public:
    explicit PanPanel (juce::AudioProcessorValueTreeState&);
    void refresh() override;

private:
    void layoutBody (juce::Rectangle<int>) override;
    PanDisplay display;
    Knob pan, spread;
};

class LofiPanel : public ModulePanel
{
public:
    explicit LofiPanel (juce::AudioProcessorValueTreeState&);
    void paintOverChildren (juce::Graphics&) override;
    void refresh() override;

private:
    void layoutBody (juce::Rectangle<int>) override;
    LofiSwitch sw;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> swAttachment;
    Knob mix, tone, bits, rate, noise, wobble, drive;
    juce::Rectangle<int> labelArea;
};

class LimiterPanel : public ModulePanel
{
public:
    LimiterPanel (juce::AudioProcessorValueTreeState&, pg::Limiter& limiter);
    void refresh() override;
    void paintOverChildren (juce::Graphics&) override;

private:
    void layoutBody (juce::Rectangle<int>) override;
    pg::Limiter& lim;
    LimiterScope scope;
    Knob gain, clip, ceiling, release, adapt, output;
    Segmented look { { "LIVE 0", "1.5 ms", "5 ms" } };
    Segmented os { { juce::String::fromUTF8 ("4\xc3\x97"), juce::String::fromUTF8 ("8\xc3\x97"), juce::String::fromUTF8 ("16\xc3\x97") } };
    LedMeter inL { 16, false }, inR { 16, false }, grM { 12, true }, outL { 16, false }, outR { 16, false };
    juce::Rectangle<int> lookLabel, osLabel, meterArea;
    float shown[5] = {};
};

class AutoGainPanel : public ModulePanel
{
public:
    AutoGainPanel (juce::AudioProcessorValueTreeState&, pg::AutoGain& autoGain);
    void refresh() override;
    void paintOverChildren (juce::Graphics&) override;

private:
    void layoutBody (juce::Rectangle<int>) override;
    pg::AutoGain& ag;
    GainMeter meter;
    Knob speed, range, trim;
    Segmented detect { { "LUFS", "RMS" } };
    PillToggle hold { "HOLD" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> holdAttachment;
    juce::Rectangle<int> readArea;
    float shownGain = 0.0f, inLvl = -100.0f, outLvl = -100.0f;
};
} // namespace pg::ui
