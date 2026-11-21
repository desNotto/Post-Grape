// Post-Grape — Module 6: Lofi.
// LOFI ON/OFF switch with a smooth crossfade (for automation).
// Wobble -> Drive -> Tone (band-pass) -> Rate (sample and hold) -> Bits -> Noise -> Mix.
// Wobble uses a modulated delay with a minimum delay of 0 samples.
// When Wobble is 0, the module adds no delay.
#pragma once

#include "Svf.h"

namespace pg
{
struct LofiSettings
{
    bool active = false;
    double toneHz = 1800.0;  // 300..6000
    double bits = 10.0;      // 4..16
    double rateHz = 16000.0; // 2000..48000
    double noise = 0.15;     // 0..1
    double wobble = 0.2;     // 0..1
    double driveDb = 6.0;    // 0..24
    double mix = 1.0;        // 0..1
};

class Lofi : public Module
{
public:
    void setSettings (const LofiSettings& s) noexcept { st = s; }

    static constexpr double kMaxWobbleMs = 4.0;

    void prepare (double sampleRate, int) override
    {
        fs = sampleRate;
        engage.prepare (fs, 25.0);
        for (auto* s : { &tone, &bits, &noise, &wobble, &drive, &mix })
            s->prepare (fs, 30.0);
        engage.reset (st.active ? 1.0 : 0.0);
        tone.reset (std::log (st.toneHz));
        bits.reset (st.bits);
        noise.reset (st.noise);
        wobble.reset (st.wobble);
        drive.reset (st.driveDb);
        mix.reset (st.mix);
        updateTone();

        delaySize = 1;
        while (delaySize < (int) (fs * kMaxWobbleMs * 0.001) + 8)
            delaySize <<= 1;
        for (auto& d : delay)
            d.assign ((size_t) delaySize, 0.0);
        reset();
    }

    void reset() override
    {
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0);
        wpos = 0;
        resetFilters();
    }

    void resetFilters() noexcept
    {
        for (auto& s : hp) s.reset();
        for (auto& s : lp) s.reset();
        holdPhase = 1.0;
        hold[0] = hold[1] = 0.0;
        hiss[0] = hiss[1] = 0.0;
        crackle[0] = crackle[1] = 0.0;
        lfo1 = 0.0; lfo2 = 0.0;
    }

    bool isSilent() const noexcept { return engage.value() <= 0.0 && ! engage.isActive(); }

