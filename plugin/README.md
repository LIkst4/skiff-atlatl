<p align="center"><img src="../docs/logo/atlatl-icon.svg" alt="" width="96"></p>

# ATLATL FM-1 (the plugin)

The FM-1 on skiff-atlatl firmware, in your DAW. Each instance of the plugin is one FM-1 track:

- **The editor inside the plugin window.** It is the web editor (`web/editor.html`), built in. Everything it does
  works here too: sounds, the sequencer, the song, the mixer, samples, backups, and the FM-1's screen live in the
  **Mirror** tab. It needs no browser and no WebMIDI; it talks through the plugin. (The editor's **Screen** tab, which
  restarts the FM-1 when it starts, is left out of the plugin, so a DAW session never restarts the unit.)
- **Automatable parameters.** All of the track's parameters (about 160, STACK and SYNTH's FILTER 2 among them) are
  plugin parameters, grouped as on the device (ENV, LFO, FX, SCL, EDIT, ARP, SEQ, DRUM). Bitwig also gets them as
  remote-control pages.
  - Moving one in the DAW (by hand or by automation) sets it on the FM-1.
  - Turning a knob on the FM-1 moves it in the DAW, so the DAW can record it as automation.
  - E1–E8 are the engine's own parameters; what each one does depends on the track's engine.
- **The sound is saved with the DAW project.** When you open the project, the plugin sends its saved values to the
  FM-1 track.
  - It only does this if the track has the same engine as when you saved.
  - If the engine is different, nothing is sent and the bar says so.
- **Notes.** The notes and MIDI (CCs, pitch bend, pressure) on the plugin's track go to the FM-1 on that track's
  channel: T1 is channel 1 … T6 is channel 6 (MIDI IN ROUTE CH1-6, the default).
- **Clock and transport.** One instance (tick **CLOCK**) sends the DAW's MIDI clock, start/stop and song position.
  - Set the FM-1's CLOCK to USB so its sequencer, arpeggiator and delays follow the DAW.
  - Loops and jumps are sent as Stop, Song Position, Continue.

## Two modes (MODE on the bar)

### DIRECT

The FM-1's sound reaches the DAW through its USB audio input ("Felucca", 44.1 or 48 kHz), live, with no added latency.
There are two ways to listen to it:

- **An audio track** in the DAW whose input is the FM-1, with monitoring on.
- **The plugin's own input.** The plugin has a stereo input ("FM-1 in"). With **MONITOR** on, it passes that input to
  its output, so the FM-1 plays on the instrument track itself. In Bitwig, set the plugin's input to the FM-1's audio
  input.

### CAPTURE

The plugin opens the FM-1's 8-channel USB audio input itself. Each instance plays its own track's sound, lined up with
the DAW's timeline, on its own instrument track: six tracks, six channels, each with its own effects, mixing and
recording in the DAW.

- **SOURCE TRACK** plays the track's own channel (3–8 for T1–T6). That signal comes after the track's level, pan,
  DRIVE, INSERT, FILTER and DUCK, but before the shared delay, reverb and chorus and before the master. It is mono, on
  both sides.
- **SOURCE MAIN** plays the master (channels 1–2), with everything. Use it on one instance for the whole mix.
- **Latency compensation.** The instance reports a fixed latency to the DAW, and the DAW plays it that much earlier.
  As a result, the FM-1's sound lands on the beat: the note at bar 5 sounds at bar 5, in playback and when recorded.
  - The latency is the round trip (a note out, its sound back) plus room for one audio block and the USB callbacks.
  - The LATENCY field can fix it by hand; leave it empty for AUTO.
- **MEASURE** times the round trip on your system. It plays C4 on the instance's track and finds the sound coming
  back. Use a sound with a quick attack, and don't mute the track.
  - Measure again after changing the DAW's buffer size or the audio driver.
  - The DAW restarts the plugin so the new latency takes effect.
- **Two clocks, no drift.** The FM-1's USB audio and your audio interface run on different clocks. The plugin follows
  both and resamples the FM-1 to the DAW's rate continuously, so the sound never drifts away from the beat. In the
  tests, two clocks 100 ppm apart stay within a few samples (0.1 ms) after two minutes, and a 997 Hz tone comes through clean
  (76 dB) with a timing wander of about 2 µs.
- **Bouncing / exporting** runs in real time, because the FM-1 has to play the part. The plugin tells the DAW so. In a
  faster-than-real-time render it outputs silence.
- Don't also listen to the FM-1's input in the DAW (DIRECT) while capturing, or you will hear it twice.
- **macOS:** the DAW needs microphone access (System Settings › Privacy & Security › Microphone) to read any audio
  input, the FM-1 included.
