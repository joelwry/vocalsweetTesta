# Installing and using SwitaxVoice on Windows (FL Studio, Audacity, any VST3 host)

Requirements: Windows 10/11, a **64-bit** host. No Visual C++ redistributable is needed (the
runtime is statically linked). SwitaxVoice installs **next to** VocalSweet; it has its own plugin
ID, so it does not replace or affect your working VocalSweet.

## 1. Install
1. Right-click `SwitaxVoice_v1.0.0_Windows_x64.zip` → **Properties** → tick **Unblock** → OK
   (the plugin is not code-signed).
2. Extract it, then copy the whole **`SwitaxVoice.vst3` folder** to
   `C:\Program Files\Common Files\VST3\`.
3. Rescan plugins in your host:
   * **FL Studio:** Options → Manage plugins → *Find installed plugins*. Favourite **SwitaxVoice**.
   * **Audacity 3.2+:** Effect → Plugin Manager → Rescan, enable SwitaxVoice.

## 2. First session (Assist)
1. Insert SwitaxVoice on the **vocal track** (insert slot, not the master).
2. Assist page → **What is on this track?** Pick *Isolated vocal* (or *Vocal with bleed* / *Vocal bus*).
3. **Compare with:** a style (Afrobeat Lead, Afrofusion, Amapiano, Melodic Rap, Afro-Pop Bright,
   Natural), or **Load reference** with a vocal you love (WAV/AIFF/FLAC/OGG/MP3). A reference is a
   *measured* comparison; the styles are *heuristic* house targets.
4. Press **LEARN**, then play 10–45 s of the part. Pauses are not counted. When enough signal
   has been heard, the analysis runs in the background (a few seconds) and the cards appear.
5. For each card: **AUDITION** (hear it without committing), set **AMOUNT** (0–150 %),
   **ACCEPT** or **REJECT**. *Details* shows the evidence, the simulated result, confidence and
   limitations. **UNDO** reverses the last accept. Accepts are normal parameter changes, so your
   DAW's undo and automation see them too.
6. **A/B** (top bar) plays the unprocessed input level-matched to the output, so louder ≠ better.

## 3. Optional: let SwitaxVoice hear the beat (sidechain)
* **FL Studio:** route the beat's mixer track to the vocal track as a **sidechain** (right-click the
  send arrow → *Sidechain to this track*). In the SwitaxVoice wrapper settings
  (*Processing* → input ports), map the sidechain to input 3-4. The right rail then shows
  "Sidechain: … LUFS" and the next LEARN adds vocal-vs-beat checks.
* Without a sidechain, SwitaxVoice does not know anything about your beat and will say so.

## 4. Channel page
Every module is adjustable: HPF, 5-band tone EQ (low-mid, box, harsh, presence, air), compressor,
de-esser, saturation, exciter, doubler and output (input/output gain, dry/wet, ceiling, limiter).
Highlighted knobs show what an auditioned card would change.

## Uninstall
Delete `C:\Program Files\Common Files\VST3\SwitaxVoice.vst3` and optionally `%APPDATA%\SwitaxVoice\`.
