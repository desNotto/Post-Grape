// Post-Grape — the complete chain of 7 modules with bypass crossfades.
// IN -> Bus EQ -> Bus Compressor -> Wider -> Mono-Maker -> True Stereo Pan -> Lofi -> Bus Limiter -> OUT
#pragma once

#include "BusComp.h"
#include "BusEQ.h"
#include "Limiter.h"
#include "Lofi.h"
#include "StereoModules.h"

namespace pg
{
enum ModuleId { kEq = 0, kComp, kWide, kMono, kPan, kLofi, kLim, kNumModules };

struct EngineSettings
{
    std::array<bool, kNumModules> on { true, true, true, true, true, true, true };
    EqSettings eq;
    CompSettings comp;
    WiderSettings wide;
    MonoSettings mono;
    PanSettings pan;
    LofiSettings lofi;
    LimiterSettings lim;
};

class Engine
{
public:
    BusEQ eq;
    BusComp comp;
    Wider wide;
    MonoMaker mono;
    TruePan pan;
    Lofi lofi;
    Limiter lim;

    void setSettings (const EngineSettings& s) noexcept
    {
        st = s;
        eq.setSettings (s.eq);
        comp.setSettings (s.comp);
        wide.setSettings (s.wide);
        mono.setSettings (s.mono);
        pan.setSettings (s.pan);
        lofi.setSettings (s.lofi);
        lim.setSettings (s.lim);
    }

    void prepare (double sampleRate, int maxBlock)
    {
        maxN = std::max (1, maxBlock);
        dryL.assign ((size_t) maxN, 0.0);
        dryR.assign ((size_t) maxN, 0.0);
        for (int m = 0; m < kNumModules; ++m)
        {
            modules[(size_t) m]->prepare (sampleRate, maxN);
            fade[(size_t) m].prepare (sampleRate, 10.0);
            fade[(size_t) m].reset (st.on[(size_t) m] ? 1.0 : 0.0);
            stale[(size_t) m] = false;
        }
    }

    void reset()
    {
        for (auto* m : modules) m->reset();
    }

    int latencySamples() const noexcept { return st.on[kLim] ? Limiter::lookaheadSamples (st.lim.lookaheadIndex, lastFs) : 0; }
    void setSampleRate (double fs) noexcept { lastFs = fs; }

    // Process a block in place. n can be larger than maxBlock (split here).
    void process (double* L, double* R, int n) noexcept
    {
        int done = 0;
        while (done < n)
        {
            const int len = std::min (maxN, n - done);
            processChunk (L + done, R + done, len);
            done += len;
        }
    }

private:
    void processChunk (double* L, double* R, int n) noexcept
    {
        for (size_t m = 0; m < (size_t) kNumModules; ++m)
        {
            auto& f = fade[m];
            f.set (st.on[m] ? 1.0 : 0.0);

            if (! f.isActive() && f.value() <= 0.0)
            {
                stale[m] = true; // bypassed: no processing
                continue;
            }
            if (stale[m])
            {
                modules[m]->reset();
                stale[m] = false;
            }

            if (! f.isActive() && f.value() >= 1.0)
            {
                modules[m]->process (L, R, n);
                continue;
            }

            // Crossfade between dry and processed signal.
            std::copy (L, L + n, dryL.begin());
            std::copy (R, R + n, dryR.begin());
            modules[m]->process (L, R, n);
            for (int i = 0; i < n; ++i)
            {
                const double w = f.next();
                L[i] = dryL[(size_t) i] + w * (L[i] - dryL[(size_t) i]);
                R[i] = dryR[(size_t) i] + w * (R[i] - dryR[(size_t) i]);
            }
        }
    }

    EngineSettings st;
    std::array<Module*, kNumModules> modules { &eq, &comp, &wide, &mono, &pan, &lofi, &lim };
    std::array<Smoother, kNumModules> fade;
    std::array<bool, kNumModules> stale {};
    std::vector<double> dryL, dryR;
    int maxN = 512;
    double lastFs = 48000.0;
};
} // namespace pg
