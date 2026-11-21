// Post-Grape — displays: EQ curve, limiter scope, mono-maker band, true pan field.
#pragma once

#include "Theme.h"
#include "dsp/Limiter.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace pg::ui
{
// Interactive EQ curve. Drag a node: frequency and gain. Wheel: Q. Double-click: gain 0.
class EqDisplay : public juce::Component
{
public:
    EqDisplay (juce::AudioProcessorValueTreeState& state, std::function<double()> sampleRate);

    void setZ (float zIn) { z = zIn; repaint(); }
    void setSelectedBand (int b) { selected = b; repaint(); }
    std::function<void (int)> onSelectBand;

    // Call from a timer: repaints when a parameter changes.
    void refresh();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    float value (const juce::String& id) const;
    juce::RangedAudioParameter* param (const juce::String& id) const { return apvts.getParameter (id); }
    float xForFreq (double f) const;
    double freqForX (float x) const;
    float yForDb (double db) const;
    double dbForY (float y) const;
    juce::Point<float> nodePos (int band) const;
    int hitNode (juce::Point<float> p) const;
    void setParam (const juce::String& id, float v);

    juce::AudioProcessorValueTreeState& apvts;
    std::function<double()> getFs;
    float z = 1.0f;
    int selected = 0, dragging = -1;
    std::vector<float> snapshot;
};

// Limiter display: input, output, gain reduction, ceiling.
class LimiterScope : public juce::Component
{
public:
    explicit LimiterScope (pg::ScopeFifo& fifo) : source (fifo) {}
    void setZ (float zIn) { z = zIn; }
    void setCeilingDb (float c) { ceilingDb = c; }
    void refresh(); // pull new columns and repaint
    void paint (juce::Graphics&) override;

private:
    pg::ScopeFifo& source;
    uint32_t readCount = 0;
    std::vector<pg::ScopeColumn> cols = std::vector<pg::ScopeColumn> (480);
    size_t head = 0;
    float z = 1.0f, ceilingDb = -0.3f, lastGrDb = 0.0f;
};

// Mono-Maker: shows the mono band below the crossover.
class MonoDisplay : public juce::Component
{
public:
    void setValues (float freqHz, float widthPct);
    void setZ (float zIn) { z = zIn; repaint(); }
    void paint (juce::Graphics&) override;

private:
    float freq = 120.0f, width = 0.0f, z = 1.0f;
};

// True Stereo Pan: positions of the left and right inputs.
class PanDisplay : public juce::Component
{
public:
    void setValues (float panPct, float spreadPct);
    void setZ (float zIn) { z = zIn; repaint(); }
    void paint (juce::Graphics&) override;

private:
    float pan = 0.0f, spread = 100.0f, z = 1.0f;
};
} // namespace pg::ui
