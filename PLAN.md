# Akashic Vault — Implementation Plan

## Locked decisions

| Decision | Choice |
|----------|--------|
| OS | **Circle bare-metal** (no Linux) |
| Codebase | **Built from scratch** — AV-106/Dreamdexed are reference only, no source copied |
| Display/UI toolkit | **LVGL** (Circle `addon/lvgl`, v9.4.0) in monochrome (I1) mode — **not** u8g2 |
| Schwung relationship | **Port the portable C/C++ DSP cores** behind a common Akashic module interface |
| Primary hardware | **RPi 4** (RPi 3 supported as a lighter build) |
| Voice model (v1) | **Single active sound generator → 2–3 slot audio-FX chain**; interfaces designed so multi-timbral can be added later |

## Hardware target

Akashic Vault HAT on RPi 3/4: 128×64 OLED (SSD1306/1309, I2C), 5 encoders via
MCP23017 (1 nav + 4 value), TRS MIDI In/Out/Thru (UART) + USB MIDI In, I2S audio
out, SD card. Pin map in `docs/HARDWARE_PLATFORM.md`. This is a **MIDI sound
module** — no on-board keybed; polyphony is driven entirely by incoming MIDI.

## Signal flow

```
TRS MIDI In ┐
USB MIDI In ┼─▶ MIDI router ─▶ [MIDI FX chain] ─▶ Sound Generator ─▶ [Audio FX 1..3] ─▶ I2S out
            └─▶ MIDI Out / Thru (configurable routing)
```

## Core abstractions (the heart of v1)

A small set of stable interfaces is the most important deliverable — every module
plugs into these, and getting them right is what makes porting Schwung cores cheap.

- `ISoundGenerator` — `NoteOn/NoteOff/CC/PitchBend/...`, `Process(float* outL, outR, nFrames)`, parameter enumeration (id, name, range, display), preset get/set.
- `IAudioFX` — `Process(float* ioL, ioR, nFrames)`, bypass, parameters, preset get/set.
- `IMidiFX` — `Process(MIDIEvent in) -> 0..N MIDIEvent out`, clock-aware, parameters.
- `IModule` base — shared parameter model + serialization so the 4-row UI and the
  preset system treat every module uniformly.
- `CModuleRegistry` — enumerates available generators/FX so the UI selector can
  list them and instantiate by id.
- `CEngine` — owns the active generator + FX slots + MIDI FX chain, runs the audio
  block callback, applies the parameter model.
- `CPreset` — full snapshot (generator id + params, FX slots + params, MIDI FX,
  global MIDI/routing) serialized to SD.

The parameter model is the linchpin: if every module exposes parameters through one
typed interface, the UI, presets, and MIDI-learn all come "for free."

## Phased roadmap

**Phase 0 — Bootstrap & bring-up (from scratch).** New repo, fresh `src/` and
Makefile. Pull in `circle-stdlib` (**upstream smuehlst/circle-stdlib**) as a
submodule, then repoint its `libs/circle` at the **ATM circle fork**
(`Akashic-Trance-Machines/circle`) on a branch carrying the **SSD1309 + MCP23017
drivers** (PRs in flight to upstream circle). The OLED + LVGL + GPIO-expander are
all Circle addons (`CSSD1309Display`, `CLVGL`, `CMCP23017`). Get a
minimal kernel building for `RPI=4` and `RPI=3` that boots and: drives the OLED via
**Circle's `CLVGL` + `CSSD1309Display`** (auto monochrome I1, mono theme — a "hello"
screen), reads all 5 encoders + clicks from `CMCP23017`, receives TRS + USB MIDI,
and plays a sine tone out over I2S. This proves every driver before any synth code
exists.

**Phase 1 — Engine + interfaces + first generator.** Implement `IModule`,
`ISoundGenerator`, parameter model, `CEngine` block callback. Wire one reference
generator end-to-end. Start with **Dexed** (already ported in Dreamdexed) to
validate the interface, then add **Braids/Plaits** as a clean macro-oscillator.

**Phase 2 — Audio FX chain.** `IAudioFX` + 2–3 ordered FX slots with per-slot
bypass. Port **CloudSeed** (already done in Dreamdexed) plus one light effect
(Junologue chorus or tape delay) to exercise the chain.

