# SwitaxVoice plugin test report

| # | Test | Result | Measurement |
|---|---|---|---|
| 1 | All parameters registered with stable host IDs | PASS | 39 / 39 parameters |
| 2 | Parameter defaults match the specification | PASS | all defaults verified |
| 3 | Parameter value text for automation lanes | PASS | 80 Hz, -18.0 dB |
| 4 | Bus layouts: mono/stereo, optional mono/stereo sidechain; others rejected | PASS | mono->mono, stereo->stereo, sidechain off/mono/stereo |
| 5 | Host bypass parameter exposed | PASS | getBypassParameter() == 'bypass' |
| 6 | processBlock: 3 rates x 3 block sizes x mono/stereo | PASS | finite, peak <= -1 dBFS, 1 ms latency reported |
| 7 | Randomly varying host block sizes (1..3000) | PASS | 63 processBlock calls, all finite |
| 8 | Host bypass (processBlockBypassed) = latency-aligned dry | PASS | max |error| 0.00e+00 |
| 9 | Audition changes the sound but not the parameters | PASS | audition output -12.00 dB, parameters unchanged |
| 10 | A/B compare = unprocessed input with level-match gain | PASS | -4.00 dB |
| 11 | Accepting a proposal = host-visible, undoable parameter gestures | PASS | 3 parameter changes, 3 gestures |
| 12 | State save -> restore (parameters, preset name, Assist settings) | PASS | 1661 bytes, 0 mismatches |
| 13 | Corrupt / empty state ignored safely | PASS | defaults retained |
| 14 | Factory presets load exact values via host programs | PASS | Clean Start, Afrobeat Lead, Afrofusion Lead, Amapiano Vocal, Melodic Rap, Afro-Pop Bright, Background Stack, Ad-lib Spice, Analysis Only |
| 15 | User preset save / load / delete (.sxpreset) | PASS | saved to /root/.config/SwitaxVoice/Presets/zz UnitTest Preset.sxpreset |
| 16 | Fast host automation (EQ, threshold, switches) is click-free | PASS | max sample step 0.0227 (limit 0.0393) |
| 17 | Live BS.1770 meters through the plugin: -23 dBFS stereo 1 kHz -> -23 LUFS in & out | PASS | in -22.99, out -22.99 LUFS, dropped 0 |
| 18 | Sidechain bus reaches the analyser | PASS | sidechain short-term -15.6 LUFS |
| 19 | Default window size fits typical screens (laptop and 125-175 % display scaling) | PASS | 1366x768 @100% 991x638 ok; 1920x1080 @125% 1152x742 ok; 1920x1080 @150% 936x603 ok; 1920x1080 @175% 783x504 ok; 2560x1440 @100% 1180x760 ok; 1280x720 @100% 916x590 ok;  |
| 20 | Editor never opens larger than the screen work area | PASS | editor 1180 x 760, work area 1920 x 1080 |
| 21 | setScale: exact size, clamped between 50 % and what fits the screen | PASS | 60 % -> 708 x 456 |
| 22 | Edge handles exist and sit on the plugin's right/bottom edges (hit area >= 8 px) | PASS | right 8 px, bottom 8 px, corner 28 px |
| 23 | Size menu: 75 % and Fit to screen | PASS | scale now 1.00 |
| 24 | Window size is remembered in the project state | PASS | reopened at 826 x 532 |
| 25 | A bogus / oversized saved size is clamped to the screen | PASS | opened at 1584 x 1020 |
| 26 | LEARN end-to-end: capture through processBlock -> background analysis -> report | PASS | 6 proposals and 1 note against Afrobeat Lead. [lowend,deess,lowmid,box,air,comp,noise] |
| 27 | Audio thread stays fast while the advisor runs (99.9th percentile < 4 ms) | PASS | median 0.338 ms, p99.9 1.425 ms, worst 3.359 ms over 2218 blocks (budget 10.7 ms) |
| 28 | Editor opens at a size that fits the screen, with the design aspect ratio | PASS | 1180 x 760 |
| 29 | Assist page shows one card per recommendation | PASS | 7 cards |
| 30 | Card: audition (params untouched), accept at 50 % amount, undo restores | PASS | 'Low-frequency hum or rumble': High-pass just below the lowest note.  [HPF Frequency 70 Hz -> 140 Hz] | audition 1 changed 1 halfway 1 undone 1 |
| 31 | Card: reject marks it without touching parameters | PASS | rejected |
| 32 | Editor snapshots rendered (real editor, live audio) | PASS | /opt/build/sx-linux/test-output/editor-snapshots |
| 33 | Factory presets exported as .sxpreset files | PASS | /opt/build/sx-linux/test-output/presets |

**Total: 33, passed: 33, failed: 0**
