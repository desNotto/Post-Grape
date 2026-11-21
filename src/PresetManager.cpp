#include "PresetManager.h"
#include "Params.h"

namespace pg
{
namespace
{
using Values = std::vector<std::pair<const char*, float>>;

struct FactoryPreset
{
    const char* name;
    Values values; // Parameters not in the list use the default value.
};

// Values are in real units (Hz, dB, %, choice index, 0/1).
const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        { "Init", {} },
        { "Drop A Wall",
          { { "eq_hp", 70.0f }, { "eq_hp_slope", 1.0f }, { "eq_lp", 12500.0f },
            { "eq_b1_freq", 95.0f }, { "eq_b1_gain", -2.5f }, { "eq_b1_q", 1.4f },
            { "eq_b2_freq", 380.0f }, { "eq_b2_gain", -3.0f }, { "eq_b2_q", 2.2f }, { "eq_b2_target", 1.0f },
            { "eq_b3_freq", 2650.0f }, { "eq_b3_gain", -6.0f }, { "eq_b3_q", 12.0f },
            { "eq_b4_freq", 6500.0f }, { "eq_b4_gain", 1.5f }, { "eq_b4_q", 0.7f }, { "eq_b4_target", 2.0f },
            { "comp_split", 160.0f }, { "comp_thresh", -20.0f }, { "comp_ratio", 4.0f }, { "comp_attack", 10.0f },
            { "comp_release", 120.0f }, { "comp_sc", 40.0f }, { "comp_makeup", 2.0f },
            { "wide_width", 115.0f }, { "wide_air", 2.0f }, { "wide_cut", 120.0f },
            { "mono_freq", 120.0f }, { "mono_width", 0.0f },
            { "lim_gain", 3.0f }, { "lim_clip", 35.0f }, { "lim_ceiling", -0.3f }, { "lim_release", 60.0f } } },
        { "Djent Tight",
          { { "eq_hp", 90.0f }, { "eq_hp_slope", 1.0f }, { "eq_lp", 11000.0f },
            { "eq_b1_freq", 150.0f }, { "eq_b1_gain", -3.0f }, { "eq_b1_q", 1.2f },
            { "eq_b2_freq", 500.0f }, { "eq_b2_gain", -2.0f }, { "eq_b2_q", 1.5f }, { "eq_b2_target", 1.0f },
            { "eq_b3_freq", 3200.0f }, { "eq_b3_gain", -5.0f }, { "eq_b3_q", 8.0f },
            { "eq_b4_freq", 7000.0f }, { "eq_b4_gain", 2.0f },
            { "comp_split", 200.0f }, { "comp_thresh", -24.0f }, { "comp_ratio", 6.0f }, { "comp_attack", 3.0f },
            { "comp_release", 80.0f }, { "comp_sc", 60.0f }, { "comp_makeup", 3.0f },
            { "wide_width", 110.0f }, { "wide_air", 1.5f }, { "wide_cut", 150.0f },
            { "mono_freq", 150.0f }, { "mono_width", 0.0f },
            { "lim_gain", 4.0f }, { "lim_clip", 45.0f }, { "lim_release", 40.0f } } },
        { "Doom Fog",
          { { "eq_hp", 45.0f }, { "eq_lp", 8500.0f },
            { "eq_b1_freq", 90.0f }, { "eq_b1_gain", 2.0f }, { "eq_b1_q", 0.7f }, { "eq_b1_type", 2.0f },
            { "eq_b2_freq", 700.0f }, { "eq_b2_gain", -2.5f }, { "eq_b2_q", 1.0f }, { "eq_b2_target", 1.0f },
            { "eq_b3_freq", 3500.0f }, { "eq_b3_gain", -4.0f }, { "eq_b3_q", 3.0f },
            { "comp_split", 120.0f }, { "comp_thresh", -18.0f }, { "comp_ratio", 3.0f }, { "comp_attack", 25.0f },
            { "comp_release", 250.0f }, { "comp_makeup", 1.5f },
            { "wide_width", 125.0f }, { "wide_cut", 100.0f },
            { "mono_freq", 100.0f }, { "mono_width", 10.0f },
            { "lim_gain", 2.0f }, { "lim_clip", 25.0f }, { "lim_release", 120.0f } } },
        { "Wide Rhythm",
          { { "eq_hp", 60.0f },
            { "comp_thresh", -14.0f }, { "comp_ratio", 2.5f },
            { "wide_width", 150.0f }, { "wide_air", 3.0f }, { "wide_cut", 180.0f },
            { "mono_freq", 140.0f }, { "mono_width", 0.0f },
            { "lim_gain", 2.0f }, { "lim_clip", 20.0f } } },
        { "Left Lean",
          { { "mono_width", 0.0f }, { "pan_pan", -45.0f }, { "pan_spread", 70.0f } } },
        { "Lofi Intro",
          { { "eq_hp", 120.0f }, { "eq_hp_slope", 1.0f }, { "eq_lp", 9000.0f },
            { "lofi_active", 1.0f }, { "lofi_tone", 1400.0f }, { "lofi_bits", 9.0f }, { "lofi_rate", 12000.0f },
            { "lofi_noise", 25.0f }, { "lofi_wobble", 35.0f }, { "lofi_drive", 8.0f }, { "lofi_mix", 100.0f },
            { "mono_width", 0.0f } } },
        { "Radio Break",
          { { "lofi_active", 1.0f }, { "lofi_tone", 2200.0f }, { "lofi_bits", 12.0f }, { "lofi_rate", 22000.0f },
            { "lofi_noise", 10.0f }, { "lofi_wobble", 0.0f }, { "lofi_drive", 12.0f },
            { "pan_spread", 40.0f }, { "mono_width", 0.0f } } },
        { "Master Safe",
          { { "lim_ceiling", -1.0f }, { "lim_look", 1.0f }, { "lim_os", 1.0f }, { "lim_release", 80.0f } } },
    };
    return presets;
}

