# Dependencies and licensing notes

VocalSweet's own source code (everything under `Source/`, `tests/`, `cmake/`) was written for
this project. It contains no code, artwork or branding from Voloco or any other commercial
product. The DSP algorithms are standard textbook techniques, implemented from scratch:
Simper/Cytomic TPT state-variable filters, a feed-forward soft-knee compressor, a split-band
de-esser, tanh saturation with antiderivative anti-aliasing, and a look-ahead limiter.

| Dependency | Version | Used for | Licence | What it means for you |
|---|---|---|---|---|
| **JUCE** | 8.0.4 | Plugin framework, GUI, VST3 wrapper | Dual: **AGPLv3** *or* commercial **JUCE 8 EULA** (free "Starter" tier for small revenue) | Fine for personal use. If you **distribute** binaries you need either a JUCE licence (Starter/Indie/Pro) or to release the full source under AGPLv3. |
| **VST3 SDK** (bundled in JUCE) | 3.7.x | VST3 interfaces | Dual: **Steinberg proprietary VST3 licence** *or* **GPLv3** | Commercial distribution under the proprietary licence needs Steinberg's signed VST3 licence agreement. The GPLv3 route needs a GPL-compatible release. "VST" is a Steinberg trademark; follow the VST3 usage guidelines. |
| **Inter** font (Regular/SemiBold/Bold) | 4.0 | UI typography, embedded as binary data | **SIL Open Font License 1.1** (`Resources/fonts/OFL-LICENSE.txt`) | Free to embed and redistribute with the licence text. |
| **pluginval** (testing only, not shipped) | 1.0.4 | Plugin validation | GPLv3 | Only used as a test tool. |
| **MSVC CRT + Windows SDK** via `xwin` (build only) | VS 17 / SDK 10.0.26100 | Cross-compiling the Windows DLL from Linux | Microsoft Visual Studio licence terms (accepted by `xwin --accept-license`) | The CRT is statically linked (`/MT`) into the plugin, so no VC++ redistributable is needed. Check the Microsoft licence terms yourself before distributing commercially. |
| **LLVM / clang-cl / lld** (build only) | 20.1 | Compiler and linker | Apache 2.0 with LLVM exception | None for the output binary. |

## Summary

* **Personal use** (your own FL Studio / Audacity sessions): no action needed.
* **Distributing** VocalSweet (free or paid): get a JUCE licence or release under AGPLv3,
  sort out VST3 licensing with Steinberg (or release under GPLv3), and keep the OFL notice for
  the Inter font.
