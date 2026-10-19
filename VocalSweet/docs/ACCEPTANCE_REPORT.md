# VocalSweet 1.0.0 — acceptance report

Date: 2026-10-19. Build host: Linux x64 cloud container (no Windows machine, no DAW available).

## What was built

| Artefact | Path (in deliverable) | Type | Verified how |
|---|---|---|---|
| **Windows VST3** | `binaries/windows-x64/VST3/VocalSweet.vst3/Contents/x86_64-win/VocalSweet.vst3` | PE32+ DLL, x86-64, MSVC ABI, static CRT; exports `GetPluginFactory` | pluginval 1.0.4 strictness 10 under Wine 9.0, GUI tests skipped: **SUCCESS** |
| Windows Standalone | `binaries/windows-x64/Standalone/VocalSweet.exe` | PE32+ GUI exe, x86-64 | Built only. Under Wine it shows no window (Direct2D), so it is **not verified**. |
| Linux VST3 | `binaries/linux-x64/VST3/VocalSweet.vst3` | ELF x86-64 shared object + moduleinfo.json | pluginval strictness 10 **including GUI tests**: **SUCCESS** |
| Linux Standalone | `binaries/linux-x64/Standalone/VocalSweet` | ELF x86-64 | Launched on Xvfb, operated with xdotool, screenshots taken |
| Source + CMake project | `Source/`, `CMakeLists.txt`, `cmake/` | C++17 / JUCE 8.0.4 | Builds natively (GCC 13) and cross (clang-cl 20) |

SHA-256 checksums: `binaries/SHA256SUMS.txt`.

## Test results (all executed in this session; raw logs in `test-results/`)

| Suite | Result | Log |
|---|---|---|
| DSP tests (framework independent) | **40 / 40 passed** | `dsp_tests_console.log`, `dsp_test_report.md` |
| Plugin tests (real JUCE processor + editor) | **16 / 16 passed** | `plugin_tests_console.log`, `plugin_test_report.md` |
| pluginval, Linux VST3, strictness 10 | **SUCCESS** (25 test groups, incl. editor) | `pluginval_linux_x64_vst3_strict10.log` |
| pluginval, **Windows VST3 under Wine**, strictness 10, `--skip-gui-tests` | **SUCCESS** (21 test groups) | `pluginval_windows_x64_vst3_under_wine_strict10_nogui.log` |
| pluginval, Windows VST3 under Wine, with GUI | Hung at "Editor" (Wine lacks Direct2D) | `pluginval_windows_x64_vst3_under_wine_GUI_attempt_hung.log` |

Key measurements (from the DSP log):

* Latency 1.000 ms (48 samples at 48 kHz), measured with an impulse and matching what is reported to the host.
* HPF/EQ: measured sine response matches the analytic curve shown in the UI (max deviation 0.000 dB at the tested points).
* Compressor: −6 dBFS sine, threshold −20 dB, 4:1 → 9.73 dB gain reduction (meter 9.74 dB). No GR below threshold.
* De-esser: sibilant band (7 kHz) −9.9 dB, 300 Hz tone unchanged (0.000 dB).
* Saturation: 0 % adds no harmonics (H2/H3 below −160 dB); the Warmth bias adds H2 with DC blocked; aliasing −62 dB at 7 kHz full drive.
* Limiter: +12 dBFS input → output peak −1.000 dBFS (ceiling −1); with LIMIT off the signal passes unclipped.
* Bypass: bit-exact (error 0) after the 20 ms crossfade; toggling is click-free.
* Dry/Wet: 0 % = input (error 0); 50 % = exact linear blend.
* Stability: 7 sample rates (22.05–192 kHz) × 9 block sizes (1–4096); min/max plus 150 random parameter sets; NaN/Inf/1e30/denormal injection. All output is finite and limiter-safe.
* SWEETEN 0→100: clarity +5.0 dB, air +4.7 dB, sibilance +0.1 dB (held by the de-esser), comp GR 3.0→5.5 dB.
* Presets: 8 named presets measurably distinct (closest pair 1.6 dB in at least one band or GR); output loudness within about 1–3 dB of input.
* CPU: about 2.5 % of one core (stereo 48 kHz, every module + doubler).

## Screenshots (all genuine; none are mock-ups)

| File | What it shows |
|---|---|
| `screenshots/01_standalone_main.png` | Running Linux standalone app (Xvfb), main page |
| `screenshots/02_standalone_preset_menu.png` | Preset dropdown open (factory presets) |
| `screenshots/03_standalone_afrofusion_sweeten.png` | Preset "Afrofusion Lead" picked and SWEETEN dragged to 81 % with the mouse |
| `screenshots/04_standalone_advanced.png` | Advanced processing page |
| `screenshots/05_standalone_options_menu.png` | Standalone wrapper options menu |
| `screenshots/06_build_and_validation_results.png` | Text rendered from the real build/test/pluginval logs |
| `screenshots/editor-render/*.png` | The real editor rendered off-screen by the test host while live audio (synthetic vocal) was processed, so meters, analyzer and GR history show real processor state |
| `screenshots/spectrogram_*.png` | Spectrograms of the dry test signal and processed outputs (ffmpeg) |

The screenshots come from the **Linux build** of the same source. No screenshot shows the plugin
inside FL Studio, Audacity or any DAW, and none shows the Windows GUI.

## Audio files

`audio-examples/input/` holds the test signals (synthetic vocal, log sweep, pink noise).
`audio-examples/processed/` holds each preset and SWEETEN at 0/50/100 (24-bit, 48 kHz).
`render_summary.csv` lists levels. **No listening evaluation was performed.**

## Not verified / remaining

* **FL Studio: not directly verified.** **Audacity: not directly verified.** Neither host was available.
* Windows GUI not verified (Wine cannot run JUCE 8's Direct2D renderer).
* No native Windows run, no Steinberg VST3 validator run, no code signing.

See `docs/KNOWN_LIMITATIONS.md`.
