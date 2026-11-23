# Vega Productions Drums (VP-18)


![screenshot](docs/screenshot.png)

## Download
- **Windows (FL Studio):** https://github.com/DJVega70/vega-drums/releases/latest/download/VegaProductions-Drums-Windows-VST3.zip
- **macOS:** https://github.com/DJVega70/vega-drums/releases/latest/download/VegaProductions-Drums-macOS-VST3-AU.zip

## What it is
An MPC-style beat machine in the VEGA FL49 colours, built for 80s and 90s old-school house.

![screenshot](docs/screenshot.png)

- **128 pads:** 8 banks (A-H) x 16, played from the 4x4 pads, your FLkey / MIDI keyboard, or the sequencer. Click higher on a pad to hit harder. **FULL LEVEL** plays every hit at full velocity. **16 LEVEL** spreads the selected sound across all 16 pads at 16 velocities.
- **Built-in sounds (synthesized, they ship with the plug-in):**
  - 909 / 808 / Deep drums: kick, snare, clap, rim, hats, toms, ride and shaker.
  - Old-school percussion: cowbell, congas, tambourine, crash and clave.
  - Keys: a house organ stab (M1-style drawbars with percussion and key click), a house piano stab, and a string-machine pad. Each plays chords (m7, maj7, m9, 7, m, maj, sus4).
  - Bass: a 303-style acid bass and an 808 sub bass.
- **12 kits:** 909 House, 808 Deep, Deep House, Chicago Jack 1986, Acid Trax 1987, Detroit 808 1988, NYC Garage 1989, Piano House 1991, Deep Jersey 1992, Organ Bump 1993, UK Garage 1997, French Filter 1998. Bank A holds the drums, and bank B holds organ and piano stabs, acid bass and sub bass in C, D#, F and G.
- **Your own samples:** WAV, AIFF, FLAC, MP3 and OGG, even straight out of a .zip (no need to unzip). Drag them from the browser onto a pad or a grid row, or right-click a pad and choose **Load sample...**.
- **Browser** with **80s / 90s / House / Deep House / Acid / Techno / Garage / Loops** filters. It reads your *Documents\Vega Drums Kits* folder, plus any sample packs in *Downloads* (MusicRadar SampleRadar zips, for example). Click a sample to hear it. Double-click a pack to **auto-kit** it: kicks, snares, claps and hats go to bank A by file name, and basses, pads, keys and loops go to bank B. Right-click a pack to load it into banks C+D, E+F or G+H instead.
- **Drum grid:**
  - 8 sequences of 1-8 bars.
  - Click to add a hit, drag to move it, and drag the right edge to make keys and bass notes longer.
  - Right-click or double-click to delete. Draw velocities in the lane below.
  - **DOUBLE** copies the sequence into the next bars, and **COPY TO** copies it to another sequence.
- **Live recording:** press **REC**, then play the pads or keys while the song runs. Hits snap to **Timing Correct** (1/4 to 1/32, with triplets). **NOTE REPEAT** rolls held pads at that division. Hold **ERASE** plus a pad while recording to wipe it as it passes. Press **ERASE** while stopped for the erase menu.
- **Swing** (50-75%), Accent, Humanize, a metronome, and a safety limiter on the main output.
- **Q-Links:** 16 knobs for the selected pad (level, tune, decay, tone, pan, chord, choke group, sound) plus swing, accent, humanize, master, time division, tempo, sequence length and drum machine.
- **Multi-out:** the main output plus 16 stereo outputs, one per pad position (A01/B01/... share output 1, and so on).
- **DRAG MIDI** drags the current sequence into your DAW as a MIDI clip.
- **MIDI in:** GM drum notes play bank A (36 kick, 38 snare, 39 clap, 42/46 hats, 56 cowbell...), and C4 (60) and up play B01, B02 ... E16.
- Projects made with the first Vega Drums still open: their 16-step patterns become sequences.

## Free sample packs that work well
These are free to use in your own music. Download them yourself (they can't be bundled with the plug-in), then drop the zips in *Documents\Vega Drums Kits* or leave them in *Downloads*:
- MusicRadar SampleRadar: retro house, deep house, house, 303 acid, 808, and classic house piano and strings.
- Bedroom Producers Blog: Cassette 808 / 909 / 606.

## Building
```
cmake -B build -DVEGA_TESTS=ON
cmake --build build --config Release --target VegaDrums_VST3 VegaDrumsTest
```
