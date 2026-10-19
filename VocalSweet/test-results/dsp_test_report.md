# VocalSweet DSP test report

| # | Test | Result | Measurement |
|---|---|---|---|
| 1 | Silence in -> silence out (all presets) | PASS | peak output = 0.0 for all 9 presets (2 s stereo) |
| 2 | Very low level (-100 dBFS sine) stays stable & quiet | PASS | output RMS -97.5 dBFS (input -103.0 dBFS RMS) |
| 3 | 1 kHz sine -18 dBFS through default chain | PASS | in -21.1 dBFS RMS -> out -19.3 dBFS RMS |
| 4 | Impulse response finite and decaying (EQ +/-9 dB) | PASS | energy after 0.5 s = 5.67e-83 of total |
| 5 | All modules off = transparent (delayed by latency only) | PASS | max |error| = 0.00e+00 |
| 6 | Broadband pink noise through all presets | PASS | max output peak -1.00 dBFS (ceiling -1.0) [Default] |
| 7 | White noise -9 dBFS RMS (Afrobeat Lead) | PASS | output peak -1.00 dBFS |
| 8 | Mono == stereo(L) for identical L/R input | PASS | max mono/stereo diff 0.00e+00, L/R diff 0.00e+00 |
| 9 | Vocal Double widens a centred source | PASS | L/R correlation 0.8524 |
| 10 | Doubler on mono bus is stable | PASS | mono peak -12.22 dBFS |
| 11 | 7 sample rates x 9 buffer sizes (22.05k-192k, 1-4096) | PASS | 63 runs stable, peak <= ceiling |
| 12 | Block-size invariance (block 1 vs 4096) | PASS | max difference 0.00e+00 |
| 13 | Latency @ 44.1 kHz | PASS | measured 44 samples = 0.998 ms (reported 44) |
| 14 | Latency @ 48.0 kHz | PASS | measured 48 samples = 1.000 ms (reported 48) |
| 15 | Latency @ 96.0 kHz | PASS | measured 96 samples = 1.000 ms (reported 96) |
| 16 | HPF response (12/24 dB) matches design | PASS | 12dB: -21.0 dB @30Hz, 24dB: -41.9 dB @30Hz, max dev 0.000 dB |
| 17 | EQ + Tone response == EQ display curve | PASS | 300 Hz -6.57 dB, 3500 Hz +6.93 dB, 11000 Hz +5.17 dB, max dev 0.000 dB |
| 18 | Compressor GR: -6 dBFS sine, thr -20, 4:1 | PASS | measured GR 9.73 dB (static peak-based 10.5 dB), meter 9.74 dB, auto makeup 3.60 dB |
| 19 | Compressor below threshold: no gain reduction | PASS | GR 0.000 dB at -30 dBFS |
| 20 | Compressor attack time (10 ms setting) | PASS | 63% of final GR after ~15 ms |
| 21 | Saturation drive 0 adds no harmonics | PASS | H2 -166.8 dB, H3 -171.3 dB |
| 22 | Saturation drive 60%: controlled harmonics | PASS | H2 -33.8 dB, H3 -20.2 dB, level change -2.98 dB |
| 23 | Warmth adds even harmonics (asymmetry), DC blocked | PASS | H2 -22.8 dB, DC -1.0e-07 |
| 24 | Saturation aliasing (ADAA) at 7 kHz, drive 100% | PASS | 13 kHz alias component -62.1 dB rel. fundamental |
| 25 | De-esser reduces sibilant band | PASS | 7 kHz band level change -9.91 dB, GR meter 10.00 dB |
| 26 | De-esser leaves 300 Hz vowel tone untouched | PASS | level change 0.000 dB, GR 0.00 dB |
| 27 | Parameter min / max / 150 random combinations | PASS | 152 runs finite, limiter ceiling respected |
| 28 | Macros at zero = detailed settings exactly | PASS | effective == base for EQ, comp, de-ess, saturation |
| 29 | Dry/Wet 0% = original (latency aligned) | PASS | max |error| 0.00e+00 |
| 30 | Dry/Wet 50% = linear blend of dry and wet | PASS | max |error| 7.45e-09 |
| 31 | Bypass = bit-exact delayed input | PASS | max |error| 0.00e+00 |
| 32 | Bypass toggle is click-free (15 ms crossfade) | PASS | max sample step during switch 0.0115 (processed 0.0053, dry 0.0115) |
| 33 | NaN / Inf / 1e30 / denormal input injection | PASS | all output finite, recovers (tail RMS -21.5 dBFS), watchdog resets 0 |
| 34 | +12 dBFS input, limiter on, ceiling -1 dBFS | PASS | output peak -1.000 dBFS |
| 35 | Limiter off: output passes level (no hidden clipping) | PASS | output peak 10.81 dBFS |
| 36 | Hot synthetic vocal (+3.5 dB over) stays under ceiling | PASS | output peak -6.370 dBFS |
| 37 | Abrupt preset / Sweeten jumps are smoothed (no clicks) | PASS | max sample step 0.0109 (limit 0.0324) |
| 38 | SWEETEN 0 -> 100: clearer, airier, more controlled | PASS | clarity +5.0 dB, air +4.7 dB, sibilance +0.1 dB, comp GR 3.0 -> 5.5 dB |
| 39 | 8 named presets are measurably distinct | PASS | closest pair differs by 1.59 dB (Afrobeat Lead / Afrofusion Lead) |
| 40 | Performance: stereo 48 kHz, all modules + doubler | PASS | 20.0 s audio in 0.499 s = 2.49% of one core (40x real-time) |

**Total: 40, passed: 40, failed: 0**
