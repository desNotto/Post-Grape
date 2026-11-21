// Post-Grape — controls: knob, power button, lofi switch, segmented switch, icon button, meters.
#pragma once

#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace pg::ui
{
// Rotary knob with a label and a value text under it.
class Knob : public juce::Component
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

    juce::Slider slider;

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::RangedAudioParameter* param = nullptr;
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
class Segmented : public juce::Component
{
public:
    explicit Segmented (juce::StringArray items);

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
    juce::StringArray items;
    int selected = 0;
    float z = 1.0f;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// Rounded square button with a vector icon (preset bar).
class IconButton : public juce::Button
{
public:
    enum class Icon { Prev, Next, Save, Rename, Delete, Folder };
    IconButton (Icon i, const juce::String& tip);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    Icon icon;
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
} // namespace pg::ui
