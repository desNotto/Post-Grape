// Post-Grape — Module 7: Bus Limiter.
// Input gain -> soft clip (oversampled) -> true-peak brickwall limiter -> output gain.
//
// Soft clip: up N x -> clip curve -> down N x (N = 4, 8, 16). Polyphase IIR filters.
// Limiter: the detector upsamples the signal N x and finds the inter-sample peaks
// (true peak). The gain is smooth and is applied at the base rate. Thus the gain
// changes do not make new high-frequency content and the output true peak stays
// under the ceiling. The release is program dependent.
//
// Lookahead: 0 ms (LIVE, zero latency) or 1.5 ms / 5 ms (reported latency).
// In LIVE mode the sample peak is exact. The true peak can be a little higher
// than the ceiling on very sharp transients, because a zero-latency limiter
// cannot see a peak before it arrives.
#pragma once

#include "Halfband.h"
#include <atomic>

namespace pg
{
struct LimiterSettings
{
    double inGainDb = 0.0;   // 0..18
    double clip = 0.0;       // 0..1 soft clip amount
    double ceilingDb = -0.3; // -12..0
    double releaseMs = 60.0; // 5..500
    double adapt = 0.7;      // 0..1 program-dependent release amount
    double outDb = 0.0;      // -12..0
    int lookaheadIndex = 0;  // 0 = LIVE, 1 = 1.5 ms, 2 = 5 ms
    int osIndex = 0;         // 0 = 4x, 1 = 8x, 2 = 16x
};

// Data for the limiter display. One column for approx. 2.5 ms of audio.
struct ScopeColumn
{
    float inPeak = 0.0f, outPeak = 0.0f, gain = 1.0f;
};

class ScopeFifo
{
public:
    static constexpr int kSize = 2048;
    void push (const ScopeColumn& c) noexcept
    {
        const uint32_t w = writeCount.load (std::memory_order_relaxed);
        data[w & (kSize - 1)] = c;
        writeCount.store (w + 1, std::memory_order_release);
    }
    uint32_t count() const noexcept { return writeCount.load (std::memory_order_acquire); }
    ScopeColumn get (uint32_t index) const noexcept { return data[index & (kSize - 1)]; }

private:
    std::array<ScopeColumn, kSize> data {};
    std::atomic<uint32_t> writeCount { 0 };
};

class Limiter : public Module
{
public:
    static constexpr double kLookaheadMs[3] = { 0.0, 1.5, 5.0 };
    static constexpr int kStages[3] = { 2, 3, 4 };
    // Slack for the group delay of the detector filters (base-rate samples).
    static constexpr int kDetectSlack = 8;

    void setSettings (const LimiterSettings& s) noexcept { st = s; }

    // Meter values (linear peak since the last read) and gain reduction in dB.
    std::atomic<float> inPeak[2] {}, outPeak[2] {};
    std::atomic<float> grDb { 0.0f };
    ScopeFifo scope;

    static int lookaheadSamples (int index, double sampleRate) noexcept
    {
        return (int) std::lround (kLookaheadMs[clampv (index, 0, 2)] * 0.001 * sampleRate);
    }

    int latencySamples() const noexcept { return look; }

    void prepare (double sampleRate, int maxBlock) override
    {
        fs = sampleRate;
        for (auto& o : clipOs) o.prepare (maxBlock);
        for (auto& o : detOs) o.prepare (maxBlock);
        inGain.prepare (fs, 20.0);
        outGain.prepare (fs, 20.0);
        ceiling.prepare (fs, 20.0);
        clipAmt.prepare (fs, 20.0);
        inGain.reset (dbToGain (st.inGainDb));
        outGain.reset (dbToGain (st.outDb));
        ceiling.reset (dbToGain (st.ceilingDb));
        clipAmt.reset (st.clip);

        const int maxLook = lookaheadSamples (2, fs) + 4;
        for (auto& d : delay) d.assign ((size_t) maxLook + 1, 0.0);
        boxBuf.assign ((size_t) maxLook + 1, 1.0);
        dqIdx.assign ((size_t) maxLook + 2, 0);
        dqVal.assign ((size_t) maxLook + 2, 1.0);
        colLen = std::max (16, (int) std::lround (fs / 400.0));
        sustainA = std::exp (-1.0 / (0.3 * fs));

        stages = kStages[clampv (st.osIndex, 0, 2)];
        look = lookaheadSamples (st.lookaheadIndex, fs);
        reset();
    }

