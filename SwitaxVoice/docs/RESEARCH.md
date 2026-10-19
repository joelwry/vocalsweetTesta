# Research notes: implementation references

Date of review: 2026-10-19. Repositories were cloned and their **source code was read**; READMEs
were not taken at face value. Nothing from these projects was copied into SwitaxVoice.

## 1. OpenTune: https://github.com/bemtorres/opentune

| | |
|---|---|
| Activity | 3 commits, all 23–24 Feb 2026. One author. No tests, no CI. |
| Licence | MIT (permissive). |
| Dependencies | JUCE 8 (FetchContent). It also ships a copy of `pluginterfaces/vst2.x/vstfxstore.h`, a Steinberg VST2 SDK header that Steinberg no longer licenses for new products. |
| Code size | about 1,300 lines (`PluginProcessor.cpp` is 433 lines). |

**What the code actually does** (`Source/PluginProcessor.cpp`):

* *Pitch detection.* A comment calls it "YIN-simple", but `detectPitchAutocorrelation()` is a
  brute-force **normalised autocorrelation**. Every 256 samples it runs an O(N × lag) double loop
  over a ring buffer, inside `processBlock` on the audio thread. There is no YIN difference
  function, no cumulative-mean normalisation and no parabolic interpolation (so pitch resolution
  is one sample of lag: about 1 % at 440 Hz / 48 kHz).
* *Pitch shifting.* The README claims "granular synthesis, formant-friendly". The code is a
  **variable-rate read of a ring buffer** (`ringReadPos += ratio`) with linear interpolation.
  When the read pointer drifts too close or too far, it jumps by ±2 samples. That causes
  discontinuities (clicks) and shifts formants together with pitch, so it is not
  formant-preserving.
* *Scale snapping.* Only Major, Minor and Chromatic. Retune is a one-pole smoother on the target
  frequency.
* *Effects.* The echo uses `juce::Array::set()` per sample. That works, but it is not a pattern
  to copy.

**Verdict.** Not suitable for professional work, and not worth adapting. Its architecture
(detect on the audio thread, shift by resampling) is the wrong foundation for VocalSweet Tune.

## 2. QPitch: https://github.com/Skynse/qpitch

| | |
|---|---|
| Activity | 33 commits (May–Oct 2026), CI for Linux/macOS/Windows, release workflow, UML docs. |
| Licence | **GPLv3**. It statically links **Rubber Band Library 4.0.0** (GPLv2+, or a commercial licence from Particular Programs). |
| Dependencies | JUCE 8.0.8 (submodule), clap-juce-extensions, Rubber Band (vendored single-file build). |

**What the code actually does:**

* `src/dsp/PitchDetector.cpp` is a correct **YIN** implementation (difference function,
  cumulative-mean normalised difference, absolute threshold with first-valley selection,
  sub-harmonic check, parabolic interpolation), run every 128 samples. It is O(N²) per analysis
  and runs on the audio thread. Its buffers use `resize()` in `setFrequencyRange()`, which is
  fine as long as that is only called from `prepare`.
* `src/dsp/PitchShifter.cpp` wraps **RubberBand::RubberBandLiveShifter** (`OptionWindowShort`,
  `OptionChannelsTogether`, optional `OptionFormantPreserved`). Latency is the engine block size
  plus the start delay, and it is reported to the host. This is a sound, formant-aware, real-time
  approach.

**Verdict.** Technically credible and the best reference for VocalSweet Tune. Adapting it would
make the product GPL, or need a commercial Rubber Band licence plus a rewrite of the QPitch parts.
This is the *option to adapt* the brief asked about:

* **Option:** build VocalSweet Tune on Rubber Band LiveShifter (licensed commercially) with our
  own YIN detector, scale mapper, retune/humanise and note-transition logic.
* **Risks:** the licence cost and terms of Rubber Band; latency (several thousand samples at
  48 kHz with the short window, which is noticeable when tracking vocals live); a CPU cost much
  higher than our current chain; correction artefacts on breathy and fast melismatic Afro-pop
  runs that need listening tests; and YIN octave errors on vocal fry and creaky onsets.
* **Decision for this milestone:** do **not** ship pitch correction yet. The analysis engine
  built now already contains a tested YIN tracker (`PitchTracker`, verified to within 1 % from
  98 to 660 Hz), which is the first reusable part VocalSweet Tune needs.

## 3. JUCE DSP tutorial and JUCE course

`juce.com/tutorials/tutorial_dsp_introduction/` and `juce.com/learn/course/` render client-side,
and the fetch returned no readable content in this environment. They were therefore **not read**.
The JUCE guidance used here comes from the JUCE 8.0.4 source and bundled examples
(`modules/juce_dsp`, `examples/Plugins`), which were read locally:

* `prepare(ProcessSpec)` / `process(context)` / `reset()` lifecycle. SwitaxVoice's chain follows
  the same contract with its own framework-independent classes, so the identical DSP also runs
  offline in the advisor and in tests without JUCE.
* Parameters via `AudioProcessorValueTreeState` with versioned `ParameterID`s. Plain values are
  read lock-free on the audio thread through `getRawParameterValue()` atomics.
* State through `copyState()` → XML → `copyXmlToBinary()`. On restore, every parameter is
  re-asserted, because APVTS skips values whose plain value did not change (the same workaround
  VocalSweet uses, and it is tested).
* Testing: `pluginval` (Tracktion, GPLv3, tool only) at strictness 10, plus our own in-process host
  tests that call `processBlock` with varying block sizes, bypass, state round trips and editor
  snapshots.

## 4. Publicly documented capabilities of the commercial categories (no code, no UI copied)

| Category | Public capabilities (marketing / manuals) | SwitaxVoice response |
|---|---|---|
| Auto-Tune Pro (Antares) | key/scale correction, retune speed, humanise, flex-tune, formant correction, graph mode, low-latency mode | Planned as **VocalSweet Tune** (see ARCHITECTURE). Not in this milestone. |
| Nectar (iZotope) | vocal channel strip, "Vocal Assistant" that listens and suggests settings, de-ess, saturation, pitch, doubler, reverb/delay | Our **Assist** is an original, explainable design: it measures, shows evidence and confidence, *simulates* each proposal on the user's audio and never auto-applies. |
| Ozone (iZotope) | mastering chain, "Master Assistant", tonal balance against genre targets, loudness and true-peak metering | Our tonal balance compares against **openly documented** heuristic house targets or a **measured** reference file. Loudness/true peak follow the public ITU-R BS.1770-4 and EBU R128 / Tech 3341 / 3342 specifications. |

No proprietary algorithm is known to us or reproduced. All techniques are textbook or published
standards (references below).

## 5. Other implementations and specifications consulted

* **libebur128** (MIT): the documented method for deriving K-weighting biquad coefficients at any
  sample rate from the analogue prototype parameters. Our implementation is written from the
  published constants and verified against EBU Tech 3341/3342 cases.
* **ITU-R BS.1770-4**: K-weighting, gating, and 4× oversampled true peak (Annex 2).
* **EBU Tech 3341 / 3342**: loudness and loudness-range compliance cases (used as tests).
* **de Cheveigné & Kawahara (2002)**: YIN fundamental frequency estimator.
* **A. Simper (Cytomic)**: TPT state-variable filter (already in VocalSweet).
* **VocalSet** (Wilkins et al. 2018, CC BY 4.0, Zenodo 1442513): real singing and speech
  recordings, used **only as test material** (not shipped).
