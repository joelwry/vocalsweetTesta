# VocalSweet plugin test report

| # | Test | Result | Measurement |
|---|---|---|---|
| 1 | All parameters registered with host-visible IDs | PASS | 41 / 41 parameters |
| 2 | Parameter defaults match specification | PASS | all defaults verified |
| 3 | Parameter value text for automation lanes | PASS | 80 Hz, -18.0 dB |
| 4 | Bus layouts: mono & stereo accepted, others rejected | PASS | mono->mono, stereo->stereo |
| 5 | Host bypass parameter exposed | PASS | getBypassParameter() == 'bypass' |
| 6 | processBlock: 3 rates x 3 blocks x mono/stereo | PASS | finite, peak <= -1 dBFS, latency = 1 ms reported to host |
| 7 | Randomly varying block sizes (1..1024) | PASS | 172 processBlock calls, all finite |
| 8 | NaN / Inf from host buffer sanitised | PASS | output finite |
| 9 | Host bypass (processBlockBypassed) = delayed dry | PASS | max |error| 0.00e+00 |
| 10 | State save -> restore (all parameters) | PASS | 1637 bytes, 0 mismatches, preset 'Warm Intimate Vocal' |
| 11 | Corrupt / empty state ignored safely | PASS | defaults retained |
| 12 | Factory presets load exact values via host programs | PASS | Default, Afrobeat Lead, Afrofusion Lead, Warm Intimate Vocal, Bright Pop Vocal, Melodic Rap, Smooth Background Vocal, Vocal Double, Natural Clean Vocal |
| 13 | User preset save / load / delete | PASS | saved to /root/.config/VocalSweet/Presets/zz UnitTest Preset.vspreset, warmth restored to 88.0 |
| 14 | Fast host automation of macros is click-free | PASS | max sample step 0.0078 (limit 0.0295) |
| 15 | Editor opens | PASS | editor size 1080 x 700 |
| 16 | Editor snapshots rendered | PASS | PNG files in /home/user/vocalsweetTesta/VocalSweet/test-results/editor-snapshots |

**Total: 16, passed: 16, failed: 0**
