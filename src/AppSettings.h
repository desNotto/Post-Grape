// Post-Grape — global user settings (for all plugin instances).
// File: %APPDATA%\desNotto\Post-Grape\Post-Grape.settings
#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace pg
{
class AppSettings
{
public:
    AppSettings()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "Post-Grape";
        o.folderName = "desNotto" + juce::String (juce::File::getSeparatorString()) + "Post-Grape";
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        o.commonToAllUsers = false;
        file = std::make_unique<juce::PropertiesFile> (o);
    }

    // Hint language. The default is Russian.
    bool hintsRussian() const { return file->getValue ("hintLanguage", "ru") != "en"; }
    void setHintsRussian (bool ru) { set ("hintLanguage", ru ? "ru" : "en"); }

    // Automatic update check (one time each day). The default is on.
    bool autoCheckUpdates() const { return file->getBoolValue ("autoCheckUpdates", true); }
    void setAutoCheckUpdates (bool on) { set ("autoCheckUpdates", on); }

    juce::int64 lastUpdateCheck() const { return (juce::int64) file->getValue ("lastUpdateCheck", "0").getLargeIntValue(); }
    void setLastUpdateCheck (juce::int64 ms) { set ("lastUpdateCheck", juce::String (ms)); }

private:
    void set (const juce::String& key, const juce::var& v)
    {
        const juce::ScopedLock sl (lock);
        file->setValue (key, v);
        file->saveIfNeeded();
    }

    std::unique_ptr<juce::PropertiesFile> file;
    juce::CriticalSection lock;
};
} // namespace pg
