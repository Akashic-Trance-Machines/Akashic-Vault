# Akashic Vault

A modular DSP synthesizer / sound module for **Akashic Vault hardware** driven by a
Raspberry Pi 3/4. Part of the Akashic Trance Machines family.

## Concept

Akashic Vault is a **MIDI sound module**: the user picks one (or more) sound
generators, chains 2–3 audio FX and one or more MIDI FX, and saves presets to the
SD card. There is no on-board keybed — the instrument is played over MIDI.

It reuses the **Dreamdexed / AV-106 UI/UX**: a 128×64 OLED with the 4‑row
hierarchical menu, one navigation encoder (scroll + back/enter) and four value
encoders (change value / step into menu / select).

## Hardware (Akashic Vault)

| Item | Detail |
|------|--------|
| Compute | Raspberry Pi 3 / 4 |
| Display | 1× OLED, 128×64 (SSD1306/SSD1309, I2C) |
| Encoders | 5 total — 1 nav (scroll/back) + 4 value, via MCP23017 |
| MIDI | TRS MIDI In / Out / Thru (UART) + USB MIDI In |
| Audio | I2S out to external DAC |
| Storage | SD card (presets, assets) |

Full pin map and UI spec: see [`docs/HARDWARE_PLATFORM.md`](docs/HARDWARE_PLATFORM.md)
and [`docs/UI_4ROW.md`](docs/UI_4ROW.md).

## Goal

Run the same or forked sound/FX modules from the **Schwung** catalog
(https://schwung.dev/catalog.html). See [`docs/SCHWUNG_NOTES.md`](docs/SCHWUNG_NOTES.md)
for the architectural analysis of what "running Schwung modules" actually means on
this platform.

## Modules

Each sound generator, audio FX and MIDI FX is its **own Git repository**, included
here as a submodule, with its parameters and menu layout declared in a
human-readable `menu.json`. See [`docs/MODULE_FORMAT.md`](docs/MODULE_FORMAT.md).

## License & credits

Akashic Vault is **GPL-3.0** (see [`LICENSE`](LICENSE)). Ported/copied code keeps
its upstream license and is credited in
[`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) and each module's `CREDITS.md`.

## Build

```bash
./scripts/setup-submodules.sh                 # one-time: circle-stdlib + CMSIS_5
./scripts/build.sh all                        # build/kernel8.img + kernel8-rpi4.img
./scripts/deploy-sdcard.sh /Volumes/AKASHIC   # both kernels on one card
```

`build.sh 3` / `build.sh 4` build a single target. See [`docs/PHASE0.md`](docs/PHASE0.md).

## Status

🚧 **Phase 0 (bring-up) scaffolded.** Architecture locked ([`PLAN.md`](PLAN.md)).
Engine interfaces + menu codegen are host-compiled clean; kernel driver wiring
(OLED/MCP23017/MIDI/I2S) is the remaining Phase 0 work — see the milestones in
[`docs/PHASE0.md`](docs/PHASE0.md).
