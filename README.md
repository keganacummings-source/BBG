# BabyGirl 2.0 - synth, drums and a 134-machine modular FX rack (VST3)

One plugin for sound design, synthesis, drums, FX and mastering. Everything from v1 is still here
(Hush analog engine, KyotoSpxrit lofi engine, Master suite, 1176 + Pultec modules, DreamShare / DreamAPI);
v2 stacks a large amount on top.

| Part | What it is |
|---|---|
| **Machine Rack** | 16 serial slots. Each slot loads any of **134 machines** (see [MACHINES.md](MACHINES.md)) with 6 machine-specific knobs, a mix control and bypass. Rack can run before Hush or after Kyoto. |
| **FORGE** | A machine that builds a different filter - shaper - delay - pitch - diffusion chain from a seed. 10,000 seeds per slot. |
| **Synth** | 16-voice poly. 10 oscillator models (Sine, Tri, BLEP Saw, PWM Square, SuperSaw, Wavetable, FM, Organ, Pluck, Hard-Sync), unison up to 7, sub, noise, 6 filter models, 2 ADSR, LFO (5 shapes), glide, bend, mod wheel, sustain. |
| **Drums** | 16 synthesized pads (808/909 kicks, snares, clap, hats with choke, toms, cowbell, crash, ride), per-pad tune/decay/level/pan, 16-step host-synced sequencer with swing, 9 factory patterns. |
| **Visualizers** | Spectrum (4096-pt, log-freq, peak hold, slope), scrolling spectrogram, triggered oscilloscope, phosphor goniometer, BS.1770 LUFS (M/S/I) + true peak + correlation. Driven on the display's vsync (60+ fps). |
| **DreamShare** | Browse factory + cloud presets, publish the full rack/synth/drum state. Cloudflare Worker in `worker/`. |
| **Original stages** | Hush, Kyoto, Master, 1176, Pultec: all kept, all on the CORE page. They are now switchable and **off by default** (so the init patch is transparent); factory presets and the legacy five turn them on. |

Two plugins are built from the same source:

* **BabyGirl** - instrument (MIDI in; synth + drums + rack).
* **BabyGirlFX** - effect (audio in; the same rack, for DAWs that will not put an instrument in an insert slot).

## Build the VST3 (GitHub Actions - no setup)

1. Push this folder to a GitHub repository (`main`).
2. Open **Actions**. The workflow runs the DSP self-test and then builds Windows, macOS (universal) and Linux.
3. Download `BabyGirl-VST3-AllPlatforms` from the run's artifacts (one zip with all three OS builds).
4. To publish a release: `git tag v2.0.0 && git push --tags` - the zips are attached to the release automatically.

Local build: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release`
(needs CMake 3.22+, a C++20 compiler, internet for JUCE 7.0.12; on Linux also the packages listed in the workflow).
DSP-only check without JUCE: `cmake -B build -DBG_BUILD_PLUGIN=OFF && cmake --build build && ctest --test-dir build`.

Install: copy `BabyGirl.vst3` / `BabyGirlFX.vst3` to `C:\Program Files\Common Files\VST3` (Windows),
`/Library/Audio/Plug-Ins/VST3` (macOS), or `~/.vst3` (Linux), then rescan in your DAW.
macOS builds are unsigned: run `xattr -dr com.apple.quarantine BabyGirl.vst3` if Gatekeeper blocks them.

## MIDI

* Synth: all channels except 10 (default).
* Drums: channel 10, pads on notes 36-51 (C1-D#2). The drum map is switchable (channel 10 only / notes 36-51 on all channels / both).
* Mod wheel (CC1) scales LFO depth, pitch bend +-2 st, sustain pedal (CC64), all-notes-off.

## DreamAPI

`worker/` is the Cloudflare Worker. Deploy with `npx wrangler deploy`; to persist community uploads create a KV
namespace and uncomment the binding in `wrangler.toml`. Point the plugin at your own worker by changing the default
endpoint in `Source/DreamApi.h`. Presets are stored as lists of non-default parameters (machines by name, so the
catalog can grow without breaking old presets).

## Tests

`tests/dsp_selftest.cpp` (pure C++, no JUCE) renders every machine at 44.1/48/96 kHz under six parameter scenarios
(defaults, all-min, all-max, mid, pseudo-random, mid-stream parameter jump) and fails on NaN/inf, runaway level or
silent output. It also checks every synth oscillator x filter pair, voice stealing, all 16 drums, sequencer step
counts, the Hilbert pair, and BS.1770 loudness (a -23 dBFS stereo 997 Hz sine must read -23 +-0.3 LUFS).

## Honest notes

* Hardware names describe the character being approximated. The DSP is original; it is not a circuit-level clone of any product.
* Pitch machines use granular shifting (a few tens of ms latency, no PDC reported). The Pitch Snap is a lightweight monophonic detector, not a studio-grade tuner.
* The editor is JUCE software-rendered; if a particular DAW's UI scaling is slow, shrink the window or hide the Scopes tab.
* Time-based machines are free-running (not tempo-synced) in this release.

## Credits

Original BabyGirl / Hush / KyotoSpxrit concept and v1 code by Kegana Cummings. Built with JUCE 7 (GPLv3 / commercial; see JUCE licensing before distributing binaries).
