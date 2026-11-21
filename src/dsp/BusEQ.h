// Post-Grape — Module 1: Bus EQ.
// Minimum-phase HPF (40-150 Hz, 12/24 dB/oct), LPF (6-16 kHz, 12 dB/oct),
// four parametric bands (Bell, Notch, Low Shelf, High Shelf, Q up to 20),
// each band operates on Stereo, Mid or Side. TPT SVF, 64-bit.
#pragma once

#include "Svf.h"

namespace pg
{
enum class EqBandType { Bell = 0, Notch, LowShelf, HighShelf };
enum class EqTarget { Stereo = 0, Mid, Side };

struct EqBandSettings
{
    double freq = 1000.0, gainDb = 0.0, q = 1.0;
    EqBandType type = EqBandType::Bell;
    EqTarget target = EqTarget::Stereo;
};

struct EqSettings
{
    double hpFreq = 40.0;
    bool hpSteep = false; // false = 12 dB/oct, true = 24 dB/oct
    double lpFreq = 16000.0;
    double outDb = 0.0;
    std::array<EqBandSettings, 4> bands;
};

inline SvfType toSvfType (EqBandType t) noexcept
{
    switch (t)
    {
        case EqBandType::Notch: return SvfType::Notch;
        case EqBandType::LowShelf: return SvfType::LowShelf;
        case EqBandType::HighShelf: return SvfType::HighShelf;
        default: return SvfType::Bell;
    }
}

// Shelf filters with high Q make big bumps. Limit the shelf Q.
inline double bandQ (const EqBandSettings& b) noexcept
{
    if (b.type == EqBandType::LowShelf || b.type == EqBandType::HighShelf)
        return clampv (b.q, 0.3, 2.0);
    return b.q;
}

// Butterworth Q values for the HPF stages.
constexpr double kQ12 = 0.70710678118654752;
constexpr double kQ24a = 0.54119610014619698;
constexpr double kQ24b = 1.30656296487637652;

class BusEQ : public Module
{
public:
    void setSettings (const EqSettings& s) noexcept { pending = s; }

    void prepare (double sampleRate, int) override
    {
        fs = sampleRate;
        for (auto* sm : allSmoothers())
            sm->prepare (fs, 30.0);
        current = pending;
        snapSmoothers();
        updateAll();
        reset();
    }

    void reset() override
    {
        for (auto& s : hpA) s.reset();
        for (auto& s : hpB) s.reset();
        for (auto& s : lp) s.reset();
        for (auto& b : bandState)
            for (auto& s : b) s.reset();
    }

    void process (double* L, double* R, int n) noexcept override
    {
        // Discrete changes: reset the states that change topology.
        if (pending.hpSteep != current.hpSteep)
        {
            for (auto& s : hpA) s.reset();
            for (auto& s : hpB) s.reset();
        }
        for (size_t b = 0; b < 4; ++b)
            if (pending.bands[b].target != current.bands[b].target)
                for (auto& s : bandState[b]) s.reset();

        current = pending;
        setTargets();

        for (int i = 0; i < n; ++i)
        {
            if (anySmoothing())
                updateAll();
            else if (needUpdate)
            {
                updateAll();
                needUpdate = false;
            }

            double l = L[i], r = R[i];

            // High-pass
            l = hpA[0].process (cHpA, l);
            r = hpA[1].process (cHpA, r);
            if (current.hpSteep)
            {
                l = hpB[0].process (cHpB, l);
                r = hpB[1].process (cHpB, r);
            }

            // Parametric bands
            for (size_t b = 0; b < 4; ++b)
            {
                const auto& c = cBand[b];
                switch (current.bands[b].target)
                {
                    case EqTarget::Stereo:
                        l = bandState[b][0].process (c, l);
                        r = bandState[b][1].process (c, r);
                        break;
                    case EqTarget::Mid:
                    {
                        double m = 0.5 * (l + r), s = 0.5 * (l - r);
                        m = bandState[b][0].process (c, m);
                        l = m + s; r = m - s;
                        break;
                    }
                    case EqTarget::Side:
                    {
                        double m = 0.5 * (l + r), s = 0.5 * (l - r);
                        s = bandState[b][0].process (c, s);
                        l = m + s; r = m - s;
                        break;
                    }
                }
            }

            // Low-pass
            l = lp[0].process (cLp, l);
            r = lp[1].process (cLp, r);

            const double og = outGain.next();
            L[i] = l * og;
            R[i] = r * og;
        }
    }

