# Post-Grape — Architecture (draft v1)

Author: desNotto. Text style: ASD-STE100 (Simplified Technical English).

## 1. Purpose

Post-Grape is a post-effect plugin for electric guitar.
Put the plugin at the end of a stereo bus that has an amp simulator.
The bus contains two guitars. One guitar is 100% left. One guitar is 100% right.
The plugin is for extremely low tunings and high-gain guitars.

## 2. Main rule: zero latency

All modules must operate with zero latency for real-time guitar play.
The plugin reports 0 samples of latency to the DAW in the default mode.

Do not use:
- linear-phase filters,
- FFT processing,
- delay lines that add latency to the dry signal.

Use:
- minimum-phase IIR filters (TPT SVF),
- polyphase IIR half-band filters for oversampling.

Note: A lookahead limiter adds latency. Thus the limiter has a lookahead switch:
- LIVE (default): lookahead is 0 ms. Latency is 0 samples.
- 1.5 ms and 5 ms: for mixing only. The plugin reports the latency to the DAW.

## 3. Format and tools

- Format: VST3, Windows x64.
- Framework: JUCE 8 (CMake, FetchContent). License: AGPLv3 (the repository is public).
- Compiler: MSVC 2022 (Build Tools 17.14), C++20.
- Internal DSP precision: 64-bit (double) in all filters.
- Output folder for built VST3: `<project>/bin/` (not in Git; see the GitHub releases).
- Source code and releases: https://github.com/desNotto/Post-Grape

## 4. Signal chain

```
IN → Bus EQ → Bus Compressor → Wider → Mono-Maker → True Stereo Pan → Lofi → Bus Limiter → Auto-Gain → OUT
```

The Wider is before the Mono-Maker. Thus the Mono-Maker removes low-frequency side content that the Wider makes.
The Lofi is before the Limiter. Thus the Limiter controls the peaks from the Lofi drive.

Each module has a power (bypass) switch. The DAW can automate all parameters and all switches.
A bypass change uses a short crossfade (10 ms) to prevent clicks.

## 5. Modules

### 5.1 Bus EQ
- High-pass filter: 40–150 Hz, 12 or 24 dB/oct, minimum phase.
- Low-pass filter: 6–16 kHz, 12 dB/oct, minimum phase.
- 4 parametric bands. Types: Bell, Notch, Low Shelf, High Shelf. Q: 0.1–20. Gain: ±18 dB.
- Each band has a target: Stereo, Mid, or Side (M/S processing).
- Structure: TPT SVF (Zavalishin / Simper), 64-bit state.
- Parameter smoothing: per-sample coefficient smoothing. The TPT structure stays stable during fast automation.

### 5.2 Bus Compressor
- Split: 80–350 Hz. Crossover: Linkwitz-Riley 4th order (2 × TPT SVF). The sum of the bands is flat in magnitude.
- The compressor operates on the low band only. The high band passes without change.
- Controls: Threshold, Ratio, Attack, Release, Sidechain HPF (20–300 Hz), Stereo Link (0–100%), Makeup.
- Detector: feed-forward, peak/RMS blend, soft knee.

### 5.3 Wider
- Mid/Side width: 0–200%.
- Side Air: high-shelf boost on the Side signal.
- Side Cut: high-pass filter on the Side signal (prevents low-frequency side energy).
- No Haas delay. Thus mono compatibility stays good.
- Correlation meter.

### 5.4 Mono-Maker
- Crossover: 60–250 Hz. Linkwitz-Riley 4th order. Zero latency.
- Low Width: 0–100%. 0% = full mono below the crossover.
- Only the Side signal of the low band changes. Thus there is no phase cancellation of the Mid signal.

### 5.5 True Stereo Pan
- Logic Pro "Stereo Pan" behavior.
- The left input and the right input have separate pan positions:
  - left position = Pan − Spread, right position = Pan + Spread (limited to −100…+100).
- Each input uses a constant-power pan law. The outputs are added.
- Result: when you pan to the right, the left guitar moves to the center/right. It does not become silent.

