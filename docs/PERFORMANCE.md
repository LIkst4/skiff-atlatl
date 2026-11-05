# Performance mode: a guide

*skiff-atlatl on the M-VAVE FM-1 · [versión en español](PERFORMANCE.es.md)*

skiff-atlatl is made to be played live, like a hardware groovebox. Patterns are queued and switch on the beat. Mutes and scenes land
together. Knob "throws" spring back by themselves. A hold-to-play effects layer covers the master. Trig conditions,
chance and parameter locks keep a loop moving without you touching it.

This guide covers where everything is and how to use it.

## The controls this guide uses

- **Layers.** Hold a page button (FX, GLO, ARP, SEQ ...) and its *layer* opens over the screen: the keys and the
  four knobs become that layer's controls while you hold it. **SHIFT + the button** (HOME held, then the button)
  keeps the layer open (the header says LOCKED); press the button again to close it.
- **Tracks.** The six tracks are T1–T6. **ALGORITHM** selects the track on every page.
- **The black keys F#3 G#3 A#3 C#4 D#4 F#4** are always the six tracks' mutes in the GLO, FX and PERFORM layers.

---

## 1. Open PERFORM

**Hold ARP.** The PERFORM map shows a row per track with its list entries (A, B, C ...), the one playing boxed and
the one waiting marked, and a row of scenes (SCN).

To play hands free, lock it: **SHIFT + ARP**. Press ARP again to leave.

While PERFORM is open:

