# Post-Grape

Guitar bus post-effect plugin (VST3, Windows x64). Author: **desNotto**.

Put Post-Grape at the end of a stereo bus that has an amp simulator.
The bus contains two guitars: one guitar is 100% left, one guitar is 100% right.
The plugin is for extremely low tunings and high-gain guitars.

**Main rule: zero latency.** In the default mode the plugin reports 0 samples of latency to the DAW.

![Post-Grape](design/screenshot.png)

## Modules

The signal goes through the modules in this order:

```
IN → Bus EQ → Bus Compressor → Wider → Mono-Maker → True Stereo Pan → Lofi → Bus Limiter → OUT
```

Each module has a power button (grape button). The DAW can automate all parameters and all switches.

| # | Module | Function |
|---|--------|----------|
| 1 | **Bus EQ** | HPF 40–150 Hz (12/24 dB/oct), LPF 6–16 kHz, 4 bands (Bell, Notch, Low Shelf, High Shelf, Q up to 20). Each band operates on Stereo, Mid or Side. TPT SVF filters, 64-bit. Drag the nodes in the graph. Mouse wheel on a node changes Q. |
| 2 | **Bus Compressor** | Splits the signal at 80–350 Hz (Linkwitz-Riley 4th order). Compresses the low band only. Threshold, Ratio, Attack, Release, Sidechain HPF, Stereo Link, Makeup. |
| 3 | **Wider** | Mid/Side width 0–200 %, Side Air (high shelf on Side), Side Cut (HPF on Side). No Haas delay: mono compatibility stays good. Correlation meter. |
| 4 | **Mono-Maker** | Mono below 60–250 Hz. Low Width 0–100 %. The Mid signal gets the matching all-pass, thus there is no phase cancellation. |
| 5 | **True Stereo Pan** | Logic Pro style stereo pan. Both guitars move together. When you pan to the right, the left guitar moves to the right too. It does not become silent. |
| 6 | **Lofi** | LOFI ON/OFF switch for automation (smooth crossfade). Tone, Bits, Rate, Noise, Wobble, Drive, Mix. |
| 7 | **Bus Limiter** | Soft clip and true-peak brickwall limiter. Oversampling 4× / 8× / 16×. Program-dependent release. Display with input, output and gain reduction. |

### Limiter lookahead and latency

- **LIVE 0** (default): no lookahead, latency 0 samples. The sample peak never goes above the ceiling.
  On normal amp-sim material the true peak stays within 0.1 dB of the ceiling.
  On very sharp synthetic transients the true peak can be up to approx. 2 dB higher, because a zero-latency limiter cannot see a peak before it arrives.
- **1.5 ms** and **5 ms**: exact true-peak limiting. The plugin reports the latency to the DAW. Use these modes for mixing and export, not for live play.

The oversampling filters are minimum-phase IIR filters. Their group delay is approx. 6 samples (0.13 ms at 48 kHz). The plugin does not report this to the DAW.

## Presets

- Factory presets: Init, Drop A Wall, Djent Tight, Doom Fog, Wide Rhythm, Left Lean, Lofi Intro, Radio Break, Master Safe.
- User presets: `Documents\desNotto\Post-Grape\Presets\*.pgpreset`.
- Preset bar: previous / next, preset list, save, rename, delete (the file goes to the Recycle Bin), open the folder.

## Window

- Drag the bottom-right corner to change the size. Width and height are independent. The layout adapts.
- Scale button: 100 % / 125 %.

## Install

1. Download `Post-Grape-vX.Y.Z-win64.zip` from [Releases](https://github.com/desNotto/Post-Grape/releases).
2. Extract `Post-Grape.vst3` to `C:\Program Files\Common Files\VST3\`.
3. Scan the plugins in the DAW.

## Build

Requirements: Windows 10/11, Visual Studio 2022 (Build Tools are sufficient, C++ workload), CMake 3.22+, Ninja, Git.

```
git clone https://github.com/desNotto/Post-Grape.git
cd Post-Grape
git clone --depth 1 --branch 8.0.17-security https://github.com/juce-framework/JUCE.git external/JUCE
build.bat
```

If `external/JUCE` is not present, CMake downloads JUCE automatically.
The build copies the plugin to `bin\Post-Grape.vst3` and runs the DSP tests (`build\PostGrapeTests.exe`).

## Source layout

| Path | Contents |
|------|----------|
| `src/dsp/` | DSP code (C++20, 64-bit, no JUCE): TPT SVF, LR4 crossover, half-band oversampler, the 7 modules, the engine. |
| `src/gui/` | Look-and-feel, knobs, switches, meters, displays, module panels. |
| `src/Params.*` | Parameter IDs and ranges. |
| `src/PresetManager.*` | Factory and user presets. |
| `tests/dsp_tests.cpp` | DSP tests (latency, flat crossover sums, notch depth, true peak, identity). |
| `design/` | UI mockup and architecture notes. |
| `ui-list.png` | UI sheet: the source of the visual style. |

## License

Post-Grape: GNU Affero General Public License v3.0 (see `LICENSE`).
JUCE 8 is used under the AGPLv3. The Nunito font is used under the SIL Open Font License 1.1 (`assets/fonts/OFL.txt`).