### 5.6 Lofi
- Main switch LOFI ON/OFF (automatable, with a smooth crossfade of 25 ms).
- Order: Wobble -> Drive -> Tone (band-pass) -> Rate (sample and hold) -> Bits -> Noise -> Mix.
- Wobble uses a modulated delay. The minimum delay is 0 samples, the maximum is 4 ms x Wobble.
  When Wobble is 0 %, the module adds no delay. Wobble is an effect, not latency.

### 5.7 Bus Limiter
- Stage 1: soft clip. Up N x -> clip curve -> down N x. The clip limit is 1 dB above the ceiling.
- Stage 2: true-peak brickwall limiter. The detector upsamples the signal N x and finds the inter-sample peaks.
  The gain is smooth and is applied at the base rate. Thus the gain changes do not make new
  high-frequency content, and the output true peak stays under the ceiling.
- Oversampling: 4x (default), 8x, 16x. Polyphase IIR half-band filters (minimum phase). First stage: 10 coefficients,
  transition 0.04. Next stages: 6 and 4 coefficients. Group delay approx. 6 samples.
- Lookahead: LIVE 0 ms (default), 1.5 ms, 5 ms. With lookahead: sliding minimum over D+1 samples,
  then a moving average over D-8 samples (8 samples of slack for the detector filters).
- Release: program dependent. A slow average of the gain reduction (300 ms) makes the release
  up to 5.5 times slower (Adapt control). Thus there is no low-frequency distortion on long notes.
- A final safety clamp keeps the sample peak at or under the ceiling.
- Display: input envelope, output envelope, gain reduction curve, ceiling lines.
- Meters: input L/R, gain reduction, output L/R.

### 5.8 Test results (tests/dsp_tests.cpp)
- Crossover sums (compressor, mono-maker): flat within 0.001 dB.
- EQ notch Q=20: depth more than 200 dB at the centre frequency.
- True Pan default and "all modules off": exact identity.
- Limiter, lookahead 1.5 / 5 ms: true peak under the ceiling.
- Limiter LIVE, amp-sim signal: true peak within 0.06 dB of the ceiling.
- Engine default: reported latency 0 samples.

### 5.9 Auto-Gain (v1.1.0)
- Last module. Default: off.
- Two loudness detectors: plugin input (copy of the block before the chain) and chain output.
- LUFS mode: K-weighting (high shelf +4 dB at 1682 Hz, high-pass 38 Hz, TPT SVF), sum of the channel powers.
  RMS mode: mean of the channel powers, no weighting.
- Detector integration time: SPEED / 6 (0.1 .. 1.5 s). Gain smoothing: SPEED / 3 (SPEED = time to follow approx. 95 %).
- Fast start: in the first second after a reset, the gain follows with a 0.15 s time constant.
- Gate: when the input is below -60 dB, the matched gain does not change. HOLD freezes the matched gain.
- Gain = clamp(matched, -RANGE, +RANGE) + TRIM. A positive gain is limited by a peak envelope
  (instant attack, 0.5 s release) so that the peaks stay under the limiter ceiling. Final clamp only for positive gain.
- Tests: limiter +12 dB -> loudness difference 0.01 dB; EQ -9 dB + TRIM +2 dB -> +2.00 dB; gain holds in silence.

## 5a. User interface additions (v1.1.0)
- Hint bar at the bottom: mouse over a module -> module hint; mouse over a control -> control hint (src/gui/Hints.cpp).
- Hint language RU / EN (flags in the header), default RU, global setting:
  %APPDATA%\desNotto\Post-Grape\Post-Grape.settings.
- Update check (src/UpdateChecker.cpp): GET https://api.github.com/repos/desNotto/Post-Grape/releases/latest,
  compare tag_name with the plugin version, download the .zip asset to the Downloads folder.
  Automatic check: one time each day when the editor opens (can be turned off).

## 6. Presets

- Factory presets (in the binary) and user presets (XML files).
- User preset folder: `Documents/desNotto/Post-Grape/Presets/`.
- Functions: load, save, save as, rename, delete, previous/next.

## 7. Window

- The window size is free in each direction (width and height are independent). The layout adapts.
- Minimum size: 980 × 700 at 100%.
- Scale button: 100% / 125%.
- All controls are vector graphics. Decorations (grapes, leaves, vines) come from `ui-list.png`.
