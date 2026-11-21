// Post-Grape — polyphase IIR half-band filters for 2x up/down sampling.
// Two all-pass chains (method of L. de Soras, "hiir"). Minimum phase,
// very small group delay, no reported latency.
#pragma once

#include "Common.h"

namespace pg
{
namespace halfband
{
inline double ipowp (double x, long n) noexcept
{
    double z = 1.0;
    while (n != 0)
    {
        if ((n & 1) != 0) z *= x;
        n >>= 1;
        x *= x;
    }
    return z;
}

inline double accNum (double q, int order, int c) noexcept
{
    double acc = 0.0, q_ii1;
    int i = 0, j = 1;
    do
    {
        q_ii1 = ipowp (q, (long) i * (i + 1));
        q_ii1 *= std::sin ((i * 2 + 1) * c * kPi / order) * j;
        acc += q_ii1;
        j = -j;
        ++i;
    } while (std::abs (q_ii1) > 1e-100);
    return acc;
}

inline double accDen (double q, int order, int c) noexcept
{
    double acc = 0.0, q_i2;
    int i = 1, j = -1;
    do
    {
        q_i2 = ipowp (q, (long) i * i);
        q_i2 *= std::cos (i * 2 * c * kPi / order) * j;
        acc += q_i2;
        j = -j;
        ++i;
    } while (std::abs (q_i2) > 1e-100);
    return acc;
}

// Calculate the all-pass coefficients. transition: normalized transition
// bandwidth (0 .. 0.5), relative to the high sample rate.
inline std::vector<double> design (int nbrCoefs, double transition)
{
    double k = std::tan ((1.0 - transition * 2.0) * kPi / 4.0);
    k *= k;
    const double kksqrt = std::pow (1.0 - k * k, 0.25);
    const double e = 0.5 * (1.0 - kksqrt) / (1.0 + kksqrt);
    const double e2 = e * e, e4 = e2 * e2;
    const double q = e * (1.0 + e4 * (2.0 + e4 * (15.0 + 150.0 * e4)));

    const int order = nbrCoefs * 2 + 1;
    std::vector<double> coefs ((size_t) nbrCoefs);
    for (int index = 0; index < nbrCoefs; ++index)
    {
        const int c = index + 1;
        const double num = accNum (q, order, c) * std::pow (q, 0.25);
        const double den = accDen (q, order, c) + 0.5;
        const double ww = num / den;
        const double wwsq = ww * ww;
        const double x = std::sqrt ((1.0 - wwsq * k) * (1.0 - wwsq / k)) / (1.0 + wwsq);
        coefs[(size_t) index] = (1.0 - x) / (1.0 + x);
    }
    return coefs;
}
} // namespace halfband

// One first-order all-pass section in the polyphase structure.
struct ApSection
{
    double c = 0.0, x1 = 0.0, y1 = 0.0;
    inline double process (double x) noexcept
    {
        const double y = c * (x - y1) + x1;
        x1 = x;
        y1 = y;
        return y;
    }
    void reset() noexcept { x1 = y1 = 0.0; }
};

// 2x up/down sampler for one channel.
class Halfband2x
{
public:
    void setCoefs (const std::vector<double>& coefs)
    {
        pathA.clear();
        pathB.clear();
        for (size_t i = 0; i < coefs.size(); ++i)
        {
            ApSection s;
            s.c = coefs[i];
            ((i & 1) == 0 ? pathA : pathB).push_back (s);
        }
        reset();
    }

    void reset() noexcept
    {
        for (auto& s : pathA) s.reset();
        for (auto& s : pathB) s.reset();
    }

    // One input sample -> two output samples.
    inline void upsample (double x, double& out0, double& out1) noexcept
    {
        double a = x, b = x;
        for (auto& s : pathA) a = s.process (a);
        for (auto& s : pathB) b = s.process (b);
        out0 = a;
        out1 = b;
    }

    // Two input samples -> one output sample.
    inline double downsample (double in0, double in1) noexcept
    {
        double a = in1, b = in0;
        for (auto& s : pathA) a = s.process (a);
        for (auto& s : pathB) b = s.process (b);
        return 0.5 * (a + b);
    }

private:
    std::vector<ApSection> pathA, pathB;
};

// Cascade of 2x stages: factor 2^stages. Up and down chains for one channel.
class Oversampler
{
public:
    static constexpr int kMaxStages = 4; // 16x

    void prepare (int maxBlock)
    {
        // First stage: steep (most critical). Next stages: lighter filters.
        const auto c1 = halfband::design (10, 0.04);
        const auto c2 = halfband::design (6, 0.13);
        const auto c3 = halfband::design (4, 0.25);
        for (int s = 0; s < kMaxStages; ++s)
        {
            const auto& c = s == 0 ? c1 : (s == 1 ? c2 : c3);
            up[(size_t) s].setCoefs (c);
            down[(size_t) s].setCoefs (c);
        }
        buf.assign ((size_t) (1 << kMaxStages) * 2, 0.0);
        (void) maxBlock;
    }

    void reset() noexcept
    {
        for (auto& s : up) s.reset();
        for (auto& s : down) s.reset();
    }

    // Upsample one sample to 'factor' samples in out[].
    inline void upsample (double x, double* out, int stages) noexcept
    {
        out[0] = x;
        int len = 1;
        for (int s = 0; s < stages; ++s)
        {
            for (int i = 0; i < len; ++i) // time order
            {
                double a, b;
                up[(size_t) s].upsample (out[i], a, b);
                buf[(size_t) (2 * i)] = a;
                buf[(size_t) (2 * i + 1)] = b;
            }
            len *= 2;
            for (int i = 0; i < len; ++i)
                out[i] = buf[(size_t) i];
        }
    }

    // Downsample 'factor' samples in in[] to one sample. in[] is changed.
    inline double downsample (double* in, int stages) noexcept
    {
        int len = 1 << stages;
        for (int s = stages - 1; s >= 0; --s)
        {
            len /= 2;
            for (int i = 0; i < len; ++i)
                in[i] = down[(size_t) s].downsample (in[2 * i], in[2 * i + 1]);
        }
        return in[0];
    }

private:
    std::array<Halfband2x, kMaxStages> up, down;
    std::vector<double> buf;
};
} // namespace pg
