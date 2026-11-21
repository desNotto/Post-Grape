// Post-Grape — audio processor.
#pragma once

#include "Params.h"
#include "PresetManager.h"
#include "dsp/Engine.h"
#include <juce_audio_processors/juce_audio_processors.h>

class PostGrapeProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    PostGrapeProcessor();
    ~PostGrapeProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Post-Grape"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    pg::PresetManager presets;
    pg::Engine engine;

    // Editor state (saved with the project).
    float uiScale = 1.0f;
    int uiWidth = 1200, uiHeight = 856;

    double currentSampleRate() const noexcept { return sampleRateNow.load(); }

private:
    template <typename T>
    void processT (juce::AudioBuffer<T>& buffer);
    void parameterChanged (const juce::String&, float) override;

    pg::params::Reader reader;
    pg::EngineSettings settings;
    std::vector<double> bufL, bufR;
    std::atomic<double> sampleRateNow { 48000.0 };
    int lastLatency = 0;

    JUCE_DECLARE_WEAK_REFERENCEABLE (PostGrapeProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PostGrapeProcessor)
};
