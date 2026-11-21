// Post-Grape — DSP tests. Run: PostGrapeTests.exe
// Each test prints PASS or FAIL. Exit code 0 = all tests pass.
#include "../src/dsp/Engine.h"
#include <cstdio>
#include <functional>
#include <string>

using namespace pg;

static int failures = 0;
static void check (bool ok, const std::string& name, double value = 0.0)
{
    std::printf ("%s  %-58s %.4f\n", ok ? "PASS" : "FAIL", name.c_str(), value);
    if (! ok) ++failures;
}

static constexpr double fs = 48000.0;

// Steady-state amplitude of a sine through a stereo processor (L channel).
static double sineGainDb (const std::function<void (double*, double*, int)>& proc, double freq,
                          double amp = 0.25, bool sideOnly = false, bool measureRight = false)
{
    const int n = (int) fs; // 1 s
    std::vector<double> L ((size_t) n), R ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double s = amp * std::sin (2.0 * kPi * freq * i / fs);
        L[(size_t) i] = s;
        R[(size_t) i] = sideOnly ? -s : s;
    }
    proc (L.data(), R.data(), n);
    // RMS over the second half (no error from the sample grid).
    double sum = 0.0;
    const auto& X = measureRight ? R : L;
    for (int i = n / 2; i < n; ++i) sum += X[(size_t) i] * X[(size_t) i];
    return gainToDb (std::sqrt (sum / (n - n / 2)) / (amp / std::sqrt (2.0)));
}

static int impulsePeakIndex (const std::function<void (double*, double*, int)>& proc)
{
    const int n = 4096;
    std::vector<double> L ((size_t) n, 0.0), R ((size_t) n, 0.0);
    L[0] = R[0] = 0.5;
    proc (L.data(), R.data(), n);
    int idx = 0;
    for (int i = 0; i < n; ++i)
        if (std::abs (L[(size_t) i]) > std::abs (L[(size_t) idx])) idx = i;
    return idx;
}