    void reset() override
    {
        for (auto& o : clipOs) o.reset();
        for (auto& o : detOs) o.reset();
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0);
        std::fill (boxBuf.begin(), boxBuf.end(), 1.0);
        boxLen = std::max (1, look - kDetectSlack);
        boxSum = (double) boxLen;
        boxPos = 0;
        dWrite = 0;
        dqHead = dqTail = 0;
        index = 0;
        env = 1.0;
        sustain = 0.0;
        colCount = 0;
        col = {};
    }

    void process (double* L, double* R, int n) noexcept override
    {
        // Discrete changes: oversampling factor and lookahead.
        const int newStages = kStages[clampv (st.osIndex, 0, 2)];
        const int newLook = lookaheadSamples (st.lookaheadIndex, fs);
        if (newStages != stages || newLook != look)
        {
            stages = newStages;
            look = newLook;
            reset();
        }

        inGain.set (dbToGain (st.inGainDb));
        outGain.set (dbToGain (st.outDb));
        ceiling.set (dbToGain (st.ceilingDb));
        clipAmt.set (clampv (st.clip, 0.0, 1.0));

        const int factor = 1 << stages;
        const int D = look;
        const double adapt = clampv (st.adapt, 0.0, 1.0);
        const double relMs = std::max (1.0, st.releaseMs);

        double osL[1 << Oversampler::kMaxStages], osR[1 << Oversampler::kMaxStages];
        float inPk[2] = { 0, 0 }, outPk[2] = { 0, 0 };
        double minGain = 1.0;

        for (int i = 0; i < n; ++i)
        {
            const double ig = inGain.next();
            const double og = outGain.next();
            const double ceil = ceiling.next();
            const double ca = clipAmt.next();
            const double l0 = L[i] * ig, r0 = R[i] * ig;
            inPk[0] = std::max (inPk[0], (float) std::abs (l0));
            inPk[1] = std::max (inPk[1], (float) std::abs (r0));

            // ---- Stage 1: soft clip at N x rate ----
            clipOs[0].upsample (l0, osL, stages);
            clipOs[1].upsample (r0, osR, stages);
            const double clipLevel = ceil * 1.122; // +1 dB above the ceiling
            for (int k = 0; k < factor; ++k)
            {
                osL[k] = softClip (osL[k], ca, clipLevel);
                osR[k] = softClip (osR[k], ca, clipLevel);
            }
            const double l1 = clipOs[0].downsample (osL, stages);
            const double r1 = clipOs[1].downsample (osR, stages);

            // ---- Stage 2: true-peak detector at N x rate ----
            detOs[0].upsample (l1, osL, stages);
            detOs[1].upsample (r1, osR, stages);
            double peak = std::max (std::abs (l1), std::abs (r1));
            for (int k = 0; k < factor; ++k)
                peak = std::max (peak, std::max (std::abs (osL[k]), std::abs (osR[k])));
            const double gt = peak > ceil ? ceil / peak : 1.0;

            // ---- Gain envelope (base rate) ----
            double l = l1, r = r1, m = gt;
            if (D > 0)
            {
                const int size = (int) delay[0].size();
                delay[0][(size_t) dWrite] = l1;
                delay[1][(size_t) dWrite] = r1;
                int rd = dWrite - D;
                if (rd < 0) rd += size;
                l = delay[0][(size_t) rd];
                r = delay[1][(size_t) rd];
                if (++dWrite >= size) dWrite = 0;
                m = slidingMin (gt, D + 1);
            }

            // Program-dependent release: a long gain reduction makes the release slower.
            const double grNow = -gainToDb (env);
            sustain = sustainA * sustain + (1.0 - sustainA) * grNow;
            const double relT = relMs * 0.001 * (1.0 + adapt * 3.0 * clampv (sustain / 6.0, 0.0, 1.5));
            const double relCoef = std::exp (-1.0 / (relT * fs));

            if (m < env)
                env = m;
            else
                env = m + (env - m) * relCoef;

            double g = env;
            if (D > 0)
            {
                boxSum += env - boxBuf[(size_t) boxPos];
                boxBuf[(size_t) boxPos] = env;
                if (++boxPos >= boxLen)
                {
                    boxPos = 0;
                    double s = 0.0; // remove numeric drift
                    for (int j = 0; j < boxLen; ++j) s += boxBuf[(size_t) j];
                    boxSum = s;
                }
                g = std::min (1.0, boxSum / (double) boxLen);
            }
            ++index;

            // Safety: the sample peak never goes above the ceiling.
            double lo = clampv (l * g, -ceil, ceil) * og;
            double ro = clampv (r * g, -ceil, ceil) * og;
            L[i] = lo;
            R[i] = ro;

            outPk[0] = std::max (outPk[0], (float) std::abs (lo));
            outPk[1] = std::max (outPk[1], (float) std::abs (ro));
            minGain = std::min (minGain, g);

            // Display data
            col.inPeak = std::max (col.inPeak, (float) std::max (std::abs (l0), std::abs (r0)));
            col.outPeak = std::max (col.outPeak, (float) std::max (std::abs (lo), std::abs (ro)));
            col.gain = std::min (col.gain, (float) g);
            if (++colCount >= colLen)
            {
                scope.push (col);
                col = {};
                colCount = 0;
            }
        }

        for (int c = 0; c < 2; ++c)
        {
            atomicMax (inPeak[c], inPk[c]);
            atomicMax (outPeak[c], outPk[c]);
        }
        grDb.store ((float) -gainToDb (minGain));
    }

