# Dependencies and licensing

SwitaxVoice's own code (`Source/`, `tests/`) was written for this project. The shared DSP
primitives come from VocalSweet (same author/project) and are used unmodified. No code, artwork,
names or UI from Auto-Tune, Nectar, Ozone, OpenTune, QPitch or any other product is included.

| Component | Version | Used for | Licence | Shipped in the plugin? |
|---|---|---|---|---|
| **JUCE** | 8.0.4 | Framework, GUI, VST3 wrapper, audio file decoding (WAV/AIFF/FLAC/OGG/MP3) | AGPLv3 **or** JUCE 8 commercial licence | Yes (statically linked) |
| **VST3 SDK** (inside JUCE) | 3.7.x | VST3 interfaces | GPLv3 **or** Steinberg proprietary licence | Yes |
| **Space Grotesk** font | 2.0 (2021) | Display typography | SIL OFL 1.1 (`Resources/fonts/SpaceGrotesk-OFL.txt`) | Yes (embedded) |
| **Inter** font | 4.0 | Body typography | SIL OFL 1.1 (`Resources/fonts/Inter-OFL.txt`) | Yes (embedded) |
| VocalSweet DSP primitives | 1.0 | Filters, dynamics, saturation, doubler | Project code | Yes |
| libebur128 (reference only) | – | K-weighting coefficient derivation method | MIT | No code copied |
| **VocalSet** dataset | 1.2 | Real-voice **test material** only | CC BY 4.0 (Wilkins, Seetharaman, Wahl, Pardo 2018) | **No** |
| pluginval | 1.0.4 | Validation tool | GPLv3 | No |
| LLVM clang-cl / lld | 20.1 | Windows cross-compiler | Apache 2.0 + LLVM exception | No |
| MSVC CRT + Windows SDK via `xwin` | VS 17 / SDK 10.0.26100 | Static CRT for the Windows build | Microsoft licence terms | CRT statically linked |

Not used (evaluated only): **Rubber Band Library** (GPLv2+/commercial, via QPitch) and
**OpenTune** (MIT). See `RESEARCH.md`.

## What this means
* **Personal use:** fine as is.
* **Distribution (free or paid):** you need a JUCE licence (or to release under AGPLv3), and you
  must sort out VST3 licensing (Steinberg agreement, or GPLv3). Keep the OFL notices for both
  fonts. "VST" is a Steinberg trademark.
* "SwitaxVoice" is not trademark-checked. Search before commercial use.