    void process (double* L, double* R, int n) noexcept override
    {
        engage.set (st.active ? 1.0 : 0.0);
        tone.set (std::log (st.toneHz));
        bits.set (st.bits);
        noise.set (st.noise);
        wobble.set (st.wobble);
        drive.set (st.driveDb);
        mix.set (st.mix);

        if (isSilent() && engage.targetValue() <= 0.0)
        {
            // Fully off: keep the delay line filled for a clean start.
            for (int i = 0; i < n; ++i)
                writeDelay (L[i], R[i]);
            resetFilters();
            return;
        }

        const double lfoInc1 = 0.55 / fs, lfoInc2 = 5.7 / fs;
        const double maxDelay = fs * kMaxWobbleMs * 0.001;
        const double hissA = std::exp (-2.0 * kPi * 3500.0 / fs);
        const double crackA = std::exp (-2.0 * kPi * 900.0 / fs);

        for (int i = 0; i < n; ++i)
        {
            if (tone.isActive())
            {
                tone.next();
                updateTone();
            }
            const double eg = engage.next();
            const double b = bits.next();
            const double nz = noise.next();
            const double wb = wobble.next();
            const double drv = dbToGain (drive.next());
            const double mx = mix.next();

            const double dryIn[2] = { L[i], R[i] };
            writeDelay (dryIn[0], dryIn[1]);

            // Wobble: slow wow + small flutter. Delay range 0 .. wb * max.
            lfo1 += lfoInc1; if (lfo1 >= 1.0) lfo1 -= 1.0;
            lfo2 += lfoInc2; if (lfo2 >= 1.0) lfo2 -= 1.0;
            const double mod = 0.8 * (0.5 - 0.5 * std::cos (2.0 * kPi * lfo1))
                             + 0.2 * (0.5 - 0.5 * std::cos (2.0 * kPi * lfo2));
            const double d = wb * maxDelay * mod;

            holdPhase += rateStep();
            const bool takeSample = holdPhase >= 1.0;
            if (takeSample)
                holdPhase -= std::floor (holdPhase);

            const double step = std::pow (2.0, 1.0 - b);
            const double comp = 1.0 / std::sqrt (drv);

            double out[2];
            for (int c = 0; c < 2; ++c)
            {
                double x = d > 0.0 ? readDelay (c, d) : dryIn[c];
                x = std::tanh (x * drv) * comp;
                x = hp[c].process (cHp, x);
                x = lp[c].process (cLp, x);

                if (st.rateHz < fs * 0.999)
                {
                    if (takeSample)
                        hold[c] = x;
                    x = hold[c];
                }

                x = std::round (x / step) * step;

                // Noise: hiss and crackle.
                hiss[c] = hissA * hiss[c] + (1.0 - hissA) * rnd.nextBipolar();
                if (rnd.nextUnit() < nz * 0.0006)
                    crackle[c] += rnd.nextBipolar() * 0.35 * nz;
                crackle[c] *= crackA;
                x += nz * 0.05 * hiss[c] + crackle[c];

                const double wet = dryIn[c] + mx * (x - dryIn[c]);
                out[c] = dryIn[c] + eg * (wet - dryIn[c]);
            }
            L[i] = out[0];
            R[i] = out[1];
        }
    }

private:
    double rateStep() const noexcept { return clampv (st.rateHz, 500.0, fs) / fs; }

    void updateTone() noexcept
    {
        const double t = std::exp (tone.value());
        cHp.design (SvfType::HighPass, fs, std::max (30.0, t / 5.0), 0.70710678118654752);
        cLp.design (SvfType::LowPass, fs, std::min (fs * 0.45, t * 2.2), 0.70710678118654752);
    }

    void writeDelay (double l, double r) noexcept
    {
        wpos = (wpos + 1) & (delaySize - 1);
        delay[0][(size_t) wpos] = l;
        delay[1][(size_t) wpos] = r;
    }

    // Read with a fractional delay (in samples). Hermite interpolation.
    double readDelay (int c, double d) const noexcept
    {
        const int di = (int) d;
        const double f = d - di;
        const int mask = delaySize - 1;
        const auto& buf = delay[(size_t) c];
        const double xm1 = buf[(size_t) ((wpos - di + 1) & mask)];
        const double x0 = buf[(size_t) ((wpos - di) & mask)];
        const double x1 = buf[(size_t) ((wpos - di - 1) & mask)];
        const double x2 = buf[(size_t) ((wpos - di - 2) & mask)];
        // For d < 1 the "future" point xm1 is the same as x0.
        const double a = di == 0 ? x0 : xm1;
        const double c0 = x0;
        const double c1 = 0.5 * (x1 - a);
        const double c2 = a - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
        const double c3 = 0.5 * (x2 - a) + 1.5 * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }

    double fs = 48000.0;
    LofiSettings st;
    Smoother engage, tone, bits, noise, wobble, drive, mix;
    SvfCoefs cHp, cLp;
    std::array<SvfState, 2> hp, lp;
    std::array<std::vector<double>, 2> delay;
    int delaySize = 1, wpos = 0;
    double holdPhase = 1.0, hold[2] = { 0, 0 }, hiss[2] = { 0, 0 }, crackle[2] = { 0, 0 };
    double lfo1 = 0.0, lfo2 = 0.0;
    Random rnd;
};
} // namespace pg