    // Coefficients for the display (uses the current target values).
    static void designBand (const EqBandSettings& b, double fs, SvfCoefs& c) noexcept
    {
        c.design (toSvfType (b.type), fs, b.freq, bandQ (b), b.gainDb);
    }

private:
    std::array<Smoother*, 15> allSmoothers() noexcept
    {
        return { &hpF, &lpF, &outGain,
                 &bf[0], &bg[0], &bq[0], &bf[1], &bg[1], &bq[1],
                 &bf[2], &bg[2], &bq[2], &bf[3], &bg[3], &bq[3] };
    }

    void setTargets() noexcept
    {
        hpF.set (std::log (current.hpFreq));
        lpF.set (std::log (current.lpFreq));
        outGain.set (dbToGain (current.outDb));
        for (size_t b = 0; b < 4; ++b)
        {
            bf[b].set (std::log (current.bands[b].freq));
            bg[b].set (current.bands[b].gainDb);
            bq[b].set (std::log (bandQ (current.bands[b])));
            if (current.bands[b].type != lastType[b])
            {
                lastType[b] = current.bands[b].type;
                needUpdate = true;
            }
        }
        if (current.hpSteep != lastSteep)
        {
            lastSteep = current.hpSteep;
            needUpdate = true;
        }
    }

    void snapSmoothers() noexcept
    {
        hpF.reset (std::log (current.hpFreq));
        lpF.reset (std::log (current.lpFreq));
        outGain.reset (dbToGain (current.outDb));
        for (size_t b = 0; b < 4; ++b)
        {
            bf[b].reset (std::log (current.bands[b].freq));
            bg[b].reset (current.bands[b].gainDb);
            bq[b].reset (std::log (bandQ (current.bands[b])));
            lastType[b] = current.bands[b].type;
        }
        lastSteep = current.hpSteep;
    }

    bool anySmoothing() const noexcept
    {
        if (hpF.isActive() || lpF.isActive())
            return true;
        for (size_t b = 0; b < 4; ++b)
            if (bf[b].isActive() || bg[b].isActive() || bq[b].isActive())
                return true;
        return false;
    }

    void updateAll() noexcept
    {
        const double hp = std::exp (hpF.next());
        if (current.hpSteep)
        {
            cHpA.design (SvfType::HighPass, fs, hp, kQ24a);
            cHpB.design (SvfType::HighPass, fs, hp, kQ24b);
        }
        else
            cHpA.design (SvfType::HighPass, fs, hp, kQ12);

        cLp.design (SvfType::LowPass, fs, std::exp (lpF.next()), kQ12);

        for (size_t b = 0; b < 4; ++b)
            cBand[b].design (toSvfType (current.bands[b].type), fs,
                             std::exp (bf[b].next()), std::exp (bq[b].next()), bg[b].next());
    }

    double fs = 48000.0;
    EqSettings pending, current;
    Smoother hpF, lpF, outGain;
    std::array<Smoother, 4> bf, bg, bq;
    std::array<EqBandType, 4> lastType {};
    bool lastSteep = false, needUpdate = true;

    SvfCoefs cHpA, cHpB, cLp;
    std::array<SvfCoefs, 4> cBand;
    std::array<SvfState, 2> hpA, hpB, lp;
    std::array<std::array<SvfState, 2>, 4> bandState;
};
} // namespace pg
