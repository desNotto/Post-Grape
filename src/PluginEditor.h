// Post-Grape — plugin window.
// Free resize in each direction (the layout adapts). Scale button 100% / 125%.
// Hint bar at the bottom (RU / EN). Update check on GitHub.
#pragma once

#include "AppSettings.h"
#include "PluginProcessor.h"
#include "UpdateChecker.h"
#include "gui/Panels.h"

class PostGrapeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PostGrapeEditor (PostGrapeProcessor&);
    ~PostGrapeEditor() override;

    void resized() override;
    void paint (juce::Graphics&) override {}

    void mouseMove (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    // Preset name field (click: preset list).
    class PresetBox : public juce::Component
    {
    public:
        explicit PresetBox (pg::PresetManager& pm) : presets (pm) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        float z = 1.0f;

    private:
        pg::PresetManager& presets;
    };

    // Hint bar at the bottom: hint text, latency, version.
    class HintBar : public juce::Component
    {
    public:
        void setHint (const juce::String& label, const juce::String& text);
        void setLatency (int samples) { if (samples != latency) { latency = samples; repaint(); } }
        void paint (juce::Graphics&) override;
        juce::Rectangle<float> latencyArea() const;
        float z = 1.0f;

    private:
        juce::String label, text;
        int latency = 0;
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
    };

    void timerCallback() override;
    void setUiScale (float s);
    void layoutContent();
    void updateHint (const juce::MouseEvent* e);
    pg::ui::Lang lang() const { return settings->hintsRussian() ? pg::ui::Lang::Ru : pg::ui::Lang::En; }

    void savePreset();
    void renamePreset();
    void deletePreset();
    void showTextDialog (const juce::String& title, const juce::String& message, const juce::String& initial,
                         const juce::String& okText, std::function<void (juce::String)> onOk);
    void showConfirm (const juce::String& title, const juce::String& message, const juce::String& okText, std::function<void()> onOk);
    void showMessage (const juce::String& title, const juce::String& message,
                      std::vector<std::pair<juce::String, std::function<void()>>> buttons, bool withAutoCheck = false);

    void onUpdateButton();
    void showUpdateResult();

    PostGrapeProcessor& proc;
    pg::ui::LookAndFeel lnf;
    juce::SharedResourcePointer<pg::AppSettings> settings;
    juce::SharedResourcePointer<pg::UpdateChecker> updates;
    Content content { *this };

    pg::ui::IconButton prevBtn { pg::ui::IconButton::Icon::Prev, "Previous preset" };
    pg::ui::IconButton nextBtn { pg::ui::IconButton::Icon::Next, "Next preset" };
    pg::ui::IconButton saveBtn { pg::ui::IconButton::Icon::Save, "Save preset" };
    pg::ui::IconButton renameBtn { pg::ui::IconButton::Icon::Rename, "Rename preset" };
    pg::ui::IconButton deleteBtn { pg::ui::IconButton::Icon::Delete, "Delete preset" };
    pg::ui::IconButton folderBtn { pg::ui::IconButton::Icon::Folder, "Open the preset folder" };
    pg::ui::IconButton updateBtn { pg::ui::IconButton::Icon::Cloud, "Check for updates" };
    PresetBox presetBox { proc.presets };
    pg::ui::FlagSwitch langSel;
    pg::ui::Segmented scaleSel { { "100%", "125%" } };
    HintBar hintBar;
    juce::ToggleButton autoCheckToggle { "Check for updates when the plugin opens (one time each day)" };

    pg::ui::EqPanel eqPanel;
    pg::ui::CompPanel compPanel;
    pg::ui::WiderPanel widerPanel;
    pg::ui::MonoPanel monoPanel;
    pg::ui::PanPanel panPanel;
    pg::ui::LofiPanel lofiPanel;
    pg::ui::LimiterPanel limPanel;
    pg::ui::AutoGainPanel agPanel;

    float scale = 1.0f;
    bool settingScale = false;
    juce::String lastPresetText;
    juce::Point<int> lastMouse { -1, -1 };
    bool mouseInside = false;

    // Update check state
    bool updateDialogPending = false; // the user clicked the button: show the result
    pg::UpdateChecker::Status lastUpdateStatus = pg::UpdateChecker::Status::Idle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PostGrapeEditor)
};
