// Post-Grape — Module 2: Bus Compressor (low band only).
// LR4 crossover at 80-350 Hz. The compressor operates on the low band.
// The high band passes without change. Sidechain HPF, stereo link. Zero latency.
#pragma once

#include "Svf.h"
#include <atomic>

namespace pg
{
struct CompSettings
{
    double splitHz = 160.0;
    double thresholdDb = -12.0;
    double ratio = 3.0;
    double attackMs = 10.0;
    double releaseMs = 120.0;
    double scHpfHz = 20.0;
    double link = 1.0;      // 0..1
    double makeupDb = 0.0;
};

class BusComp : public Module
{
public:
    void setSettings (const CompSettings& s) noexcept { st = s; }

    // Gain reduction in dB (positive value). For the meter.
    std::atomic<float> grDb { 0.0f };

    void prepare (double sampleRate, int) override
    {
        fs = sampleRate;
        split.prepare (fs, 30.0);
        sc.prepare (fs, 30.0);
        makeup.prepare (fs, 30.0);
        split.reset (std::log (st.splitHz));
        sc.reset (std::log (st.scHpfHz));
        makeup.reset (dbToGain (st.makeupDb));
        updateFilters();
        reset();
    }

    void reset() override
    {
        for (auto& x : xo) x.reset();
        for (auto& s : scState) s.reset();
        env[0] = env[1] = 0.0;
        grDb.store (0.0f);
    }

    void process (double* L, double* R, int n) noexcept override
    {
        split.set (std::log (st.splitHz));
        sc.set (std::log (st.scHpfHz));
        makeup.set (dbToGain (st.makeupDb));

        const double aAtt = std::exp (-1.0 / (std::max (0.05, st.attackMs) * 0.001 * fs));
        const double aRel = std::exp (-1.0 / (std::max (1.0, st.releaseMs) * 0.001 * fs));
        const double T = st.thresholdDb, ratio = std::max (1.0, st.ratio);
        const double knee = 6.0, link = clampv (st.link, 0.0, 1.0);
        double maxGr = 0.0;

        for (int i = 0; i < n; ++i)
        {
            if (split.isActive() || sc.isActive())
            {
                split.next();
                sc.next();
                updateFilters();
            }
            const double mk = makeup.next();

            double low[2], high[2], lvl[2];
            double in[2] = { L[i], R[i] };
            for (int c = 0; c < 2; ++c)
            {
                xo[c].split (in[c], low[c], high[c]);
                const double s = scState[c].process (cSc, low[c]);
                lvl[c] = gainToDb (std::abs (s));
            }

            const double lmax = std::max (lvl[0], lvl[1]);
            double g[2];
            for (int c = 0; c < 2; ++c)
            {
                const double x = (1.0 - link) * lvl[c] + link * lmax;
                const double y = staticCurve (x, T, ratio, knee);
                const double target = y - x; // <= 0
                const double a = target < env[c] ? aAtt : aRel;
                env[c] = a * env[c] + (1.0 - a) * target;
                g[c] = dbToGain (env[c]) * mk;
                maxGr = std::max (maxGr, -env[c]);
            }

            L[i] = high[0] + low[0] * g[0];
            R[i] = high[1] + low[1] * g[1];
        }

        grDb.store ((float) maxGr);
    }

    static double staticCurve (double x, double T, double ratio, double knee) noexcept
    {
        const double d = x - T;
        if (2.0 * d < -knee)
            return x;
        if (2.0 * d > knee)
            return T + d / ratio;
        const double t = d + knee * 0.5;
        return x + (1.0 / ratio - 1.0) * t * t / (2.0 * knee);
    }

private:
    void updateFilters() noexcept
    {
        const double f = std::exp (split.value());
        for (auto& x : xo) x.design (fs, f);
        cSc.design (SvfType::HighPass, fs, std::exp (sc.value()), 0.70710678118654752);
    }

    double fs = 48000.0;
    CompSettings st;
    Smoother split, sc, makeup;
    std::array<LR4Crossover, 2> xo;
    std::array<SvfState, 2> scState;
    SvfCoefs cSc;
    double env[2] = { 0.0, 0.0 };
};
} // namespace pg
