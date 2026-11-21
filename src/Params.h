// Post-Grape — parameter IDs, ranges and conversion to DSP settings.
#pragma once

#include "dsp/Engine.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace pg::params
{
// Module power switches (same order as pg::ModuleId).
inline const char* const kModuleOn[kNumModules] = { "eq_on", "comp_on", "wide_on", "mono_on", "pan_on", "lofi_on", "lim_on", "ag_on" };

inline juce::String bandId (int band, const char* name) { return "eq_b" + juce::String (band + 1) + "_" + name; }

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Fast access to the raw parameter values on the audio thread.
class Reader
{
public:
    explicit Reader (juce::AudioProcessorValueTreeState& apvts);
    void read (EngineSettings& s) const noexcept;

private:
    std::atomic<float>* p (const juce::String& id) const;
    juce::AudioProcessorValueTreeState& state;

    std::array<std::atomic<float>*, kNumModules> on {};
    std::atomic<float> *eqHp, *eqSlope, *eqLp, *eqOut;
    std::array<std::atomic<float>*, 4> bFreq {}, bGain {}, bQ {}, bType {}, bTarget {};
    std::atomic<float> *cSplit, *cThr, *cRatio, *cAtt, *cRel, *cSc, *cLink, *cMk;
    std::atomic<float> *wWidth, *wAir, *wCut;
    std::atomic<float> *mFreq, *mWidth;
    std::atomic<float> *pPan, *pSpread;
    std::atomic<float> *lActive, *lTone, *lBits, *lRate, *lNoise, *lWobble, *lDrive, *lMix;
    std::atomic<float> *limGain, *limClip, *limCeil, *limRel, *limAdapt, *limOut, *limLook, *limOs;
    std::atomic<float> *agSpeed, *agRange, *agTrim, *agDetect, *agHold;
};
} // namespace pg::params