**Phase 3 — UI integration.** Rebuild the 4-row UI in **LVGL** (monochrome),
matching the visual spec in `docs/UI_4ROW.md`. Menus: generator selector + params,
FX slot selectors + params, using the established **selector pattern**. Nav encoder
= scroll/back/enter; the 4 value encoders map directly to the 4 visible rows
(custom routing, not LVGL group navigation).

**Phase 4 — Presets on SD.** Save/load/browse, text-input naming screen (already
specified in `docs/UI_4ROW.md`). Define the on-disk format (see open items).

**Phase 5 — MIDI FX + global MIDI/routing.** `IMidiFX` chain; port **Super Arp**
and a **Euclidean** sequencer (Eucalypso). Global page: input channel(s), TRS vs
USB source, Out/Thru routing, clock source.

**Phase 6 — Expand the module catalog.** Port more generators/FX (priority list
below), one at a time, each verified on hardware.

**Phase 7 — Polish & optimize.** RPi3 headroom tuning, CPU/voice profiling,
denormal handling, boot time, edge cases.

## Porting priority (from the Schwung catalog)

Generators — easy/clean cores first: **Dexed** (done) → **Braids/Plaits** →
**OB-Xd** → **Hera (Juno-60)** → **HUSH ONE (SH-101)** / **NuSaw** → **Chiptune**.
Heavier (RAM/SD dependent, later): **FluidLite (SF2)**, **sfizz (SFZ)**, Surge-class.

Audio FX: **CloudSeed** (done) → **Junologue Chorus** → **TapeDelay** → **PSX
Verb** → **Gate** / **Usefulity (utility)** → **Vocoder** (needs line-in).

MIDI FX: **Super Arp** → **Eucalypso** (Euclidean).

Out of scope for bare-metal Circle (need Linux/network/heavy runtimes): Webstream,
Radio Garden, AirPlay, Stems, Time Stretch, NAM (neural), CLAP host, RNBO.

## Licensing

Schwung is MIT, but bundled engines vary (MIT, BSD, **GPL**, CC). Track the license
of every ported core in a `THIRD_PARTY_LICENSES.md` and keep GPL-incompatible
engines isolated or reimplemented if the project license requires it. Decide the
Akashic Vault project license early (Dreamdexed/AV-106 ship GPL-style LICENSE).

## Locked parameters (confirmed)

1. **Audio format**: **48 kHz**, stereo, float internal, block size **256**
   (same on RPi3; revisit only if RPi3 underruns).
2. **Preset on-disk format**: **versioned binary blob** (header with format version
   + module ids) **+ a human-readable text index** for browsing.
3. **FX slots**: **3** (each may be "None").
4. **Multi-timbral hook**: **yes** — `CEngine` is built to host N generators
   internally; the v1 UI exposes one.
5. **MIDI clock**: **both, selectable** — internal clock or external sync from
   TRS/USB.

## Module distribution & menu format (confirmed)

- **Each module lives in its own Git repository** and is brought into the main repo
  as a **submodule** under `src/modules/<kind>/<name>/`. This keeps third-party DSP
  cores, their licenses, and their credits self-contained and independently
  updatable.
- **Each module ships a `menu.json`** that declares its parameters and menu/
  sub-menu structure in a human-readable form (id, label, type, range, default,
  display formatting, grouping into sub-pages). This JSON is the **source of truth**
  for the module's UI.
- **Recommended pipeline: build-time codegen.** Because modules are compiled into
  the kernel (no dynamic loading on bare-metal Circle), a small generator script
  turns each `menu.json` into C parameter tables (`TParamDesc[]` + menu tree) at
  build time. Zero runtime JSON parsing, type-checked, but humans still edit JSON.
  (Alternative — parse JSON from SD at runtime — is possible but adds a parser and
  buys little here since modules aren't hot-loadable. Flagging for confirmation.)
- Schema and per-module repo layout: see `docs/MODULE_FORMAT.md`.

## Licensing & attribution (confirmed)

- **Project license: GPL-3.0** (`LICENSE`). Consistent with Dreamdexed/AV-106 and
  GPL-friendly for the engines we port.
- **Any copied/ported code must comply with its upstream license and credit the
  source.** Keep the original license header in vendored files, record every
  third-party source in `THIRD_PARTY_LICENSES.md` (and a `CREDITS`/`NOTICE` in each
  module repo), and verify GPL-compatibility before merging. Engines that are not
  GPL-compatible are either isolated appropriately or reimplemented.
