# SwitaxVoice / VocalSweet ecosystem architecture

## Product family

| Product | Status | Purpose |
|---|---|---|
| **VocalSweet** | Shipped (v1.0, unchanged) | One-knob vocal sweetener. |
| **SwitaxVoice** (= *VocalSweet Analyze + Studio*) | **This milestone (v1.0)** | Explainable vocal analysis (Assist) plus a full vocal channel strip that the proposals drive. |
| VocalSweet Tune | Designed, not built | Key/scale pitch correction, retune speed, humanise, formant-aware shifting. |
| VocalSweet Layers | Partly (doubler) | Doubling, width, ad-lib FX, later harmony generation. |
| VocalSweet Master | Designed | Vocal-bus tonal balance, dynamics, width, loudness/peak. |

**Why this milestone.** Analysis creates the most value and builds the most reusable
foundations: loudness, true peak, spectrum, pitch tracking, sibilance and stereo measurement are
needed by Analyze, Tune (pitch tracking), Studio (assist) and Master (loudness, tonal balance).
Pairing analysis with a working channel strip means every recommendation can be **auditioned
and applied**, not merely displayed. Pitch correction was deferred deliberately (see
RESEARCH.md §2).

## Layers and shared code

```
VocalSweet/Source/dsp/      (shared DSP primitives, used read-only by SwitaxVoice)
    Svf.h        TPT state-variable filter, smoothing, dB helpers
    Dynamics.h   compressor, split-band de-esser, look-ahead limiter, delay line
    Tone.h       ADAA saturation, exciter, doubler
SwitaxVoice/Source/core/     (framework independent)
    Params.h     parameter table (IDs, ranges, defaults, units)
    Chain.h      real-time channel strip built from the shared primitives
    Presets.h    factory presets
SwitaxVoice/Source/analysis/ (framework independent, NOT real-time)
    Loudness.h   K-weighting, M/S/I loudness, LRA, true peak
    Spectrum.h   FFT, calibrated 1/6-octave bands
    Analyzer.h   activity gating, clipping, pitch (YIN), sibilance, stereo, Measurements
    Targets.h    contexts, heuristic house targets (pitch-adaptive), measured references
    Advisor.h    rules -> bounded proposals -> offline simulation -> Report
SwitaxVoice/Source/          (JUCE)
    PluginProcessor   parameters, audition override, level-matched A/B, state, sidechain bus
    AnalysisEngine    lock-free FIFO from the audio thread, worker thread, pool jobs
    PresetManager     factory + user presets (*.sxpreset XML)
    ui/               LookAndFeel, widgets, Assist page, Channel page
```

The framework-independent layers compile without JUCE. The same `Chain` class runs in the
plugin (real time) and in the advisor (offline simulation), and the tests exercise both.
Future products reuse `analysis/` unchanged. The shared `vs::dsp` primitives should move to a
top-level `common/` folder once VocalSweet itself is re-released. They were left in place now so
the known-working VocalSweet build is byte-for-byte untouched.

### Parameter conventions
lowerCamelCase IDs that never change once shipped (`ParameterID` version 1); natural units
(dB, Hz, ms, %, :1); every module has an `<module>On` switch with a 20 ms click-free crossfade;
gains are smoothed per sample and filter coefficients every 16 samples.

### Preset format
XML (`<SwitaxVoicePreset name version …paramId="value"…/>`), one attribute per parameter ID,
missing IDs fall back to defaults (forward compatible). Factory presets are code (`Presets.h`) and
exposed as host programs. Exported copies are in `presets/`.

