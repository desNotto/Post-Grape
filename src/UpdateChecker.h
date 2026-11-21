// Post-Grape — checks GitHub for a new release and downloads the release zip.
// Only one request goes to api.github.com. No user data is sent.
// The plugin cannot replace its own VST3 file while the DAW uses it, so the
// zip goes to the Downloads folder and the user replaces the file.
#pragma once

#include <juce_core/juce_core.h>

namespace pg
{
class UpdateChecker : private juce::Thread
{
public:
    enum class Status { Idle, Checking, UpToDate, Available, Error, Downloading, Downloaded, DownloadError };

    static constexpr const char* kRepo = "desNotto/Post-Grape";

    UpdateChecker();
    ~UpdateChecker() override;

    void startCheck();
    void startDownload();

    Status status() const;
    juce::String latestVersion() const;
    juce::String releaseUrl() const;
    juce::String errorText() const;
    juce::File downloadedFile() const;
    juce::String assetName() const;      // zip for this operating system (empty = none)

    // Keyword in the release zip name for this operating system: "macOS" or "win64".
    static juce::String platformKeyword();
    // Select the release zip for this operating system from the GitHub "assets" array.
    static int selectAsset (const juce::StringArray& names);

    static juce::String currentVersion();
    // Returns true when version a is newer than version b ("1.2.0" > "1.1.9").
    static bool isNewer (const juce::String& a, const juce::String& b);

private:
    void run() override;
    void doCheck();
    void doDownload();
    void setStatus (Status s);

    mutable juce::CriticalSection lock;
    Status state = Status::Idle;
    bool wantDownload = false;
    juce::String latest, htmlUrl, assetUrl, asset, error;
    juce::File downloaded;
};
} // namespace pg
