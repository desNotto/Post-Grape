// Post-Grape — manual test of the update checker (network). Not part of the plugin.
// Build: cmake --build build --target PostGrapeUpdateTest
// The test target says that the current version is 1.0.0, thus any release is newer.
#include "../src/UpdateChecker.h"
#include <cstdio>

int main()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* name) { std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", name); fails += ok ? 0 : 1; };

    check (pg::UpdateChecker::isNewer ("1.1.0", "1.0.9"), "isNewer 1.1.0 > 1.0.9");
    check (! pg::UpdateChecker::isNewer ("1.0.1", "1.0.1"), "isNewer 1.0.1 = 1.0.1");
    check (pg::UpdateChecker::isNewer ("v2.0", "1.9.9"), "isNewer v2.0 > 1.9.9");

    pg::UpdateChecker u;
    u.startCheck();
    for (int i = 0; i < 200 && u.status() == pg::UpdateChecker::Status::Checking; ++i)
        juce::Thread::sleep (100);
    std::printf ("latest = %s, page = %s\n", u.latestVersion().toRawUTF8(), u.releaseUrl().toRawUTF8());
    check (u.status() == pg::UpdateChecker::Status::Available, "check: update available for 1.0.0");

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
