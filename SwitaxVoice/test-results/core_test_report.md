# SwitaxVoice core tests

| # | Test | Result | Measurement |
|---|---|---|---|
| 1 | EBU 3341 case 1: -23 dBFS sine -> -23.0 LUFS (M, S, I) | PASS | I -22.993, M -22.993, S -22.993 |
| 2 | EBU 3341 case 2: -33 dBFS sine -> -33.0 LUFS | PASS | I -32.993 |
| 3 | EBU 3341 case 3: relative gate (-36/-23/-36) -> -23.0 | PASS | I -23.014 |
| 4 | EBU 3341 case 4: absolute + relative gate -> -23.0 | PASS | I -23.014 |
| 5 | EBU 3342 LRA: -20 / -30 dBFS -> 10 LU | PASS | LRA 10.00 LU |
| 6 | EBU 3342 LRA: -20 / -15 dBFS -> 5 LU | PASS | LRA 5.00 LU |
| 7 | EBU 3342 LRA: -40 / -20 dBFS -> 20 LU | PASS | LRA 20.00 LU |
| 8 | Loudness at 22.05-192 kHz within 0.1 LU | PASS | 22050:-22.96 44100:-22.99 88200:-23.01 96000:-23.01 192000:-23.02  |
| 9 | Mono channel (G = 1.0): -20 dBFS sine -> -23.0 LUFS | PASS | I -23.004 |
| 10 | K-weighting response (1 kHz +0.7 dB, 10 kHz +4.0 dB, 30 Hz < -6 dB) | PASS | 0.698, 4.042, -8.304 dB |
| 11 | True peak: fs/4 sine at 45 deg (sample peak -3.01 dB) -> 0 dBTP | PASS | sample -3.01 dB, true -0.05 dBTP |
| 12 | True peak: 997 Hz sine at -6 dBFS -> -6.0 dBTP | PASS | -6.000 dBTP |
| 13 | Spectrum: pink noise (Kellet approximation) flat within +/-2 dB, 63 Hz-16 kHz | PASS | spread 3.34 dB |
| 14 | Spectrum calibration: band sum = 20 Hz-20 kHz share of mean square | PASS | bands -11.43 dB, expected -11.33 dB |
| 15 | Spectrum: 0 dBFS 1 kHz sine -> -3.01 dB in its band | PASS | -3.04 dB at 959 Hz band |
| 16 | Pitch (YIN): harmonic tones 98-660 Hz within 1 % | PASS | 98->98.0 196->196.0 440->440.2 660->660.4  |
| 17 | Pitch (YIN): white noise is reported unvoiced | PASS | voiced 0 % |
| 18 | Activity gating: 5 s tone + 5 s silence -> 5 s active | PASS | 5.00 s |
| 19 | Clipping: hard-clipped sine detected, clean -0.5 dBFS sine not | PASS | clipped 880 runs, clean 0 |
| 20 | Clipping: flat tops below full scale (pre-gain clipping) detected | PASS | 880 flat-top runs |
| 21 | Stereo correlation: identical +1, inverted -1, independent ~0 | PASS | 1.000 / -1.000 / -0.002, inverted mono loss -200 dB |
| 22 | Sibilance: strong 's' bursts detected; none when absent | PASS | 10 events with, 0 without |
| 23 | Sibilance: tonal balance excludes sibilant frames | PASS | 36 frames skipped (with), 0 (without) |
| 24 | Chain latency = 1 ms look-ahead | PASS | 48 samples |
| 25 | Chain: silence in -> silence out (below -180 dBFS) | PASS | peak 3.82e-18 |
| 26 | Chain: block size 512 vs 37 identical output | PASS | max diff 0.00e+00 |
| 27 | Chain: bypass is bit-exact (latency compensated) | PASS | all samples identical |
| 28 | Chain: level-matched compare gain on the bypass path | PASS | -6.00 dB |
| 29 | Chain: EQ display curve matches measured response | PASS | max error 0.000 dB |
| 30 | Chain: 60 random parameter sets + NaN/Inf input stay finite | PASS | all outputs finite |
| 31 | Chain CPU (stereo 48 kHz, all modules): < 10 % of one core | PASS | 5.05 % of real time |
| 32 | Advisor: silence -> no recommendations, explains why | PASS | Only 0.0 s of signal was captured. Play at least 5 seconds of the part and try again. |
| 33 | Advisor: clean voice vs its own reference -> no recommendations (no false positives) | PASS | none |
| 34 | Advisor: +8 dB mud at 300 Hz -> low-mid cut near 300 Hz, at most 4 dB | PASS | 300 Hz, -4.0 dB; 150 Hz-500 Hz versus target: +4.5 dB -> +2.1 dB (all tone proposals applied). |
| 35 | Advisor: +9 dB resonance at 3.2 kHz -> harsh cut near 3.2 kHz | PASS | 3400 Hz, -4.0 dB |
| 36 | Advisor: rumble -> high-pass below the lowest sung note | PASS | HPF 140 Hz (lowest note 196 Hz) |
| 37 | Advisor: strong sibilance -> de-esser proposal with simulated reduction | PASS | Sibilant peaks relative to the body: +7.3 dB -> +0.8 dB. / High end between sibilants changes by +0.0 dB (kept above -2 dB so vowels stay open). |
| 38 | Advisor: +/-7 dB phrase swings -> compression, bounded gain reduction | PASS | Momentary loudness range 16.6 dB -> 9.5 dB. / Average gain reduction while singing 0.0 dB -> 5.9 dB. |
| 39 | Advisor: heavily limited source -> no extra compression proposed | PASS | lowmid,box,air,level |
| 40 | Advisor: clipped recording -> clipping note (information only, no parameter changes) | PASS | 96 runs of 3+ consecutive samples at full scale (>= -0.004 dBFS); first at 0:02.02, 0:02.02, 0:02.02, 0:02.02, 0:02.03, 0:02.03. |
| 41 | Advisor: polarity-inverted stereo -> mono-compatibility warning | PASS | lowend,harsh,airx,stereo |
| 42 | Advisor: full-mix context -> no vocal processing changes, offers analysis-only mode | PASS | mixonly,mixtone,mixloud |
| 43 | Advisor: sidechain beat -> vocal-vs-beat level check uses it | PASS | beat |
| 44 | Advisor: applying the plan reduces the total deviation from the target | PASS | sum |dev| 65 -> 41 dB |
| 45 | Advisor bounds (8 random voices x 6 targets): cuts <= 4 dB, boosts <= 3 dB, GR <= 6.5 dB | PASS | 48 analyses within bounds |
| 46 | Real voice: sung take vs its own reference -> no recommendations | PASS | none |
| 47 | Real voice: +7 dB at 300 Hz injected -> found within 1/3 octave | PASS | 300 Hz |
| 48 | Real voice: +8 dB at 3.5 kHz injected -> found within 1/3 octave | PASS | 3400 Hz |
| 49 | Real voice: speech has real sibilants detected, not flagged as excessive | PASS | 26 events, air,comp,gain,box,presence,level |
| 50 | Real voice: sung take vs Afrobeat house target -> bounded proposals | PASS | box,air,comp,level |

**Total: 50, passed: 50, failed: 0**
