# SwitaxVoice Assist on real recordings

Material: VocalSet (Wilkins et al. 2018, CC BY 4.0), 13 clips: singer f1 sung (arpeggios, scales, long tones; belt, breathy, straight, vibrato, forte) and three singers speaking.
Default SwitaxVoice settings unless noted. These reports are produced by the same code the plugin runs.

## Real sung voice vs its own reference (neutral settings)

**No issues beyond tolerance against Reference: self. Your current settings already fit this target.**

Raw: -18.8 LUFS, level range 23.2 dB, crest 18.1 dB, f0 256-732 Hz (95 % voiced), sibilant events 0, noise floor -71.9 dBFS.

## Real sung voice with +7 dB at 300 Hz injected, vs the original

**1 proposal and 0 notes against Reference: original.**

Raw: -18.2 LUFS, level range 22.1 dB, crest 18.2 dB, f0 256-704 Hz (98 % voiced), sibilant events 0, noise floor -71.5 dBFS.

### [Tone] Low-mid build-up (muddiness)  (confidence 55 %)
- **MEASURED**: 150 Hz-500 Hz averages +4.1 dB versus the target (tolerance +/-3.0 dB); largest at 302 Hz (+6.2 dB).
- **HEURISTIC**: Too much 150-500 Hz energy makes a vocal sound muddy or 'woolly' and masks the beat's low-mids (heuristic).
- **PROPOSAL**: Gentle cut at the measured frequency (kept small on purpose).  [Low-Mid Frequency 250 Hz -> 300 Hz; Low-Mid Gain +0.0 dB -> -4.0 dB]
- **SIMULATED**: 150 Hz-500 Hz versus target: +4.1 dB -> +1.6 dB (all tone proposals applied).
- *Limitations*: Compared with your reference; mic, singer and arrangement differences are normal. Trust your ears.

## Real sung voice with +8 dB at 3.5 kHz injected, vs the original

**1 proposal and 0 notes against Reference: original.**

Raw: -18.1 LUFS, level range 23.0 dB, crest 18.3 dB, f0 256-723 Hz (88 % voiced), sibilant events 0, noise floor -71.9 dBFS.

### [Tone] Harsh upper-mid resonance  (confidence 80 %)
- **MEASURED**: 2.0 kHz-5.0 kHz averages +3.3 dB versus the target (tolerance +/-3.0 dB); largest at 3.4 kHz (+6.0 dB).
- **MEASURED**: At 3.4 kHz the spectrum stands 8.9 dB above its local trend in 93 % of active frames (a persistent resonance rather than moving notes).
- **HEURISTIC**: A persistent peak in 2-5 kHz is perceived as harshness or 'ear fatigue', especially at high playback levels (heuristic).
- **PROPOSAL**: Gentle cut at the measured frequency (kept small on purpose).  [Harsh Frequency 3.2 kHz -> 3.4 kHz; Harsh Gain +0.0 dB -> -4.0 dB]
- **SIMULATED**: 2.0 kHz-5.0 kHz versus target: +3.3 dB -> +1.4 dB (all tone proposals applied).
- *Limitations*: Compared with your reference; mic, singer and arrangement differences are normal. Trust your ears.

## Three real speakers vs Afrobeat Lead house target (default settings)

**6 proposals and 0 notes against Afrobeat Lead.**

Raw: -36.5 LUFS, level range 17.0 dB, crest 19.4 dB, f0 186-287 Hz (81 % voiced), sibilant events 26, noise floor -66.2 dBFS.

### [Tone] Lacks air  (confidence 51 %)
- **MEASURED**: 8.0 kHz-16.0 kHz averages -7.5 dB versus the target (tolerance +/-3.5 dB).
- **HEURISTIC**: Too little energy above 8 kHz sounds dull or distant compared with modern Afro vocals (heuristic).
- **PROPOSAL**: Moderate boost (limited so it cannot turn harsh; the de-esser check below runs with it applied).  [Air Gain +0.0 dB -> +3.0 dB]
- **SIMULATED**: 8.0 kHz-16.0 kHz versus target: -7.5 dB -> -6.3 dB (all tone proposals applied).
- *Limitations*: Boosting cannot add what the microphone did not capture; a dull mic or a distant take may need re-recording rather than EQ.

