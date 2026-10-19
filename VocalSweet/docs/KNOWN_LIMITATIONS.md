# Known limitations and remaining work

## Verification gaps (be aware before relying on it)

* **Not yet run on a real Windows PC.** The Windows x64 VST3 was cross-compiled on Linux
  (clang-cl 20 + MSVC CRT/SDK via xwin). The real Windows DLL **was loaded and validated under
  Wine** with pluginval 1.0.4 at strictness 10 (scan, open, programs, audio processing, state
  restoration, automation, parameter thread safety, buses, fuzzing): all passed with
  `--skip-gui-tests`.
* **Windows GUI not verified.** Under Wine the editor did not open: pluginval's Editor test hung
  and the standalone .exe showed no window. Wine's Direct2D/DirectComposition support, which
  JUCE 8 uses on Windows, is incomplete, so this is expected. It does not show a problem on real
  Windows, but it is not proof that the UI works there either. The same editor code runs, renders
  and passes tests on Linux (see screenshots).
* **FL Studio and Audacity compatibility have not been directly verified.** Neither host was
  available in the build environment. The plugin is a standard VST3 (mono/stereo, programs,
  host bypass, latency reporting, state chunks), which both hosts support, but please test it.
* **No listening tests.** Nobody listened to the processed audio during development. Audio
  quality claims are limited to the numeric measurements in `test-results/`. The "vocal" used
  for tests and the WAV examples is a **synthetic formant-filtered signal**, not a real singer.
* The plugin is **not code-signed**, so Windows SmartScreen may warn; unblock the zip.
* The Windows bundle has no `moduleinfo.json` (VST3 manifest). Generating it requires running the
  plugin on Windows. It is optional, and hosts scan the DLL directly. On Windows you can create it
  by building natively with VST3_AUTO_MANIFEST on (the default for native builds).

## Design limitations

* The compressor uses a peak-sensing detector. A program-dependent RMS option and side-chain EQ
  would be useful additions.
* The doubler sums the input to mono before creating its two voices, so stereo input loses its
  original stereo image in the doubled voices only; the dry/main path is untouched.
* Saturation uses first-order ADAA plus a warm pre-filter instead of oversampling. Measured
  aliasing is −62 dB relative to the fundamental for a 7 kHz tone at full drive.
* No pitch correction or formant shifting (Voloco-style autotune is out of scope here).
* No undo/redo history inside the editor (the host's undo still works through automation
  gestures).
* There is no A/B compare, and user presets can be saved and loaded but not renamed in the UI
  (rename the `.vspreset` file).
* No macOS (AU/VST3) build was produced. The CMake project supports it (add `AU` to FORMATS), but
  it needs a Mac to build.

## Suggested next steps

1. Load the VST3 in FL Studio and Audacity on Windows 10/11 and check that the UI opens, presets,
   automation and project recall work, and CPU use.
2. Run the Steinberg VST3 validator and pluginval (with GUI tests) on native Windows.
3. Do listening tests on real Afrobeat/Afrofusion/rap vocals and fine-tune preset values.
4. Code-sign the DLL. Get JUCE and VST3 licences before any public distribution.
