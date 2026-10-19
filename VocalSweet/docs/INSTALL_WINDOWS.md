# Installing VocalSweet on Windows (FL Studio, Audacity, any VST3 host)

Requirements: Windows 10 or 11, **64-bit** host. No Visual C++ redistributable is needed
because the runtime is statically linked.

## 1. Unblock and extract

1. Right-click the downloaded `VocalSweet_VST3_Windows_x64.zip` → **Properties** → tick
   **Unblock** (if shown) → OK. The plugin is not code-signed, so Windows may otherwise flag it.
2. Extract the zip.

## 2. Copy the plugin

Copy the whole **`VocalSweet.vst3` folder** (keep the folder structure
`VocalSweet.vst3\Contents\x86_64-win\VocalSweet.vst3`) to:

```
C:\Program Files\Common Files\VST3\
```

(Administrator permission is needed for that folder. Any custom VST3 folder your DAW scans
also works.)

## 3a. FL Studio

1. **Options → Manage plugins**. Make sure `C:\Program Files\Common Files\VST3` is in the
   search paths.
2. Click **Find installed plugins** (or "Find more plugins") and wait for the scan.
3. Tick the star/favourite next to **VocalSweet** so it appears in the plugin database.
4. Open the **Mixer** (F9), select the vocal's insert track, click an empty slot → **VocalSweet**.
5. Pick a preset from the VocalSweet dropdown (or with FL's preset arrows: presets are exposed
   as programs), then turn **SWEETEN**.

## 3b. Audacity (3.2 or newer, 64-bit)

1. **Edit → Preferences → Effects**: make sure VST3 is enabled.
2. **Effect → Plugin Manager → Rescan** (or restart Audacity). Enable **VocalSweet**.
3. Either select the vocal clip → **Effect → VocalSweet…** (destructive, offline), or open the
   track's **Effects** panel → **Add effect → VocalSweet** for real-time use.

## 4. Getting a good result quickly

1. Set the input level so the vocal peaks around −12 to −6 dBFS (watch the INPUT meter, or use
   the Input knob).
2. Choose the preset closest to the style (e.g. *Afrobeat Lead*, *Melodic Rap*).
3. Turn **SWEETEN** until the vocal sounds polished. Use **Tone** for dark/bright, **Warmth** for
   body, **Presence** to push it forward, and **Control** for steadier levels.
4. Watch the GAIN REDUCTION panel: 3–8 dB of COMP on peaks is typical for a lead vocal.
5. Save your own version with **SAVE**.

## Uninstall

Delete `C:\Program Files\Common Files\VST3\VocalSweet.vst3` and, optionally,
`%APPDATA%\VocalSweet\`.

## Standalone app (optional)

`VocalSweet.exe` (in the full deliverable) runs without a DAW. Use **Options → Audio/MIDI
Settings** to pick your interface. Input is muted at start to avoid feedback; un-mute it in the
settings and use headphones.