int main()
{
    // ---- Oversampler ----
    for (int stages = 2; stages <= 4; ++stages)
    {
        Oversampler os;
        os.prepare (512);
        auto proc = [&] (double* L, double*, int n) {
            double buf[16];
            for (int i = 0; i < n; ++i)
            {
                os.upsample (L[i], buf, stages);
                L[i] = os.downsample (buf, stages);
            }
        };
        const double g1k = sineGainDb (proc, 1000.0);
        os.reset();
        const double g15k = sineGainDb (proc, 15000.0);
        os.reset();
        check (std::abs (g1k) < 0.01, "Oversampler " + std::to_string (1 << stages) + "x: 1 kHz gain (dB)", g1k);
        check (std::abs (g15k) < 0.1, "Oversampler " + std::to_string (1 << stages) + "x: 15 kHz gain (dB)", g15k);
        os.reset();
        const int pk = impulsePeakIndex (proc);
        check (pk <= 8, "Oversampler " + std::to_string (1 << stages) + "x: impulse peak index (samples)", pk);
    }

    // Image rejection of the first stage: upsample a 20 kHz tone, measure the 76 kHz image (at 96 kHz the image is 28 kHz).
    {
        Halfband2x hb;
        hb.setCoefs (halfband::design (10, 0.04));
        const int n = 48000;
        std::vector<double> up ((size_t) n * 2);
        for (int i = 0; i < n; ++i)
            hb.upsample (std::sin (2.0 * kPi * 18000.0 * i / fs), up[(size_t) 2 * i], up[(size_t) 2 * i + 1]);
        // Goertzel at 30 kHz image (96k - 18k... image of 18 kHz at 2x rate = 48k - 18k = 30k)
        auto goertzel = [&] (double f) {
            const double w = 2.0 * kPi * f / (fs * 2.0);
            double s1 = 0, s2 = 0;
            for (int i = n; i < 2 * n; ++i) { const double s = up[(size_t) i] + 2.0 * std::cos (w) * s1 - s2; s2 = s1; s1 = s; }
            return std::sqrt (s1 * s1 + s2 * s2 - 2.0 * std::cos (w) * s1 * s2) / (n / 2.0);
        };
        const double sig = goertzel (18000.0), img = goertzel (30000.0);
        check (gainToDb (img / sig) < -70.0, "Half-band stage 1: image rejection 18k->30k (dB)", gainToDb (img / sig));
    }

    // ---- SVF display response = DSP response ----
    {
        SvfCoefs c;
        c.design (SvfType::Bell, fs, 1000.0, 2.0, 6.0);
        SvfState s;
        auto proc = [&] (double* L, double*, int n) { for (int i = 0; i < n; ++i) L[i] = s.process (c, L[i]); };
        const double meas = sineGainDb (proc, 1000.0);
        const double disp = gainToDb (std::abs (c.response (1000.0, fs)));
        check (std::abs (meas - 6.0) < 0.05 && std::abs (disp - 6.0) < 0.01, "SVF bell +6 dB @1k: measured (dB)", meas);
    }

    // ---- Bus EQ ----
    {
        BusEQ eq;
        EqSettings s;
        s.hpFreq = 40.0; s.lpFreq = 16000.0;
        eq.setSettings (s);
        eq.prepare (fs, 512);
        auto proc = [&] (double* L, double* R, int n) { eq.process (L, R, n); };
        const double g = sineGainDb (proc, 1000.0);
        check (std::abs (g) < 0.02, "EQ flat: 1 kHz gain (dB)", g);

        s.bands[2].type = EqBandType::Notch; s.bands[2].freq = 2650.0; s.bands[2].q = 20.0;
        eq.setSettings (s); eq.prepare (fs, 512);
        const double gn = sineGainDb (proc, 2650.0);
        check (gn < -40.0, "EQ notch Q=20 @2650: depth (dB)", gn);
        eq.prepare (fs, 512);
        const double gnn = sineGainDb (proc, 2000.0);
        check (std::abs (gnn) < 0.3, "EQ notch Q=20: 2000 Hz unchanged (dB)", gnn);

        // Side-only band: mono signal must not change.
        EqSettings s2;
        s2.bands[0].gainDb = 12.0; s2.bands[0].freq = 1000.0; s2.bands[0].target = EqTarget::Side;
        eq.setSettings (s2); eq.prepare (fs, 512);
        const double gm = sineGainDb (proc, 1000.0);
        check (std::abs (gm) < 0.02, "EQ side band +12 dB: mono signal unchanged (dB)", gm);
        eq.prepare (fs, 512);
        const double gs = sineGainDb (proc, 1000.0, 0.25, true);
        check (std::abs (gs - 12.0) < 0.1, "EQ side band +12 dB: side signal (dB)", gs);

        // Fast automation: no NaN / no explosion.
        EqSettings s3;
        eq.setSettings (s3); eq.prepare (fs, 512);
        std::vector<double> L (64), R (64);
        double mx = 0.0;
        Random rnd;
        for (int blk = 0; blk < 4000; ++blk)
        {
            s3.bands[1].freq = 20.0 * std::pow (1000.0, rnd.nextUnit());
            s3.bands[1].gainDb = 18.0 * rnd.nextBipolar();
            s3.bands[1].q = 0.1 * std::pow (200.0, rnd.nextUnit());
            s3.hpFreq = 40.0 + 110.0 * rnd.nextUnit();
            eq.setSettings (s3);
            for (int i = 0; i < 64; ++i) L[(size_t) i] = R[(size_t) i] = 0.5 * rnd.nextBipolar();
            eq.process (L.data(), R.data(), 64);
            for (double v : L) mx = std::max (mx, std::abs (v));
        }
        check (std::isfinite (mx) && mx < 20.0, "EQ random fast automation: max output", mx);
    }

    // ---- Bus Compressor: no compression -> flat magnitude ----
    {
        BusComp c;
        CompSettings s; s.thresholdDb = 0.0; s.ratio = 1.0;
        c.setSettings (s);
        double worst = 0.0;
        for (double f : { 40.0, 100.0, 160.0, 300.0, 1000.0, 8000.0 })
        {
            c.prepare (fs, 512);
            const double g = sineGainDb ([&] (double* L, double* R, int n) { c.process (L, R, n); }, f);
            worst = std::max (worst, std::abs (g));
        }
        check (worst < 0.02, "Comp ratio 1: crossover sum is flat (max dB error)", worst);

        // Compression of the low band only.
        CompSettings s2; s2.thresholdDb = -30.0; s2.ratio = 10.0; s2.splitHz = 160.0;
        c.setSettings (s2);
        c.prepare (fs, 512);
        const double gl = sineGainDb ([&] (double* L, double* R, int n) { c.process (L, R, n); }, 60.0, 0.5);
        c.prepare (fs, 512);
        const double gh = sineGainDb ([&] (double* L, double* R, int n) { c.process (L, R, n); }, 2000.0, 0.5);
        check (gl < -15.0, "Comp: 60 Hz is compressed (dB)", gl);
        check (std::abs (gh) < 0.1, "Comp: 2 kHz is not compressed (dB)", gh);
    }

    // ---- Mono-Maker ----
    {
        MonoMaker m;
        MonoSettings s; s.freqHz = 120.0; s.width = 0.0;
        m.setSettings (s);
        m.prepare (fs, 512);
        const double side50 = sineGainDb ([&] (double* L, double* R, int n) { m.process (L, R, n); }, 50.0, 0.25, true);
        check (side50 < -25.0, "Mono-Maker 0%: side @50 Hz removed (dB)", side50);
        m.prepare (fs, 512);
        const double mid50 = sineGainDb ([&] (double* L, double* R, int n) { m.process (L, R, n); }, 50.0);
        check (std::abs (mid50) < 0.02, "Mono-Maker 0%: mid @50 Hz unchanged (dB)", mid50);
        m.prepare (fs, 512);
        const double side2k = sineGainDb ([&] (double* L, double* R, int n) { m.process (L, R, n); }, 2000.0, 0.25, true);
        check (std::abs (side2k) < 0.05, "Mono-Maker 0%: side @2 kHz unchanged (dB)", side2k);
    }

    // ---- True Stereo Pan ----
    {
        TruePan p;
        PanSettings s;
        p.setSettings (s);
        p.prepare (fs, 512);
        std::vector<double> L (256), R (256);
        Random rnd;
        for (int i = 0; i < 256; ++i) { L[(size_t) i] = rnd.nextBipolar(); R[(size_t) i] = rnd.nextBipolar(); }
        auto L0 = L, R0 = R;
        p.process (L.data(), R.data(), 256);
        double err = 0.0;
        for (int i = 0; i < 256; ++i) err = std::max ({ err, std::abs (L[(size_t) i] - L0[(size_t) i]), std::abs (R[(size_t) i] - R0[(size_t) i]) });
        check (err < 1e-12, "True Pan default: exact identity (max error)", err);

        // Pan hard right: left guitar is not lost (it moves to the right output).
        s.pan = 1.0; s.spread = 1.0;
        p.setSettings (s); p.prepare (fs, 512);
        std::vector<double> l2 (1024, 0.0), r2 (1024, 0.0);
        for (int i = 0; i < 1024; ++i) l2[(size_t) i] = std::sin (2 * kPi * 440 * i / fs) * 0.5; // left guitar only
        p.process (l2.data(), r2.data(), 1024);
        double pk = 0.0;
        for (int i = 512; i < 1024; ++i) pk = std::max (pk, std::abs (r2[(size_t) i]));
        check (gainToDb (pk / 0.5) > -3.1, "True Pan R100: left guitar in right output (dB)", gainToDb (pk / 0.5));
    }

    // ---- Limiter ----
    for (int combo = 0; combo < 5; ++combo)
    {
        const int osI = combo < 3 ? combo : 0;
        const int la = combo < 3 ? 0 : combo - 2;
        Limiter lim;
        LimiterSettings s; s.ceilingDb = -1.0; s.inGainDb = 12.0; s.clip = 0.3; s.osIndex = osI; s.lookaheadIndex = la;
        lim.setSettings (s);
        lim.prepare (fs, 512);
        // Loud chug signal: 55 Hz saw bursts + noise.
        const int n = 96000;
        std::vector<double> L ((size_t) n), R ((size_t) n);
        Random rnd;
        for (int i = 0; i < n; ++i)
        {
            const double t = i / fs;
            const double saw = 2.0 * (t * 55.0 - std::floor (t * 55.0 + 0.5));
            const double env = std::fmod (t, 0.25) < 0.15 ? 1.0 : 0.2;
            L[(size_t) i] = 0.4 * env * (0.7 * saw + 0.3 * rnd.nextBipolar());
            R[(size_t) i] = 0.4 * env * (0.7 * saw + 0.3 * rnd.nextBipolar());
        }
        lim.process (L.data(), R.data(), n);
        // True peak of the output: measure with 4x oversampling (fresh oversampler).
        Oversampler meter; meter.prepare (512);
        double tp = 0.0, buf[16];
        for (int i = 0; i < n; ++i)
        {
            meter.upsample (L[(size_t) i], buf, 2);
            for (int k = 0; k < 4; ++k) tp = std::max (tp, std::abs (buf[k]));
        }
        const double tpDb = gainToDb (tp);
        const std::string nm = "Limiter " + std::to_string (4 << osI) + "x " + (la == 0 ? "LIVE" : (la == 1 ? "1.5ms" : "5ms"));
        if (la == 0)
            check (tpDb <= -1.0 + 2.5, nm + " (harsh synthetic): true peak <= ceiling+2.5 (dBTP)", tpDb);
        else
            check (tpDb <= -1.0 + 0.1, nm + ": true peak <= ceiling+0.1 (dBTP)", tpDb);
        double sp = 0.0;
        for (double v : L) sp = std::max (sp, std::abs (v));
        check (gainToDb (sp) <= -1.0 + 1e-9, nm + ": sample peak <= ceiling (dBFS)", gainToDb (sp));
    }
    {
        Limiter lim;
        LimiterSettings s; s.ceilingDb = -0.3; s.inGainDb = 0.0; s.clip = 0.0;
        lim.setSettings (s);
        lim.prepare (fs, 512);
        const int pk = impulsePeakIndex ([&] (double* L, double* R, int n) { lim.process (L, R, n); });
        check (pk <= 10, "Limiter LIVE: impulse peak index (samples)", pk);
        s.lookaheadIndex = 1;
        lim.setSettings (s);
        lim.prepare (fs, 512);
        check (lim.latencySamples() == 72, "Limiter 1.5 ms: reported latency (samples)", lim.latencySamples());

        // Lookahead: a step of +12 dB must stay under the ceiling (sample peak).
        s.lookaheadIndex = 2;
        lim.setSettings (s);
        lim.prepare (fs, 512);
        std::vector<double> L (48000, 0.0), R (48000, 0.0);
        for (int i = 0; i < 48000; ++i) L[(size_t) i] = R[(size_t) i] = (i > 10000 ? 3.0 : 0.1) * std::sin (2 * kPi * 100 * i / fs);
        lim.process (L.data(), R.data(), 48000);
        double pkv = 0.0;
        for (double v : L) pkv = std::max (pkv, std::abs (v));
        check (gainToDb (pkv) <= -0.3 + 1e-9, "Limiter 5 ms lookahead: sample peak (dBFS)", gainToDb (pkv));
    }

    // Realistic amp-sim signal (cabinet band: 80 Hz .. 6 kHz). LIVE mode true peak.
    for (int osI = 0; osI < 3; ++osI)
    {
        Limiter lim;
        LimiterSettings s; s.ceilingDb = -1.0; s.inGainDb = 9.0; s.clip = 0.35; s.osIndex = osI;
        lim.setSettings (s);
        lim.prepare (fs, 512);
        const int n = 96000;
        std::vector<double> L ((size_t) n), R ((size_t) n);
        SvfCoefs lp, hp; lp.design (SvfType::LowPass, fs, 6000.0, 0.7071); hp.design (SvfType::HighPass, fs, 80.0, 0.7071);
        SvfState a[2], b[2], c2[2];
        Random rnd;
        for (int i = 0; i < n; ++i)
        {
            const double t = i / fs;
            const double env = std::fmod (t, 0.125) < 0.09 ? 1.0 : 0.15;
            for (int c = 0; c < 2; ++c)
            {
                const double ph = t * 55.0 * (c ? 1.003 : 1.0);
                double x = env * (0.6 * std::tanh (8.0 * std::sin (2 * kPi * ph)) + 0.25 * rnd.nextBipolar());
                x = c2[c].process (lp, b[c].process (lp, a[c].process (hp, x)));
                (c ? R : L)[(size_t) i] = 0.5 * x;
            }
        }
        lim.process (L.data(), R.data(), n);
        Oversampler meter; meter.prepare (512);
        double tp = 0.0, buf[16];
        for (int i = 0; i < n; ++i)
        {
            meter.upsample (L[(size_t) i], buf, 3);
            for (int k = 0; k < 8; ++k) tp = std::max (tp, std::abs (buf[k]));
        }
        check (gainToDb (tp) <= -1.0 + 0.5, "Limiter " + std::to_string (4 << osI) + "x LIVE (amp-sim signal): true peak <= ceiling+0.5", gainToDb (tp));
    }

    // ---- Full engine ----
    {
        Engine e;
        EngineSettings s;
        s.lofi.active = false;
        s.lim.clip = 0.0;
        s.comp.thresholdDb = 0.0;
        e.setSettings (s);
        e.setSampleRate (fs);
        e.prepare (fs, 512);
        const int pk = impulsePeakIndex ([&] (double* L, double* R, int n) { e.process (L, R, n); });
        check (pk <= 10, "Engine default: impulse peak index (samples)", pk);
        check (e.latencySamples() == 0, "Engine default: reported latency (samples)", e.latencySamples());

        // All modules bypassed -> exact identity.
        for (auto& o : s.on) o = false;
        e.setSettings (s);
        e.prepare (fs, 512);
        std::vector<double> L (1000), R (1000);
        Random rnd;
        for (int i = 0; i < 1000; ++i) { L[(size_t) i] = rnd.nextBipolar(); R[(size_t) i] = rnd.nextBipolar(); }
        auto L0 = L;
        e.process (L.data(), R.data(), 1000);
        double err = 0.0;
        for (int i = 0; i < 1000; ++i) err = std::max (err, std::abs (L[(size_t) i] - L0[(size_t) i]));
        check (err == 0.0, "Engine all bypassed: exact identity (max error)", err);

        // Lofi on: no NaN.
        EngineSettings s2;
        s2.lofi.active = true; s2.lofi.wobble = 1.0; s2.lofi.noise = 1.0;
        e.setSettings (s2);
        e.prepare (fs, 512);
        std::vector<double> A (48000), B (48000);
        for (int i = 0; i < 48000; ++i) A[(size_t) i] = B[(size_t) i] = 0.5 * std::sin (2 * kPi * 220 * i / fs);
        e.process (A.data(), B.data(), 48000);
        bool finite = true;
        for (double v : A) finite = finite && std::isfinite (v);
        check (finite, "Engine with Lofi (wobble, noise): finite output", 1.0);
    }

    // ---- Auto-Gain ----
    {
        // Guitar-like test signal: band-limited noise chugs, 10 s.
        const int n = (int) (fs * 10.0);
        auto makeSignal = [&] (std::vector<double>& L, std::vector<double>& R, double amp) {
            L.assign ((size_t) n, 0.0); R.assign ((size_t) n, 0.0);
            SvfCoefs lp, hp; lp.design (SvfType::LowPass, fs, 5000.0, 0.7071); hp.design (SvfType::HighPass, fs, 70.0, 0.7071);
            SvfState a[2], b[2]; Random rnd (1234);
            for (int i = 0; i < n; ++i)
            {
                const double t = i / fs;
                const double env = std::fmod (t, 0.25) < 0.17 ? 1.0 : 0.25;
                for (int c = 0; c < 2; ++c)
                {
                    double x = env * (0.6 * std::tanh (6.0 * std::sin (2 * kPi * 55.0 * t * (c ? 1.004 : 1.0))) + 0.3 * rnd.nextBipolar());
                    (c ? R : L)[(size_t) i] = amp * b[c].process (lp, a[c].process (hp, x));
                }
            }
        };
        // K-weighted loudness of the last 3 s.
        auto loudness = [&] (const std::vector<double>& L, const std::vector<double>& R) {
            LoudnessDetector d; d.prepare (fs);
            double sum = 0.0; int cnt = 0;
            for (int i = 0; i < n; ++i)
            {
                const double p = d.power (L[(size_t) i], R[(size_t) i], true);
                if (i >= n - (int) (fs * 3.0)) { sum += p; ++cnt; }
            }
            return LoudnessDetector::levelDb (sum / cnt, true);
        };
        auto runEngine = [&] (EngineSettings s, std::vector<double>& L, std::vector<double>& R) {
            Engine e; e.setSettings (s); e.setSampleRate (fs); e.prepare (fs, 512);
            for (int pos = 0; pos < n; pos += 512)
            {
                const int len = std::min (512, n - pos);
                e.process (L.data() + pos, R.data() + pos, len);
            }
        };

        std::vector<double> L, R;
        makeSignal (L, R, 0.25);
        const double inLufs = loudness (L, R);

        // 1. Limiter +12 dB, Auto-Gain on: output loudness = input loudness.
        {
            EngineSettings s;
            for (auto& o : s.on) o = false;
            s.on[kLim] = true; s.on[kAg] = true;
            s.lim.inGainDb = 12.0; s.lim.ceilingDb = -0.3;
            auto l = L, r = R;
            runEngine (s, l, r);
            const double diff = loudness (l, r) - inLufs;
            check (std::abs (diff) < 1.0, "Auto-Gain: limiter +12 dB, loudness difference (dB)", diff);
            double pk = 0.0;
            for (double v : l) pk = std::max (pk, std::abs (v));
            check (gainToDb (pk) <= -0.3 + 1e-9, "Auto-Gain: sample peak <= ceiling (dBFS)", gainToDb (pk));
        }
        // 2. Auto-Gain alone: almost no change.
        {
            EngineSettings s;
            for (auto& o : s.on) o = false;
            s.on[kAg] = true;
            auto l = L, r = R;
            runEngine (s, l, r);
            const double diff = loudness (l, r) - inLufs;
            check (std::abs (diff) < 0.1, "Auto-Gain alone: loudness difference (dB)", diff);
        }
        // 3. EQ output -9 dB, TRIM +2 dB: output = input + 2 dB (boost with peak safety).
        {
            EngineSettings s;
            for (auto& o : s.on) o = false;
            s.on[kEq] = true; s.on[kAg] = true;
            s.eq.outDb = -9.0; s.ag.trimDb = 2.0;
            auto l = L, r = R;
            runEngine (s, l, r);
            const double diff = loudness (l, r) - inLufs;
            check (std::abs (diff - 2.0) < 0.5, "Auto-Gain: EQ -9 dB, TRIM +2 dB: difference (dB)", diff);
        }
        // 4. Silence: the gain does not change.
        {
            AutoGain ag;
            AutoGainSettings s;
            ag.setSettings (s);
            ag.prepare (fs, 512);
            std::vector<double> l (L.begin(), L.begin() + (int) fs * 5), r (R.begin(), R.begin() + (int) fs * 5);
            std::vector<double> inl = l, inr = r;
            for (auto& v : l) v *= 0.5; // processed signal is 6 dB quieter
            for (auto& v : r) v *= 0.5;
            for (int pos = 0; pos + 512 <= (int) l.size(); pos += 512)
            {
                ag.setInput (inl.data() + pos, inr.data() + pos);
                ag.process (l.data() + pos, r.data() + pos, 512);
            }
            const double before = ag.matchedGainDb();
            std::vector<double> z (512, 0.0), z2 (512, 0.0), zi (512, 0.0);
            for (int k = 0; k < (int) (fs * 3.0 / 512); ++k)
            {
                ag.setInput (zi.data(), zi.data());
                ag.process (z.data(), z2.data(), 512);
            }
            check (std::abs (ag.matchedGainDb() - before) < 0.01, "Auto-Gain: gain holds in silence (dB change)", ag.matchedGainDb() - before);
            check (std::abs (before - 6.02) < 0.5, "Auto-Gain: -6 dB processed signal, matched gain (dB)", before);
        }
    }

    std::printf ("\n%s: %d failure(s)\n", failures == 0 ? "ALL PASS" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
