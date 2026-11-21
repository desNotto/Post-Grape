// Post-Grape — common DSP helpers.
// All DSP code uses 64-bit (double) values. The DSP code does not use JUCE.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pg
{
constexpr double kPi = 3.14159265358979323846;

inline double dbToGain (double db) noexcept { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g) noexcept { return 20.0 * std::log10 (std::max (g, 1.0e-12)); }

template <typename T>
inline T clampv (T v, T lo, T hi) noexcept { return std::min (hi, std::max (lo, v)); }

// Linear ramp for parameter changes. One call to next() for each sample.
class Smoother
{
public:
    void prepare (double sampleRate, double rampMs) noexcept
    {
        steps = std::max (1, (int) std::lround (sampleRate * rampMs * 0.001));
    }

    void reset (double v) noexcept { cur = target = v; left = 0; inc = 0.0; }

    void set (double t) noexcept
    {
        if (t == target)
            return;
        target = t;
        left = steps;
        inc = (target - cur) / (double) steps;
    }

    double next() noexcept
    {
        if (left > 0)
        {
            cur += inc;
            if (--left == 0)
                cur = target;
        }
        return cur;
    }

    bool isActive() const noexcept { return left > 0; }
    double value() const noexcept { return cur; }
    double targetValue() const noexcept { return target; }

private:
    double cur = 0.0, target = 0.0, inc = 0.0;
    int left = 0, steps = 1;
};

// Small fast random generator (xorshift). Real-time safe.
class Random
{
public:
    explicit Random (uint32_t seed = 0x9E3779B9u) : s (seed ? seed : 1u) {}
    uint32_t nextU() noexcept
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    // Value in the range -1 .. +1.
    double nextBipolar() noexcept { return (double) nextU() * (2.0 / 4294967295.0) - 1.0; }
    // Value in the range 0 .. 1.
    double nextUnit() noexcept { return (double) nextU() * (1.0 / 4294967295.0); }

private:
    uint32_t s;
};

// Base class for one module of the chain. The module processes the stereo signal in place.
class Module
{
public:
    virtual ~Module() = default;
    virtual void prepare (double sampleRate, int maxBlock) = 0;
    virtual void reset() = 0;
    virtual void process (double* L, double* R, int n) noexcept = 0;
};
} // namespace pg