- **Windows:** the plugin uses the FM-1 through WASAPI in shared mode. If the DAW's ASIO driver has the FM-1 open, the
  plugin can't use it. Use your regular interface for the DAW's output; the FM-1 doesn't need to be the DAW's device.

## The bar

| Control | What it does |
| --- | --- |
| **TRACK** | The FM-1 track this instance plays and edits. |
| **NOTES** | Sends the DAW's notes and MIDI for this track to the FM-1. |
| **CLOCK** | Sends the clock and transport. Turn it on in one instance only. |
| **MONITOR** | (DIRECT) Passes the plugin's audio input to its output. |
| **MODE** | DIRECT or CAPTURE (above). |
| **SOURCE** | (CAPTURE) TRACK: the track's own channel. MAIN: the master. |
| **INPUT** | (CAPTURE) The FM-1's audio input. AUTO picks the one named Felucca / FM-1. |
| **LATENCY** | (CAPTURE) The latency reported, in ms. Empty is AUTO (the round trip plus room). |
| **MEASURE** | (CAPTURE) Times the round trip. The readout shows it, the latency, and the input's channels and rate. |
| **OFFSET** | Moves the notes and the clock earlier or later than the DAW, by up to ±50 ms. |
| **PORT** | The FM-1's MIDI port. AUTO picks the first one named Felucca / FM-1. |
| **Status** | Shows whether the FM-1 is connected, its port, its firmware version, and any warnings. |

## Install

Download the zip for your system from the **plugin** workflow's artifacts on GitHub (Actions › plugin › the latest run).

| | CLAP | VST3 | AU | Standalone |
| --- | --- | --- | --- | --- |
| **Windows** | `%COMMONPROGRAMFILES%\CLAP` | `%COMMONPROGRAMFILES%\VST3` | — | `ATLATL FM-1.exe` |
| **macOS** | `~/Library/Audio/Plug-Ins/CLAP` | `~/Library/Audio/Plug-Ins/VST3` | `~/Library/Audio/Plug-Ins/Components` | `ATLATL FM-1.app` |
| **Linux** | `~/.clap` | `~/.vst3` | — | `ATLATL FM-1` |

- **macOS:** the build is not notarized. After copying, run
  `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/*/ATLATL*`.
- **Windows:** the window uses Microsoft Edge WebView2, which comes with Windows 11 and current Windows 10.
- **Linux:** the window needs GTK 3 and WebKitGTK 4.1 (`libwebkit2gtk-4.1-0`). The CLAP opens its window
  floating; the VST3 opens inside the DAW's.

## Things to know

- **One MIDI port, shared.** The plugin's instances share one connection to the FM-1. They must run in the same
  process.
  - In Bitwig, set the plug-in hosting mode for this plugin to *Within Bitwig*, *Together* or *By Manufacturer*, not
    *By Plug-in* or *Individually*.
  - Other DAWs run all plugins in one process by default.
- **Windows: a MIDI port has one user at a time.** Turn the FM-1 off in the DAW's own MIDI settings so the plugin can
  open it. Close the web editor in the browser too.
- **macOS and Linux** share the port with other programs. Still, don't send the DAW's own MIDI clock to the FM-1
  while the plugin sends its own.
- The editor and the plugin take turns on the port, because the FM-1 answers one request at a time. Large transfers
  (backups, samples) slow the automation down while they run.
- The parameters follow this firmware's layout. With other firmware on the FM-1, the bar warns you, and only the
  editor and the notes work.

## Build

```sh
cmake -S plugin -B build/plugin -DCMAKE_BUILD_TYPE=Release
cmake --build build/plugin --config Release
ctest --test-dir build/plugin -C Release     # the link (a fake FM-1) and CAPTURE (simulated clocks)
```

- **Dependencies** (fetched from GitHub when you configure): [CLAP](https://github.com/free-audio/clap) and
  [clap-wrapper](https://github.com/free-audio/clap-wrapper) (MIT; the VST3 SDK, MIT since 3.8, and Apple's AudioUnit
  SDK, Apache-2.0, come through it), [CHOC](https://github.com/Tracktion/choc) (ISC; its WebView) and [RtMidi](https://github.com/thestk/rtmidi)
  (MIT-like), [miniaudio](https://github.com/mackron/miniaudio) (public domain or MIT-0; CAPTURE's audio input).
- **Linux** also needs `libgtk-3-dev libwebkit2gtk-4.1-dev libasound2-dev`.
- **After a parameter is added to the firmware,** regenerate `plugin/src/gen/params.h` with
  `plugin/tools/gen_params.py` (see its header). The host tests check that it matches the firmware.
