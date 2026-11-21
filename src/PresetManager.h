// Post-Grape — presets: factory presets (in the binary) and user presets (XML files).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace pg
{
class PresetManager
{
public:
    struct Entry
    {
        juce::String name;
        bool factory = false;
        juce::File file; // user presets only
    };

    static constexpr const char* kExtension = ".pgpreset";

    explicit PresetManager (juce::AudioProcessorValueTreeState& state);

    static juce::File userFolder();

    void refresh();
    const std::vector<Entry>& entries() const noexcept { return list; }

    juce::String currentName() const;
    int currentIndex() const;
    bool isCurrentFactory() const;
    bool isDirty() const noexcept { return dirty.load(); }

    void load (int index);
    void loadNext() { step (1); }
    void loadPrevious() { step (-1); }

    // Save the current state as a user preset. Overwrites a preset with the same name.
    bool save (const juce::String& name);
    bool rename (int index, const juce::String& newName);
    bool remove (int index);
    bool userPresetExists (const juce::String& name) const;

    // Called when the list or the current preset changes (message thread).
    std::function<void()> onChange;

    // Call from the processor after setStateInformation.
    void stateRestored();
    // Call when a parameter changes (marks the preset as modified).
    void markDirty() { dirty = true; }

private:
    void step (int delta);
    void setCurrentName (const juce::String& name);
    void loadFactory (int factoryIndex);
    static juce::String legalName (const juce::String& name);
    void notify();

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<Entry> list;
    std::atomic<bool> dirty { false };
    bool loading = false;
};
} // namespace pg
