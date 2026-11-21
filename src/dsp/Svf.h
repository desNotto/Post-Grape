// Post-Grape — Topology-Preserving Transform State Variable Filter (TPT SVF).
// Structure: A. Simper (Cytomic), "Linear trapezoidal integrated SVF".
// The structure is stable during fast parameter changes. 64-bit state.
//
// Output = m0 * input + m1 * band + m2 * low.
#pragma once

#include "Common.h"
#include <complex>

namespace pg
{
enum class SvfType
{
    Bell,
    Notch,
    LowShelf,
    HighShelf,
    LowPass,
    HighPass,
    BandPass,
    AllPass
};

struct SvfCoefs
{
    double g = 0.0, k = 1.0, a1 = 1.0, a2 = 0.0, a3 = 0.0;
    double m0 = 1.0, m1 = 0.0, m2 = 0.0;

    // Calculate the coefficients. freq in Hz, q > 0, gainDb for Bell / shelves.
    void design (SvfType type, double sampleRate, double freq, double q, double gainDb = 0.0) noexcept
    {
        freq = clampv (freq, 5.0, sampleRate * 0.49);
        q = std::max (q, 0.025);
        const double w = std::tan (kPi * freq / sampleRate);
        const double A = std::pow (10.0, gainDb / 40.0);

        switch (type)
        {
            case SvfType::Bell:
                g = w; k = 1.0 / (q * A);
                m0 = 1.0; m1 = k * (A * A - 1.0); m2 = 0.0;
                break;
            case SvfType::Notch:
                g = w; k = 1.0 / q;
                m0 = 1.0; m1 = -k; m2 = 0.0;
                break;
            case SvfType::LowShelf:
                g = w / std::sqrt (A); k = 1.0 / q;
                m0 = 1.0; m1 = k * (A - 1.0); m2 = A * A - 1.0;
                break;
            case SvfType::HighShelf:
                g = w * std::sqrt (A); k = 1.0 / q;
                m0 = A * A; m1 = k * (1.0 - A) * A; m2 = 1.0 - A * A;
                break;
            case SvfType::LowPass:
                g = w; k = 1.0 / q;
                m0 = 0.0; m1 = 0.0; m2 = 1.0;
                break;
            case SvfType::HighPass:
                g = w; k = 1.0 / q;
                m0 = 1.0; m1 = -k; m2 = -1.0;
                break;
            case SvfType::BandPass:
                g = w; k = 1.0 / q;
                m0 = 0.0; m1 = 1.0; m2 = 0.0;
                break;
            case SvfType::AllPass:
                g = w; k = 1.0 / q;
                m0 = 1.0; m1 = -2.0 * k; m2 = 0.0;
                break;
        }

        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    // Exact frequency response of the digital filter at frequency f.
    // The TPT SVF is the bilinear transform of the analog prototype with pre-warp.
    std::complex<double> response (double f, double sampleRate) const noexcept
    {
        const double wd = std::tan (kPi * clampv (f, 1.0, sampleRate * 0.4999) / sampleRate);
        const std::complex<double> s (0.0, wd / g);
        const std::complex<double> den = s * s + k * s + 1.0;
        const std::complex<double> band = s / den;
        const std::complex<double> low = 1.0 / den;
        return m0 + m1 * band + m2 * low;
    }
};

struct SvfState
{
    double ic1 = 0.0, ic2 = 0.0;

    void reset() noexcept { ic1 = ic2 = 0.0; }

    // Calculate band (v1) and low (v2) outputs.
    inline void tick (const SvfCoefs& c, double v0, double& v1, double& v2) noexcept
    {
        const double v3 = v0 - ic2;
        v1 = c.a1 * ic1 + c.a2 * v3;
        v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
    }

    inline double process (const SvfCoefs& c, double v0) noexcept
    {
        double v1, v2;
        tick (c, v0, v1, v2);
        return c.m0 * v0 + c.m1 * v1 + c.m2 * v2;
    }

    // Low-pass and high-pass outputs of one 2nd-order stage (same input).
    inline void lowHigh (const SvfCoefs& c, double v0, double& low, double& high) noexcept
    {
        double v1, v2;
        tick (c, v0, v1, v2);
        low = v2;
        high = v0 - c.k * v1 - v2;
    }
};

// Linkwitz-Riley 4th-order crossover with TPT SVF stages. Zero latency.
// low + high = 2nd-order all-pass (flat magnitude).
// Use allPass() on other signals to keep the same phase.
class LR4Crossover
{
public:
    void design (double sampleRate, double freq) noexcept
    {
        c.design (SvfType::LowPass, sampleRate, freq, 0.70710678118654752);
    }

    void reset() noexcept { s1.reset(); sLow.reset(); sHigh.reset(); }

    inline void split (double x, double& low, double& high) noexcept
    {
        double l1, h1, dummyL, dummyH;
        s1.lowHigh (c, x, l1, h1);
        sLow.lowHigh (c, l1, low, dummyH);
        sHigh.lowHigh (c, h1, dummyL, high);
    }

    const SvfCoefs& coefs() const noexcept { return c; }

private:
    SvfCoefs c;
    SvfState s1, sLow, sHigh;
};

// 2nd-order all-pass with the same phase as the sum of an LR4 crossover.
class LR4AllPass
{
public:
    void design (double sampleRate, double freq) noexcept
    {
        c.design (SvfType::AllPass, sampleRate, freq, 0.70710678118654752);
    }
    void reset() noexcept { s.reset(); }
    inline double process (double x) noexcept { return s.process (c, x); }

private:
    SvfCoefs c;
    SvfState s;
};
} // namespace pg
