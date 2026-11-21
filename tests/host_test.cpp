// Post-Grape — manual test host. Loads the VST3 from bin/Post-Grape/
// (Windows: one file, macOS: bundle), makes an instance, processes audio
// and checks the result. Not part of the plugin.
// Build: cmake --build build --target PostGrapeHostTest
#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdio>
#if JUCE_WINDOWS
 #include <windows.h>
#else
 #include <unistd.h>
#endif

static int fails = 0;
static void check (bool ok, const juce::String& name)
{
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", name.toRawUTF8());
    std::fflush (stdout);
    fails += ok ? 0 : 1;
}

static void runTests()
{
    const juce::File file (POSTGRAPE_PLUGIN_PATH);
    check (file.exists(), "plugin exists: " + file.getFullPathName());

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> types;
    juce::MessageManager::callSync ([&] { format.findAllTypesForFile (types, file.getFullPathName()); });
    check (types.size() == 1, "plugin found in the single file (types: " + juce::String (types.size()) + ")");
    if (types.isEmpty())
        return;
    const auto d = *types[0];
    std::printf ("name=%s vendor=%s version=%s category=%s\n", d.name.toRawUTF8(), d.manufacturerName.toRawUTF8(),
                 d.version.toRawUTF8(), d.category.toRawUTF8());
    check (d.name == "Post-Grape" && d.manufacturerName == "desNotto", "name and vendor");

    juce::String error;
    auto plugin = format.createInstanceFromDescription (d, 48000.0, 256, error); // blocks this thread only
    check (plugin != nullptr, "instance created " + error);
    if (plugin == nullptr)
        return;

    plugin->setPlayConfigDetails (2, 2, 48000.0, 256);
    plugin->prepareToPlay (48000.0, 256);
    check (plugin->getLatencySamples() == 0, "reported latency 0 samples");
    check (plugin->getParameters().size() > 60, "parameters: " + juce::String (plugin->getParameters().size()));

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    double inSum = 0.0, outSum = 0.0;
    bool finite = true;
    for (int b = 0; b < 400; ++b)
    {
        for (int i = 0; i < 256; ++i)
        {
            const float s = 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi * 110.0f * (float) (b * 256 + i) / 48000.0f);
            buffer.setSample (0, i, s);
            buffer.setSample (1, i, -s);
            if (b > 100) inSum += (double) s * s;
        }
        plugin->processBlock (buffer, midi);
        for (int i = 0; i < 256; ++i)
        {
            const float l = buffer.getSample (0, i);
            finite = finite && std::isfinite (l);
            if (b > 100) outSum += (double) l * l;
        }
    }
    const double diffDb = 10.0 * std::log10 (outSum / inSum);
    check (finite, "output is finite");
    check (std::abs (diffDb) < 1.0, "Init preset: level change " + juce::String (diffDb, 3) + " dB (< 1 dB)");

    juce::MemoryBlock state;
    plugin->getStateInformation (state);
    check (state.getSize() > 100, "state saved (" + juce::String ((int) state.getSize()) + " bytes)");
    plugin->setStateInformation (state.getData(), (int) state.getSize());

    juce::MessageManager::callSync ([&] {
        if (plugin->hasEditor())
        {
            auto* ed = plugin->createEditorIfNeeded(); // owned by the plugin
            check (ed != nullptr && ed->getWidth() > 900, "editor created (" + juce::String (ed != nullptr ? ed->getWidth() : 0) + " px)");
            if (ed != nullptr)
            {
                plugin->editorBeingDeleted (ed);
                delete ed;
            }
        }
        plugin->releaseResources();
    });
    plugin.release(); // see the note in main()
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    runTests();
    std::puts (fails == 0 ? "ALL PASS" : "FAILED");
    std::fflush (stdout);
    // Note: this console host has no host message loop. The JUCE VST3 module unload
    // does not finish in this setup, so the test ends the process here.
    // (The Standalone app and DAWs close the plugin correctly.)
   #if JUCE_WINDOWS
    ::TerminateProcess (::GetCurrentProcess(), (unsigned) fails);
   #else
    _exit (fails);
   #endif
    return fails;
}