const juce::Identifier kPresetNameProp ("presetName");
} // namespace

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state) : apvts (state)
{
    if (! apvts.state.hasProperty (kPresetNameProp))
        apvts.state.setProperty (kPresetNameProp, "Init", nullptr);
    refresh();
}

juce::File PresetManager::userFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("desNotto")
        .getChildFile ("Post-Grape")
        .getChildFile ("Presets");
}

void PresetManager::refresh()
{
    list.clear();
    for (const auto& f : factoryPresets())
        list.push_back ({ f.name, true, {} });

    auto dir = userFolder();
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles (juce::File::findFiles, false, juce::String ("*") + kExtension);
        files.sort();
        for (const auto& f : files)
            list.push_back ({ f.getFileNameWithoutExtension(), false, f });
    }
}

juce::String PresetManager::currentName() const
{
    return apvts.state.getProperty (kPresetNameProp, "Init").toString();
}

int PresetManager::currentIndex() const
{
    const auto name = currentName();
    // User presets first (a user preset can have the same name as a factory preset).
    for (int i = (int) list.size() - 1; i >= 0; --i)
        if (list[(size_t) i].name == name)
            return i;
    return -1;
}

bool PresetManager::isCurrentFactory() const
{
    const int i = currentIndex();
    return i >= 0 && list[(size_t) i].factory;
}

void PresetManager::setCurrentName (const juce::String& name)
{
    apvts.state.setProperty (kPresetNameProp, name, nullptr);
}

void PresetManager::notify()
{
    if (onChange)
        onChange();
}

void PresetManager::loadFactory (int factoryIndex)
{
    const auto& preset = factoryPresets()[(size_t) factoryIndex];
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& [id, value] : preset.values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}

void PresetManager::load (int index)
{
    if (index < 0 || index >= (int) list.size())
        return;
    const auto entry = list[(size_t) index];
    loading = true;

    if (entry.factory)
        loadFactory (index);
    else if (auto xml = juce::XmlDocument::parse (entry.file))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            // Set each parameter (the host gets the changes).
            for (auto* p : apvts.processor.getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                {
                    auto child = tree.getChildWithProperty ("id", rp->paramID);
                    const float v = child.isValid() ? (float) child.getProperty ("value", rp->convertFrom0to1 (rp->getDefaultValue()))
                                                    : rp->convertFrom0to1 (rp->getDefaultValue());
                    rp->setValueNotifyingHost (rp->convertTo0to1 (v));
                }
        }
    }

    setCurrentName (entry.name);
    loading = false;
    dirty = false;
    notify();
}

void PresetManager::step (int delta)
{
    if (list.empty())
        return;
    int i = currentIndex();
    if (i < 0) i = 0;
    else i = (i + delta + (int) list.size()) % (int) list.size();
    load (i);
}

juce::String PresetManager::legalName (const juce::String& name)
{
    return juce::File::createLegalFileName (name.trim()).substring (0, 64);
}

bool PresetManager::userPresetExists (const juce::String& name) const
{
    return userFolder().getChildFile (legalName (name) + kExtension).existsAsFile();
}

bool PresetManager::save (const juce::String& rawName)
{
    const auto name = legalName (rawName);
    if (name.isEmpty())
        return false;
    auto dir = userFolder();
    if (! dir.createDirectory())
        return false;

    setCurrentName (name);
    auto state = apvts.copyState();
    auto xml = state.createXml();
    if (xml == nullptr || ! xml->writeTo (dir.getChildFile (name + kExtension)))
        return false;

    dirty = false;
    refresh();
    notify();
    return true;
}

bool PresetManager::rename (int index, const juce::String& rawName)
{
    if (index < 0 || index >= (int) list.size() || list[(size_t) index].factory)
        return false;
    const auto name = legalName (rawName);
    if (name.isEmpty())
        return false;
    auto target = userFolder().getChildFile (name + kExtension);
    if (target.existsAsFile() && target != list[(size_t) index].file)
        return false;

    const bool wasCurrent = currentIndex() == index;
    if (! list[(size_t) index].file.moveFileTo (target))
        return false;

    // Store the new name inside the file too.
    if (auto xml = juce::XmlDocument::parse (target))
    {
        xml->setAttribute (kPresetNameProp, name);
        xml->writeTo (target);
    }
    if (wasCurrent)
        setCurrentName (name);
    refresh();
    notify();
    return true;
}

bool PresetManager::remove (int index)
{
    if (index < 0 || index >= (int) list.size() || list[(size_t) index].factory)
        return false;
    const bool wasCurrent = currentIndex() == index;
    // Move to the recycle bin (the user can restore the file).
    if (! list[(size_t) index].file.moveToTrash())
        return false;
    refresh();
    if (wasCurrent)
        load (0);
    else
        notify();
    return true;
}

void PresetManager::stateRestored()
{
    if (! apvts.state.hasProperty (kPresetNameProp))
        setCurrentName ("Init");
    refresh();
    dirty = false;
    notify();
}
} // namespace pg
