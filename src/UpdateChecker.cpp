#include "UpdateChecker.h"

namespace pg
{
namespace
{
std::unique_ptr<juce::InputStream> openUrl (const juce::URL& url, int timeoutMs)
{
    // A generic client name only (no user data).
    const juce::String headers = "User-Agent: Post-Grape-Updater\r\nAccept: application/vnd.github+json\r\n";
    return url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                      .withExtraHeaders (headers)
                                      .withConnectionTimeoutMs (timeoutMs)
                                      .withNumRedirectsToFollow (8));
}

juce::Array<int> versionParts (juce::String v)
{
    v = v.trim().trimCharactersAtStart ("vV");
    juce::Array<int> parts;
    for (const auto& p : juce::StringArray::fromTokens (v, ".", ""))
        parts.add (p.retainCharacters ("0123456789").getIntValue());
    while (parts.size() < 3)
        parts.add (0);
    return parts;
}
} // namespace

UpdateChecker::UpdateChecker() : juce::Thread ("Post-Grape update check") {}

UpdateChecker::~UpdateChecker() { stopThread (10000); }

juce::String UpdateChecker::currentVersion() { return JucePlugin_VersionString; }

bool UpdateChecker::isNewer (const juce::String& a, const juce::String& b)
{
    const auto pa = versionParts (a), pb = versionParts (b);
    for (int i = 0; i < 3; ++i)
        if (pa[i] != pb[i])
            return pa[i] > pb[i];
    return false;
}

void UpdateChecker::setStatus (Status s)
{
    const juce::ScopedLock sl (lock);
    state = s;
}

UpdateChecker::Status UpdateChecker::status() const { const juce::ScopedLock sl (lock); return state; }
juce::String UpdateChecker::latestVersion() const { const juce::ScopedLock sl (lock); return latest; }
juce::String UpdateChecker::releaseUrl() const { const juce::ScopedLock sl (lock); return htmlUrl; }
juce::String UpdateChecker::errorText() const { const juce::ScopedLock sl (lock); return error; }
juce::File UpdateChecker::downloadedFile() const { const juce::ScopedLock sl (lock); return downloaded; }
juce::String UpdateChecker::assetName() const { const juce::ScopedLock sl (lock); return asset; }

juce::String UpdateChecker::platformKeyword()
{
   #if JUCE_MAC
    return "macOS";
   #else
    return "win64";
   #endif
}

int UpdateChecker::selectAsset (const juce::StringArray& names)
{
    for (int i = 0; i < names.size(); ++i)
        if (names[i].endsWithIgnoreCase (".zip") && names[i].containsIgnoreCase (platformKeyword()))
            return i;
   #if ! JUCE_MAC
    // Old releases (v1.0.0 .. v1.1.1) have only one zip, without a platform name: Windows.
    for (int i = 0; i < names.size(); ++i)
        if (names[i].endsWithIgnoreCase (".zip") && ! names[i].containsIgnoreCase ("mac"))
            return i;
   #endif
    return -1;
}

void UpdateChecker::startCheck()
{
    if (isThreadRunning())
        return;
    {
        const juce::ScopedLock sl (lock);
        state = Status::Checking;
        wantDownload = false;
    }
    startThread();
}

void UpdateChecker::startDownload()
{
    if (isThreadRunning())
        return;
    {
        const juce::ScopedLock sl (lock);
        if (assetUrl.isEmpty())
            return;
        state = Status::Downloading;
        wantDownload = true;
    }
    startThread();
}

void UpdateChecker::run()
{
    bool download;
    {
        const juce::ScopedLock sl (lock);
        download = wantDownload;
    }
    if (download)
        doDownload();
    else
        doCheck();
}

void UpdateChecker::doCheck()
{
    const juce::URL url ("https://api.github.com/repos/" + juce::String (kRepo) + "/releases/latest");
    auto stream = openUrl (url, 8000);
    if (stream == nullptr)
    {
        const juce::ScopedLock sl (lock);
        error = "Cannot connect to GitHub.";
        state = Status::Error;
        return;
    }
    const auto json = juce::JSON::parse (stream->readEntireStreamAsString());
    const auto tag = json.getProperty ("tag_name", {}).toString();
    if (tag.isEmpty())
    {
        const juce::ScopedLock sl (lock);
        error = "GitHub sent no release data.";
        state = Status::Error;
        return;
    }

    // Select the zip for this operating system (Windows: "win64", macOS: "macOS").
    juce::String zipUrl, zipName;
    if (auto* assets = json.getProperty ("assets", {}).getArray())
    {
        juce::StringArray names;
        for (const auto& a : *assets)
            names.add (a.getProperty ("name", {}).toString());
        const int i = selectAsset (names);
        if (i >= 0)
        {
            zipUrl = (*assets)[i].getProperty ("browser_download_url", {}).toString();
            zipName = names[i];
        }
    }

    const juce::ScopedLock sl (lock);
    latest = tag.trimCharactersAtStart ("vV");
    htmlUrl = json.getProperty ("html_url", {}).toString();
    assetUrl = zipUrl;
    asset = zipName;
    state = isNewer (latest, currentVersion()) ? Status::Available : Status::UpToDate;
}

void UpdateChecker::doDownload()
{
    juce::String urlText, name;
    {
        const juce::ScopedLock sl (lock);
        urlText = assetUrl;
        name = asset;
    }
    auto folder = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Downloads");
    if (! folder.isDirectory())
        folder = juce::File::getSpecialLocation (juce::File::userDesktopDirectory);
    auto target = folder.getNonexistentChildFile (juce::File::createLegalFileName (name).upToLastOccurrenceOf (".zip", false, true), ".zip", false);

    auto stream = openUrl (juce::URL (urlText), 15000);
    bool ok = stream != nullptr;
    if (ok)
    {
        auto temp = target.withFileExtension (".part");
        {
            juce::FileOutputStream out (temp);
            ok = out.openedOk();
            if (ok)
            {
                const auto written = out.writeFromInputStream (*stream, -1);
                out.flush();
                ok = written > 1000 && ! threadShouldExit();
            }
        }
        ok = ok && temp.moveFileTo (target);
        if (! ok)
            temp.deleteFile();
    }

    const juce::ScopedLock sl (lock);
    if (ok)
    {
        downloaded = target;
        state = Status::Downloaded;
    }
    else
    {
        error = "Cannot download the file.";
        state = Status::DownloadError;
    }
}
} // namespace pg
