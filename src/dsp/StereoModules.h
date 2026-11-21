// Post-Grape — Modules 3, 4, 5: Wider, Mono-Maker, True Stereo Pan.
// All modules have zero latency.
#pragma once

#include "Svf.h"
#include <atomic>

namespace pg
{
// ---------------------------------------------------------------------------
// Module 3: Wider. Mid/Side width, Side Air (high shelf on Side),
// Side Cut (high-pass on Side). No Haas delay: mono compatibility stays good.
struct WiderSettings
{
    double width = 1.0;     // 0..2
    double airDb = 0.0;     // 0..9
    double sideCutHz = 20.0; // 20 = off
};

class Wider : public Module
{
public:
    void setSettings (const WiderSettings& s) noexcept { st = s; }

    // Stereo correlation of the output (-1..+1). For the meter.
    std::atomic<float> correlation { 1.0f };

    static constexpr double kAirFreq = 4500.0;
    static constexpr double kCutOff = 20.5;

    void prepare (double sampleRate, int) override
    {
        fs = sampleRate;
        width.prepare (fs, 30.0);
        air.prepare (fs, 30.0);
        cut.prepare (fs, 30.0);
        width.reset (st.width);
        air.reset (st.airDb);
        cut.reset (std::log (std::max (st.sideCutHz, 20.0)));
        update();
        corrA = std::exp (-1.0 / (0.3 * fs));
        reset();
    }

    void reset() override
    {
        sAir.reset();
        sCut.reset();
        eLR = 0.0; eLL = eRR = 1.0e-9;
        correlation.store (1.0f);
    }

    void process (double* L, double* R, int n) noexcept override
    {
        width.set (st.width);
        air.set (st.airDb);
        cut.set (std::log (std::max (st.sideCutHz, 20.0)));

        for (int i = 0; i < n; ++i)
        {
            if (air.isActive() || cut.isActive())
            {
                air.next();
                cut.next();
                update();
            }
            const double w = width.next();

            const double m = 0.5 * (L[i] + R[i]);
            double s = 0.5 * (L[i] - R[i]);
            if (cutOn)
                s = sCut.process (cCut, s);
            s = sAir.process (cAir, s);
            s *= w;

            const double l = m + s, r = m - s;
            L[i] = l;
            R[i] = r;

            eLR = corrA * eLR + (1.0 - corrA) * l * r;
            eLL = corrA * eLL + (1.0 - corrA) * l * l;
            eRR = corrA * eRR + (1.0 - corrA) * r * r;
        }

        const double den = std::sqrt (eLL * eRR);
        correlation.store (den > 1.0e-10 ? (float) clampv (eLR / den, -1.0, 1.0) : 1.0f);
    }

private:
    void update() noexcept
    {
        cAir.design (SvfType::HighShelf, fs, kAirFreq, 0.6, air.value());
        const double f = std::exp (cut.value());
        cutOn = f > kCutOff;
        cCut.design (SvfType::HighPass, fs, f, 0.70710678118654752);
    }

    double fs = 48000.0;
    WiderSettings st;
    Smoother width, air, cut;
    SvfCoefs cAir, cCut;
    SvfState sAir, sCut;
    bool cutOn = false;
    double corrA = 0.999, eLR = 0.0, eLL = 1.0e-9, eRR = 1.0e-9;
};

// ---------------------------------------------------------------------------
// Module 4: Mono-Maker. LR4 crossover on the Side signal only.
// The Mid signal goes through the matching all-pass. Thus Mid and Side keep
// the same phase, and there is no phase cancellation.
struct MonoSettings
{
    double freqHz = 120.0; // 60..250
    double width = 0.0;    // 0..1 (low band width)
};

class MonoMaker : public Module
{
public:
    void setSettings (const MonoSettings& s) noexcept { st = s; }

    void prepare (double sampleRate, int) override
    {
        fs = sampleRate;
        freq.prepare (fs, 30.0);
        width.prepare (fs, 30.0);
        freq.reset (std::log (st.freqHz));
        width.reset (st.width);
        update();
        reset();
    }

    void reset() override
    {
        xo.reset();
        ap.reset();
    }

    void process (double* L, double* R, int n) noexcept override
    {
        freq.set (std::log (st.freqHz));
        width.set (clampv (st.width, 0.0, 1.0));

        for (int i = 0; i < n; ++i)
        {
            if (freq.isActive())
            {
                freq.next();
                update();
            }
            const double w = width.next();

            const double m = ap.process (0.5 * (L[i] + R[i]));
            double sLow, sHigh;
            xo.split (0.5 * (L[i] - R[i]), sLow, sHigh);
            const double s = sHigh + w * sLow;

            L[i] = m + s;
            R[i] = m - s;
        }
    }

private:
    void update() noexcept
    {
        const double f = std::exp (freq.value());
        xo.design (fs, f);
        ap.design (fs, f);
    }

    double fs = 48000.0;
    MonoSettings st;
    Smoother freq, width;
    LR4Crossover xo;
    LR4AllPass ap;
};

// ---------------------------------------------------------------------------
// Module 5: True Stereo Pan (Logic Pro "Stereo Pan" behavior).
// Left input position = pan - spread, right input position = pan + spread.
// Each input uses a constant-power pan law. The outputs are added.
// Default (pan 0, spread 1) is an exact identity.
struct PanSettings
{
    double pan = 0.0;    // -1..+1
    double spread = 1.0; // 0..1
};

class TruePan : public Module
{
public:
    void setSettings (const PanSettings& s) noexcept { st = s; }

    void prepare (double sampleRate, int) override
    {
        pan.prepare (sampleRate, 30.0);
        spread.prepare (sampleRate, 30.0);
        pan.reset (st.pan);
        spread.reset (st.spread);
        update();
    }

    void reset() override {}

    void process (double* L, double* R, int n) noexcept override
    {
        pan.set (clampv (st.pan, -1.0, 1.0));
        spread.set (clampv (st.spread, 0.0, 1.0));

        for (int i = 0; i < n; ++i)
        {
            if (pan.isActive() || spread.isActive())
            {
                pan.next();
                spread.next();
                update();
            }
            const double l = L[i], r = R[i];
            L[i] = l * gLL + r * gRL;
            R[i] = l * gLR + r * gRR;
        }
    }

    // Positions of the two inputs (-1..+1). Also used by the display.
    static void positions (double pan, double spread, double& posL, double& posR) noexcept
    {
        posL = clampv (pan - spread, -1.0, 1.0);
        posR = clampv (pan + spread, -1.0, 1.0);
    }

private:
    static void law (double pos, double& gl, double& gr) noexcept
    {
        const double th = (pos + 1.0) * kPi * 0.25;
        gl = std::cos (th);
        gr = std::sin (th);
        // Exact values at the edges.
        if (pos <= -1.0) { gl = 1.0; gr = 0.0; }
        if (pos >= 1.0) { gl = 0.0; gr = 1.0; }
    }

    void update() noexcept
    {
        double pl, pr;
        positions (pan.value(), spread.value(), pl, pr);
        law (pl, gLL, gLR);
        law (pr, gRL, gRR);
    }

    PanSettings st;
    Smoother pan, spread;
    double gLL = 1.0, gLR = 0.0, gRL = 0.0, gRR = 1.0;
};
} // namespace pg