### [Dynamics] Level jumps between words and phrases  (confidence 72 %)
- **MEASURED**: Momentary loudness range 13.4 dB (10th-95th percentile of 400 ms blocks); target 5.0-8.5 dB.
- **MEASURED**: Compressor currently reduces 0.9 dB on average while you sing (3.6 dB on the loudest 5 %).
- **HEURISTIC**: Large level differences make quiet words disappear under the beat while loud words jump out (heuristic for this style).
- **PROPOSAL**: More level control from the compressor (bounded so it never averages more than about 6 dB of gain reduction).  [Comp Ratio 2.5:1 -> 3.0:1; Comp Threshold -18.0 dB -> -30.5 dB]
- **SIMULATED**: Momentary loudness range 13.4 dB -> 8.8 dB.
- **SIMULATED**: Average gain reduction while singing 0.9 dB -> 5.9 dB.
- **SIMULATED**: Stopped short of the target to avoid over-compression. Ride the remaining phrase-level differences with clip gain or fader automation.
- *Limitations*: The loudness range includes deliberate dynamics (soft verses, belted hooks). If the variation is musical, keep it.

### [Level] Input level is low for the processors  (confidence 74 %)
- **MEASURED**: Input integrated loudness -36.5 LUFS; sample peak -17.0 dBFS.
- **HEURISTIC**: The compressor, de-esser and saturation are calibrated for vocals around -20 LUFS; at this level they barely react.
- **PROPOSAL**: Raise Input Gain (the level-match step below compensates the output).  [Input Gain +0.0 dB -> +16.5 dB]
- **SIMULATED**: Level into the processors: about -20.0 LUFS.
- *Limitations*: Gain staging only; it does not change tone. Leaves headroom: peaks after gain stay below 0 dBFS inside the plugin's float path.

### [Tone] Boxy / nasal midrange  (confidence 60 %)
- **MEASURED**: 500 Hz-1.5 kHz averages +4.6 dB versus the target (tolerance +/-3.5 dB); largest at 604 Hz (+8.2 dB).
- **HEURISTIC**: Excess 500 Hz-1.5 kHz often comes from a small room or close mic and sounds boxy or nasal (heuristic).
- **PROPOSAL**: Gentle cut at the measured frequency (kept small on purpose).  [Box Frequency 800 Hz -> 605 Hz; Box Gain +0.0 dB -> -3.5 dB]
- **SIMULATED**: 500 Hz-1.5 kHz versus target: +4.6 dB -> +2.8 dB (all tone proposals applied).
- *Limitations*: Compared with a house target. Voices differ: a deep or breathy voice can legitimately sit outside it.

### [Tone] Lacks presence  (confidence 35 %)
- **MEASURED**: 2.0 kHz-5.0 kHz averages -4.5 dB versus the target (tolerance +/-3.5 dB).
- **HEURISTIC**: Too little 2-5 kHz energy pushes a vocal behind the beat, especially against dense Afrobeat percussion (heuristic).
- **PROPOSAL**: Moderate boost (limited so it cannot turn harsh; the de-esser check below runs with it applied).  [Presence Frequency 4.0 kHz -> 4.8 kHz; Presence Gain +0.0 dB -> +2.0 dB]
- **SIMULATED**: 2.0 kHz-5.0 kHz versus target: -4.5 dB -> -3.1 dB (all tone proposals applied).
- *Limitations*: Boosting cannot add what the microphone did not capture; a dull mic or a distant take may need re-recording rather than EQ.

