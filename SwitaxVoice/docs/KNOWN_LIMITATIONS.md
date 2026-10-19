# Known limitations and what remains unverified

## Not verified (please test)
* **Not run on a real Windows PC or in FL Studio / Audacity.** The Windows VST3 was
  cross-compiled on Linux and validated with pluginval under Wine **without GUI tests**. JUCE 8's
  Direct2D editor cannot open under Wine (the same as VocalSweet v1, whose UI you confirmed works
  on your PC). The editor itself is tested and rendered on Linux.
* **FL Studio sidechain routing to a VST3 input** follows FL's documented mechanism but was not
  tried here.
* **No listening tests** were possible. Claims about audio are measurements and simulations.
* **The house targets are heuristic.** They were written from general production practice, made
  pitch-adaptive, and sanity-checked on VocalSet recordings (classical-trained singers, not Afro
  vocalists). They were not measured from commercial Afrobeat/Amapiano records. Use a reference
  file for measured comparisons. The confidence values are a transparent formula, not a
  calibrated probability.
* No Windows `moduleinfo.json` (VST3 manifest). It is optional; hosts scan the DLL.

## Design limitations
* **No pitch correction yet** (see RESEARCH.md: the QPitch/Rubber Band option and its risks).
* Analysis needs a LEARN pass: proposals do not update continuously while you sing.
* Tone analysis averages active, non-sibilant frames of a mono sum. Strongly different L/R
  content is assessed only by the stereo checks.
* Below about 1.8 kHz the tool reports broad build-up only. It does not claim narrow resonances
  there, because the melody's own harmonics look like resonances at 1/6-octave resolution.
* Sibilance detection is spectral, not phonetic: hi-hat bleed or breaths can count.
* Proposal predictions assume the earlier proposals in the list are accepted (they are computed
  as a cumulative plan). Changing the amount does not re-run the simulation.
* There is no noise gate or de-clipper. These issues are reported with advice, not fixed.
* The doubler sums stereo to mono for its voices (inherited from VocalSweet).
* The report is not stored in the DAW project (settings, the reference path and all parameters
  are). Press LEARN again after reopening.
* Not code-signed. No macOS build was produced (the CMake project supports AU/VST3 on a Mac).