    // Soft clip curve. amount 0 = no change. level = output limit.
    static double softClip (double x, double amount, double level) noexcept
    {
        if (amount <= 1.0e-6)
            return x;
        const double t = 1.0 - 0.85 * amount;
        const double u = std::abs (x) / level;
        if (u <= t)
            return x;
        const double y = t + (1.0 - t) * std::tanh ((u - t) / (1.0 - t));
        return std::copysign (y * level, x);
    }

private:
    static void atomicMax (std::atomic<float>& a, float v) noexcept
    {
        float cur = a.load (std::memory_order_relaxed);
        while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
    }

    // Minimum of the last 'window' values (monotonic queue).
    double slidingMin (double v, int window) noexcept
    {
        const int cap = (int) dqIdx.size();
        while (dqHead != dqTail)
        {
            int last = dqTail - 1;
            if (last < 0) last += cap;
            if (dqVal[(size_t) last] >= v)
                dqTail = last;
            else
                break;
        }
        dqIdx[(size_t) dqTail] = index;
        dqVal[(size_t) dqTail] = v;
        if (++dqTail >= cap) dqTail = 0;
        while (dqIdx[(size_t) dqHead] <= index - window)
            if (++dqHead >= cap) dqHead = 0;
        return dqVal[(size_t) dqHead];
    }

    double fs = 48000.0;
    LimiterSettings st;
    Smoother inGain, outGain, ceiling, clipAmt;
    std::array<Oversampler, 2> clipOs, detOs;
    int stages = 2, look = 0;

    std::array<std::vector<double>, 2> delay;
    int dWrite = 0;
    std::vector<double> boxBuf;
    double boxSum = 0.0;
    int boxPos = 0, boxLen = 1;
    std::vector<int64_t> dqIdx;
    std::vector<double> dqVal;
    int dqHead = 0, dqTail = 0;
    int64_t index = 0;

    double env = 1.0, sustain = 0.0, sustainA = 0.999;
    int colLen = 120, colCount = 0;
    ScopeColumn col;
};
} // namespace pg