### [Level] Processing makes the vocal louder  (confidence 85 %)
- **SIMULATED**: Integrated loudness -36.5 LUFS in, -22.0 LUFS out with all proposals (+14.5 LU).
- **HEURISTIC**: Louder almost always sounds 'better' at first. Matching the level lets you judge the processing honestly and keeps your fader where it was.
- **PROPOSAL**: Level-match the output (apply this last).  [Output Gain +0.0 dB -> -14.5 dB]
- **SIMULATED**: Output loudness about -36.5 LUFS (same as the input).
- *Limitations*: Assumes all proposals above are accepted. The limiter can make the final value differ by a fraction of a dB.

## Real sung voice vs Afrobeat Lead house target (default settings)

**4 proposals and 0 notes against Afrobeat Lead.**

Raw: -18.8 LUFS, level range 23.2 dB, crest 18.1 dB, f0 256-732 Hz (95 % voiced), sibilant events 0, noise floor -71.9 dBFS.

### [Tone] Boxy / nasal midrange  (confidence 70 %)
- **MEASURED**: 500 Hz-1.5 kHz averages +7.4 dB versus the target (tolerance +/-3.5 dB); largest at 678 Hz (+11.1 dB).
- **HEURISTIC**: Excess 500 Hz-1.5 kHz often comes from a small room or close mic and sounds boxy or nasal (heuristic).
- **PROPOSAL**: Gentle cut at the measured frequency (kept small on purpose).  [Box Frequency 800 Hz -> 680 Hz; Box Gain +0.0 dB -> -3.5 dB]
- **SIMULATED**: 500 Hz-1.5 kHz versus target: +7.4 dB -> +6.8 dB (all tone proposals applied).
- *Limitations*: Compared with a house target. Voices differ: a deep or breathy voice can legitimately sit outside it.

### [Tone] Lacks air  (confidence 49 %)
- **MEASURED**: 8.0 kHz-16.0 kHz averages -15.2 dB versus the target (tolerance +/-3.5 dB).
- **HEURISTIC**: Too little energy above 8 kHz sounds dull or distant compared with modern Afro vocals (heuristic).
- **PROPOSAL**: Moderate boost (limited so it cannot turn harsh; the de-esser check below runs with it applied).  [Air Gain +0.0 dB -> +3.0 dB]
- **SIMULATED**: 8.0 kHz-16.0 kHz versus target: -15.2 dB -> -12.4 dB (all tone proposals applied).
- *Limitations*: Boosting cannot add what the microphone did not capture; a dull mic or a distant take may need re-recording rather than EQ.

### [Dynamics] Level jumps between words and phrases  (confidence 72 %)
- **MEASURED**: Momentary loudness range 19.6 dB (10th-95th percentile of 400 ms blocks); target 5.0-8.5 dB.
- **MEASURED**: Compressor currently reduces 0.5 dB on average while you sing (3.3 dB on the loudest 5 %).
- **HEURISTIC**: Large level differences make quiet words disappear under the beat while loud words jump out (heuristic for this style).
- **PROPOSAL**: More level control from the compressor (bounded so it never averages more than about 6 dB of gain reduction).  [Comp Ratio 2.5:1 -> 3.0:1; Comp Threshold -18.0 dB -> -35.0 dB]
- **SIMULATED**: Momentary loudness range 19.6 dB -> 12.5 dB.
- **SIMULATED**: Average gain reduction while singing 0.5 dB -> 6.0 dB.
- **SIMULATED**: Stopped short of the target to avoid over-compression. Ride the remaining phrase-level differences with clip gain or fader automation.
- *Limitations*: The loudness range includes deliberate dynamics (soft verses, belted hooks). If the variation is musical, keep it.

### [Level] Processing makes the vocal quieter  (confidence 85 %)
- **SIMULATED**: Integrated loudness -18.8 LUFS in, -22.6 LUFS out with all proposals (-3.8 LU).
- **HEURISTIC**: Louder almost always sounds 'better' at first. Matching the level lets you judge the processing honestly and keeps your fader where it was.
- **PROPOSAL**: Level-match the output (apply this last).  [Output Gain +0.0 dB -> +4.0 dB]
- **SIMULATED**: Output loudness about -18.8 LUFS (same as the input).
- *Limitations*: Assumes all proposals above are accepted. The limiter can make the final value differ by a fraction of a dB.

