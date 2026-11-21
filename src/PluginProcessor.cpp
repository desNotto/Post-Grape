#include "PluginProcessor.h"
#include "PluginEditor.h"

PostGrapeProcessor::PostGrapeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PostGrape", pg::params::createLayout()),
      presets (apvts),
      reader (apvts)
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.addParameterListener (rp->paramID, this);
    reader.read (settings);
    engine.setSettings (settings);
}

PostGrapeProcessor::~PostGrapeProcessor()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.removeParameterListener (rp->paramID, this);
}

void PostGrapeProcessor::parameterChanged (const juce::String&, float)
{
    presets.markDirty();
}

bool PostGrapeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PostGrapeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateNow.store (sampleRate);
    const int maxBlock = juce::jmax (16, samplesPerBlock);
    bufL.assign ((size_t) maxBlock, 0.0);
    bufR.assign ((size_t) maxBlock, 0.0);

    reader.read (settings);
    engine.setSettings (settings);
    engine.setSampleRate (sampleRate);
    engine.prepare (sampleRate, maxBlock);

    lastLatency = engine.latencySamples();
    setLatencySamples (lastLatency);
}

template <typename T>
void PostGrapeProcessor::processT (juce::AudioBuffer<T>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    if (n <= 0 || buffer.getNumChannels() < 2)
        return;

    reader.read (settings);
    engine.setSettings (settings);

    const int latency = engine.latencySamples();
    if (latency != lastLatency)
    {
        lastLatency = latency;
        setLatencySamples (latency);
    }

    auto* chL = buffer.getWritePointer (0);
    auto* chR = buffer.getWritePointer (1);

    int done = 0;
    const int maxBlock = (int) bufL.size();
    while (done < n)
    {
        const int len = juce::jmin (maxBlock, n - done);
        for (int i = 0; i < len; ++i)
        {
            bufL[(size_t) i] = (double) chL[done + i];
            bufR[(size_t) i] = (double) chR[done + i];
        }
        engine.process (bufL.data(), bufR.data(), len);
        for (int i = 0; i < len; ++i)
        {
            chL[done + i] = (T) bufL[(size_t) i];
            chR[done + i] = (T) bufR[(size_t) i];
        }
        done += len;
    }

    for (int c = 2; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);
}

void PostGrapeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { processT (buffer); }
void PostGrapeProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) { processT (buffer); }

juce::AudioProcessorEditor* PostGrapeProcessor::createEditor() { return new PostGrapeEditor (*this); }

void PostGrapeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiScale", uiScale, nullptr);
    state.setProperty ("uiWidth", uiWidth, nullptr);
    state.setProperty ("uiHeight", uiHeight, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PostGrapeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    auto tree = juce::ValueTree::fromXml (*xml);
    uiScale = juce::jlimit (1.0f, 1.25f, (float) tree.getProperty ("uiScale", 1.0f));
    uiWidth = (int) tree.getProperty ("uiWidth", 1200);
    uiHeight = (int) tree.getProperty ("uiHeight", 856);
    apvts.replaceState (tree);

    juce::MessageManager::callAsync ([safe = juce::WeakReference<PostGrapeProcessor> (this)] {
        if (safe != nullptr)
            safe->presets.stateRestored();
    });
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PostGrapeProcessor(); }