### UI components
`Knob` (segmented ring), `LevelMeter`, `LoudnessPanel` (BS.1770 readouts, GR bars, correlation),
`EqDisplay` (filter response computed from the DSP's own equations over live spectra),
`ModuleCard`, `RecCard`. All are reusable across the family through `SwitaxLookAndFeel`.

## Real-time design

* The audio thread does chain processing, one copy of the input into a preallocated scratch
  buffer, and `AnalysisEngine::push()` (`juce::AbstractFifo`, no locks, no allocation).
  Overflow is counted and shown in the footer; it is never hidden.
* Worker thread (low priority): live loudness/true peak/correlation/spectra and the LEARN capture.
* Thread-pool job: the advisor (seconds of work) and reference-file decoding. The audio thread is
  never blocked; the test measures the worst `processBlock` time while the advisor runs.
* Audition: an atomic override array the audio thread reads instead of the parameters. Nothing
  is written to the host, so automation and undo history are unaffected.
* A/B compare: forces the latency-compensated bypass path and applies the measured loudness
  difference (output minus input, BS.1770) so the comparison is level-matched.

## Analysis engine (Mission 4)

### Signals actually available
SwitaxVoice analyses **only its own input**, plus an optional **sidechain** (route the beat to it).
The user declares the *context*: isolated vocal, vocal with bleed, vocal bus, full mix or
instrumental. The context changes which rules run and lowers confidence when appropriate. Every
report lists what was analysed and what was **not** available. In a mix context, the vocal
processing proposals are disabled; SwitaxVoice reports loudness and balance information and
offers an analysis-only mode.

### Measurements (facts)
| Quantity | Method |
|---|---|
| Integrated / momentary / short-term loudness, LRA | ITU-R BS.1770-4, EBU Tech 3342 (verified with 3341/3342 cases) |
| True peak | 4× polyphase windowed-sinc oversampling |
| Activity | 100 ms blocks above max(-60 dBFS, loud blocks - 30 dB), from per-channel power |
| Level range | P95 - P10 of momentary loudness (gated) |
| Crest, PLR, noise floor | peak vs active RMS; true peak - integrated; P10 of 100 ms levels |
| Clipping | runs of >= 3 samples at full scale; runs of >= 4 identical samples below full scale |
| Spectrum | Hann-windowed FFT, 1/6-octave bands (Parseval-calibrated), active non-sibilant frames only |
| Resonance persistence | fraction of frames in which a band exceeds its 1-octave trend by 3 dB |
| Pitch | YIN on a 16 kHz decimated copy; p5/p50/p95 and voiced fraction |
| Sibilance | 10 ms windows; events where (4.5-11 kHz) - (150 Hz-2.5 kHz) exceeds its own median by 15 dB. This is invariant to static EQ, so before and after are compared on the same moments. |
| Stereo | correlation (full band and below 150 Hz), side/mid ratio, mono fold-down loss |

### Heuristics (interpretations) and their guards
* Tonal deviation is measured against the target after removing the **median** offset (a big
  local problem cannot hide itself).
* House targets are **pitch-adaptive** below about 1.2 kHz: a voice's low-mid spectrum follows
  the notes sung, so a fixed curve would label sopranos "boxy" and baritones "muddy".
* "Persistent resonance" is only claimed at 1.8 kHz and above. Lower down, the melody's own
  harmonics form persistent peaks.
* Low end is judged relative to the lowest sustained fundamental, not a fixed 80 Hz.

### Proposals (suggestions) and their bounds
EQ cuts of at most 4 dB per band, boosts of at most 3 dB, no stacking beyond -8 dB; compression
bounded at about 6 dB average gain reduction; the de-esser search rejects settings that dull the
high end between sibilants by more than 2 dB; no automatic loudness maximisation. The output
level-match proposal keeps loudness equal to the input.

### Simulation
Each proposal is rendered through the real chain on the captured audio, and the targeted metric
is re-measured: the card shows "before -> after" on the user's audio.

### Confidence
`duration factor x (0.5 evidence strength + 0.5 consistency across frames) x context factor x
target factor` (house target 0.8, measured reference 1.0), clamped to 5-95 %. The basis is shown
on every card.

### UI contract for every recommendation
Evidence (MEASURED), likely issue (HEURISTIC), proposal (SUGGESTION), predicted outcome
(SIMULATED), confidence with basis, limitations, and **Audition / Amount (0-150 %) / Accept /
Reject**, plus Accept-all, Audition-all and Undo.

## Next steps for the family
1. **VocalSweet Tune:** reuse `PitchTracker` (move it to a real-time-safe incremental form),
   add scale mapping, note segmentation and retune/humanise, then a formant-preserving shifter
   (Rubber Band under a commercial licence, or an in-house PSOLA/phase-vocoder with spectral
   envelope preservation). Measure latency, CPU and artefacts on VocalSet and real Afro vocals.
2. **Layers:** stereo-preserving doubler, ad-lib throws (delay/reverb), and later harmony voices
   that reuse the Tune shifter.
3. **Master:** reuse `Loudness.h`, `Spectrum.h`, `Targets.h` (mix targets), and add a linked
   multiband compressor and mid/side EQ.