| Control | What it does |
| --- | --- |
| **SELECT** (turn) | the **LAUNCH** point: **LOOP** (the track's pass end), **BAR**, **BEAT** or **NOW** |
| **PRESETS** (turn) | the **KEYS**: **TRACK** (the white keys are the selected track's entries) or **SCENE** (they are the 16 scenes) |
| **White keys** | launch an entry (KEYS TRACK) or recall a scene (KEYS SCENE), at the LAUNCH point |
| **Black keys** | the tracks' mutes (below) |
| **KNOB 1–4** | throws (below) |
| **OCT−** | cancels what is staged and not yet launched |
| **OCT+** | keeps the throws where they are instead of letting them glide back |

## 2. Launch patterns (KEYS TRACK)

Each track has a list of 16 entries, **A–P**. Each entry is a pattern and how many times it repeats. You build the
lists on **SEQ › SONG**.

1. Pick the track with **ALGORITHM**.
2. Set **LAUNCH** with SELECT. BAR is the usual choice, and NOW works for stutters.
3. Press a white key: F3 is entry A, G3 is B, and so on. The entry is *staged*: it blinks on the map and switches
   at the launch point, so the change lands on time.

Play stays on while you switch. With **SONG ON** each track moves down its own list by itself. With **SONG OFF** each
track loops the entry you launched.

## 3. Scenes (KEYS SCENE)

A scene stores, for all six tracks at once, the entry each one is on and every mute.

- **Store:** set up the tracks the way you want, then hold **SAVE** and press a white key.
- **Recall:** press the key. Everything is staged and lands together at the LAUNCH point, like a pattern change on
  the whole box.

The 16 scenes are kept with the project.

## 4. Mutes

The black keys F#3 to F#4 are the mutes for T1 to T6, the same in GLO, FX and PERFORM.

- **Tap** a black key to mute or unmute the track. In PERFORM this waits for the LAUNCH point, so mutes land on the
  grid. In GLO and FX it happens at once.
- **Hold** a black key to cut the track only while you hold it.

## 5. Throws (KNOB 1–4 in PERFORM)

Turn a knob in PERFORM and it pushes one of the selected track's values away for a moment:

| Knob | Throw |
| --- | --- |
| KNOB 1 | the tone: CUTOFF, or the engine's main tone control (on SYNTH, SHAPE A) |
| KNOB 2 | REVERB send |
| KNOB 3 | DELAY send |
| KNOB 4 | DIST |

Let go and the value glides back over the time set in **MENU › CONTROL › RETURN**: **SNAP**, **1 BEAT** or **1 BAR**.
Press **OCT+** to keep it where it is instead. Throws are never saved and ride on top of automation and locks.

## 6. The FX layer: performance effects on the master

**Hold FX.** The white keys are effects that last while held:

| Key | Effect | Key | Effect |
| --- | --- | --- | --- |
| F3 | REPEAT 1/8 | F4 | FREEZE |
| G3 | REPEAT 1/16 | G4 | OCT UP |
| A3 | REPEAT 1/32 | A4 | OCT DN |
| B3 | REVERSE | B4 | GATE (the mix chopped on 16ths) |
| C4 | LPF sweep (closes over a bar) | C5 | DUB (into the delay, the echoes ring out) |
| D4 | HPF sweep (opens over a bar) | D5 | FLANGE |
| E4 | TAPE STOP | E5 | VINYL |

REPEAT and REVERSE start on the next sixteenth. **KNOB 1–4** are the layer's macros: FILTER (left low-pass, right
high-pass), CRUSH, THROW (the mix into delay and reverb) and DEPTH. Let go of FX and they return the way throws do.
SHIFT + FX latches the effects.

## 7. The GLO layer: fills, solos and tempo

**Hold GLO.**

- **F4 FILL**: while held, steps with the **FILL** condition play and steps with **!FILL** rest. Hold it at the end of
  a phrase for a fill.
- **F3 to D4**: solo T1 to T6 while held. **E4** unmutes everything. **G4** taps the tempo.
- **KNOB 1–4**: the levels of the four tracks shown.
- **GLO + PLAY** restarts from the top without stopping. **GLO + SELECT** sets the tempo.

## 8. Make a loop evolve on its own

These are set per step on **SEQ › NOTES / FEEL**. Select steps, then turn the knobs.

- **CHANCE** (FEEL, KNOB 1): 1–100 % odds that a step plays. Turn it past 100 % for the **trig conditions**:
  - **1:2, 2:2, 1:3 ... 4:4**: play on the Nth pass of every M passes.
  - **FILL / !FILL**: only while FILL is held, or only while it isn't.
  - **PRE / !PRE**: only if this track's previous condition passed, or only if it didn't.
  - **1ST / !1ST**: only on the first pass after PLAY, or only on every pass after it.
  - **NEI / !NEI**: only if the condition on the track *before* this one passed, or only if it didn't. T2 asks T1,
    T3 asks T2, and so on. Use it for call and response between tracks.
- **RATCHET** (FEEL, KNOB 2): ×1 to ×4 repeats inside a step, or FLAM. On the SEQ **RETRIG** page, **RETRIG VEL** fades
  the repeats in (+) or out (−).
- **NUDGE** (FEEL, KNOB 3): moves a step off the grid, in 64ths.
- **Parameter locks:** hold a step and turn KNOB 1–4. Those values change for that step only (p-locks).
  Every lock and recorded knob move is listed on **SEQ › AUTOMATION**.
- **MOMENTARY:** hold **LFO** and turn knobs on any sound or FX page. When you let go they come back, like a temporary
  tweak.

## 9. The arpeggiator, live

Set the arp on **ARP / ARP 2**: MODE, RATE (with **4T, 8T, 16T, 32T** triplets), OCTAVE, GATE, SWING and HOLD. The
**ARP STEP** page adds a step pattern to the arp:

- **KNOB 1 STEPS**: the pattern's length, 1–16. It runs against the notes' own cycle, so a 3-step pattern over 4 notes
  makes a polymeter.
- **KNOB 2 STEP**: the step to edit (PRESETS moves it too).
- **KNOB 3 TRANSP**: that step's note moved −24 to +24 semitones.
- **KNOB 4 STATE**: **ON**, **OFF** (a rest) or **STACC** (a quarter of the gate).

The default is 16 steps, all ON at 0, which plays the arpeggiator as it was. The pattern is kept with the project and
can also be edited in the web editor (Sound tab, ARP STEP). Many SYNTH presets now bring their own arp and LFO
settings: one with its arp on switches it on, one without leaves yours alone.

## 10. A way to play a set

1. Build each track's list on SEQ › SONG: A the intro, B the main part, C the break, and so on. Leave **SONG OFF**
   so each track loops what you launch.
2. Store the main moments as scenes: hold SAVE and press a white key, in PERFORM with KEYS SCENE.
3. Lock PERFORM (**SHIFT + ARP**) with **LAUNCH BAR**.
4. Play the arrangement:
   - move between scenes with the white keys;
   - bring tracks in and out with the black keys (tap to latch, hold to cut);
   - throw the cutoff, reverb and delay with the knobs.
5. Hold **FX** for repeats, tape stops and filter sweeps on the transitions, and **GLO + F4** for fills.
6. Let **CHANCE**, the **trig conditions** and the **ARP STEP** pattern keep the loops moving between your moves.

To see the screen big while you play, open the editor's **Screen** tab. It also works with Patrick F. Kellogg's
[FM-1 Mirror](https://github.com/pfkellogg/fm1-mirror) app.

---

<sub>Prophet-5, Sequential, Oberheim, Mutable Instruments, Plaits, Rings, Roland, TR-808, TR-909, TR-626, TB-303 and Devil Fish are trademarks or names of their respective owners. They are named only to describe what a feature was modelled on; skiff-atlatl is not affiliated with, endorsed or supported by them.</sub>
