// Post-Grape — manual test of the update checker (network). Not part of the plugin.
// Build: cmake --build build --target PostGrapeUpdateTest
// The test target says that the current version is 1.0.0, thus any release is newer.
#include "../src/UpdateChecker.h"
#include <cstdio>

int main()
{
    int fails = 0;
    auto check = [&] (bool ok, const juce::String& name) { std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", name.toRawUTF8()); fails += ok ? 0 : 1; };

    check (pg::UpdateChecker::isNewer ("1.1.0", "1.0.9"), "isNewer 1.1.0 > 1.0.9");
    check (! pg::UpdateChecker::isNewer ("1.0.1", "1.0.1"), "isNewer 1.0.1 = 1.0.1");
    check (pg::UpdateChecker::isNewer ("v2.0", "1.9.9"), "isNewer v2.0 > 1.9.9");

    // Asset selection: each system takes its own zip.
    const juce::StringArray both { "Post-Grape-v1.2.0-win64.zip", "Post-Grape-v1.2.0-macOS.zip" };
    const juce::StringArray macFirst { "Post-Grape-v1.2.0-macOS.zip", "Post-Grape-v1.2.0-win64.zip" };
    const juce::StringArray oldWin { "Post-Grape-v1.0.1-win64.zip" };
    const juce::StringArray oldNoName { "Post-Grape.zip" };
   #if JUCE_MAC
    check (pg::UpdateChecker::selectAsset (both) == 1 && pg::UpdateChecker::selectAsset (macFirst) == 0, "macOS takes the macOS zip");
    check (pg::UpdateChecker::selectAsset (oldWin) == -1, "macOS never takes a Windows zip");
   #else
    check (pg::UpdateChecker::selectAsset (both) == 0 && pg::UpdateChecker::selectAsset (macFirst) == 1, "Windows takes the win64 zip");
    check (pg::UpdateChecker::selectAsset (oldNoName) == 0, "Windows takes an old zip without a platform name");
   #endif

    pg::UpdateChecker u;
    u.startCheck();
    for (int i = 0; i < 200 && u.status() == pg::UpdateChecker::Status::Checking; ++i)
        juce::Thread::sleep (100);
    std::printf ("latest = %s, asset = %s, page = %s\n", u.latestVersion().toRawUTF8(), u.assetName().toRawUTF8(),
                 u.releaseUrl().toRawUTF8());
    check (u.status() == pg::UpdateChecker::Status::Available, "check: update available for 1.0.0");
    check (u.assetName().containsIgnoreCase (pg::UpdateChecker::platformKeyword()),
           "check: the release has a zip for this system (" + pg::UpdateChecker::platformKeyword() + ")");

    u.startDownload();
    for (int i = 0; i < 600 && u.status() == pg::UpdateChecker::Status::Downloading; ++i)
        juce::Thread::sleep (100);
    const auto f = u.downloadedFile();
    std::printf ("file = %s (%lld bytes)\n", f.getFullPathName().toRawUTF8(), (long long) f.getSize());
    check (u.status() == pg::UpdateChecker::Status::Downloaded && f.getSize() > 100000, "download: zip saved");
    if (f.existsAsFile())
        f.deleteFile(); // test file only
    return fails;
}
