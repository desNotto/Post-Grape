// Post-Grape — hint texts (RU / EN) for the hint bar at the bottom of the window.
// Mouse over a module: module hint. Mouse over a control: control hint.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace pg::ui
{
enum class Lang { Ru, En };

// Text for a hint key. Keys: module keys ("eq", "comp", ...), parameter IDs
// ("comp_thresh", "eq_b_freq" for all EQ bands) and control keys ("lufs", "save", ...).
juce::String hintText (const juce::String& key, Lang lang);

// Text when the mouse is not over a module or a control.
juce::String idleHint (Lang lang);

// Hint key for a parameter ID (EQ band parameters share one key).
juce::String hintKeyForParam (const juce::String& paramId);

// A component that gives a hint for a point in its area.
struct HintSource
{
    virtual ~HintSource() = default;
    // Returns false when there is no hint at this point.
    virtual bool getHint (juce::Point<int> local, juce::String& key, juce::String& label) const = 0;
};

// Set a fixed hint on any component (buttons, displays).
void setHint (juce::Component& c, const juce::String& key, const juce::String& label);
bool getFixedHint (const juce::Component& c, juce::String& key, juce::String& label);
} // namespace pg::ui
