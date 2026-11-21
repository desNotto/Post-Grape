// Post-Grape — plugin window.
// Free resize in each direction (the layout adapts). Scale button 100% / 125%.
#pragma once

#include "PluginProcessor.h"
#include "gui/Panels.h"

class PostGrapeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PostGrapeEditor (PostGrapeProcessor&);
    ~PostGrapeEditor() override;

    void resized() override;
    void paint (juce::Graphics&) override {}

private:
    // Preset name field (click: preset list).
    class PresetBox : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit PresetBox (pg::PresetManager& pm) : presets (pm) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        float z = 1.0f;

    private:
        pg::PresetManager& presets;
    };

    // All controls. The content has the logical size; the editor scales it.
    class Content : public juce::Component
    {
    public:
        explicit Content (PostGrapeEditor& e) : owner (e) {}
        void paint (juce::Graphics&) override;
        void resized() override;
        PostGrapeEditor& owner;
        float z = 1.0f;
        juce::Rectangle<float> latencyChip;
    };

    void timerCallback() override;
    void setUiScale (float s);
    void layoutContent();
    void savePreset();
    void renamePreset();
    void deletePreset();
    void showTextDialog (const juce::String& title, const juce::String& message, const juce::String& initial,
                         const juce::String& okText, std::function<void (juce::String)> onOk);
    void showConfirm (const juce::String& title, const juce::String& message, const juce::String& okText, std::function<void()> onOk);

    PostGrapeProcessor& proc;
    pg::ui::LookAndFeel lnf;
    juce::TooltipWindow tooltips { nullptr, 600 };
    Content content { *this };

    pg::ui::IconButton prevBtn { pg::ui::IconButton::Icon::Prev, "Previous preset" };
    pg::ui::IconButton nextBtn { pg::ui::IconButton::Icon::Next, "Next preset" };
    pg::ui::IconButton saveBtn { pg::ui::IconButton::Icon::Save, "Save preset" };
    pg::ui::IconButton renameBtn { pg::ui::IconButton::Icon::Rename, "Rename preset" };
    pg::ui::IconButton deleteBtn { pg::ui::IconButton::Icon::Delete, "Delete preset" };
    pg::ui::IconButton folderBtn { pg::ui::IconButton::Icon::Folder, "Open the preset folder" };
    PresetBox presetBox { proc.presets };
    pg::ui::Segmented scaleSel { { "100%", "125%" } };

    pg::ui::EqPanel eqPanel;
    pg::ui::CompPanel compPanel;
    pg::ui::WiderPanel widerPanel;
    pg::ui::MonoPanel monoPanel;
    pg::ui::PanPanel panPanel;
    pg::ui::LofiPanel lofiPanel;
    pg::ui::LimiterPanel limPanel;

    float scale = 1.0f;
    bool settingScale = false;
    juce::String lastPresetText;
    int lastLatency = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PostGrapeEditor)
};
