// Post-Grape — Module 8: Auto-Gain (the last module of the chain).
// Measures the loudness at the plugin input and at the plugin output.
// Sets the output loudness equal to the input loudness. Zero latency.
//
// Detection: LUFS (K-weighting, ITU-R BS.1770) or RMS (no weighting).
// The gain changes slowly (SPEED). RANGE limits the correction. TRIM adds an offset.
// HOLD freezes the matched gain. In silence (input below -60 dB) the gain does not change.
// A positive gain never pushes the peaks above the ceiling.
#pragma once

#include "Svf.h"
#include <atomic>

namespace pg
{
struct AutoGainSettings
{
    double speedSec = 3.0;  // 0.3..10
    double rangeDb = 12.0;  // 0..24
    double trimDb = 0.0;    // -12..+12
    bool lufs = true;       // true = LUFS (K-weighting), false = RMS
    bool hold = false;
    double ceilingDb = 0.0; // peak limit for positive gain (limiter ceiling)
};

// Loudness detector for one stereo signal.
class LoudnessDetector
{
public:
    void prepare (double fs)
    {
        // K-weighting: high shelf +4 dB at 1682 Hz, then high-pass at 38 Hz.
        shelf.design (SvfType::HighShelf, fs, 1681.97, 0.7071, 3.99984);
        hp.design (SvfType::HighPass, fs, 38.135, 0.5003);
        reset();
    }
    void reset() noexcept
    {
        for (auto& s : sShelf) s.reset();
        for (auto& s : sHp) s.reset();
        energy = 0.0;
    }
    // Returns the instantaneous power of one sample pair.
    inline double power (double l, double r, bool lufs) noexcept
    {
        if (lufs)
        {
            l = sHp[0].process (hp, sShelf[0].process (shelf, l));
            r = sHp[1].process (hp, sShelf[1].process (shelf, r));
            return l * l + r * r; // BS.1770: sum of the channel powers
        }
        return 0.5 * (l * l + r * r); // RMS: mean of the channels
    }
    inline void integrate (double p, double a) noexcept { energy = a * energy + (1.0 - a) * p; }
    double energy = 0.0;

    // Level in LUFS or dBFS (RMS).
    static double levelDb (double e, bool lufs) noexcept
    {
        return (lufs ? -0.691 : 0.0) + 10.0 * std::log10 (std::max (e, 1.0e-12));
    }

private:
    SvfCoefs shelf, hp;
    std::array<SvfState, 2> sShelf, sHp;
};

class AutoGain : public Module
{
public:
    void setSettings (const AutoGainSettings& s) noexcept { st = s; }
    // The unprocessed plugin input of the current block (n samples).
    void setInput (const double* l, const double* r) noexcept { inL = l; inR = r; }

    // Meter values.
    std::atomic<float> gainDb { 0.0f }, inLevel { -100.0f }, outLevel { -100.0f };

    static constexpr double kGateDb = -60.0;

    void prepare (double sampleRate, int) override
    {
        fs = sampleRate;
        detIn.prepare (fs);
        detOut.prepare (fs);
        trim.prepare (fs, 50.0);
        trim.reset (st.trimDb);
        peakRel = std::exp (-1.0 / (0.5 * fs));
        reset();
    }

    void reset() override
    {
        detIn.reset();
        detOut.reset();
        match = 0.0;
        peakEnv = 0.0;
        warm = 0;
        gainDb.store (0.0f);
    }

    void process (double* L, double* R, int n) noexcept override
    {
        trim.set (st.trimDb);
        const double speed = clampv (st.speedSec, 0.1, 20.0);
        // SPEED = time to follow approx. 95 % of a loudness change (3 time constants).
        const double aDet = std::exp (-1.0 / (clampv (speed / 6.0, 0.1, 1.5) * fs));
        const double aGain = std::exp (-1.0 / (speed / 3.0 * fs));
        const double aFast = std::exp (-1.0 / (0.15 * fs));
        const int warmup = (int) (fs * 1.0);
        const double range = clampv (st.rangeDb, 0.0, 48.0);
        const double ceil = dbToGain (std::min (0.0, st.ceilingDb));
        const double gateE = std::pow (10.0, (kGateDb + (st.lufs ? 0.691 : 0.0)) / 10.0);
        double last = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const double xl = inL != nullptr ? inL[i] : L[i];
            const double xr = inR != nullptr ? inR[i] : R[i];
            detIn.integrate (detIn.power (xl, xr, st.lufs), aDet);
            detOut.integrate (detOut.power (L[i], R[i], st.lufs), aDet);

            // Matched gain (frozen by HOLD and in silence).
            if (! st.hold && detIn.energy > gateE && detOut.energy > gateE * 1.0e-3)
            {
                const double target = clampv (10.0 * std::log10 (detIn.energy / detOut.energy), -range, range);
                // Fast start: the first second after a reset follows quickly.
                const double a = warm < warmup ? aFast : aGain;
                match = target + (match - target) * a;
                if (warm < warmup) ++warm;
            }
            double g = clampv (match, -range, range) + trim.next();

            // Peak safety for positive gain.
            const double pk = std::max (std::abs (L[i]), std::abs (R[i]));
            peakEnv = pk > peakEnv ? pk : peakEnv * peakRel;
            if (g > 0.0 && peakEnv > 1.0e-9)
                g = std::min (g, gainToDb (ceil / peakEnv));

            const double lin = dbToGain (g);
            L[i] *= lin;
            R[i] *= lin;
            if (g > 0.0) // a boost must not make new overs
            {
                L[i] = clampv (L[i], -ceil, ceil);
                R[i] = clampv (R[i], -ceil, ceil);
            }
            last = g;
        }

        gainDb.store ((float) last);
        inLevel.store ((float) LoudnessDetector::levelDb (detIn.energy, st.lufs));
        outLevel.store ((float) LoudnessDetector::levelDb (detOut.energy, st.lufs));
        inL = inR = nullptr;
    }

    double matchedGainDb() const noexcept { return match; }

private:
    double fs = 48000.0;
    AutoGainSettings st;
    LoudnessDetector detIn, detOut;
    Smoother trim;
    double match = 0.0, peakEnv = 0.0, peakRel = 0.9999;
    int warm = 0;
    const double* inL = nullptr;
    const double* inR = nullptr;
};
} // namespace pg
