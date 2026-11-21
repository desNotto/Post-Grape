// Post-Grape — controls: knob, power button, lofi switch, segmented switch, icon button, meters.
#pragma once

#include "Hints.h"
#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace pg::ui
{
// Slider with fine adjustment: hold Shift when you click, then drag (5 times slower).
class FineSlider : public juce::Slider
{
public:
    void mouseDown (const juce::MouseEvent& e) override
    {
        setMouseDragSensitivity (e.mods.isShiftDown() ? 1100 : 220);
        juce::Slider::mouseDown (e);
    }
};

// Rotary knob with a label and a value text under it.
class Knob : public juce::Component, public HintSource
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& label,
          bool decorated = false);

    void bind (const juce::String& paramId);
    void setLabel (const juce::String& l) { label = l; repaint(); }
    void setGeometry (float diameter, float z);
    float getDiameter() const noexcept { return diam; }
    // Size of the component for the current geometry.
    juce::Point<int> preferredSize() const;

    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    bool getHint (juce::Point<int>, juce::String& key, juce::String& label) const override;

    FineSlider slider;

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::RangedAudioParameter* param = nullptr;
    juce::String paramId;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::String label;
    bool deco;
    float diam = 50.0f, z = 1.0f;
};

// Round power button with a grape icon (module on / off).
class PowerButton : public juce::ToggleButton
{
public:
    PowerButton();
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    juce::Image greyGrapes;
};

// Big LOFI ON/OFF switch (pill with a swirl and a grape nub).
class LofiSwitch : public juce::ToggleButton
{
public:
    LofiSwitch();
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    juce::Image greyGrapes, greySwirl;
};

// Row of buttons; one is selected. The width fits the content.
class Segmented : public juce::Component, public HintSource
{
public:
    explicit Segmented (juce::StringArray items);

    // Hint key for each item, and a prefix for the hint label.
    void setHints (juce::StringArray keys, const juce::String& labelPrefix = {}) { hintKeys = std::move (keys); hintPrefix = labelPrefix; }
    bool getHint (juce::Point<int>, juce::String& key, juce::String& label) const override;
    int itemAt (int x) const;

    // Bind to a choice parameter (index = choice index).
    void bind (juce::AudioProcessorValueTreeState& state, const juce::String& paramId);
    void unbind();

    void setSelected (int index, bool notify);
    int getSelected() const noexcept { return selected; }
    void setZ (float zIn);
    int preferredWidth() const;
    int preferredHeight() const { return juce::roundToInt (24.0f * z); }

    std::function<void (int)> onChange;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    std::vector<float> itemWidths() const;
    juce::StringArray items, hintKeys;
    juce::String hintPrefix;
    int selected = 0;
    float z = 1.0f;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// Rounded square button with a vector icon (preset bar).
class IconButton : public juce::Button
{
public:
    enum class Icon { Prev, Next, Save, Rename, Delete, Folder, Cloud };
    void setBadge (bool b) { if (b != badge) { badge = b; repaint(); } }
    IconButton (Icon i, const juce::String& tip);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    Icon icon;
    bool badge = false;
};

// Vertical LED meter. Level mode: 0..1 from bottom. GR mode: 0..1 from top (pink).
class LedMeter : public juce::Component
{
public:
    LedMeter (int segments, bool gainReduction) : n (segments), gr (gainReduction) {}
    void setValue (float v);
    void paint (juce::Graphics&) override;

private:
    int n;
    bool gr;
    float value = 0.0f;
};

// Vertical stereo correlation meter (-1 .. +1).
class CorrMeter : public juce::Component
{
public:
    void setValue (float v);
    void paint (juce::Graphics&) override;

private:
    float value = 1.0f;
};
// Language switch: two flags (Russia, USA). Index 0 = RU, 1 = EN.
class FlagSwitch : public juce::Component, public HintSource
{
public:
    std::function<void (bool russian)> onChange;
    void setRussian (bool ru) { if (ru != russian) { russian = ru; repaint(); } }
    void setZ (float zIn) { z = zIn; repaint(); }
    int preferredWidth() const { return juce::roundToInt (2.0f * 36.0f * z); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool getHint (juce::Point<int>, juce::String& key, juce::String& label) const override;

private:
    bool russian = true;
    float z = 1.0f;
};

// Small pill toggle (HOLD).
class PillToggle : public juce::ToggleButton
{
public:
    explicit PillToggle (const juce::String& text) : juce::ToggleButton (text) {}
    void setZ (float zIn) { z = zIn; repaint(); }
    int preferredWidth() const;
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    float z = 1.0f;
};

// Bipolar vertical gain meter (Auto-Gain). 0 dB in the middle.
class GainMeter : public juce::Component
{
public:
    void setValue (float db, float rangeDb);
    void paint (juce::Graphics&) override;

private:
    float value = 0.0f, range = 12.0f;
};
} // namespace pg::ui
