# SwitaxVoice 1.0.1 test report (2026-10-23; 1.0.0 was 2026-10-19)

## What was built
| Target | Toolchain | Result |
|---|---|---|
| Linux x64 VST3 + Standalone + both test executables | GCC 13, JUCE 8.0.4, Release | Compiled, all tests pass |
| **Windows x64 VST3** (`SwitaxVoice.vst3`, PE32+ DLL, 7.7 MB) | clang-cl 20 + lld-link + MSVC CRT/SDK (xwin), static CRT | Compiled |
| Windows x64 Standalone (`SwitaxVoice.exe`) | same | Compiled. Not run (it needs a Windows desktop) |
| VocalSweet 1.0 (regression check) | unchanged sources (`git diff` = 0 lines) | Rebuilt, DSP and plugin tests 2/2 pass |

## Results
| Suite | Result | Log |
|---|---|---|
| Core tests (`SwitaxCoreTests`) | **50 / 50 passed** (399 s) | `test-results/core_test_report.md`, `core_tests_console.log` |
| Plugin tests (`SwitaxPluginTests`) | **33 / 33 passed** | `test-results/plugin_test_report.md`, `plugin_tests_console.log` |
| pluginval 1.0.4, Linux VST3, strictness 10, **with GUI tests** (Xvfb) | **SUCCESS**, 25 test groups | `pluginval_linux_x64_vst3_strict10_with_gui.log` |
| pluginval 1.0.4 (Windows build), **Windows DLL under Wine**, strictness 10, `--skip-gui-tests` | **SUCCESS**, 21 test groups | `pluginval_windows_x64_vst3_under_wine_strict10_nogui.log` |
| Real recordings (VocalSet, CC BY 4.0) | 5 / 5 checks pass, with a full report | `real_recordings_report.md` |

## Numerical accuracy (known signals)
| Check | Expected | Measured |
|---|---|---|
| EBU 3341 case 1 (1 kHz, -23 dBFS stereo) | -23.0 ±0.1 LUFS (M, S, I) | -22.993 |
| EBU 3341 case 2 | -33.0 ±0.1 | -32.993 |
| EBU 3341 cases 3 and 4 (gating) | -23.0 ±0.1 | -23.014 / -23.014 |
| EBU 3342 LRA (-20/-30, -20/-15, -40/-20) | 10 / 5 / 20 ±1 LU | 10.00 / 5.00 / 20.00 |
| 22.05-192 kHz sample rates | -23.0 ±0.1 | -22.96 … -23.02 |
| True peak, fs/4 sine at 45° | 0 dBTP (sample peak -3.01) | -0.05 dBTP |
| YIN pitch, 98 / 196 / 440 / 660 Hz | ±1 % | 98.0 / 196.0 / 440.2 / 660.4 |
| Live meters through the plugin (FIFO + worker thread) | -23 LUFS | -22.99 in / -22.99 out, 0 samples dropped |
| EQ display curve vs measured chain response | < 0.25 dB | 0.000 dB max error |

## Advisor behaviour
* Clean voice vs its own reference, synthetic and **real**: **no recommendations** (no false positives).
* Injected faults are found at the right place: +8 dB mud at 300 Hz → a cut at 300 Hz (real voice:
  300 Hz); +9 dB at 3.2 kHz → 3.4 kHz (real: +8 dB at 3.5 kHz → 3.4 kHz, persistent in 98 % of
  frames); rumble → HPF 140 Hz for a 196 Hz lowest note; strong sibilance → de-esser, simulated
  +8.0 → -0.7 dB with vowel high end -2.0 dB; ±7 dB phrase swings → compression, 16.5 → 9.6 dB
  range at 5.9 dB GR. A pre-limited source gets **no** extra compression. A clipped take gets an
  information-only note. Polarity-inverted stereo gets a mono-compatibility warning. Full-mix
  context gets no vocal processing changes. A sidechain beat is used.
* Safety bounds over 48 analyses (8 random faulty voices × 6 targets): every cut ≤ 4 dB, every
  boost ≤ 3 dB, plan GR ≤ 6.5 dB.
* Real speech (3 speakers): 26 real sibilants detected and correctly **not** judged excessive.

## Real-time / CPU / latency
* Latency 48 samples at 48 kHz (1.00 ms), reported to the host; host bypass is bit-exact delayed dry.
* DSP CPU: **5.05 %** of one core (stereo, 48 kHz, every module on, 20 s render, this container's CPU).
* Worst `processBlock` while the advisor ran in the background: **0.69 ms** for a 512-sample
  (10.7 ms) block.
* Analysis time: about 5-15 s for 10-35 s of captured audio, on a background thread.

## Screenshots (genuine renders)
`screenshots/01-04` are the real editor rendered by the plugin test host while live audio flowed
(Assist report, details + auditioning, Channel page after Accept all). `05` is the Linux
Standalone app running on a virtual X display. **No screenshot from Windows or a DAW exists.**

## Not verified
Running in FL Studio or Audacity, the editor on real Windows (Wine cannot run JUCE 8's
Direct2D editor), FL sidechain routing, and listening tests. See `KNOWN_LIMITATIONS.md`.
